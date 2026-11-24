#include "doctest.h"
#include "../DatabaseManager.h"
#include "../ProductRepository.h"
#include "../InventoryRepository.h"
#include "../TransactionRepository.h"
#include "../POS.h"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <fstream>

static const char* TEMP_DB = "tests/phase2_foundation_test.db";

static void removeTempDb() {
    std::remove(TEMP_DB);
}

TEST_CASE("Database initialization seeds catalog once") {
    removeTempDb();

    DatabaseManager database;
    REQUIRE(database.open(TEMP_DB));
    REQUIRE(database.initializeSchema());

    ProductRepository products(database);
    std::vector<ProductRecord> catalog;
    std::string error;
    REQUIRE(products.getActive(catalog, error));
    REQUIRE(catalog.size() == 8);

    ProductRecord record;
    REQUIRE(products.findById("P001", record, error));
    CHECK(record.name == "Chocolate Chip Cookie");
    CHECK(record.priceCents == 6500);
    CHECK(record.categoryId == 1);
    CHECK(record.imagePath == "assets/products/chocolate_chip.jpg");
    REQUIRE(products.findById("P006", record, error));
    CHECK(record.name == "Nutella");
    CHECK(record.priceCents == 6500);
    CHECK(record.categoryId == 1);
    REQUIRE(products.findById("P007", record, error));
    CHECK(record.name == "Dubai Chewy Cookie (1 Piece)");
    CHECK(record.priceCents == 11000);
    CHECK(record.categoryId == 1);
    REQUIRE(products.findById("P008", record, error));
    CHECK(record.name == "Dubai Chewy Cookie (4 Pieces)");
    CHECK(record.priceCents == 42000);
    CHECK(record.categoryId == 1);

    InventoryRepository inventory(database);
    int stock = -1;
    REQUIRE(inventory.getStock("P001", stock, error));
    CHECK(stock == 100);
    REQUIRE(inventory.getStock("P006", stock, error));
    CHECK(stock == 100);
    REQUIRE(inventory.getStock("P007", stock, error));
    CHECK(stock == 100);
    REQUIRE(inventory.getStock("P008", stock, error));
    CHECK(stock == 100);

    REQUIRE(products.update("P001", 1, "Chocolate Chip Cookie", 6600, error));
    REQUIRE(inventory.adjust("P001", -27, "Verify catalog seeding preserves stock", error));

    database.close();

    DatabaseManager reopened;
    REQUIRE(reopened.open(TEMP_DB));
    REQUIRE(reopened.initializeSchema());
    ProductRepository reopenedProducts(reopened);
    catalog.clear();
    REQUIRE(reopenedProducts.getActive(catalog, error));
    CHECK(catalog.size() == 8);
    REQUIRE(reopenedProducts.findById("P001", record, error));
    CHECK(record.priceCents == 6600);
    CHECK(record.categoryId == 1);
    CHECK(record.imagePath == "assets/products/chocolate_chip.jpg");

    InventoryRepository reopenedInventory(reopened);
    REQUIRE(reopenedInventory.getStock("P001", stock, error));
    CHECK(stock == 73);
    REQUIRE(reopenedInventory.getStock("P006", stock, error));
    CHECK(stock == 100);

    reopened.close();
    removeTempDb();
}

TEST_CASE("Deleting a seeded product keeps it deleted after database reinitialization") {
    const char* path = "tests/deleted_seed_product_test.db";
    std::remove(path);

    std::string error;
    ProductRecord deletedRecord{};
    bool imagePathUnreferenced = false;
    {
        DatabaseManager database;
        REQUIRE(database.open(path));
        REQUIRE(database.initializeSchema());

        ProductRepository products(database);
        REQUIRE(products.deleteProduct("P001", deletedRecord,
                                       imagePathUnreferenced, error));
        CHECK(deletedRecord.productId == "P001");
        ProductRecord record{};
        CHECK_FALSE(products.findById("P001", record, error));
    }

    {
        DatabaseManager reopened;
        REQUIRE(reopened.open(path));
        REQUIRE(reopened.initializeSchema());

        ProductRepository products(reopened);
        ProductRecord record{};
        CHECK_FALSE(products.findById("P001", record, error));
        REQUIRE(products.findById("P002", record, error));

        std::vector<ProductRecord> catalog;
        REQUIRE(products.getActive(catalog, error));
        CHECK(catalog.size() == 7);
    }

    std::remove(path);
}

