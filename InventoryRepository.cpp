#include "InventoryRepository.h"

#include <ctime>
#include <iomanip>
#include <sstream>

static std::string currentTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm local;
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&local, &now);
#endif
    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%dT%H:%M:%S");
    return output.str();
}

InventoryRepository::InventoryRepository(DatabaseManager& database)
    : database_(database) {}

bool InventoryRepository::getStock(const std::string& productId, int& quantity,
                                   std::string& error) const {
    sqlite3* db = database_.getHandle();
    sqlite3_stmt* statement = nullptr;
    if (!db) {
        error = "Database is not open.";
        return false;
    }
    if (sqlite3_prepare_v2(db,
                           "SELECT quantity FROM inventory WHERE product_id = ?;",
                           -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_text(statement, 1, productId.c_str(), -1, SQLITE_TRANSIENT);
    int result = sqlite3_step(statement);
    if (result != SQLITE_ROW) {
        error = "Inventory record not found.";
        sqlite3_finalize(statement);
        return false;
    }
    quantity = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return true;
}

bool InventoryRepository::restock(const std::string& productId, int quantity,
                                  const std::string& reason,
                                  std::string& error,
                                  const std::string& operatorName) {
    if (quantity <= 0) {
        error = "Restock quantity must be positive.";
        return false;
    }
    if (reason.empty()) {
        error = "Restock reason cannot be empty.";
        return false;
    }
    return adjust(productId, quantity, reason, error, operatorName);
}

bool InventoryRepository::adjust(const std::string& productId,
                                 int quantityChange,
                                 const std::string& reason,
                                 std::string& error,
                                 const std::string& operatorName) {
    if (quantityChange == 0) {
        error = "Stock adjustment cannot be zero.";
        return false;
    }
    if (reason.empty()) {
        error = "Stock adjustment reason cannot be empty.";
        return false;
    }

    sqlite3* db = database_.getHandle();
    if (!db) {
        error = "Database is not open.";
        return false;
    }
    if (!database_.beginTransaction()) {
        error = database_.getLastError();
        return false;
    }

    sqlite3_stmt* read = nullptr;
    if (sqlite3_prepare_v2(db,
                           "SELECT quantity FROM inventory WHERE product_id = ?;",
                           -1, &read, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_bind_text(read, 1, productId.c_str(), -1, SQLITE_TRANSIENT);
    int result = sqlite3_step(read);
    if (result != SQLITE_ROW) {
        error = "Inventory record not found.";
        sqlite3_finalize(read);
        database_.rollbackTransaction();
        return false;
    }
    int oldQuantity = sqlite3_column_int(read, 0);
    sqlite3_finalize(read);

    if (quantityChange < 0 && oldQuantity + quantityChange < 0) {
        error = "Stock adjustment would make quantity negative.";
        database_.rollbackTransaction();
        return false;
    }
    int resultingQuantity = oldQuantity + quantityChange;

    sqlite3_stmt* update = nullptr;
    if (sqlite3_prepare_v2(db,
                           "UPDATE inventory SET quantity = ? WHERE product_id = ?;",
                           -1, &update, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_bind_int(update, 1, resultingQuantity);
    sqlite3_bind_text(update, 2, productId.c_str(), -1, SQLITE_TRANSIENT);
    result = sqlite3_step(update);
    sqlite3_finalize(update);
    if (result != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }

    sqlite3_stmt* log = nullptr;
    if (sqlite3_prepare_v2(db,
                           "INSERT INTO inventory_logs"
                           "(product_id, quantity_change, reason, resulting_quantity, "
                           "logged_at, operator_name) "
                           "VALUES(?, ?, ?, ?, ?, ?);",
                           -1, &log, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    std::string timestamp = currentTimestamp();
    sqlite3_bind_text(log, 1, productId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(log, 2, quantityChange);
    sqlite3_bind_text(log, 3, reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(log, 4, resultingQuantity);
    sqlite3_bind_text(log, 5, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(log, 6, operatorName.c_str(), -1, SQLITE_TRANSIENT);
    result = sqlite3_step(log);
    sqlite3_finalize(log);
    if (result != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }

    if (!database_.commitTransaction()) {
        error = database_.getLastError();
        return false;
    }
    return true;
}

bool InventoryRepository::getLatestLog(const std::string& productId,
                                       InventoryLog& log,
                                       std::string& error) const {
    sqlite3* db = database_.getHandle();
    sqlite3_stmt* statement = nullptr;
    if (!db) {
        error = "Database is not open.";
        return false;
    }
    if (sqlite3_prepare_v2(db,
                           "SELECT log_id, product_id, quantity_change, reason, "
                           "resulting_quantity, logged_at, operator_name "
                           "FROM inventory_logs WHERE product_id = ? "
                           "ORDER BY log_id DESC LIMIT 1;",
                           -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_text(statement, 1, productId.c_str(), -1, SQLITE_TRANSIENT);
    int result = sqlite3_step(statement);
    if (result != SQLITE_ROW) {
        error = "Inventory log not found.";
        sqlite3_finalize(statement);
        return false;
    }
    log.logId = sqlite3_column_int(statement, 0);
    log.productId = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
    log.quantityChange = sqlite3_column_int(statement, 2);
    log.reason = reinterpret_cast<const char*>(sqlite3_column_text(statement, 3));
    log.resultingQuantity = sqlite3_column_int(statement, 4);
    log.loggedAt = reinterpret_cast<const char*>(sqlite3_column_text(statement, 5));
    const unsigned char* operatorText = sqlite3_column_text(statement, 6);
    log.operatorName = operatorText
        ? reinterpret_cast<const char*>(operatorText) : "Unknown";
    sqlite3_finalize(statement);
    return true;
}
