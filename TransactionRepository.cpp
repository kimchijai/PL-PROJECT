#include "TransactionRepository.h"

#include <ctime>
#include <iomanip>
#include <sstream>

static std::string transactionTimestamp() {
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

TransactionRepository::TransactionRepository(DatabaseManager& database)
    : database_(database) {}

bool TransactionRepository::createAtomic(
    const std::vector<TransactionItemInput>& items,
    int cashReceivedCents,
    TransactionRecord& transaction,
    std::vector<TransactionItemRecord>& savedItems,
    std::string& error,
    const std::string& operatorName) {
    savedItems.clear();
    if (items.empty()) {
        error = "Cart is empty.";
        return false;
    }
    std::vector<TransactionItemInput> normalizedItems;
    for (const auto& input : items) {
        bool merged = false;
        for (auto& normalized : normalizedItems) {
            if (normalized.productId == input.productId) {
                normalized.quantity += input.quantity;
                merged = true;
                break;
            }
        }
        if (!merged) normalizedItems.push_back(input);
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

    int orderTotalCents = 0;
    for (const auto& input : normalizedItems) {
        if (input.quantity <= 0) {
            error = "Cart quantities must be positive.";
            database_.rollbackTransaction();
            return false;
        }

        sqlite3_stmt* product = nullptr;
        if (sqlite3_prepare_v2(db,
                               "SELECT name, price_cents, active "
                               "FROM products WHERE product_id = ?;",
                               -1, &product, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_text(product, 1, input.productId.c_str(), -1, SQLITE_TRANSIENT);
        int result = sqlite3_step(product);
        if (result != SQLITE_ROW) {
            error = "Product not found: " + input.productId;
            sqlite3_finalize(product);
            database_.rollbackTransaction();
            return false;
        }
        if (sqlite3_column_int(product, 2) == 0) {
            error = "Inactive product: " + input.productId;
            sqlite3_finalize(product);
            database_.rollbackTransaction();
            return false;
        }
        const char* nameText = reinterpret_cast<const char*>(sqlite3_column_text(product, 0));
        std::string snapshotName = nameText ? nameText : "";
        int unitPriceCents = sqlite3_column_int(product, 1);
        sqlite3_finalize(product);

        sqlite3_stmt* inventory = nullptr;
        if (sqlite3_prepare_v2(db,
                               "SELECT quantity FROM inventory WHERE product_id = ?;",
                               -1, &inventory, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_text(inventory, 1, input.productId.c_str(), -1, SQLITE_TRANSIENT);
        result = sqlite3_step(inventory);
        if (result != SQLITE_ROW) {
            error = "Inventory record not found: " + input.productId;
            sqlite3_finalize(inventory);
            database_.rollbackTransaction();
            return false;
        }
        int stock = sqlite3_column_int(inventory, 0);
        sqlite3_finalize(inventory);
        if (input.quantity > stock) {
            error = "Insufficient stock for " + input.productId
                    + ". Available: " + std::to_string(stock);
            database_.rollbackTransaction();
            return false;
        }

        int subtotalCents = unitPriceCents * input.quantity;
        orderTotalCents += subtotalCents;
        savedItems.push_back({
            input.productId,
            snapshotName,
            unitPriceCents,
            input.quantity,
            subtotalCents
        });
    }

    if (cashReceivedCents < orderTotalCents) {
        error = "Insufficient payment.";
        savedItems.clear();
        database_.rollbackTransaction();
        return false;
    }

    sqlite3_stmt* insertTransaction = nullptr;
    if (sqlite3_prepare_v2(db,
                           "INSERT INTO transactions"
                           "(transaction_date, order_total_cents, cash_received_cents, "
                           "change_cents, operator_name) "
                           "VALUES(?, ?, ?, ?, ?);",
                           -1, &insertTransaction, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    std::string timestamp = transactionTimestamp();
    sqlite3_bind_text(insertTransaction, 1, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(insertTransaction, 2, orderTotalCents);
    sqlite3_bind_int(insertTransaction, 3, cashReceivedCents);
    sqlite3_bind_int(insertTransaction, 4, cashReceivedCents - orderTotalCents);
    sqlite3_bind_text(insertTransaction, 5, operatorName.c_str(),
                      -1, SQLITE_TRANSIENT);
    int result = sqlite3_step(insertTransaction);
    if (result != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        sqlite3_finalize(insertTransaction);
        database_.rollbackTransaction();
        return false;
    }
    int transactionId = static_cast<int>(sqlite3_last_insert_rowid(db));
    sqlite3_finalize(insertTransaction);

    for (const auto& item : savedItems) {
        sqlite3_stmt* insertItem = nullptr;
        if (sqlite3_prepare_v2(db,
                               "INSERT INTO transaction_items"
                               "(transaction_id, product_id, product_name, unit_price_cents,"
                               " quantity, subtotal_cents) VALUES(?, ?, ?, ?, ?, ?);",
                               -1, &insertItem, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_int(insertItem, 1, transactionId);
        sqlite3_bind_text(insertItem, 2, item.productId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insertItem, 3, item.productName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(insertItem, 4, item.unitPriceCents);
        sqlite3_bind_int(insertItem, 5, item.quantity);
        sqlite3_bind_int(insertItem, 6, item.subtotalCents);
        result = sqlite3_step(insertItem);
        sqlite3_finalize(insertItem);
        if (result != SQLITE_DONE) {
            error = sqlite3_errmsg(db);
            database_.rollbackTransaction();
            return false;
        }

        sqlite3_stmt* deduct = nullptr;
        if (sqlite3_prepare_v2(db,
                               "UPDATE inventory SET quantity = quantity - ? "
                               "WHERE product_id = ? "
                               "AND quantity >= ?;",
                               -1, &deduct, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_int(deduct, 1, item.quantity);
        sqlite3_bind_text(deduct, 2, item.productId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(deduct, 3, item.quantity);
        result = sqlite3_step(deduct);
        bool deducted = sqlite3_changes(db) == 1;
        sqlite3_finalize(deduct);
        if (result != SQLITE_DONE || !deducted) {
            error = "Insufficient stock or stock update failed for " + item.productId;
            database_.rollbackTransaction();
            return false;
        }

        sqlite3_stmt* stock = nullptr;
        if (sqlite3_prepare_v2(db,
                               "SELECT quantity FROM inventory WHERE product_id = ?;",
                               -1, &stock, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_text(stock, 1, item.productId.c_str(), -1, SQLITE_TRANSIENT);
        result = sqlite3_step(stock);
        int resultingQuantity = result == SQLITE_ROW ? sqlite3_column_int(stock, 0) : -1;
        sqlite3_finalize(stock);
        if (result != SQLITE_ROW) {
            error = "Unable to read resulting stock for " + item.productId;
            database_.rollbackTransaction();
            return false;
        }

        sqlite3_stmt* log = nullptr;
        if (sqlite3_prepare_v2(db,
                               "INSERT INTO inventory_logs"
                               "(product_id, quantity_change, reason, resulting_quantity, "
                               "logged_at, operator_name) "
                               "VALUES(?, ?, 'Checkout sale', ?, ?, ?);",
                               -1, &log, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_text(log, 1, item.productId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(log, 2, -item.quantity);
        sqlite3_bind_int(log, 3, resultingQuantity);
        sqlite3_bind_text(log, 4, timestamp.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(log, 5, operatorName.c_str(), -1, SQLITE_TRANSIENT);
        result = sqlite3_step(log);
        sqlite3_finalize(log);
        if (result != SQLITE_DONE) {
            error = sqlite3_errmsg(db);
            database_.rollbackTransaction();
            return false;
        }
    }

    if (!database_.commitTransaction()) {
        error = database_.getLastError();
        savedItems.clear();
        return false;
    }

    transaction = {
        transactionId,
        timestamp,
        orderTotalCents,
        cashReceivedCents,
        cashReceivedCents - orderTotalCents,
        "completed",
        0,
        operatorName
    };
    return true;
}

bool TransactionRepository::getById(int transactionId,
                                    TransactionRecord& transaction,
                                    std::string& error) const {
    sqlite3* db = database_.getHandle();
    sqlite3_stmt* statement = nullptr;
    if (!db) {
        error = "Database is not open.";
        return false;
    }
    if (sqlite3_prepare_v2(db,
                           "SELECT transaction_id, transaction_date, order_total_cents,"
                           " cash_received_cents, change_cents, status, refunded_cents, "
                           "operator_name "
                           "FROM transactions WHERE transaction_id = ?;",
                           -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_int(statement, 1, transactionId);
    int result = sqlite3_step(statement);
    if (result != SQLITE_ROW) {
        error = "Transaction not found.";
        sqlite3_finalize(statement);
        return false;
    }
    transaction.transactionId = sqlite3_column_int(statement, 0);
    const char* dateText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
    transaction.transactionDate = dateText ? dateText : "";
    transaction.orderTotalCents = sqlite3_column_int(statement, 2);
    transaction.cashReceivedCents = sqlite3_column_int(statement, 3);
    transaction.changeCents = sqlite3_column_int(statement, 4);
    const char* statusText =
        reinterpret_cast<const char*>(sqlite3_column_text(statement, 5));
    transaction.status = statusText ? statusText : "completed";
    transaction.refundedCents = sqlite3_column_int(statement, 6);
    const char* operatorText =
        reinterpret_cast<const char*>(sqlite3_column_text(statement, 7));
    transaction.operatorName = operatorText ? operatorText : "Unknown";
    sqlite3_finalize(statement);
    return true;
}

bool TransactionRepository::getItems(int transactionId,
                                     std::vector<TransactionItemRecord>& items,
                                     std::string& error) const {
    sqlite3* db = database_.getHandle();
    sqlite3_stmt* statement = nullptr;
    if (!db) {
        error = "Database is not open.";
        return false;
    }
    if (sqlite3_prepare_v2(db,
                           "SELECT product_id, product_name, unit_price_cents,"
                           " quantity, subtotal_cents "
                           "FROM transaction_items WHERE transaction_id = ? "
                           "ORDER BY transaction_item_id;",
                           -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_int(statement, 1, transactionId);
    items.clear();
    while (sqlite3_step(statement) == SQLITE_ROW) {
        const char* idText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        const char* nameText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        items.push_back({
            idText ? idText : "",
            nameText ? nameText : "",
            sqlite3_column_int(statement, 2),
            sqlite3_column_int(statement, 3),
            sqlite3_column_int(statement, 4)
        });
    }
    sqlite3_finalize(statement);
    return true;
}

bool TransactionRepository::getRefunds(
    int transactionId,
    std::vector<TransactionRefundRecord>& refunds,
    std::string& error) const {
    sqlite3* db = database_.getHandle();
    if (!db) {
        error = "Database is not open.";
        return false;
    }
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "SELECT refund_id, transaction_id, refund_date, refund_type, "
            "reason, refund_cents, operator_name "
            "FROM transaction_refunds WHERE transaction_id = ? "
            "ORDER BY refund_id;",
            -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_int(statement, 1, transactionId);
    refunds.clear();
    int result = SQLITE_ROW;
    while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
        const char* dateText =
            reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
        const char* typeText =
            reinterpret_cast<const char*>(sqlite3_column_text(statement, 3));
        const char* reasonText =
            reinterpret_cast<const char*>(sqlite3_column_text(statement, 4));
        const char* operatorText =
            reinterpret_cast<const char*>(sqlite3_column_text(statement, 6));
        refunds.push_back({
            sqlite3_column_int(statement, 0),
            sqlite3_column_int(statement, 1),
            dateText ? dateText : "",
            typeText ? typeText : "",
            reasonText ? reasonText : "",
            sqlite3_column_int(statement, 5),
            operatorText ? operatorText : "Unknown"
        });
    }
    sqlite3_finalize(statement);
    if (result != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        return false;
    }
    return true;
}

bool TransactionRepository::listRecent(std::vector<TransactionRecord>& transactions,
                                       int limit,
                                       std::string& error) const {
    sqlite3* db = database_.getHandle();
    if (!db) {
        error = "Database is not open.";
        return false;
    }
    if (limit <= 0) {
        error = "History limit must be positive.";
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db,
                           "SELECT transaction_id, transaction_date, order_total_cents,"
                           " cash_received_cents, change_cents, status, refunded_cents, "
                           "operator_name "
                           "FROM transactions ORDER BY transaction_id DESC LIMIT ?;",
                           -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_int(statement, 1, limit);
    transactions.clear();
    while (sqlite3_step(statement) == SQLITE_ROW) {
        const char* dateText =
            reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        const char* statusText =
            reinterpret_cast<const char*>(sqlite3_column_text(statement, 5));
        const char* operatorText =
            reinterpret_cast<const char*>(sqlite3_column_text(statement, 7));
        transactions.push_back({
            sqlite3_column_int(statement, 0),
            dateText ? dateText : "",
            sqlite3_column_int(statement, 2),
            sqlite3_column_int(statement, 3),
            sqlite3_column_int(statement, 4),
            statusText ? statusText : "completed",
            sqlite3_column_int(statement, 6),
            operatorText ? operatorText : "Unknown"
        });
    }
    sqlite3_finalize(statement);
    return true;
}

bool TransactionRepository::listForDate(
    const std::string& date,
    std::vector<TransactionRecord>& transactions,
    std::string& error) const {
    sqlite3* db = database_.getHandle();
    if (!db) {
        error = "Database is not open.";
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "SELECT DISTINCT t.transaction_id, t.transaction_date, "
            "t.order_total_cents, t.cash_received_cents, t.change_cents, "
            "t.status, t.refunded_cents, t.operator_name "
            "FROM transactions t "
            "JOIN transaction_items ti ON ti.transaction_id = t.transaction_id "
            "WHERE t.status != 'voided' AND t.status != 'refunded' "
            "AND substr(t.transaction_date, 1, 10) = ? "
            "ORDER BY t.transaction_date DESC, t.transaction_id DESC;",
            -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_text(statement, 1, date.c_str(), -1, SQLITE_TRANSIENT);
    transactions.clear();
    int result = SQLITE_ROW;
    while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
        const char* dateText =
            reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        const char* statusText =
            reinterpret_cast<const char*>(sqlite3_column_text(statement, 5));
        const char* operatorText =
            reinterpret_cast<const char*>(sqlite3_column_text(statement, 7));
        transactions.push_back({
            sqlite3_column_int(statement, 0),
            dateText ? dateText : "",
            sqlite3_column_int(statement, 2),
            sqlite3_column_int(statement, 3),
            sqlite3_column_int(statement, 4),
            statusText ? statusText : "completed",
            sqlite3_column_int(statement, 6),
            operatorText ? operatorText : "Unknown"
        });
    }
    sqlite3_finalize(statement);
    if (result != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        return false;
    }
    return true;
}

bool TransactionRepository::getDailySales(
    const std::string& date,
    DailySalesRecord& report,
    std::vector<ProductSalesRecord>& productSales,
    std::string& error) const {
    sqlite3* db = database_.getHandle();
    if (!db) {
        error = "Database is not open.";
        return false;
    }

    sqlite3_stmt* summary = nullptr;
    if (sqlite3_prepare_v2(db,
                           "SELECT COUNT(DISTINCT t.transaction_id),"
                           " COALESCE(SUM(ti.quantity - "
                           "  COALESCE((SELECT SUM(rfi.quantity) "
                           "             FROM transaction_refund_items rfi "
                           "             JOIN transaction_refunds r ON r.refund_id = rfi.refund_id "
                           "             WHERE rfi.transaction_item_id = ti.transaction_item_id), 0)), 0),"
                           " COALESCE(SUM(ti.subtotal_cents - "
                           "  COALESCE((SELECT SUM(rfi.subtotal_cents) "
                           "             FROM transaction_refund_items rfi "
                           "             JOIN transaction_refunds r ON r.refund_id = rfi.refund_id "
                           "             WHERE rfi.transaction_item_id = ti.transaction_item_id), 0)), 0) "
                           "FROM transactions t "
                           "JOIN transaction_items ti ON ti.transaction_id = t.transaction_id "
                           "WHERE t.status != 'voided' "
                           "AND t.status != 'refunded' "
                           "AND substr(t.transaction_date, 1, 10) = ?;",
                           -1, &summary, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_text(summary, 1, date.c_str(), -1, SQLITE_TRANSIENT);
    int result = sqlite3_step(summary);
    if (result != SQLITE_ROW) {
        error = sqlite3_errmsg(db);
        sqlite3_finalize(summary);
        return false;
    }
    report = {
        sqlite3_column_int(summary, 0),
        sqlite3_column_int(summary, 1),
        sqlite3_column_int(summary, 2)
    };
    sqlite3_finalize(summary);

    sqlite3_stmt* products = nullptr;
    if (sqlite3_prepare_v2(db,
                           "SELECT ti.product_id, ti.product_name,"
                           " SUM(ti.quantity - COALESCE((SELECT SUM(rfi.quantity)"
                           "  FROM transaction_refund_items rfi"
                           "  JOIN transaction_refunds r ON r.refund_id = rfi.refund_id"
                           "  WHERE rfi.transaction_item_id = ti.transaction_item_id), 0)),"
                           " SUM(ti.subtotal_cents - COALESCE((SELECT SUM(rfi.subtotal_cents)"
                           "  FROM transaction_refund_items rfi"
                           "  JOIN transaction_refunds r ON r.refund_id = rfi.refund_id"
                           "  WHERE rfi.transaction_item_id = ti.transaction_item_id), 0)) "
                           "FROM transactions t "
                           "JOIN transaction_items ti "
                           "ON ti.transaction_id = t.transaction_id "
                           "WHERE substr(t.transaction_date, 1, 10) = ? "
                           "AND t.status != 'voided' "
                           "AND t.status != 'refunded' "
                           "GROUP BY ti.product_id "
                           "HAVING SUM(ti.quantity - COALESCE((SELECT SUM(rfi.quantity)"
                           "  FROM transaction_refund_items rfi"
                           "  WHERE rfi.transaction_item_id = ti.transaction_item_id), 0)) > 0 "
                           "ORDER BY SUM(ti.quantity - COALESCE((SELECT SUM(rfi.quantity)"
                           "  FROM transaction_refund_items rfi"
                           "  WHERE rfi.transaction_item_id = ti.transaction_item_id), 0)) DESC,"
                           " ti.product_name ASC;",
                           -1, &products, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_text(products, 1, date.c_str(), -1, SQLITE_TRANSIENT);
    productSales.clear();
    while (sqlite3_step(products) == SQLITE_ROW) {
        const char* idText =
            reinterpret_cast<const char*>(sqlite3_column_text(products, 0));
        const char* nameText =
            reinterpret_cast<const char*>(sqlite3_column_text(products, 1));
        productSales.push_back({
            idText ? idText : "",
            nameText ? nameText : "",
            sqlite3_column_int(products, 2),
            sqlite3_column_int(products, 3)
        });
    }
    sqlite3_finalize(products);
    return true;
}

bool TransactionRepository::getDailyOperatorSales(
    const std::string& date,
    std::vector<OperatorSalesRecord>& operatorSales,
    std::string& error) const {
    sqlite3* db = database_.getHandle();
    if (!db) {
        error = "Database is not open.";
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "SELECT COALESCE(NULLIF(t.operator_name, ''), 'Unknown'), "
            "COUNT(*), COALESCE(SUM(t.order_total_cents - t.refunded_cents), 0) "
            "FROM transactions t "
            "WHERE t.status != 'voided' AND t.status != 'refunded' "
            "AND substr(t.transaction_date, 1, 10) = ? "
            "AND EXISTS(SELECT 1 FROM transaction_items ti "
            "           WHERE ti.transaction_id = t.transaction_id) "
            "GROUP BY COALESCE(NULLIF(t.operator_name, ''), 'Unknown') "
            "ORDER BY COUNT(*) DESC, 1 ASC;",
            -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_text(statement, 1, date.c_str(), -1, SQLITE_TRANSIENT);
    operatorSales.clear();
    int result = SQLITE_ROW;
    while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
        const char* nameText =
            reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        operatorSales.push_back({
            nameText ? nameText : "Unknown",
            sqlite3_column_int(statement, 1),
            sqlite3_column_int(statement, 2)
        });
    }
    sqlite3_finalize(statement);
    if (result != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        return false;
    }
    return true;
}

bool TransactionRepository::getSalesReport(
    const std::string& startDate,
    const std::string& endDate,
    SalesReportRecord& report,
    std::vector<ProductSalesRecord>& productSales,
    std::vector<OperatorSalesRecord>& operatorSales,
    std::vector<TransactionRefundRecord>& refunds,
    std::string& error) const {
    report = {};
    productSales.clear();
    operatorSales.clear();
    refunds.clear();
    if (startDate > endDate) {
        error = "Report start date must not be after the end date.";
        return false;
    }

    sqlite3* db = database_.getHandle();
    if (!db) {
        error = "Database is not open.";
        return false;
    }

    sqlite3_stmt* summary = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "SELECT COUNT(*), COALESCE(SUM(t.order_total_cents), 0) "
            "FROM transactions t "
            "WHERE substr(t.transaction_date, 1, 10) >= ? "
            "AND substr(t.transaction_date, 1, 10) <= ? "
            "AND t.status != 'voided' "
            "AND EXISTS(SELECT 1 FROM transaction_items ti "
            "           WHERE ti.transaction_id = t.transaction_id);",
            -1, &summary, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_text(summary, 1, startDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(summary, 2, endDate.c_str(), -1, SQLITE_TRANSIENT);
    int result = sqlite3_step(summary);
    if (result != SQLITE_ROW) {
        error = sqlite3_errmsg(db);
        sqlite3_finalize(summary);
        return false;
    }
    report.transactionCount = sqlite3_column_int(summary, 0);
    report.grossSalesCents = sqlite3_column_int(summary, 1);
    sqlite3_finalize(summary);

    sqlite3_stmt* products = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "SELECT ti.product_id, ti.product_name, SUM(ti.quantity), "
            "SUM(ti.subtotal_cents) "
            "FROM transaction_items ti "
            "JOIN transactions t ON t.transaction_id = ti.transaction_id "
            "WHERE substr(t.transaction_date, 1, 10) >= ? "
            "AND substr(t.transaction_date, 1, 10) <= ? "
            "AND t.status != 'voided' "
            "GROUP BY ti.product_id, ti.product_name "
            "ORDER BY SUM(ti.quantity) DESC, ti.product_name ASC;",
            -1, &products, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_text(products, 1, startDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(products, 2, endDate.c_str(), -1, SQLITE_TRANSIENT);
    while ((result = sqlite3_step(products)) == SQLITE_ROW) {
        const unsigned char* idText = sqlite3_column_text(products, 0);
        const unsigned char* nameText = sqlite3_column_text(products, 1);
        const int quantity = sqlite3_column_int(products, 2);
        productSales.push_back({
            idText ? reinterpret_cast<const char*>(idText) : "",
            nameText ? reinterpret_cast<const char*>(nameText) : "",
            quantity,
            sqlite3_column_int(products, 3)
        });
        report.productsSold += quantity;
    }
    sqlite3_finalize(products);
    if (result != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        return false;
    }

    sqlite3_stmt* operators = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "SELECT COALESCE(NULLIF(t.operator_name, ''), 'Unknown'), "
            "COUNT(*), COALESCE(SUM(t.order_total_cents), 0) "
            "FROM transactions t "
            "WHERE substr(t.transaction_date, 1, 10) >= ? "
            "AND substr(t.transaction_date, 1, 10) <= ? "
            "AND t.status != 'voided' "
            "AND EXISTS(SELECT 1 FROM transaction_items ti "
            "           WHERE ti.transaction_id = t.transaction_id) "
            "GROUP BY COALESCE(NULLIF(t.operator_name, ''), 'Unknown') "
            "ORDER BY COUNT(*) DESC, 1 ASC;",
            -1, &operators, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_text(operators, 1, startDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(operators, 2, endDate.c_str(), -1, SQLITE_TRANSIENT);
    while ((result = sqlite3_step(operators)) == SQLITE_ROW) {
        const unsigned char* nameText = sqlite3_column_text(operators, 0);
        operatorSales.push_back({
            nameText ? reinterpret_cast<const char*>(nameText) : "Unknown",
            sqlite3_column_int(operators, 1),
            sqlite3_column_int(operators, 2)
        });
    }
    sqlite3_finalize(operators);
    if (result != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        return false;
    }

    sqlite3_stmt* refundQuery = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "SELECT refund_id, transaction_id, refund_date, refund_type, "
            "reason, refund_cents, operator_name "
            "FROM transaction_refunds "
            "WHERE substr(refund_date, 1, 10) >= ? "
            "AND substr(refund_date, 1, 10) <= ? "
            "ORDER BY refund_date DESC, refund_id DESC;",
            -1, &refundQuery, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_text(refundQuery, 1, startDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(refundQuery, 2, endDate.c_str(), -1, SQLITE_TRANSIENT);
    while ((result = sqlite3_step(refundQuery)) == SQLITE_ROW) {
        const unsigned char* dateText = sqlite3_column_text(refundQuery, 2);
        const unsigned char* typeText = sqlite3_column_text(refundQuery, 3);
        const unsigned char* reasonText = sqlite3_column_text(refundQuery, 4);
        const unsigned char* operatorText = sqlite3_column_text(refundQuery, 6);
        const std::string type =
            typeText ? reinterpret_cast<const char*>(typeText) : "";
        const int amount = sqlite3_column_int(refundQuery, 5);
        refunds.push_back({
            sqlite3_column_int(refundQuery, 0),
            sqlite3_column_int(refundQuery, 1),
            dateText ? reinterpret_cast<const char*>(dateText) : "",
            type,
            reasonText ? reinterpret_cast<const char*>(reasonText) : "",
            amount,
            operatorText ? reinterpret_cast<const char*>(operatorText) : "Unknown"
        });
        if (type == "void") {
            ++report.voidCount;
            report.voidedCents += amount;
        } else {
            ++report.refundCount;
            report.refundedCents += amount;
        }
    }
    sqlite3_finalize(refundQuery);
    if (result != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        return false;
    }

    report.netSalesCents = report.grossSalesCents - report.refundedCents;
    return true;
}

bool TransactionRepository::fullRefund(int transactionId,
                                       const std::string& reason,
                                       TransactionRefundRecord& refund,
                                       std::string& error,
                                       const std::string& operatorName) {
    if (reason.empty()) {
        error = "Refund reason cannot be empty.";
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
                           "SELECT order_total_cents, status, refunded_cents "
                           "FROM transactions WHERE transaction_id = ?;",
                           -1, &read, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_bind_int(read, 1, transactionId);
    if (sqlite3_step(read) != SQLITE_ROW) {
        error = "Transaction not found.";
        sqlite3_finalize(read);
        database_.rollbackTransaction();
        return false;
    }
    int orderTotal = sqlite3_column_int(read, 0);
    const char* statusText =
        reinterpret_cast<const char*>(sqlite3_column_text(read, 1));
    std::string status = statusText ? statusText : "completed";
    int alreadyRefunded = sqlite3_column_int(read, 2);
    sqlite3_finalize(read);
    if (status == "refunded" || status == "voided" || alreadyRefunded >= orderTotal) {
        error = "Transaction is already refunded or voided.";
        database_.rollbackTransaction();
        return false;
    }

    sqlite3_stmt* items = nullptr;
    if (sqlite3_prepare_v2(db,
                           "SELECT transaction_item_id, product_id, product_name,"
                           " unit_price_cents, quantity, subtotal_cents "
                           "FROM transaction_items WHERE transaction_id = ? "
                           "ORDER BY transaction_item_id;",
                           -1, &items, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_bind_int(items, 1, transactionId);

    std::string timestamp = transactionTimestamp();
    sqlite3_stmt* refundInsert = nullptr;
    if (sqlite3_prepare_v2(db,
                           "INSERT INTO transaction_refunds"
                           "(transaction_id, refund_date, refund_type, reason, "
                           "refund_cents, operator_name) "
                           "VALUES(?, ?, 'refund', ?, ?, ?);",
                           -1, &refundInsert, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        sqlite3_finalize(items);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_bind_int(refundInsert, 1, transactionId);
    sqlite3_bind_text(refundInsert, 2, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(refundInsert, 3, reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(refundInsert, 4, orderTotal - alreadyRefunded);
    sqlite3_bind_text(refundInsert, 5, operatorName.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(refundInsert) != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        sqlite3_finalize(refundInsert);
        sqlite3_finalize(items);
        database_.rollbackTransaction();
        return false;
    }
    int refundId = static_cast<int>(sqlite3_last_insert_rowid(db));
    sqlite3_finalize(refundInsert);

    while (sqlite3_step(items) == SQLITE_ROW) {
        int transactionItemId = sqlite3_column_int(items, 0);
        const char* productIdText =
            reinterpret_cast<const char*>(sqlite3_column_text(items, 1));
        const char* nameText =
            reinterpret_cast<const char*>(sqlite3_column_text(items, 2));
        int unitPrice = sqlite3_column_int(items, 3);
        int originalQuantity = sqlite3_column_int(items, 4);

        sqlite3_stmt* previously = nullptr;
        if (sqlite3_prepare_v2(db,
                               "SELECT COALESCE(SUM(rfi.quantity), 0) "
                               "FROM transaction_refund_items rfi "
                               "JOIN transaction_refunds r ON r.refund_id = rfi.refund_id "
                               "WHERE rfi.transaction_item_id = ?;",
                               -1, &previously, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            sqlite3_finalize(items);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_int(previously, 1, transactionItemId);
        int previousQuantity = 0;
        if (sqlite3_step(previously) == SQLITE_ROW)
            previousQuantity = sqlite3_column_int(previously, 0);
        sqlite3_finalize(previously);
        int refundQuantity = originalQuantity - previousQuantity;
        if (refundQuantity <= 0) continue;
        int refundSubtotal = unitPrice * refundQuantity;

        sqlite3_stmt* refundItem = nullptr;
        if (sqlite3_prepare_v2(db,
                               "INSERT INTO transaction_refund_items"
                               "(refund_id, transaction_item_id, product_id, product_name,"
                               " unit_price_cents, quantity, subtotal_cents) "
                               "VALUES(?, ?, ?, ?, ?, ?, ?);",
                               -1, &refundItem, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            sqlite3_finalize(items);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_int(refundItem, 1, refundId);
        sqlite3_bind_int(refundItem, 2, transactionItemId);
        sqlite3_bind_text(refundItem, 3,
                          productIdText ? productIdText : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(refundItem, 4,
                          nameText ? nameText : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(refundItem, 5, unitPrice);
        sqlite3_bind_int(refundItem, 6, refundQuantity);
        sqlite3_bind_int(refundItem, 7, refundSubtotal);
        if (sqlite3_step(refundItem) != SQLITE_DONE) {
            error = sqlite3_errmsg(db);
            sqlite3_finalize(refundItem);
            sqlite3_finalize(items);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_finalize(refundItem);

        sqlite3_stmt* stock = nullptr;
        if (sqlite3_prepare_v2(db,
                               "UPDATE inventory SET quantity = quantity + ? "
                               "WHERE product_id = ?;",
                               -1, &stock, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            sqlite3_finalize(items);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_int(stock, 1, refundQuantity);
        sqlite3_bind_text(stock, 2,
                          productIdText ? productIdText : "", -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stock) != SQLITE_DONE || sqlite3_changes(db) != 1) {
            error = "Inventory restoration failed.";
            sqlite3_finalize(stock);
            sqlite3_finalize(items);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_finalize(stock);

        sqlite3_stmt* resulting = nullptr;
        int resultingQuantity = -1;
        if (sqlite3_prepare_v2(db,
                               "SELECT quantity FROM inventory WHERE product_id = ?;",
                               -1, &resulting, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(resulting, 1,
                              productIdText ? productIdText : "", -1, SQLITE_TRANSIENT);
            if (sqlite3_step(resulting) == SQLITE_ROW)
                resultingQuantity = sqlite3_column_int(resulting, 0);
        }
        sqlite3_finalize(resulting);

        sqlite3_stmt* log = nullptr;
        if (sqlite3_prepare_v2(db,
                               "INSERT INTO inventory_logs"
                               "(product_id, quantity_change, reason, resulting_quantity, "
                               "logged_at, operator_name) "
                               "VALUES(?, ?, ?, ?, ?, ?);",
                               -1, &log, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            sqlite3_finalize(items);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_text(log, 1, productIdText ? productIdText : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(log, 2, refundQuantity);
        sqlite3_bind_text(log, 3, ("Refund transaction #" + std::to_string(transactionId)).c_str(),
                          -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(log, 4, resultingQuantity);
        sqlite3_bind_text(log, 5, timestamp.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(log, 6, operatorName.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(log) != SQLITE_DONE) {
            error = sqlite3_errmsg(db);
            sqlite3_finalize(log);
            sqlite3_finalize(items);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_finalize(log);
    }
    sqlite3_finalize(items);

    sqlite3_stmt* update = nullptr;
    if (sqlite3_prepare_v2(db,
                           "UPDATE transactions "
                           "SET status='refunded', refunded_cents=? "
                           "WHERE transaction_id=?;",
                           -1, &update, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_bind_int(update, 1, orderTotal);
    sqlite3_bind_int(update, 2, transactionId);
    if (sqlite3_step(update) != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        sqlite3_finalize(update);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_finalize(update);

    if (!database_.commitTransaction()) {
        error = database_.getLastError();
        return false;
    }
    refund = {refundId, transactionId, timestamp, "refund", reason,
              orderTotal - alreadyRefunded, operatorName};
    return true;
}

bool TransactionRepository::partialRefund(
    int transactionId,
    const std::vector<TransactionRefundItemInput>& items,
    const std::string& reason,
    TransactionRefundRecord& refund,
    std::string& error,
    const std::string& operatorName) {
    if (reason.empty()) {
        error = "Refund reason cannot be empty.";
        return false;
    }
    if (items.empty()) {
        error = "Select at least one item to refund.";
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
                           "SELECT order_total_cents, status, refunded_cents "
                           "FROM transactions WHERE transaction_id=?;",
                           -1, &read, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_bind_int(read, 1, transactionId);
    if (sqlite3_step(read) != SQLITE_ROW) {
        error = "Transaction not found.";
        sqlite3_finalize(read);
        database_.rollbackTransaction();
        return false;
    }
    int orderTotal = sqlite3_column_int(read, 0);
    const char* statusText =
        reinterpret_cast<const char*>(sqlite3_column_text(read, 1));
    std::string status = statusText ? statusText : "completed";
    int alreadyRefunded = sqlite3_column_int(read, 2);
    sqlite3_finalize(read);
    if (status == "refunded" || status == "voided") {
        error = "Transaction is already refunded or voided.";
        database_.rollbackTransaction();
        return false;
    }

    int precomputedRefundCents = 0;
    for (const auto& input : items) {
        if (input.quantity <= 0) continue;
        sqlite3_stmt* original = nullptr;
        if (sqlite3_prepare_v2(db,
                               "SELECT transaction_item_id, unit_price_cents, quantity "
                               "FROM transaction_items "
                               "WHERE transaction_id=? AND product_id=?;",
                               -1, &original, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_int(original, 1, transactionId);
        sqlite3_bind_text(original, 2, input.productId.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(original) != SQLITE_ROW) {
            error = "Transaction item not found: " + input.productId;
            sqlite3_finalize(original);
            database_.rollbackTransaction();
            return false;
        }
        int transactionItemId = sqlite3_column_int(original, 0);
        int unitPrice = sqlite3_column_int(original, 1);
        int originalQuantity = sqlite3_column_int(original, 2);
        sqlite3_finalize(original);

        sqlite3_stmt* previously = nullptr;
        if (sqlite3_prepare_v2(db,
                               "SELECT COALESCE(SUM(quantity),0) "
                               "FROM transaction_refund_items "
                               "WHERE transaction_item_id=?;",
                               -1, &previously, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_int(previously, 1, transactionItemId);
        int alreadyQuantity = 0;
        if (sqlite3_step(previously) == SQLITE_ROW)
            alreadyQuantity = sqlite3_column_int(previously, 0);
        sqlite3_finalize(previously);
        if (input.quantity > originalQuantity - alreadyQuantity) {
            error = "Refund quantity exceeds remaining quantity for " + input.productId;
            database_.rollbackTransaction();
            return false;
        }
        precomputedRefundCents += unitPrice * input.quantity;
    }
    if (precomputedRefundCents <= 0) {
        error = "Refund amount must be greater than zero.";
        database_.rollbackTransaction();
        return false;
    }
    if (alreadyRefunded + precomputedRefundCents > orderTotal) {
        error = "Refund amount exceeds the remaining transaction total.";
        database_.rollbackTransaction();
        return false;
    }

    std::string timestamp = transactionTimestamp();
    sqlite3_stmt* refundInsert = nullptr;
    if (sqlite3_prepare_v2(db,
                           "INSERT INTO transaction_refunds"
                           "(transaction_id, refund_date, refund_type, reason, "
                           "refund_cents, operator_name) "
                           "VALUES(?, ?, 'refund', ?, ?, ?);",
                           -1, &refundInsert, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_bind_int(refundInsert, 1, transactionId);
    sqlite3_bind_text(refundInsert, 2, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(refundInsert, 3, reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(refundInsert, 4, precomputedRefundCents);
    sqlite3_bind_text(refundInsert, 5, operatorName.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(refundInsert) != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        sqlite3_finalize(refundInsert);
        database_.rollbackTransaction();
        return false;
    }
    int refundId = static_cast<int>(sqlite3_last_insert_rowid(db));
    sqlite3_finalize(refundInsert);

    int refundCents = 0;
    for (const auto& input : items) {
        if (input.quantity <= 0) continue;

        sqlite3_stmt* original = nullptr;
        if (sqlite3_prepare_v2(db,
                               "SELECT transaction_item_id, product_name,"
                               " unit_price_cents, quantity "
                               "FROM transaction_items "
                               "WHERE transaction_id=? AND product_id=?;",
                               -1, &original, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_int(original, 1, transactionId);
        sqlite3_bind_text(original, 2, input.productId.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(original) != SQLITE_ROW) {
            error = "Transaction item not found: " + input.productId;
            sqlite3_finalize(original);
            database_.rollbackTransaction();
            return false;
        }
        int transactionItemId = sqlite3_column_int(original, 0);
        const char* nameText =
            reinterpret_cast<const char*>(sqlite3_column_text(original, 1));
        int unitPrice = sqlite3_column_int(original, 2);
        int originalQuantity = sqlite3_column_int(original, 3);
        sqlite3_finalize(original);

        sqlite3_stmt* previously = nullptr;
        if (sqlite3_prepare_v2(db,
                               "SELECT COALESCE(SUM(rfi.quantity),0) "
                               "FROM transaction_refund_items rfi "
                               "WHERE rfi.transaction_item_id=?;",
                               -1, &previously, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_int(previously, 1, transactionItemId);
        int alreadyQuantity = 0;
        if (sqlite3_step(previously) == SQLITE_ROW)
            alreadyQuantity = sqlite3_column_int(previously, 0);
        sqlite3_finalize(previously);
        if (input.quantity > originalQuantity - alreadyQuantity) {
            error = "Refund quantity exceeds remaining quantity for " + input.productId;
            database_.rollbackTransaction();
            return false;
        }
        int subtotal = unitPrice * input.quantity;
        refundCents += subtotal;

        sqlite3_stmt* refundItem = nullptr;
        if (sqlite3_prepare_v2(db,
                               "INSERT INTO transaction_refund_items"
                               "(refund_id, transaction_item_id, product_id, product_name,"
                               " unit_price_cents, quantity, subtotal_cents) "
                               "VALUES(?, ?, ?, ?, ?, ?, ?);",
                               -1, &refundItem, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_int(refundItem, 1, refundId);
        sqlite3_bind_int(refundItem, 2, transactionItemId);
        sqlite3_bind_text(refundItem, 3, input.productId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(refundItem, 4,
                          nameText ? nameText : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(refundItem, 5, unitPrice);
        sqlite3_bind_int(refundItem, 6, input.quantity);
        sqlite3_bind_int(refundItem, 7, subtotal);
        if (sqlite3_step(refundItem) != SQLITE_DONE) {
            error = sqlite3_errmsg(db);
            sqlite3_finalize(refundItem);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_finalize(refundItem);

        sqlite3_stmt* stock = nullptr;
        if (sqlite3_prepare_v2(db,
                               "UPDATE inventory SET quantity = quantity + ? "
                               "WHERE product_id=?;",
                               -1, &stock, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_int(stock, 1, input.quantity);
        sqlite3_bind_text(stock, 2, input.productId.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stock) != SQLITE_DONE || sqlite3_changes(db) != 1) {
            error = "Inventory restoration failed.";
            sqlite3_finalize(stock);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_finalize(stock);

        sqlite3_stmt* resulting = nullptr;
        int resultingQuantity = -1;
        if (sqlite3_prepare_v2(db,
                               "SELECT quantity FROM inventory WHERE product_id=?;",
                               -1, &resulting, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(resulting, 1, input.productId.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(resulting) == SQLITE_ROW)
                resultingQuantity = sqlite3_column_int(resulting, 0);
        }
        sqlite3_finalize(resulting);

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
        sqlite3_bind_text(log, 1, input.productId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(log, 2, input.quantity);
        std::string logReason = "Partial refund transaction #" + std::to_string(transactionId);
        sqlite3_bind_text(log, 3, logReason.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(log, 4, resultingQuantity);
        sqlite3_bind_text(log, 5, timestamp.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(log, 6, operatorName.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(log) != SQLITE_DONE) {
            error = sqlite3_errmsg(db);
            sqlite3_finalize(log);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_finalize(log);
    }

    if (refundCents <= 0) {
        error = "Refund amount must be greater than zero.";
        database_.rollbackTransaction();
        return false;
    }
    int newRefunded = alreadyRefunded + refundCents;
    if (newRefunded > orderTotal) {
        error = "Refund amount exceeds the remaining transaction total.";
        database_.rollbackTransaction();
        return false;
    }

    sqlite3_stmt* updateRefund = nullptr;
    if (sqlite3_prepare_v2(db,
                           "UPDATE transaction_refunds SET refund_cents=? "
                           "WHERE refund_id=?;",
                           -1, &updateRefund, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_bind_int(updateRefund, 1, refundCents);
    sqlite3_bind_int(updateRefund, 2, refundId);
    if (sqlite3_step(updateRefund) != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        sqlite3_finalize(updateRefund);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_finalize(updateRefund);

    const char* newStatus = newRefunded >= orderTotal ? "refunded" : "partial_refund";
    sqlite3_stmt* update = nullptr;
    if (sqlite3_prepare_v2(db,
                           "UPDATE transactions SET status=?, refunded_cents=? "
                           "WHERE transaction_id=?;",
                           -1, &update, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_bind_text(update, 1, newStatus, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(update, 2, newRefunded);
    sqlite3_bind_int(update, 3, transactionId);
    if (sqlite3_step(update) != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        sqlite3_finalize(update);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_finalize(update);

    if (!database_.commitTransaction()) {
        error = database_.getLastError();
        return false;
    }
    refund = {refundId, transactionId, timestamp, "refund", reason,
              refundCents, operatorName};
    return true;
}

bool TransactionRepository::voidTransaction(
    int transactionId,
    const std::string& reason,
    TransactionRefundRecord& refund,
    std::string& error,
    const std::string& operatorName) {
    if (reason.empty()) {
        error = "Void reason cannot be empty.";
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
                           "SELECT order_total_cents, status, refunded_cents "
                           "FROM transactions WHERE transaction_id=?;",
                           -1, &read, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_bind_int(read, 1, transactionId);
    if (sqlite3_step(read) != SQLITE_ROW) {
        error = "Transaction not found.";
        sqlite3_finalize(read);
        database_.rollbackTransaction();
        return false;
    }
    int orderTotal = sqlite3_column_int(read, 0);
    const char* statusText =
        reinterpret_cast<const char*>(sqlite3_column_text(read, 1));
    std::string status = statusText ? statusText : "completed";
    int alreadyRefunded = sqlite3_column_int(read, 2);
    sqlite3_finalize(read);
    if (status == "voided" || status == "refunded") {
        error = "Transaction is already refunded or voided.";
        database_.rollbackTransaction();
        return false;
    }

    std::string timestamp = transactionTimestamp();
    sqlite3_stmt* refundInsert = nullptr;
    if (sqlite3_prepare_v2(db,
                           "INSERT INTO transaction_refunds"
                           "(transaction_id, refund_date, refund_type, reason, "
                           "refund_cents, operator_name) "
                           "VALUES(?, ?, 'void', ?, ?, ?);",
                           -1, &refundInsert, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    int voidAmount = orderTotal - alreadyRefunded;
    sqlite3_bind_int(refundInsert, 1, transactionId);
    sqlite3_bind_text(refundInsert, 2, timestamp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(refundInsert, 3, reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(refundInsert, 4, voidAmount);
    sqlite3_bind_text(refundInsert, 5, operatorName.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(refundInsert) != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        sqlite3_finalize(refundInsert);
        database_.rollbackTransaction();
        return false;
    }
    int refundId = static_cast<int>(sqlite3_last_insert_rowid(db));
    sqlite3_finalize(refundInsert);

    sqlite3_stmt* items = nullptr;
    if (sqlite3_prepare_v2(db,
                           "SELECT transaction_item_id, product_id, product_name,"
                           " unit_price_cents, quantity "
                           "FROM transaction_items WHERE transaction_id=?;",
                           -1, &items, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_bind_int(items, 1, transactionId);
    while (sqlite3_step(items) == SQLITE_ROW) {
        int transactionItemId = sqlite3_column_int(items, 0);
        const char* productText =
            reinterpret_cast<const char*>(sqlite3_column_text(items, 1));
        const char* nameText =
            reinterpret_cast<const char*>(sqlite3_column_text(items, 2));
        int unitPrice = sqlite3_column_int(items, 3);
        int originalQuantity = sqlite3_column_int(items, 4);

        sqlite3_stmt* previously = nullptr;
        if (sqlite3_prepare_v2(db,
                               "SELECT COALESCE(SUM(quantity),0) "
                               "FROM transaction_refund_items "
                               "WHERE transaction_item_id=?;",
                               -1, &previously, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            sqlite3_finalize(items);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_int(previously, 1, transactionItemId);
        int previousQuantity = 0;
        if (sqlite3_step(previously) == SQLITE_ROW)
            previousQuantity = sqlite3_column_int(previously, 0);
        sqlite3_finalize(previously);
        int restoreQuantity = originalQuantity - previousQuantity;
        if (restoreQuantity <= 0) continue;

        sqlite3_stmt* refundItem = nullptr;
        if (sqlite3_prepare_v2(db,
                               "INSERT INTO transaction_refund_items"
                               "(refund_id, transaction_item_id, product_id, product_name,"
                               " unit_price_cents, quantity, subtotal_cents) "
                               "VALUES(?, ?, ?, ?, ?, ?, ?);",
                               -1, &refundItem, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            sqlite3_finalize(items);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_int(refundItem, 1, refundId);
        sqlite3_bind_int(refundItem, 2, transactionItemId);
        sqlite3_bind_text(refundItem, 3, productText ? productText : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(refundItem, 4, nameText ? nameText : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(refundItem, 5, unitPrice);
        sqlite3_bind_int(refundItem, 6, restoreQuantity);
        sqlite3_bind_int(refundItem, 7, unitPrice * restoreQuantity);
        if (sqlite3_step(refundItem) != SQLITE_DONE) {
            error = sqlite3_errmsg(db);
            sqlite3_finalize(refundItem);
            sqlite3_finalize(items);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_finalize(refundItem);

        sqlite3_stmt* stock = nullptr;
        if (sqlite3_prepare_v2(db,
                               "UPDATE inventory SET quantity=quantity+? "
                               "WHERE product_id=?;",
                               -1, &stock, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            sqlite3_finalize(items);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_int(stock, 1, restoreQuantity);
        sqlite3_bind_text(stock, 2, productText ? productText : "", -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stock) != SQLITE_DONE || sqlite3_changes(db) != 1) {
            error = "Inventory restoration failed.";
            sqlite3_finalize(stock);
            sqlite3_finalize(items);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_finalize(stock);

        sqlite3_stmt* resulting = nullptr;
        int resultingQuantity = -1;
        if (sqlite3_prepare_v2(db,
                               "SELECT quantity FROM inventory WHERE product_id=?;",
                               -1, &resulting, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(resulting, 1, productText ? productText : "", -1, SQLITE_TRANSIENT);
            if (sqlite3_step(resulting) == SQLITE_ROW)
                resultingQuantity = sqlite3_column_int(resulting, 0);
        }
        sqlite3_finalize(resulting);

        sqlite3_stmt* log = nullptr;
        if (sqlite3_prepare_v2(db,
                               "INSERT INTO inventory_logs"
                               "(product_id, quantity_change, reason, resulting_quantity, "
                               "logged_at, operator_name) "
                               "VALUES(?, ?, ?, ?, ?, ?);",
                               -1, &log, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            sqlite3_finalize(items);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_text(log, 1, productText ? productText : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(log, 2, restoreQuantity);
        std::string logReason = "Void transaction #" + std::to_string(transactionId);
        sqlite3_bind_text(log, 3, logReason.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(log, 4, resultingQuantity);
        sqlite3_bind_text(log, 5, timestamp.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(log, 6, operatorName.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(log) != SQLITE_DONE) {
            error = sqlite3_errmsg(db);
            sqlite3_finalize(log);
            sqlite3_finalize(items);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_finalize(log);
    }
    sqlite3_finalize(items);

    sqlite3_stmt* update = nullptr;
    if (sqlite3_prepare_v2(db,
                           "UPDATE transactions SET status='voided', refunded_cents=? "
                           "WHERE transaction_id=?;",
                           -1, &update, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_bind_int(update, 1, orderTotal);
    sqlite3_bind_int(update, 2, transactionId);
    if (sqlite3_step(update) != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        sqlite3_finalize(update);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_finalize(update);

    if (!database_.commitTransaction()) {
        error = database_.getLastError();
        return false;
    }
    refund = {refundId, transactionId, timestamp, "void", reason,
              voidAmount, operatorName};
    return true;
}