TEST_CASE("Inventory-log migration preserves legacy activity without product references") {
    const char* path = "tests/phase4_operator_migration.db";
    std::remove(path);
    sqlite3* legacyDatabase = nullptr;
    REQUIRE(sqlite3_open(path, &legacyDatabase) == SQLITE_OK);
    const char* legacySchema =
        "CREATE TABLE categories(category_id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE);"
        "CREATE TABLE products(product_id TEXT PRIMARY KEY, category_id INTEGER NOT NULL, "
        "name TEXT NOT NULL, price_cents INTEGER NOT NULL, active INTEGER NOT NULL DEFAULT 1, "
        "FOREIGN KEY(category_id) REFERENCES categories(category_id));"
        "CREATE TABLE inventory(product_id TEXT PRIMARY KEY, quantity INTEGER NOT NULL, "
        "FOREIGN KEY(product_id) REFERENCES products(product_id));"
        "CREATE TABLE inventory_logs(log_id INTEGER PRIMARY KEY, product_id TEXT NOT NULL, "
        "quantity_change INTEGER NOT NULL, reason TEXT NOT NULL, "
        "resulting_quantity INTEGER NOT NULL, logged_at TEXT NOT NULL, "
        "FOREIGN KEY(product_id) REFERENCES products(product_id));"
        "CREATE TABLE transactions(transaction_id INTEGER PRIMARY KEY, "
        "transaction_date TEXT NOT NULL, order_total_cents INTEGER NOT NULL, "
        "cash_received_cents INTEGER NOT NULL, change_cents INTEGER NOT NULL);"
        "CREATE TABLE transaction_items(transaction_item_id INTEGER PRIMARY KEY, "
        "transaction_id INTEGER NOT NULL, product_id TEXT NOT NULL, "
        "product_name TEXT NOT NULL, unit_price_cents INTEGER NOT NULL, "
        "quantity INTEGER NOT NULL, subtotal_cents INTEGER NOT NULL);"
        "INSERT INTO categories VALUES(1, 'Cookies');"
        "INSERT INTO products VALUES('LEGACY-1', 1, 'Legacy Cookie', 500, 1);"
        "INSERT INTO inventory VALUES('LEGACY-1', 7);"
        "INSERT INTO inventory_logs VALUES(1, 'LEGACY-1', 2, 'Legacy restock', 7, "
        "'2025-01-02 10:00:00');"
        "INSERT INTO transactions VALUES(42, '2025-01-02 10:00:00', 500, 500, 0);"
        "INSERT INTO transaction_items VALUES(1, 42, 'LEGACY-1', 'Legacy Cookie', "
        "500, 1, 500);";
    char* migrationError = nullptr;
    const int legacyResult = sqlite3_exec(
        legacyDatabase, legacySchema, nullptr, nullptr, &migrationError);
    const std::string legacyError = migrationError ? migrationError : "";
    sqlite3_free(migrationError);
    sqlite3_close(legacyDatabase);
    REQUIRE_MESSAGE(legacyResult == SQLITE_OK, legacyError);

    {
        DatabaseManager database;
        REQUIRE(database.open(path));
        REQUIRE(database.initializeSchema());

        std::string error;
        TransactionRepository transactions(database);
        TransactionRecord transaction{};
        REQUIRE(transactions.getById(42, transaction, error));
        CHECK(transaction.transactionId == 42);
        CHECK(transaction.orderTotalCents == 500);
        CHECK(transaction.operatorName == "Unknown");

        InventoryRepository inventory(database);
        InventoryLog log{};
        REQUIRE(inventory.getLatestLog("LEGACY-1", log, error));
        CHECK(log.resultingQuantity == 7);
        CHECK(log.operatorName == "Unknown");

        sqlite3_stmt* logForeignKeys = nullptr;
        REQUIRE(sqlite3_prepare_v2(database.getHandle(),
                                   "PRAGMA foreign_key_list(inventory_logs);",
                                   -1, &logForeignKeys, nullptr) == SQLITE_OK);
        CHECK(sqlite3_step(logForeignKeys) == SQLITE_DONE);
        sqlite3_finalize(logForeignKeys);

        std::string currentOperator;
        REQUIRE(database.getCurrentOperatorName(currentOperator, error));
        CHECK(currentOperator.empty());

        ProductRepository products(database);
        ProductRecord deletedRecord{};
        bool imagePathUnreferenced = false;
        REQUIRE(products.deleteProduct("LEGACY-1", deletedRecord,
                                       imagePathUnreferenced, error));
        CHECK(deletedRecord.productId == "LEGACY-1");

        sqlite3_stmt* retainedHistory = nullptr;
        REQUIRE(sqlite3_prepare_v2(
            database.getHandle(),
            "SELECT COUNT(*) FROM inventory_logs WHERE product_id = 'LEGACY-1';",
            -1, &retainedHistory, nullptr) == SQLITE_OK);
        REQUIRE(sqlite3_step(retainedHistory) == SQLITE_ROW);
        CHECK(sqlite3_column_int(retainedHistory, 0) == 2);
        sqlite3_finalize(retainedHistory);
        REQUIRE(transactions.getById(42, transaction, error));
        CHECK(transaction.transactionId == 42);
    }
    std::remove(path);
}

