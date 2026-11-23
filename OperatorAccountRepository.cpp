#include "OperatorAccountRepository.h"

#include <algorithm>
#include <charconv>
#include <limits>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

namespace {
constexpr int PBKDF2_ITERATIONS = 600000;
constexpr std::size_t SALT_LENGTH = 16;
constexpr std::size_t KEY_LENGTH = 32;
constexpr std::size_t MAX_PASSWORD_LENGTH = 1024;
constexpr std::size_t MAX_USERNAME_LENGTH = 32;

std::string toHex(const unsigned char* bytes, std::size_t length) {
    static const char digits[] = "0123456789abcdef";
    std::string encoded;
    encoded.reserve(length * 2);
    for (std::size_t i = 0; i < length; ++i) {
        encoded.push_back(digits[bytes[i] >> 4]);
        encoded.push_back(digits[bytes[i] & 0x0f]);
    }
    return encoded;
}

bool fromHex(const std::string& encoded, std::string& bytes) {
    if (encoded.empty() || encoded.size() % 2 != 0)
        return false;
    auto decode = [](char value) -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
    };
    bytes.clear();
    bytes.reserve(encoded.size() / 2);
    for (std::size_t i = 0; i < encoded.size(); i += 2) {
        const int high = decode(encoded[i]);
        const int low = decode(encoded[i + 1]);
        if (high < 0 || low < 0)
            return false;
        bytes.push_back(static_cast<char>((high << 4) | low));
    }
    return true;
}

bool deriveKey(const std::string& password, const std::string& salt,
               int iterations, std::string& key) {
    key.resize(KEY_LENGTH);
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                    nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) < 0)
        return false;
    const NTSTATUS result = BCryptDeriveKeyPBKDF2(
        algorithm,
        reinterpret_cast<PUCHAR>(const_cast<char*>(password.data())),
        static_cast<ULONG>(password.size()),
        reinterpret_cast<PUCHAR>(const_cast<char*>(salt.data())),
        static_cast<ULONG>(salt.size()),
        static_cast<ULONGLONG>(iterations),
        reinterpret_cast<PUCHAR>(&key[0]),
        static_cast<ULONG>(key.size()), 0);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return result >= 0;
#else
    if (password.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())
        || salt.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return false;
    return PKCS5_PBKDF2_HMAC(
        password.data(), static_cast<int>(password.size()),
        reinterpret_cast<const unsigned char*>(salt.data()),
        static_cast<int>(salt.size()), iterations, EVP_sha256(),
        static_cast<int>(key.size()),
        reinterpret_cast<unsigned char*>(&key[0])) == 1;
#endif
}

bool randomSalt(std::string& salt) {
    salt.resize(SALT_LENGTH);
#ifdef _WIN32
    return BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&salt[0]),
                           static_cast<ULONG>(salt.size()),
                           BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
#else
    return RAND_bytes(reinterpret_cast<unsigned char*>(&salt[0]),
                      static_cast<int>(salt.size())) == 1;
#endif
}

bool hashPassword(const std::string& password, std::string& encoded) {
    std::string salt;
    std::string key;
    if (!randomSalt(salt) || !deriveKey(password, salt, PBKDF2_ITERATIONS, key))
        return false;
    encoded = "pbkdf2-sha256$" + std::to_string(PBKDF2_ITERATIONS) + "$"
        + toHex(reinterpret_cast<const unsigned char*>(salt.data()), salt.size())
        + "$"
        + toHex(reinterpret_cast<const unsigned char*>(key.data()), key.size());
    return true;
}

bool verifyPassword(const std::string& password, const std::string& encoded) {
    const std::size_t first = encoded.find('$');
    const std::size_t second = first == std::string::npos
        ? std::string::npos : encoded.find('$', first + 1);
    const std::size_t third = second == std::string::npos
        ? std::string::npos : encoded.find('$', second + 1);
    if (first == std::string::npos || second == std::string::npos
        || third == std::string::npos
        || encoded.substr(0, first) != "pbkdf2-sha256")
        return false;

    const std::string count = encoded.substr(first + 1, second - first - 1);
    int iterations = 0;
    const auto parsed = std::from_chars(
        count.data(), count.data() + count.size(), iterations);
    if (parsed.ec != std::errc{} || parsed.ptr != count.data() + count.size())
        return false;
    if (iterations < PBKDF2_ITERATIONS || iterations > 2000000)
        return false;

    std::string salt;
    std::string expected;
    if (!fromHex(encoded.substr(second + 1, third - second - 1), salt)
        || salt.size() != SALT_LENGTH
        || !fromHex(encoded.substr(third + 1), expected)
        || expected.size() != KEY_LENGTH)
        return false;

    std::string actual;
    if (!deriveKey(password, salt, iterations, actual))
        return false;
    unsigned char difference = 0;
    for (std::size_t i = 0; i < KEY_LENGTH; ++i) {
        difference |= static_cast<unsigned char>(actual[i] ^ expected[i]);
    }
    return difference == 0;
}

