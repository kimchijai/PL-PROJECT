#include "doctest.h"
#include "../gui/PosController.h"
#include "../DatabaseManager.h"

#include <cstdio>

namespace {
const char* ACCOUNT_DB = "tests/operator_accounts_test.db";
const std::string ADMIN_PASSWORD = "AdminPassword!2026";
}

TEST_CASE("First administrator setup stores a salted password hash and cannot repeat") {
    std::remove(ACCOUNT_DB);
    PosController controller(ACCOUNT_DB);
    REQUIRE(controller.isReady());

    std::string error;
    bool hasAdministrator = true;
    REQUIRE(controller.hasAdministrator(hasAdministrator, error));
    CHECK_FALSE(hasAdministrator);
    CHECK_FALSE(controller.createInitialAdministrator(
        "bad username", ADMIN_PASSWORD, error));
    CHECK_FALSE(controller.createInitialAdministrator(
        "first_admin", "short", error));
    CHECK_FALSE(controller.login("first_admin", ADMIN_PASSWORD, error));

    REQUIRE(controller.createInitialAdministrator(
        "first_admin", ADMIN_PASSWORD, error));
    REQUIRE(controller.hasAdministrator(hasAdministrator, error));
    CHECK(hasAdministrator);
    CHECK_FALSE(controller.createInitialAdministrator(
        "second_admin", ADMIN_PASSWORD, error));
    CHECK(error.find("already") != std::string::npos);
    CHECK_FALSE(controller.isAuthenticated());
    CHECK_FALSE(controller.login("first_admin", "wrong password", error));
    CHECK_FALSE(controller.login("first_admin", std::string(1025, 'x'), error));
    REQUIRE(controller.login("FIRST_ADMIN", ADMIN_PASSWORD, error));
    CHECK(controller.isAuthenticated());
    CHECK(controller.isAdministrator());
    CHECK(controller.currentOperatorName() == "first_admin");

    DatabaseManager database;
    REQUIRE(database.open(ACCOUNT_DB));
    sqlite3_stmt* statement = nullptr;
    REQUIRE(sqlite3_prepare_v2(
        database.getHandle(),
        "SELECT password_hash FROM operator_accounts WHERE username = 'first_admin';",
        -1, &statement, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(statement) == SQLITE_ROW);
    const std::string passwordHash = reinterpret_cast<const char*>(
        sqlite3_column_text(statement, 0));
    CHECK(passwordHash != ADMIN_PASSWORD);
    CHECK(passwordHash.find("pbkdf2-sha256$600000$") == 0);
    sqlite3_finalize(statement);
    database.close();

    controller.logout();
    CHECK_FALSE(controller.isAuthenticated());
    CHECK(controller.currentOperatorName().empty());
    std::remove(ACCOUNT_DB);
}

TEST_CASE("Administrator can edit and deactivate accounts without changing activity snapshots") {
    std::remove(ACCOUNT_DB);
    PosController administrator(ACCOUNT_DB);
    REQUIRE(administrator.isReady());

    std::string error;
    REQUIRE(administrator.createInitialAdministrator(
        "main_admin", ADMIN_PASSWORD, error));
    REQUIRE(administrator.login("main_admin", ADMIN_PASSWORD, error));
    CHECK_FALSE(administrator.deactivateOperatorAccount(
        administrator.currentOperatorAccountId(), error));

    REQUIRE(administrator.createOperatorAccount(
        "cashier_one", "CashierPassword!2026", "operator", error));
    std::vector<OperatorAccount> accounts;
    REQUIRE(administrator.listOperatorAccounts(accounts, error));
    REQUIRE(accounts.size() == 2);
    int cashierId = 0;
    int adminId = 0;
    for (const auto& account : accounts) {
        if (account.username == "cashier_one")
            cashierId = account.accountId;
        if (account.username == "main_admin")
            adminId = account.accountId;
    }
    REQUIRE(cashierId > 0);
    REQUIRE(adminId > 0);

    CHECK_FALSE(administrator.updateOperatorAccount(
        cashierId, "cashier_two", "operator", "short", error));
    REQUIRE(administrator.updateOperatorAccount(
        cashierId, "cashier_two", "operator",
        "UpdatedCashierPassword!26", error));

    PosController cashier(ACCOUNT_DB);
    REQUIRE(cashier.isReady());
    CHECK_FALSE(cashier.login("cashier_one", "CashierPassword!2026", error));
    REQUIRE(cashier.login("cashier_two", "UpdatedCashierPassword!26", error));
    CHECK_FALSE(cashier.isAdministrator());
    CHECK_FALSE(cashier.createOperatorAccount(
        "unauthorized", "UnauthorizedPassword!26", "operator", error));

    REQUIRE(cashier.addToCart("P001", 1, error));
    TransactionRecord sale{};
    std::vector<TransactionItemRecord> items;
    REQUIRE(cashier.checkout(6500, sale, items, error));
    CHECK(sale.operatorName == "cashier_two");

    REQUIRE(administrator.deactivateOperatorAccount(cashierId, error));
    CHECK_FALSE(cashier.adjustStock("P001", -1, "After account deactivation", error));
    CHECK(error.find("inactive") != std::string::npos);

    TransactionRecord historicalSale{};
    REQUIRE(administrator.transaction(sale.transactionId, historicalSale, error));
    CHECK(historicalSale.operatorName == "cashier_two");
    REQUIRE(administrator.deactivateOperatorAccount(cashierId, error) == false);

    REQUIRE(administrator.createOperatorAccount(
        "backup_admin", "BackupAdminPassword!26", "administrator", error));
    REQUIRE(administrator.listOperatorAccounts(accounts, error));
    int backupAdminId = 0;
    for (const auto& account : accounts) {
        if (account.username == "backup_admin")
            backupAdminId = account.accountId;
    }
    REQUIRE(backupAdminId > 0);
    administrator.logout();
    REQUIRE(administrator.login("backup_admin", "BackupAdminPassword!26", error));
    REQUIRE(administrator.deactivateOperatorAccount(adminId, error));
    CHECK_FALSE(administrator.deactivateOperatorAccount(backupAdminId, error));
    CHECK(error.find("Log out") != std::string::npos);
    DatabaseManager database;
    REQUIRE(database.open(ACCOUNT_DB));
    OperatorAccountRepository repository(database);
    CHECK_FALSE(repository.deactivateAccount(backupAdminId, error));
    CHECK(error.find("last active administrator") != std::string::npos);
    CHECK_FALSE(repository.updateAccount(
        backupAdminId, "backup_admin", "operator", "", error));
    CHECK(error.find("last active administrator") != std::string::npos);
    database.close();

    std::remove(ACCOUNT_DB);
}