TEST_CASE("Database schema passes foreign-key and integrity checks") {
    removeTempDb();

    DatabaseManager database;
    REQUIRE(database.open(TEMP_DB));
    REQUIRE(database.initializeSchema());

    sqlite3_stmt* foreignKeys = nullptr;
    REQUIRE(sqlite3_prepare_v2(database.getHandle(),
                               "PRAGMA foreign_keys;",
                               -1, &foreignKeys, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(foreignKeys) == SQLITE_ROW);
    CHECK(sqlite3_column_int(foreignKeys, 0) == 1);
    sqlite3_finalize(foreignKeys);

    sqlite3_stmt* integrity = nullptr;
    REQUIRE(sqlite3_prepare_v2(database.getHandle(),
                               "PRAGMA integrity_check;",
                               -1, &integrity, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(integrity) == SQLITE_ROW);
    CHECK(std::string(reinterpret_cast<const char*>(
              sqlite3_column_text(integrity, 0))) == "ok");
    sqlite3_finalize(integrity);

    sqlite3_stmt* foreignKeyCheck = nullptr;
    REQUIRE(sqlite3_prepare_v2(database.getHandle(),
                               "PRAGMA foreign_key_check;",
                               -1, &foreignKeyCheck, nullptr) == SQLITE_OK);
    CHECK(sqlite3_step(foreignKeyCheck) == SQLITE_DONE);
    sqlite3_finalize(foreignKeyCheck);

    database.close();
    removeTempDb();
}

TEST_CASE("Sales reports filter transaction and refund activity by their dates") {
    const char* path = "tests/phase4_sales_report_ranges.db";
    std::remove(path);
    DatabaseManager database;
    REQUIRE(database.open(path));
    REQUIRE(database.initializeSchema());

    const char* reportData =
        "INSERT INTO transactions(transaction_id, transaction_date, order_total_cents, "
        "cash_received_cents, change_cents, status, refunded_cents, operator_name) "
        "VALUES"
        "(101, '2026-03-01T10:00:00', 10000, 10000, 0, 'partial_refund', 2500, 'Alice'),"
        "(102, '2026-03-07T10:00:00', 7000, 7000, 0, 'refunded', 7000, 'Bob'),"
        "(103, '2026-03-08T10:00:00', 4000, 4000, 0, 'completed', 0, 'Alice'),"
        "(104, '2026-03-04T10:00:00', 9000, 9000, 0, 'voided', 9000, 'Alice');"
        "INSERT INTO transaction_items(transaction_item_id, transaction_id, product_id, "
        "product_name, unit_price_cents, quantity, subtotal_cents) VALUES"
        "(101, 101, 'P001', 'Cookie snapshot', 5000, 2, 10000),"
        "(102, 102, 'P002', 'Brownie snapshot', 7000, 1, 7000),"
        "(103, 103, 'P003', 'Outside-range snapshot', 4000, 1, 4000),"
        "(104, 104, 'P004', 'Voided snapshot', 9000, 1, 9000);"
        "INSERT INTO transaction_refunds(refund_id, transaction_id, refund_date, "
        "refund_type, reason, refund_cents, operator_name) VALUES"
        "(1, 101, '2026-03-03T11:00:00', 'refund', 'Partial return', 2500, 'Bob'),"
        "(2, 102, '2026-03-08T11:00:00', 'refund', 'Outside-range refund', 7000, 'Bob'),"
        "(3, 104, '2026-03-05T11:00:00', 'void', 'Cancelled sale', 9000, 'Bob');";
    char* sqlError = nullptr;
    const int insertResult = sqlite3_exec(
        database.getHandle(), reportData, nullptr, nullptr, &sqlError);
    const std::string insertError = sqlError ? sqlError : "";
    sqlite3_free(sqlError);
    REQUIRE_MESSAGE(insertResult == SQLITE_OK, insertError);

    TransactionRepository transactions(database);
    SalesReportRecord report{};
    std::vector<ProductSalesRecord> products;
    std::vector<OperatorSalesRecord> operators;
    std::vector<TransactionRefundRecord> refunds;
    std::string error;
    REQUIRE(transactions.getSalesReport(
        "2026-03-01", "2026-03-07", report, products, operators, refunds, error));
    CHECK(report.transactionCount == 2);
    CHECK(report.productsSold == 3);
    CHECK(report.grossSalesCents == 17000);
    CHECK(report.refundCount == 1);
    CHECK(report.refundedCents == 2500);
    CHECK(report.voidCount == 1);
    CHECK(report.voidedCents == 9000);
    CHECK(report.netSalesCents == 14500);
    REQUIRE(products.size() == 2);
    CHECK(products[0].productId == "P001");
    CHECK(products[0].quantitySold == 2);
    REQUIRE(operators.size() == 2);
    CHECK(operators[0].operatorName == "Alice");
    CHECK(operators[0].salesCents == 10000);
    REQUIRE(refunds.size() == 2);
    CHECK(refunds[0].refundId == 3);
    CHECK(refunds[1].refundId == 1);

    REQUIRE(transactions.getSalesReport(
        "2026-04-01", "2026-04-30", report, products, operators, refunds, error));
    CHECK(report.transactionCount == 0);
    CHECK(report.grossSalesCents == 0);
    CHECK(report.refundCount == 0);
    CHECK(report.voidCount == 0);
    CHECK(products.empty());
    CHECK(operators.empty());
    CHECK(refunds.empty());

    CHECK_FALSE(transactions.getSalesReport(
        "2026-03-07", "2026-03-01", report, products, operators, refunds, error));
    CHECK(error.find("start date") != std::string::npos);
    CHECK(report.transactionCount == 0);
    CHECK(products.empty());
    CHECK(refunds.empty());

    database.close();
    std::remove(path);
}

TEST_CASE("Product repository persists, searches, updates, and deactivates") {
    removeTempDb();

    DatabaseManager database;
    REQUIRE(database.open(TEMP_DB));
    REQUIRE(database.initializeSchema());

    ProductRepository products(database);
    std::string error;
    REQUIRE(products.create("P100", 1, "Test Cookie", 1255, 12,
                            "assets/products/uploads/P100.png", error));

    ProductRecord record;
    REQUIRE(products.findById("P100", record, error));
    CHECK(record.name == "Test Cookie");
    CHECK(record.priceCents == 1255);
    CHECK(record.active);
    CHECK(record.imagePath == "assets/products/uploads/P100.png");

    std::vector<ProductRecord> matches;
    REQUIRE(products.search("test", matches, error));
    REQUIRE(matches.size() == 1);
    CHECK(matches[0].productId == "P100");

    REQUIRE(products.update("P100", 1, "Test Cookie Deluxe", 1300,
                            "assets/products/uploads/P100.jpg", error));
    REQUIRE(products.findById("P100", record, error));
    CHECK(record.name == "Test Cookie Deluxe");
    CHECK(record.priceCents == 1300);
    CHECK(record.imagePath == "assets/products/uploads/P100.jpg");

    REQUIRE(products.deactivate("P100", error));
    REQUIRE(products.findById("P100", record, error));
    CHECK_FALSE(record.active);

    CHECK_FALSE(products.create("P100", 1, "Duplicate", 100, 5, error));
    CHECK_FALSE(products.create("", 1, "Invalid", 100, 5, error));
    CHECK_FALSE(products.create("P101", 1, "Invalid", -1, 5, error));
    CHECK_FALSE(products.create("P102", 1, "Invalid", 100, -1, error));

    database.close();
    removeTempDb();
}

TEST_CASE("Product deletion preserves history and writes off remaining stock") {
    removeTempDb();

    DatabaseManager database;
    REQUIRE(database.open(TEMP_DB));
    REQUIRE(database.initializeSchema());

    ProductRepository products(database);
    InventoryRepository inventory(database);
    std::string error;
    REQUIRE(products.create("P100", 1, "Deletable Cookie", 100, 0,
                            "assets/products/uploads/shared.png", error));
    REQUIRE(products.create("P101", 1, "Shared Image Cookie", 100, 0,
                            "assets/products/uploads/shared.png", error));
    REQUIRE(products.create("P102", 1, "Cookie With Stock", 100, 2, error));
    REQUIRE(products.create("P103", 1, "Cookie With Inventory History", 100, 0,
                            error));
    REQUIRE(products.create("P104", 1, "Cookie With Transaction History", 100, 0,
                            error));
    REQUIRE(products.create("P105", 1, "Unique Image Cookie", 100, 0,
                            "assets/products/uploads/unique.png", error));

    ProductRecord deletedRecord{};
    bool imagePathUnreferenced = false;
    REQUIRE(products.deleteProduct("P100", deletedRecord,
                                  imagePathUnreferenced, error));
    CHECK(deletedRecord.productId == "P100");
    CHECK_FALSE(imagePathUnreferenced);
    ProductRecord record{};
    CHECK_FALSE(products.findById("P100", record, error));
    REQUIRE(products.findById("P101", record, error));
    CHECK(record.imagePath == "assets/products/uploads/shared.png");

    REQUIRE(products.deleteProduct("P102", deletedRecord,
                                   imagePathUnreferenced, error, "Test Operator"));
    CHECK(deletedRecord.productId == "P102");
    CHECK_FALSE(products.findById("P102", record, error));
    sqlite3_stmt* stockWriteoff = nullptr;
    REQUIRE(sqlite3_prepare_v2(
        database.getHandle(),
        "SELECT quantity_change, resulting_quantity, reason, operator_name "
        "FROM inventory_logs WHERE product_id = 'P102';",
        -1, &stockWriteoff, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(stockWriteoff) == SQLITE_ROW);
    CHECK(sqlite3_column_int(stockWriteoff, 0) == -2);
    CHECK(sqlite3_column_int(stockWriteoff, 1) == 0);
    CHECK(std::string(reinterpret_cast<const char*>(
              sqlite3_column_text(stockWriteoff, 2)))
          .find("written off") != std::string::npos);
    CHECK(std::string(reinterpret_cast<const char*>(
              sqlite3_column_text(stockWriteoff, 3))) == "Test Operator");
    sqlite3_finalize(stockWriteoff);

    REQUIRE(inventory.restock("P103", 1, "Seed movement for delete test", error));
    REQUIRE(inventory.adjust("P103", -1, "Return stock to zero", error));
    REQUIRE(products.deleteProduct("P103", deletedRecord,
                                   imagePathUnreferenced, error));
    CHECK_FALSE(products.findById("P103", record, error));
    InventoryLog retainedLog{};
    REQUIRE(inventory.getLatestLog("P103", retainedLog, error));
    CHECK(retainedLog.resultingQuantity == 0);
    CHECK(retainedLog.reason == "Return stock to zero");

    REQUIRE(sqlite3_exec(
        database.getHandle(),
        "INSERT INTO transactions(transaction_id, transaction_date, order_total_cents, "
        "cash_received_cents, change_cents) VALUES(900, '2026-01-01', 100, 100, 0);"
        "INSERT INTO transaction_items(transaction_id, product_id, product_name, "
        "unit_price_cents, quantity, subtotal_cents) "
        "VALUES(900, 'P104', 'Cookie With Transaction History', 100, 1, 100);"
        "INSERT INTO transaction_refunds(refund_id, transaction_id, refund_date, "
        "refund_type, reason, refund_cents) "
        "VALUES(901, 900, '2026-01-02', 'refund', 'Return', 100);"
        "INSERT INTO transaction_refund_items(refund_id, transaction_item_id, "
        "product_id, product_name, unit_price_cents, quantity, subtotal_cents) "
        "VALUES(901, 1, 'P104', 'Cookie With Transaction History', 100, 1, 100);",
        nullptr, nullptr, nullptr) == SQLITE_OK);
    REQUIRE(products.deleteProduct("P104", deletedRecord,
                                   imagePathUnreferenced, error));
    CHECK_FALSE(products.findById("P104", record, error));
    sqlite3_stmt* retainedTransaction = nullptr;
    REQUIRE(sqlite3_prepare_v2(
        database.getHandle(),
        "SELECT COUNT(*) FROM transaction_items WHERE product_id = 'P104';",
        -1, &retainedTransaction, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(retainedTransaction) == SQLITE_ROW);
    CHECK(sqlite3_column_int(retainedTransaction, 0) == 1);
    sqlite3_finalize(retainedTransaction);
    REQUIRE(sqlite3_prepare_v2(
        database.getHandle(),
        "SELECT COUNT(*) FROM transaction_refund_items WHERE product_id = 'P104';",
        -1, &retainedTransaction, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(retainedTransaction) == SQLITE_ROW);
    CHECK(sqlite3_column_int(retainedTransaction, 0) == 1);
    sqlite3_finalize(retainedTransaction);

    REQUIRE(products.deleteProduct("P105", deletedRecord,
                                  imagePathUnreferenced, error));
    CHECK(imagePathUnreferenced);
    CHECK(deletedRecord.imagePath == "assets/products/uploads/unique.png");

    database.close();
    removeTempDb();
}

TEST_CASE("Daily dashboard transactions match the KPI count and sort newest first") {
    removeTempDb();

    DatabaseManager database;
    REQUIRE(database.open(TEMP_DB));
    REQUIRE(database.initializeSchema());

    auto insertTransaction = [&](int id, const char* date,
                                 const char* status, bool withItem) {
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(
                database.getHandle(),
                "INSERT INTO transactions(transaction_id, transaction_date, "
                "order_total_cents, cash_received_cents, change_cents, status) "
                "VALUES(?, ?, 100, 100, 0, ?);",
                -1, &statement, nullptr) != SQLITE_OK)
            return false;
        sqlite3_bind_int(statement, 1, id);
        sqlite3_bind_text(statement, 2, date, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 3, status, -1, SQLITE_TRANSIENT);
        const int insertResult = sqlite3_step(statement);
        sqlite3_finalize(statement);
        if (insertResult != SQLITE_DONE || !withItem)
            return insertResult == SQLITE_DONE;

        if (sqlite3_prepare_v2(
                database.getHandle(),
                "INSERT INTO transaction_items(transaction_id, product_id, "
                "product_name, unit_price_cents, quantity, subtotal_cents) "
                "VALUES(?, 'P001', 'Chocolate Chip Cookie', 100, 1, 100);",
                -1, &statement, nullptr) != SQLITE_OK)
            return false;
        sqlite3_bind_int(statement, 1, id);
        const int itemResult = sqlite3_step(statement);
        sqlite3_finalize(statement);
        return itemResult == SQLITE_DONE;
    };

    for (int id = 1; id <= 7; ++id) {
        const std::string time = "2026-03-15T" +
            std::string(id < 10 ? "0" : "") + std::to_string(id) + ":00:00";
        REQUIRE(insertTransaction(id, time.c_str(), "completed", true));
    }
    REQUIRE(insertTransaction(8, "2026-03-14T23:59:59", "completed", true));
    REQUIRE(insertTransaction(9, "2026-03-15T17:00:00", "voided", true));
    REQUIRE(insertTransaction(10, "2026-03-15T18:00:00", "refunded", true));
    REQUIRE(insertTransaction(11, "2026-03-15T19:00:00", "completed", false));

    TransactionRepository transactions(database);
    DailySalesRecord report{};
    std::vector<ProductSalesRecord> products;
    std::vector<TransactionRecord> today;
    std::string error;
    REQUIRE(transactions.getDailySales("2026-03-15", report, products, error));
    REQUIRE(transactions.listForDate("2026-03-15", today, error));
    CHECK(report.transactionCount == 7);
    REQUIRE(today.size() == static_cast<std::size_t>(report.transactionCount));
    for (int index = 0; index < 7; ++index)
        CHECK(today[static_cast<std::size_t>(index)].transactionId == 7 - index);

    database.close();
    removeTempDb();
}

TEST_CASE("Inventory repository restocks, adjusts, and rejects negative stock") {
    removeTempDb();

    DatabaseManager database;
    REQUIRE(database.open(TEMP_DB));
    REQUIRE(database.initializeSchema());

    InventoryRepository inventory(database);
    std::string error;

    REQUIRE(inventory.restock("P001", 25, "Supplier delivery", error));
    int stock = 0;
    REQUIRE(inventory.getStock("P001", stock, error));
    CHECK(stock == 125);

    InventoryLog log;
    REQUIRE(inventory.getLatestLog("P001", log, error));
    CHECK(log.quantityChange == 25);
    CHECK(log.resultingQuantity == 125);
    CHECK(log.reason == "Supplier delivery");

    REQUIRE(inventory.adjust("P001", -30, "Damaged stock", error));
    REQUIRE(inventory.getStock("P001", stock, error));
    CHECK(stock == 95);

    CHECK_FALSE(inventory.adjust("P001", -96, "Invalid adjustment", error));
    REQUIRE(inventory.getStock("P001", stock, error));
    CHECK(stock == 95);

    CHECK_FALSE(inventory.restock("P001", 0, "Invalid restock", error));
    CHECK_FALSE(inventory.adjust("P001", 5, "", error));

    database.close();
    removeTempDb();
}

TEST_CASE("POS console product and inventory workflows use repositories") {
    const char* tempDb = "tests/phase2_pos_workflows.db";
    std::remove(tempDb);

    std::istringstream input(
        "8\n"
        "2\nP100\nTask Cookie\n12.50\n5\n"
        "5\nTask\n"
        "3\nP100\nTask Cookie Deluxe\n13.25\n"
        "1\n"
        "6\n"
        "9\n"
        "2\nP100\n3\nSupplier delivery\n"
        "3\nP100\n-2\nDamaged stock\n"
        "1\n"
        "4\n"
        "8\n"
        "4\nP100\n"
        "5\nTask\n"
        "6\n"
        "2\nP100\n1\n"
        "7\n"
    );

    std::ostringstream output;
    std::streambuf* oldInput = std::cin.rdbuf(input.rdbuf());
    std::streambuf* oldOutput = std::cout.rdbuf(output.rdbuf());

    {
        POS pos(tempDb);
        std::string operatorError;
        REQUIRE(pos.createInitialAdministrator(
            "inventory_admin", "InventoryTestPassword!23", operatorError));
        REQUIRE(pos.login("inventory_admin",
                          "InventoryTestPassword!23", operatorError));
        pos.run();
    }

    std::cout.rdbuf(oldOutput);
    std::cin.rdbuf(oldInput);

    const std::string result = output.str();
    CHECK(result.find("Product P100 added successfully.") != std::string::npos);
    CHECK(result.find("Task Cookie") != std::string::npos);
    CHECK(result.find("Task Cookie Deluxe") != std::string::npos);
    CHECK(result.find("Restocked P100 by 3.") != std::string::npos);
    CHECK(result.find("Adjusted stock for P100 by -2.") != std::string::npos);
    CHECK(result.find("OUT OF STOCK") == std::string::npos);
    CHECK(result.find("LOW STOCK") != std::string::npos);
    CHECK(result.find("P100 deactivated successfully.") != std::string::npos);
    CHECK(result.find("Inactive") != std::string::npos);
    CHECK(result.find("Product not found or inactive.") != std::string::npos);

    std::remove(tempDb);
}

TEST_CASE("Transaction checkout commits atomically and preserves snapshots") {
    removeTempDb();

    DatabaseManager database;
    REQUIRE(database.open(TEMP_DB));
    REQUIRE(database.initializeSchema());

    TransactionRepository transactions(database);
    std::vector<TransactionItemInput> items = {{"P001", 2}};
    TransactionRecord transaction;
    std::vector<TransactionItemRecord> savedItems;
    std::string error;

    REQUIRE(transactions.createAtomic(items, 13000, transaction, savedItems, error));
    CHECK(transaction.transactionId == 1);
    CHECK(transaction.orderTotalCents == 13000);
    CHECK(transaction.changeCents == 0);
    REQUIRE(savedItems.size() == 1);
    CHECK(savedItems[0].productName == "Chocolate Chip Cookie");
    CHECK(savedItems[0].unitPriceCents == 6500);

    InventoryRepository inventory(database);
    int stock = 0;
    REQUIRE(inventory.getStock("P001", stock, error));
    CHECK(stock == 98);

    ProductRepository products(database);
    REQUIRE(products.update("P001", 1, "Renamed Cookie", 9000, error));

    TransactionRecord retrieved;
    REQUIRE(transactions.getById(transaction.transactionId, retrieved, error));
    CHECK(retrieved.orderTotalCents == 13000);
    std::vector<TransactionItemRecord> historicalItems;
    REQUIRE(transactions.getItems(transaction.transactionId, historicalItems, error));
    REQUIRE(historicalItems.size() == 1);
    CHECK(historicalItems[0].productName == "Chocolate Chip Cookie");
    CHECK(historicalItems[0].unitPriceCents == 6500);

    database.close();
    removeTempDb();
}

TEST_CASE("Checkout rejects insufficient stock and payment without changes") {
    removeTempDb();

    DatabaseManager database;
    REQUIRE(database.open(TEMP_DB));
    REQUIRE(database.initializeSchema());

    InventoryRepository inventory(database);
    std::string error;
    REQUIRE(inventory.adjust("P001", -98, "Test stock setup", error));

    TransactionRepository transactions(database);
    TransactionRecord transaction;
    std::vector<TransactionItemRecord> savedItems;
    std::vector<TransactionItemInput> items = {{"P001", 3}};
    CHECK_FALSE(transactions.createAtomic(items, 19500, transaction, savedItems, error));
    CHECK(error.find("Insufficient stock") != std::string::npos);

    int stock = 0;
    REQUIRE(inventory.getStock("P001", stock, error));
    CHECK(stock == 2);

    items = {{"P001", 1}};
    CHECK_FALSE(transactions.createAtomic(items, 6000, transaction, savedItems, error));
    CHECK(error == "Insufficient payment.");
    REQUIRE(inventory.getStock("P001", stock, error));
    CHECK(stock == 2);

    sqlite3_stmt* count = nullptr;
    REQUIRE(sqlite3_prepare_v2(database.getHandle(),
                               "SELECT COUNT(*) FROM transactions;",
                               -1, &count, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(count) == SQLITE_ROW);
    CHECK(sqlite3_column_int(count, 0) == 0);
    sqlite3_finalize(count);

    database.close();
    removeTempDb();
}

TEST_CASE("Database failure rolls back transaction, items, stock, and logs") {
    removeTempDb();

    DatabaseManager database;
    REQUIRE(database.open(TEMP_DB));
    REQUIRE(database.initializeSchema());

    char* message = nullptr;
    REQUIRE(sqlite3_exec(database.getHandle(),
                         "CREATE TRIGGER fail_transaction_item "
                         "BEFORE INSERT ON transaction_items "
                         "BEGIN SELECT RAISE(ABORT, 'forced transaction item failure'); END;",
                         nullptr, nullptr, &message) == SQLITE_OK);
    sqlite3_free(message);

    TransactionRepository transactions(database);
    TransactionRecord transaction;
    std::vector<TransactionItemRecord> savedItems;
    std::string error;
    std::vector<TransactionItemInput> items = {{"P001", 2}};

    CHECK_FALSE(transactions.createAtomic(items, 13000, transaction, savedItems, error));
    CHECK(error.find("forced transaction item failure") != std::string::npos);

    InventoryRepository inventory(database);
    int stock = 0;
    REQUIRE(inventory.getStock("P001", stock, error));
    CHECK(stock == 100);

    sqlite3_stmt* countTransactions = nullptr;
    sqlite3_stmt* countItems = nullptr;
    sqlite3_stmt* countLogs = nullptr;
    REQUIRE(sqlite3_prepare_v2(database.getHandle(),
                               "SELECT COUNT(*) FROM transactions;",
                               -1, &countTransactions, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_prepare_v2(database.getHandle(),
                               "SELECT COUNT(*) FROM transaction_items;",
                               -1, &countItems, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_prepare_v2(database.getHandle(),
                               "SELECT COUNT(*) FROM inventory_logs;",
                               -1, &countLogs, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(countTransactions) == SQLITE_ROW);
    REQUIRE(sqlite3_step(countItems) == SQLITE_ROW);
    REQUIRE(sqlite3_step(countLogs) == SQLITE_ROW);
    CHECK(sqlite3_column_int(countTransactions, 0) == 0);
    CHECK(sqlite3_column_int(countItems, 0) == 0);
    CHECK(sqlite3_column_int(countLogs, 0) == 0);
    sqlite3_finalize(countTransactions);
    sqlite3_finalize(countItems);
    sqlite3_finalize(countLogs);

    database.close();
    removeTempDb();
}

TEST_CASE("Transaction history and daily sales preserve committed snapshots") {
    removeTempDb();

    DatabaseManager database;
    REQUIRE(database.open(TEMP_DB));
    REQUIRE(database.initializeSchema());

    TransactionRepository transactions(database);
    std::vector<TransactionItemInput> items = {
        {"P001", 2},
        {"P002", 1}
    };
    TransactionRecord transaction;
    std::vector<TransactionItemRecord> savedItems;
    std::string error;
    REQUIRE(transactions.createAtomic(items, 20000,
                                      transaction, savedItems, error));

    std::vector<TransactionRecord> history;
    REQUIRE(transactions.listRecent(history, 50, error));
    REQUIRE(history.size() == 1);
    CHECK(history[0].transactionId == transaction.transactionId);

    TransactionRecord found;
    REQUIRE(transactions.getById(transaction.transactionId, found, error));
    CHECK(found.orderTotalCents == 20000);

    std::vector<TransactionItemRecord> historicalItems;
    REQUIRE(transactions.getItems(transaction.transactionId, historicalItems, error));
    REQUIRE(historicalItems.size() == 2);

    std::string date = found.transactionDate.substr(0, 10);
    DailySalesRecord report;
    std::vector<ProductSalesRecord> productSales;
    REQUIRE(transactions.getDailySales(date, report, productSales, error));
    CHECK(report.transactionCount == 1);
    CHECK(report.productsSold == 3);
    CHECK(report.grossSalesCents == 20000);
    REQUIRE(productSales.size() == 2);
    CHECK(productSales.front().quantitySold == 2);
    CHECK(productSales.front().productName == "Chocolate Chip Cookie");

    database.close();
    removeTempDb();
}

TEST_CASE("POS history and reports menu handles receipts and empty results") {
    const char* tempDb = "tests/phase2_history_pos.db";
    std::remove(tempDb);

    std::istringstream input(
        "10\n"
        "1\n"
        "2\n999999\n"
        "3\nnot-a-date\n"
        "3\n2020-01-01\n"
        "4\n"
        "7\n"
    );

    std::ostringstream output;
    std::streambuf* oldInput = std::cin.rdbuf(input.rdbuf());
    std::streambuf* oldOutput = std::cout.rdbuf(output.rdbuf());

    {
        POS pos(tempDb);
        std::string operatorError;
        REQUIRE(pos.createInitialAdministrator(
            "history_admin", "HistoryTestPassword!23", operatorError));
        REQUIRE(pos.login("history_admin",
                          "HistoryTestPassword!23", operatorError));
        pos.run();
    }

    std::cout.rdbuf(oldOutput);
    std::cin.rdbuf(oldInput);

    const std::string result = output.str();
    CHECK(result.find("No completed transactions.") != std::string::npos);
    CHECK(result.find("Transaction not found.") != std::string::npos);
    CHECK(result.find("Invalid date. Use YYYY-MM-DD.") != std::string::npos);
    CHECK(result.find("Completed transactions: 0") != std::string::npos);
    CHECK(result.find("Total products sold: 0") != std::string::npos);
    CHECK(result.find("No product sales for this date.") != std::string::npos);
    CHECK(result.find("Goodbye!") != std::string::npos);

    std::remove(tempDb);
}

TEST_CASE("Database backup validates, restores, and rejects corrupted files") {
    const char* tempDb = "tests/phase4_backup_restore.db";
    const char* validBackup = "tests/phase4_valid_backup.db";
    const char* corruptBackup = "tests/phase4_corrupt_backup.db";
    std::remove(tempDb);
    std::remove(validBackup);
    std::remove(corruptBackup);

    DatabaseManager database;
    std::string error;
    REQUIRE(database.open(tempDb));
    REQUIRE(database.initializeSchema());

    InventoryRepository inventory(database);
    REQUIRE(inventory.adjust("P001", -20, "Create backup test stock",
                             error));
    int stock = 0;
    REQUIRE(inventory.getStock("P001", stock, error));
    CHECK(stock == 80);

    REQUIRE(database.backupDatabase(validBackup, error));
    REQUIRE(database.validateDatabaseFile(validBackup, error));

    std::ofstream corrupt(corruptBackup, std::ios::binary);
    REQUIRE(corrupt.is_open());
    corrupt << "this is not a sqlite database";
    corrupt.close();
    CHECK_FALSE(database.validateDatabaseFile(corruptBackup, error));

    REQUIRE(inventory.adjust("P001", -10, "Change before restore",
                             error));
    REQUIRE(inventory.getStock("P001", stock, error));
    CHECK(stock == 70);

    REQUIRE(database.restoreDatabase(validBackup, error));
    REQUIRE(inventory.getStock("P001", stock, error));
    CHECK(stock == 80);

    sqlite3_stmt* integrity = nullptr;
    REQUIRE(sqlite3_prepare_v2(database.getHandle(),
                               "PRAGMA integrity_check;",
                               -1, &integrity, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(integrity) == SQLITE_ROW);
    CHECK(std::string(reinterpret_cast<const char*>(
              sqlite3_column_text(integrity, 0))) == "ok");
    sqlite3_finalize(integrity);

    database.close();
    std::remove(tempDb);
    std::remove(validBackup);
    std::remove(corruptBackup);
}

TEST_CASE("Full refund restores stock and prevents duplicate refunds") {
    const char* tempDb = "tests/phase4_full_refund.db";
    std::remove(tempDb);
    DatabaseManager database;
    std::string error;
    REQUIRE(database.open(tempDb));
    REQUIRE(database.initializeSchema());
    TransactionRepository transactions(database);
    InventoryRepository inventory(database);

    std::vector<TransactionItemInput> items = {{"P001", 2}};
    TransactionRecord transaction;
    std::vector<TransactionItemRecord> saved;
    REQUIRE(transactions.createAtomic(items, 13000, transaction, saved, error));

    TransactionRefundRecord refund{};
    INFO("Full refund error: " << error);
    REQUIRE(transactions.fullRefund(transaction.transactionId,
                                    "Customer return", refund, error));
    CHECK(refund.refundCents == 13000);
    CHECK(refund.refundType == "refund");

    int stock = 0;
    REQUIRE(inventory.getStock("P001", stock, error));
    CHECK(stock == 100);
    TransactionRecord retrieved;
    REQUIRE(transactions.getById(transaction.transactionId, retrieved, error));
    CHECK(retrieved.status == "refunded");
    CHECK(retrieved.refundedCents == 13000);
    CHECK_FALSE(transactions.fullRefund(transaction.transactionId,
                                        "Duplicate attempt", refund, error));

    DailySalesRecord report;
    std::vector<ProductSalesRecord> productSales;
    std::string date = transaction.transactionDate.substr(0, 10);
    REQUIRE(transactions.getDailySales(date, report, productSales, error));
    CHECK(report.transactionCount == 0);
    CHECK(report.productsSold == 0);
    CHECK(report.grossSalesCents == 0);

    database.close();
    std::remove(tempDb);
}

TEST_CASE("Partial refund and void restore only remaining stock") {
    const char* tempDb = "tests/phase4_partial_void_refund.db";
    std::remove(tempDb);
    DatabaseManager database;
    std::string error;
    REQUIRE(database.open(tempDb));
    REQUIRE(database.initializeSchema());
    TransactionRepository transactions(database);
    InventoryRepository inventory(database);

    std::vector<TransactionItemInput> items = {{"P001", 3}};
    TransactionRecord transaction;
    std::vector<TransactionItemRecord> saved;
    REQUIRE(transactions.createAtomic(items, 19500, transaction, saved, error));

    TransactionRefundRecord partial{};
    std::vector<TransactionRefundItemInput> partialItems = {{"P001", 1}};
    INFO("Partial refund error: " << error);
    REQUIRE(transactions.partialRefund(transaction.transactionId,
                                       partialItems, "Damaged cookie",
                                       partial, error));
    CHECK(partial.refundCents == 6500);

    int stock = 0;
    REQUIRE(inventory.getStock("P001", stock, error));
    CHECK(stock == 98);
    TransactionRecord retrieved;
    REQUIRE(transactions.getById(transaction.transactionId, retrieved, error));
    CHECK(retrieved.status == "partial_refund");
    CHECK(retrieved.refundedCents == 6500);

    CHECK_FALSE(transactions.partialRefund(transaction.transactionId,
                                           {{"P001", 3}},
                                           "Too many", partial, error));
    REQUIRE(inventory.getStock("P001", stock, error));
    CHECK(stock == 98);

    TransactionRefundRecord finalPartial{};
    REQUIRE(transactions.partialRefund(transaction.transactionId,
                                       {{"P001", 2}},
                                       "Return remaining items",
                                       finalPartial, error));
    REQUIRE(inventory.getStock("P001", stock, error));
    CHECK(stock == 100);
    REQUIRE(transactions.getById(transaction.transactionId, retrieved, error));
    CHECK(retrieved.status == "refunded");
    CHECK(retrieved.refundedCents == 19500);

    std::vector<TransactionItemInput> voidItems = {{"P002", 1}};
    TransactionRecord voidTarget;
    std::vector<TransactionItemRecord> voidSaved;
    REQUIRE(transactions.createAtomic(voidItems, 7000,
                                      voidTarget, voidSaved, error));
    TransactionRefundRecord voidRefund{};
    REQUIRE(transactions.voidTransaction(voidTarget.transactionId,
                                         "Cashier correction",
                                         voidRefund, error));
    CHECK(voidRefund.refundType == "void");
    CHECK(voidRefund.refundCents == 7000);
    REQUIRE(inventory.getStock("P002", stock, error));
    CHECK(stock == 100);
    REQUIRE(transactions.getById(voidTarget.transactionId, retrieved, error));
    CHECK(retrieved.status == "voided");
    CHECK_FALSE(transactions.voidTransaction(voidTarget.transactionId,
                                             "Duplicate void",
                                             voidRefund, error));

    database.close();
    std::remove(tempDb);
}

TEST_CASE("Refund failure rolls back refund record, status, and inventory") {
    const char* tempDb = "tests/phase4_refund_rollback.db";
    std::remove(tempDb);
    DatabaseManager database;
    std::string error;
    REQUIRE(database.open(tempDb));
    REQUIRE(database.initializeSchema());
    TransactionRepository transactions(database);
    InventoryRepository inventory(database);

    std::vector<TransactionItemInput> items = {{"P001", 2}};
    TransactionRecord transaction;
    std::vector<TransactionItemRecord> saved;
    REQUIRE(transactions.createAtomic(items, 13000, transaction, saved, error));

    char* message = nullptr;
    REQUIRE(sqlite3_exec(database.getHandle(),
                         "CREATE TRIGGER fail_refund_item "
                         "BEFORE INSERT ON transaction_refund_items "
                         "BEGIN SELECT RAISE(ABORT, 'forced refund item failure'); END;",
                         nullptr, nullptr, &message) == SQLITE_OK);
    sqlite3_free(message);

    TransactionRefundRecord refund{};
    bool rollbackResult = transactions.fullRefund(
        transaction.transactionId, "Test rollback", refund, error);
    INFO("Rollback refund result=" << rollbackResult << " error=" << error);
    CHECK_FALSE(rollbackResult);
    CHECK(error.find("forced refund item failure") != std::string::npos);

    int stock = 0;
    REQUIRE(inventory.getStock("P001", stock, error));
    CHECK(stock == 98);
    TransactionRecord retrieved;
    REQUIRE(transactions.getById(transaction.transactionId, retrieved, error));
    CHECK(retrieved.status == "completed");
    CHECK(retrieved.refundedCents == 0);

    sqlite3_stmt* count = nullptr;
    REQUIRE(sqlite3_prepare_v2(database.getHandle(),
                               "SELECT COUNT(*) FROM transaction_refunds;",
                               -1, &count, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(count) == SQLITE_ROW);
    CHECK(sqlite3_column_int(count, 0) == 0);
    sqlite3_finalize(count);

    database.close();
    std::remove(tempDb);
}