bool readAccount(sqlite3_stmt* statement, OperatorAccount& account) {
    if (sqlite3_column_type(statement, 0) == SQLITE_NULL
        || sqlite3_column_type(statement, 1) == SQLITE_NULL
        || sqlite3_column_type(statement, 2) == SQLITE_NULL)
        return false;
    account.accountId = sqlite3_column_int(statement, 0);
    account.username = reinterpret_cast<const char*>(
        sqlite3_column_text(statement, 1));
    account.role = reinterpret_cast<const char*>(
        sqlite3_column_text(statement, 2));
    account.active = sqlite3_column_int(statement, 3) != 0;
    return true;
}

bool bindText(sqlite3_stmt* statement, int index, const std::string& value) {
    return sqlite3_bind_text(statement, index, value.c_str(), -1,
                             SQLITE_TRANSIENT) == SQLITE_OK;
}

bool activeAdministratorCount(sqlite3* db, int& count, std::string& error) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db, "SELECT COUNT(*) FROM operator_accounts "
                "WHERE role = 'administrator' AND active = 1;",
            -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    const int result = sqlite3_step(statement);
    if (result == SQLITE_ROW)
        count = sqlite3_column_int(statement, 0);
    else
        error = sqlite3_errmsg(db);
    sqlite3_finalize(statement);
    return result == SQLITE_ROW;
}
}

OperatorAccountRepository::OperatorAccountRepository(DatabaseManager& database)
    : database_(database) {}

bool OperatorAccountRepository::validUsername(const std::string& username) {
    if (username.size() < 3 || username.size() > MAX_USERNAME_LENGTH)
        return false;
    return std::all_of(username.begin(), username.end(), [](unsigned char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
            || (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' || ch == '-';
    });
}

bool OperatorAccountRepository::validPassword(const std::string& password) {
    return password.size() >= 12 && password.size() <= MAX_PASSWORD_LENGTH;
}

bool OperatorAccountRepository::validRole(const std::string& role) {
    return role == "administrator" || role == "operator";
}

bool OperatorAccountRepository::hasAdministrator(bool& exists,
                                                  std::string& error) const {
    sqlite3* db = database_.getHandle();
    if (!db) {
        error = "Database is not open.";
        return false;
    }
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db, "SELECT EXISTS(SELECT 1 FROM operator_accounts "
                "WHERE role = 'administrator');",
            -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    const int result = sqlite3_step(statement);
    if (result == SQLITE_ROW)
        exists = sqlite3_column_int(statement, 0) != 0;
    else
        error = sqlite3_errmsg(db);
    sqlite3_finalize(statement);
    return result == SQLITE_ROW;
}

bool OperatorAccountRepository::createInitialAdministrator(
    const std::string& username, const std::string& password,
    OperatorAccount& account, std::string& error) {
    if (!validUsername(username)) {
        error = "Username must be 3-32 characters using letters, numbers, '.', '_' or '-'.";
        return false;
    }
    if (!validPassword(password)) {
        error = "Password must be at least 12 characters.";
        return false;
    }
    if (!database_.beginTransaction()) {
        error = database_.getLastError();
        return false;
    }

    bool exists = false;
    if (!hasAdministrator(exists, error)) {
        database_.rollbackTransaction();
        return false;
    }
    if (exists) {
        error = "Administrator setup has already been completed.";
        database_.rollbackTransaction();
        return false;
    }
    if (!insertAccount(username, password, "administrator", account, error)) {
        database_.rollbackTransaction();
        return false;
    }
    if (!database_.commitTransaction()) {
        error = database_.getLastError();
        return false;
    }
    return true;
}

bool OperatorAccountRepository::authenticate(
    const std::string& username, const std::string& password,
    OperatorAccount& account, std::string& error) const {
    sqlite3* db = database_.getHandle();
    if (!db) {
        error = "Database is not open.";
        return false;
    }
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db, "SELECT account_id, username, role, active, password_hash "
                "FROM operator_accounts WHERE username = ? COLLATE NOCASE;",
            -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    if (!bindText(statement, 1, username)) {
        error = sqlite3_errmsg(db);
        sqlite3_finalize(statement);
        return false;
    }

    const int result = sqlite3_step(statement);
    bool found = false;
    bool active = false;
    OperatorAccount candidate{};
    std::string encodedHash;
    if (result == SQLITE_ROW) {
        const unsigned char* hashText = sqlite3_column_text(statement, 4);
        encodedHash = hashText
            ? reinterpret_cast<const char*>(hashText) : "";
        found = readAccount(statement, candidate);
        active = candidate.active;
    } else if (result != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        sqlite3_finalize(statement);
        return false;
    }
    sqlite3_finalize(statement);
    if (!found) {
        encodedHash =
            "pbkdf2-sha256$600000$00000000000000000000000000000000$"
            "0000000000000000000000000000000000000000000000000000000000000000";
    }
    const bool passwordWithinLimit = password.size() <= MAX_PASSWORD_LENGTH;
    const std::string passwordToVerify = passwordWithinLimit ? password : "";
    const bool passwordMatches = verifyPassword(passwordToVerify, encodedHash);
    const bool valid = found && active && passwordWithinLimit && passwordMatches;
    if (!valid) {
        error = "Invalid username or password.";
        return false;
    }
    account = candidate;
    return true;
}

bool OperatorAccountRepository::getActiveAccount(
    int accountId, OperatorAccount& account, std::string& error) const {
    sqlite3* db = database_.getHandle();
    if (!db) {
        error = "Database is not open.";
        return false;
    }
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db, "SELECT account_id, username, role, active "
                "FROM operator_accounts WHERE account_id = ? AND active = 1;",
            -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_int(statement, 1, accountId);
    const int result = sqlite3_step(statement);
    if (result == SQLITE_ROW) {
        const bool ok = readAccount(statement, account);
        sqlite3_finalize(statement);
        if (!ok) error = "Operator account record is invalid.";
        return ok;
    }
    if (result == SQLITE_DONE)
        error = "The logged-in account is inactive or no longer exists.";
    else
        error = sqlite3_errmsg(db);
    sqlite3_finalize(statement);
    return false;
}

bool OperatorAccountRepository::listAccounts(
    std::vector<OperatorAccount>& accounts, std::string& error) const {
    sqlite3* db = database_.getHandle();
    if (!db) {
        error = "Database is not open.";
        return false;
    }
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db, "SELECT account_id, username, role, active "
                "FROM operator_accounts ORDER BY username COLLATE NOCASE;",
            -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    std::vector<OperatorAccount> found;
    int result = SQLITE_OK;
    while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
        OperatorAccount account{};
        if (!readAccount(statement, account)) {
            error = "Operator account record is invalid.";
            sqlite3_finalize(statement);
            return false;
        }
        found.push_back(std::move(account));
    }
    sqlite3_finalize(statement);
    if (result != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        return false;
    }
    accounts = std::move(found);
    return true;
}

bool OperatorAccountRepository::insertAccount(
    const std::string& username, const std::string& password,
    const std::string& role, OperatorAccount& account, std::string& error) {
    std::string passwordHash;
    if (!hashPassword(password, passwordHash)) {
        error = "Secure password hashing failed.";
        return false;
    }
    sqlite3* db = database_.getHandle();
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db, "INSERT INTO operator_accounts"
                "(username, password_hash, role) VALUES(?, ?, ?);",
            -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    if (!bindText(statement, 1, username)
        || !bindText(statement, 2, passwordHash)
        || !bindText(statement, 3, role)) {
        error = sqlite3_errmsg(db);
        sqlite3_finalize(statement);
        return false;
    }
    const int result = sqlite3_step(statement);
    sqlite3_finalize(statement);
    if (result != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        return false;
    }
    account = {static_cast<int>(sqlite3_last_insert_rowid(db)),
               username, role, true};
    return true;
}

bool OperatorAccountRepository::createAccount(
    const std::string& username, const std::string& password,
    const std::string& role, OperatorAccount& account, std::string& error) {
    if (!validUsername(username)) {
        error = "Username must be 3-32 characters using letters, numbers, '.', '_' or '-'.";
        return false;
    }
    if (!validPassword(password)) {
        error = "Password must be at least 12 characters.";
        return false;
    }
    if (!validRole(role)) {
        error = "Choose a valid account role.";
        return false;
    }
    return insertAccount(username, password, role, account, error);
}

bool OperatorAccountRepository::updateAccount(
    int accountId, const std::string& username, const std::string& role,
    const std::string& newPassword, std::string& error) {
    if (!validUsername(username)) {
        error = "Username must be 3-32 characters using letters, numbers, '.', '_' or '-'.";
        return false;
    }
    if (!validRole(role)) {
        error = "Choose a valid account role.";
        return false;
    }
    if (!newPassword.empty() && !validPassword(newPassword)) {
        error = "Password must be at least 12 characters.";
        return false;
    }

    std::string passwordHash;
    if (!newPassword.empty() && !hashPassword(newPassword, passwordHash)) {
        error = "Secure password hashing failed.";
        return false;
    }
    if (!database_.beginTransaction()) {
        error = database_.getLastError();
        return false;
    }
    OperatorAccount existing{};
    if (!getActiveAccount(accountId, existing, error)) {
        database_.rollbackTransaction();
        return false;
    }
    if (existing.role == "administrator" && role != "administrator") {
        int activeAdmins = 0;
        if (!activeAdministratorCount(
                database_.getHandle(), activeAdmins, error)) {
            database_.rollbackTransaction();
            return false;
        }
        if (activeAdmins <= 1) {
            error = "The last active administrator cannot be changed to an operator.";
            database_.rollbackTransaction();
            return false;
        }
    }
    sqlite3_stmt* statement = nullptr;
    const char* sql = newPassword.empty()
        ? "UPDATE operator_accounts SET username = ?, role = ?, "
          "updated_at = CURRENT_TIMESTAMP WHERE account_id = ? AND active = 1;"
        : "UPDATE operator_accounts SET username = ?, role = ?, password_hash = ?, "
          "updated_at = CURRENT_TIMESTAMP WHERE account_id = ? AND active = 1;";
    if (sqlite3_prepare_v2(database_.getHandle(), sql, -1,
                           &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(database_.getHandle());
        database_.rollbackTransaction();
        return false;
    }
    bool bound = bindText(statement, 1, username)
        && bindText(statement, 2, role);
    if (newPassword.empty())
        bound = bound && sqlite3_bind_int(statement, 3, accountId) == SQLITE_OK;
    else {
        bound = bound && bindText(statement, 3, passwordHash)
            && sqlite3_bind_int(statement, 4, accountId) == SQLITE_OK;
    }
    if (!bound) {
        error = sqlite3_errmsg(database_.getHandle());
        sqlite3_finalize(statement);
        database_.rollbackTransaction();
        return false;
    }
    const int result = sqlite3_step(statement);
    const bool changed = sqlite3_changes(database_.getHandle()) == 1;
    sqlite3_finalize(statement);
    if (result != SQLITE_DONE || !changed) {
        error = result == SQLITE_DONE
            ? "Operator account was not updated."
            : sqlite3_errmsg(database_.getHandle());
        database_.rollbackTransaction();
        return false;
    }
    if (!database_.commitTransaction()) {
        error = database_.getLastError();
        return false;
    }
    return true;
}

bool OperatorAccountRepository::deactivateAccount(
    int accountId, std::string& error) {
    if (!database_.beginTransaction()) {
        error = database_.getLastError();
        return false;
    }
    OperatorAccount existing{};
    if (!getActiveAccount(accountId, existing, error)) {
        database_.rollbackTransaction();
        return false;
    }
    if (existing.role == "administrator") {
        int activeAdmins = 0;
        if (!activeAdministratorCount(
                database_.getHandle(), activeAdmins, error)) {
            database_.rollbackTransaction();
            return false;
        }
        if (activeAdmins <= 1) {
            error = "The last active administrator cannot be deactivated.";
            database_.rollbackTransaction();
            return false;
        }
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            database_.getHandle(),
            "UPDATE operator_accounts SET active = 0, "
            "updated_at = CURRENT_TIMESTAMP WHERE account_id = ? AND active = 1;",
            -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(database_.getHandle());
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_bind_int(statement, 1, accountId);
    const int result = sqlite3_step(statement);
    const bool changed = sqlite3_changes(database_.getHandle()) == 1;
    sqlite3_finalize(statement);
    if (result != SQLITE_DONE || !changed) {
        error = result == SQLITE_DONE
            ? "Operator account was not deactivated."
            : sqlite3_errmsg(database_.getHandle());
        database_.rollbackTransaction();
        return false;
    }
    if (!database_.commitTransaction()) {
        error = database_.getLastError();
        return false;
    }
    return true;
}
