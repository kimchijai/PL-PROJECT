#include "DatabaseManager.h"

#include <sstream>
#include <cstdio>

DatabaseManager::DatabaseManager() : database_(nullptr) {}

DatabaseManager::~DatabaseManager() {
    close();
}

bool DatabaseManager::open(const std::string& databasePath) {
    close();
    if (sqlite3_open(databasePath.c_str(), &database_) != SQLITE_OK) {
        lastError_ = database_ ? sqlite3_errmsg(database_) : "Unable to open SQLite database.";
        close();
        return false;
    }
    if (sqlite3_exec(database_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(database_);
        close();
        return false;
    }
    return true;
}

void DatabaseManager::close() {
    if (database_) {
        sqlite3_close(database_);
        database_ = nullptr;
    }
}

bool DatabaseManager::execute(const std::string& sql) {
    char* message = nullptr;
    int result = sqlite3_exec(database_, sql.c_str(), nullptr, nullptr, &message);
    if (result != SQLITE_OK) {
        lastError_ = message ? message : sqlite3_errmsg(database_);
        sqlite3_free(message);
        return false;
    }
    sqlite3_free(message);
    return true;
}

bool DatabaseManager::initializeSchema() {
    if (!database_) {
        lastError_ = "Database is not open.";
        return false;
    }

    const char* schema =
        "CREATE TABLE IF NOT EXISTS categories ("
        "category_id INTEGER PRIMARY KEY,"
        "name TEXT NOT NULL UNIQUE"
        ");"
        "CREATE TABLE IF NOT EXISTS products ("
        "product_id TEXT PRIMARY KEY,"
        "category_id INTEGER NOT NULL,"
        "name TEXT NOT NULL,"
        "price_cents INTEGER NOT NULL CHECK(price_cents >= 0),"
        "active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0, 1)),"
        "FOREIGN KEY(category_id) REFERENCES categories(category_id)"
        ");"
        "CREATE TABLE IF NOT EXISTS product_seed_suppressions ("
        "product_id TEXT PRIMARY KEY,"
        "product_name TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS inventory ("
        "product_id TEXT PRIMARY KEY,"
        "quantity INTEGER NOT NULL CHECK(quantity >= 0),"
        "FOREIGN KEY(product_id) REFERENCES products(product_id)"
        ");"
        "CREATE TABLE IF NOT EXISTS inventory_logs ("
        "log_id INTEGER PRIMARY KEY,"
        "product_id TEXT NOT NULL,"
        "quantity_change INTEGER NOT NULL,"
        "reason TEXT NOT NULL,"
        "resulting_quantity INTEGER NOT NULL CHECK(resulting_quantity >= 0),"
        "logged_at TEXT NOT NULL,"
        "operator_name TEXT NOT NULL DEFAULT 'Unknown'"
        ");"
        "CREATE TABLE IF NOT EXISTS transactions ("
        "transaction_id INTEGER PRIMARY KEY,"
        "transaction_date TEXT NOT NULL,"
        "order_total_cents INTEGER NOT NULL,"
        "cash_received_cents INTEGER NOT NULL,"
        "change_cents INTEGER NOT NULL,"
        "operator_name TEXT NOT NULL DEFAULT 'Unknown'"
        ");"
        "CREATE TABLE IF NOT EXISTS transaction_items ("
        "transaction_item_id INTEGER PRIMARY KEY,"
        "transaction_id INTEGER NOT NULL,"
        "product_id TEXT NOT NULL,"
        "product_name TEXT NOT NULL,"
        "unit_price_cents INTEGER NOT NULL,"
        "quantity INTEGER NOT NULL CHECK(quantity > 0),"
        "subtotal_cents INTEGER NOT NULL,"
        "FOREIGN KEY(transaction_id) REFERENCES transactions(transaction_id)"
        ");";

    char* message = nullptr;
    int result = sqlite3_exec(database_, schema, nullptr, nullptr, &message);
    if (result != SQLITE_OK) {
        lastError_ = message ? message : sqlite3_errmsg(database_);
        sqlite3_free(message);
        return false;
    }
    sqlite3_free(message);
    if (!execute("ALTER TABLE products ADD COLUMN image_path TEXT NOT NULL DEFAULT '';")) {
        if (lastError_.find("duplicate column name") == std::string::npos)
            return false;
    }
    if (!execute("ALTER TABLE transactions ADD COLUMN status TEXT NOT NULL DEFAULT 'completed';")) {
        if (lastError_.find("duplicate column name") == std::string::npos)
            return false;
    }
    if (!execute("ALTER TABLE transactions ADD COLUMN refunded_cents INTEGER NOT NULL DEFAULT 0;")) {
        if (lastError_.find("duplicate column name") == std::string::npos)
            return false;
    }
    if (!execute("ALTER TABLE transactions ADD COLUMN operator_name TEXT NOT NULL DEFAULT 'Unknown';")) {
        if (lastError_.find("duplicate column name") == std::string::npos)
            return false;
    }
    if (!execute("ALTER TABLE inventory_logs ADD COLUMN operator_name TEXT NOT NULL DEFAULT 'Unknown';")) {
        if (lastError_.find("duplicate column name") == std::string::npos)
            return false;
    }
    if (!migrateInventoryLogProductReference())
        return false;

    const char* refundSchema =
        "CREATE TABLE IF NOT EXISTS transaction_refunds ("
        "refund_id INTEGER PRIMARY KEY,"
        "transaction_id INTEGER NOT NULL,"
        "refund_date TEXT NOT NULL,"
        "refund_type TEXT NOT NULL CHECK(refund_type IN ('refund','void')),"
        "reason TEXT NOT NULL,"
        "refund_cents INTEGER NOT NULL CHECK(refund_cents >= 0),"
        "operator_name TEXT NOT NULL DEFAULT 'Unknown',"
        "FOREIGN KEY(transaction_id) REFERENCES transactions(transaction_id)"
        ");"
        "CREATE TABLE IF NOT EXISTS transaction_refund_items ("
        "refund_item_id INTEGER PRIMARY KEY,"
        "refund_id INTEGER NOT NULL,"
        "transaction_item_id INTEGER NOT NULL,"
        "product_id TEXT NOT NULL,"
        "product_name TEXT NOT NULL,"
        "unit_price_cents INTEGER NOT NULL,"
        "quantity INTEGER NOT NULL,"
        "subtotal_cents INTEGER NOT NULL,"
        "FOREIGN KEY(refund_id) REFERENCES transaction_refunds(refund_id),"
        "FOREIGN KEY(transaction_item_id) REFERENCES transaction_items(transaction_item_id)"
        ");";
    result = sqlite3_exec(database_, refundSchema, nullptr, nullptr, &message);
    if (result != SQLITE_OK) {
        lastError_ = message ? message : sqlite3_errmsg(database_);
        sqlite3_free(message);
        return false;
    }
    sqlite3_free(message);
    if (!execute("ALTER TABLE transaction_refunds ADD COLUMN operator_name TEXT NOT NULL DEFAULT 'Unknown';")) {
        if (lastError_.find("duplicate column name") == std::string::npos)
            return false;
    }
    if (!execute(
            "CREATE TABLE IF NOT EXISTS app_settings("
            "setting_key TEXT PRIMARY KEY,"
            "setting_value TEXT NOT NULL"
            ");"
            "INSERT OR IGNORE INTO app_settings(setting_key, setting_value) "
            "VALUES('current_operator', '');"
            "CREATE TABLE IF NOT EXISTS operator_accounts("
            "account_id INTEGER PRIMARY KEY,"
            "username TEXT NOT NULL COLLATE NOCASE UNIQUE,"
            "password_hash TEXT NOT NULL,"
            "role TEXT NOT NULL CHECK(role IN ('administrator','operator')),"
            "active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1)),"
            "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
            ");"))
        return false;
    if (!seedBakeryProducts())
        return false;

    const char* existingImageMappings =
        "UPDATE products SET image_path='assets/products/chocolate_chip.jpg' WHERE product_id='P001' AND image_path='';"
        "UPDATE products SET image_path='assets/products/double_chocolate.jpg' WHERE product_id='P002' AND image_path='';"
        "UPDATE products SET image_path='assets/products/red_velvet.jpg' WHERE product_id='P003' AND image_path='';"
        "UPDATE products SET image_path='assets/products/smores.jpg' WHERE product_id='P004' AND image_path='';"
        "UPDATE products SET image_path='assets/products/brookies.jpg' WHERE product_id='P005' AND image_path='';"
        "UPDATE products SET image_path='assets/products/nutella.jpg' WHERE product_id='P006' AND image_path='';"
        "UPDATE products SET image_path='assets/products/dubai_chewy_one piece.jpg' WHERE product_id='P007' AND image_path='';"
        "UPDATE products SET image_path='assets/products/dubai_chewy_four pieces.jpg' WHERE product_id='P008' AND image_path='';";
    return execute(existingImageMappings);
}

bool DatabaseManager::migrateInventoryLogProductReference() {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_, "PRAGMA foreign_key_list(inventory_logs);",
                           -1, &statement, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(database_);
        return false;
    }

    bool referencesProducts = false;
    int result = SQLITE_OK;
    while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
        const unsigned char* table = sqlite3_column_text(statement, 2);
        if (table && std::string(reinterpret_cast<const char*>(table)) == "products")
            referencesProducts = true;
    }
    sqlite3_finalize(statement);
    if (result != SQLITE_DONE) {
        lastError_ = sqlite3_errmsg(database_);
        return false;
    }
    if (!referencesProducts)
        return true;

    if (!beginTransaction())
        return false;
    const char* migration =
        "CREATE TABLE inventory_logs_without_product_fk ("
        "log_id INTEGER PRIMARY KEY,"
        "product_id TEXT NOT NULL,"
        "quantity_change INTEGER NOT NULL,"
        "reason TEXT NOT NULL,"
        "resulting_quantity INTEGER NOT NULL CHECK(resulting_quantity >= 0),"
        "logged_at TEXT NOT NULL,"
        "operator_name TEXT NOT NULL DEFAULT 'Unknown'"
        ");"
        "INSERT INTO inventory_logs_without_product_fk "
        "(log_id, product_id, quantity_change, reason, resulting_quantity, "
        "logged_at, operator_name) "
        "SELECT log_id, product_id, quantity_change, reason, resulting_quantity, "
        "logged_at, operator_name FROM inventory_logs;"
        "DROP TABLE inventory_logs;"
        "ALTER TABLE inventory_logs_without_product_fk RENAME TO inventory_logs;";
    if (!execute(migration)) {
        rollbackTransaction();
        return false;
    }
    return commitTransaction();
}

bool DatabaseManager::seedBakeryProducts() {
    if (!database_) {
        lastError_ = "Database is not open.";
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_,
                           "INSERT OR IGNORE INTO categories(category_id, name) VALUES(1, 'Cookies');",
                           -1, &statement, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(database_);
        return false;
    }
    bool ok = sqlite3_step(statement) == SQLITE_DONE;
    if (!ok) lastError_ = sqlite3_errmsg(database_);
    sqlite3_finalize(statement);
    if (!ok) return false;

    if (sqlite3_prepare_v2(database_,
                           "INSERT OR IGNORE INTO categories(name) VALUES('Cookies');",
                           -1, &statement, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(database_);
        return false;
    }
    ok = sqlite3_step(statement) == SQLITE_DONE;
    if (!ok) lastError_ = sqlite3_errmsg(database_);
    sqlite3_finalize(statement);
    if (!ok) return false;

    int cookiesCategoryId = 0;
    if (sqlite3_prepare_v2(database_,
                           "SELECT category_id FROM categories WHERE name = 'Cookies';",
                           -1, &statement, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(database_);
        return false;
    }
    const int categoryResult = sqlite3_step(statement);
    if (categoryResult == SQLITE_ROW)
        cookiesCategoryId = sqlite3_column_int(statement, 0);
    else
        lastError_ = categoryResult == SQLITE_DONE
            ? "Cookies category could not be initialized."
            : sqlite3_errmsg(database_);
    sqlite3_finalize(statement);
    if (categoryResult != SQLITE_ROW) return false;

    struct SeedProduct {
        const char* productId;
        const char* name;
        int priceCents;
    };
    const SeedProduct seeds[] = {
        {"P001", "Chocolate Chip Cookie", 6500},
        {"P002", "Double Chocolate Cookie", 7000},
        {"P003", "Red Velvet Cookie", 7000},
        {"P004", "S'mores Cookie", 7500},
        {"P005", "Brookies", 7500},
        {"P006", "Nutella", 6500},
        {"P007", "Dubai Chewy Cookie (1 Piece)", 11000},
        {"P008", "Dubai Chewy Cookie (4 Pieces)", 42000}
    };

    if (!beginTransaction()) return false;
    for (const auto& seed : seeds) {
        sqlite3_stmt* existing = nullptr;
        if (sqlite3_prepare_v2(database_,
                               "SELECT 1 FROM products "
                               "WHERE product_id = ? OR lower(name) = lower(?) LIMIT 1;",
                               -1, &existing, nullptr) != SQLITE_OK) {
            lastError_ = sqlite3_errmsg(database_);
            rollbackTransaction();
            return false;
        }
        sqlite3_bind_text(existing, 1, seed.productId, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(existing, 2, seed.name, -1, SQLITE_TRANSIENT);
        const int existingResult = sqlite3_step(existing);
        sqlite3_finalize(existing);
        if (existingResult == SQLITE_ROW) continue;
        if (existingResult != SQLITE_DONE) {
            lastError_ = sqlite3_errmsg(database_);
            rollbackTransaction();
            return false;
        }

        sqlite3_stmt* suppression = nullptr;
        if (sqlite3_prepare_v2(
                database_,
                "SELECT 1 FROM product_seed_suppressions "
                "WHERE product_id = ? OR lower(product_name) = lower(?) LIMIT 1;",
                -1, &suppression, nullptr) != SQLITE_OK) {
            lastError_ = sqlite3_errmsg(database_);
            rollbackTransaction();
            return false;
        }
        sqlite3_bind_text(suppression, 1, seed.productId, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(suppression, 2, seed.name, -1, SQLITE_TRANSIENT);
        const int suppressionResult = sqlite3_step(suppression);
        sqlite3_finalize(suppression);
        if (suppressionResult == SQLITE_ROW) continue;
        if (suppressionResult != SQLITE_DONE) {
            lastError_ = sqlite3_errmsg(database_);
            rollbackTransaction();
            return false;
        }

        sqlite3_stmt* insert = nullptr;
        if (sqlite3_prepare_v2(database_,
                               "INSERT OR IGNORE INTO products"
                               "(product_id, category_id, name, price_cents, active) "
                               "VALUES(?, ?, ?, ?, 1);",
                               -1, &insert, nullptr) != SQLITE_OK) {
            lastError_ = sqlite3_errmsg(database_);
            rollbackTransaction();
            return false;
        }
        sqlite3_bind_text(insert, 1, seed.productId, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(insert, 2, cookiesCategoryId);
        sqlite3_bind_text(insert, 3, seed.name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(insert, 4, seed.priceCents);
        const int step = sqlite3_step(insert);
        const bool inserted = step == SQLITE_DONE && sqlite3_changes(database_) == 1;
        sqlite3_finalize(insert);
        if (step != SQLITE_DONE) {
            lastError_ = sqlite3_errmsg(database_);
            rollbackTransaction();
            return false;
        }

        if (!inserted) continue;
        sqlite3_stmt* inventory = nullptr;
        if (sqlite3_prepare_v2(database_,
                               "INSERT OR IGNORE INTO inventory(product_id, quantity) "
                               "VALUES(?, 100);",
                               -1, &inventory, nullptr) != SQLITE_OK) {
            lastError_ = sqlite3_errmsg(database_);
            rollbackTransaction();
            return false;
        }
        sqlite3_bind_text(inventory, 1, seed.productId, -1, SQLITE_TRANSIENT);
        const int inventoryStep = sqlite3_step(inventory);
        sqlite3_finalize(inventory);
        if (inventoryStep != SQLITE_DONE) {
            lastError_ = sqlite3_errmsg(database_);
            rollbackTransaction();
            return false;
        }
    }
    return commitTransaction();
}

bool DatabaseManager::beginTransaction() {
    return execute("BEGIN;");
}

bool DatabaseManager::commitTransaction() {
    if (execute("COMMIT;")) return true;
    execute("ROLLBACK;");
    return false;
}

bool DatabaseManager::rollbackTransaction() {
    return execute("ROLLBACK;");
}

bool DatabaseManager::backupDatabase(const std::string& destinationPath,
                                     std::string& error) {
    if (!database_) {
        error = "Database is not open.";
        return false;
    }
    if (destinationPath.empty()) {
        error = "Backup path cannot be empty.";
        return false;
    }

    sqlite3* destination = nullptr;
    if (sqlite3_open(destinationPath.c_str(), &destination) != SQLITE_OK) {
        error = destination
            ? sqlite3_errmsg(destination)
            : "Unable to create backup file.";
        if (destination) sqlite3_close(destination);
        return false;
    }

    sqlite3_backup* backup = sqlite3_backup_init(destination, "main", database_, "main");
    if (!backup) {
        error = sqlite3_errmsg(destination);
        sqlite3_close(destination);
        return false;
    }
    int stepResult = sqlite3_backup_step(backup, -1);
    sqlite3_backup_finish(backup);
    int destinationResult = sqlite3_errcode(destination);
    sqlite3_close(destination);
    if (stepResult != SQLITE_DONE) {
        error = "SQLite backup failed.";
        std::remove(destinationPath.c_str());
        return false;
    }
    if (destinationResult != SQLITE_OK) {
        error = "SQLite backup destination failed.";
        std::remove(destinationPath.c_str());
        return false;
    }

    std::string validationError;
    if (!validateDatabaseFile(destinationPath, validationError)) {
        error = "Created backup is invalid: " + validationError;
        std::remove(destinationPath.c_str());
        return false;
    }
    return true;
}

bool DatabaseManager::validateDatabaseFile(const std::string& sourcePath,
                                           std::string& error) const {
    if (sourcePath.empty()) {
        error = "Backup path cannot be empty.";
        return false;
    }
    sqlite3* source = nullptr;
    if (sqlite3_open_v2(sourcePath.c_str(), &source,
                        SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        error = source
            ? sqlite3_errmsg(source)
            : "Unable to open backup file.";
        if (source) sqlite3_close(source);
        return false;
    }

    const char* requiredTables[] = {
        "categories", "products", "inventory",
        "inventory_logs", "transactions", "transaction_items"
    };
    for (const char* table : requiredTables) {
        sqlite3_stmt* statement = nullptr;
        const char* sql =
            "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?;";
        if (sqlite3_prepare_v2(source, sql, -1, &statement, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(source);
            sqlite3_close(source);
            return false;
        }
        sqlite3_bind_text(statement, 1, table, -1, SQLITE_TRANSIENT);
        int result = sqlite3_step(statement);
        sqlite3_finalize(statement);
        if (result != SQLITE_ROW) {
            error = "Backup is missing required table: " + std::string(table);
            sqlite3_close(source);
            return false;
        }
    }

    sqlite3_stmt* integrity = nullptr;
    if (sqlite3_prepare_v2(source, "PRAGMA integrity_check;",
                           -1, &integrity, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(source);
        sqlite3_close(source);
        return false;
    }
    int integrityResult = sqlite3_step(integrity);
    std::string integrityText;
    if (integrityResult == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(integrity, 0);
        integrityText = text ? reinterpret_cast<const char*>(text) : "";
    }
    sqlite3_finalize(integrity);
    if (integrityResult != SQLITE_ROW || integrityText != "ok") {
        error = integrityText.empty()
            ? "SQLite integrity check failed."
            : "SQLite integrity check failed: " + integrityText;
        sqlite3_close(source);
        return false;
    }

    sqlite3_stmt* foreignKey = nullptr;
    if (sqlite3_prepare_v2(source, "PRAGMA foreign_key_check;",
                           -1, &foreignKey, nullptr) == SQLITE_OK) {
        int foreignKeyResult = sqlite3_step(foreignKey);
        sqlite3_finalize(foreignKey);
        if (foreignKeyResult == SQLITE_ROW) {
            error = "Backup contains foreign-key violations.";
            sqlite3_close(source);
            return false;
        }
    }

    sqlite3_close(source);
    return true;
}

bool DatabaseManager::restoreDatabase(const std::string& sourcePath,
                                      std::string& error) {
    if (!database_) {
        error = "Database is not open.";
        return false;
    }
    std::string validationError;
    if (!validateDatabaseFile(sourcePath, validationError)) {
        error = "Invalid backup file: " + validationError;
        return false;
    }

    sqlite3* source = nullptr;
    if (sqlite3_open_v2(sourcePath.c_str(), &source,
                        SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        error = source
            ? sqlite3_errmsg(source)
            : "Unable to open backup file.";
        if (source) sqlite3_close(source);
        return false;
    }

    sqlite3_backup* restore = sqlite3_backup_init(database_, "main", source, "main");
    if (!restore) {
        error = sqlite3_errmsg(database_);
        sqlite3_close(source);
        return false;
    }
    int stepResult = sqlite3_backup_step(restore, -1);
    sqlite3_backup_finish(restore);
    sqlite3_close(source);
    if (stepResult != SQLITE_DONE) {
        error = "Database restore failed.";
        return false;
    }
    if (!execute("PRAGMA foreign_keys = ON;")) {
        error = "Restored database could not enable foreign keys.";
        return false;
    }
    if (!initializeSchema()) {
        error = "Restored database migration failed: " + getLastError();
        return false;
    }
    return true;
}

sqlite3* DatabaseManager::getHandle() const {
    return database_;
}

std::string DatabaseManager::getLastError() const {
    return lastError_;
}

bool DatabaseManager::getCurrentOperatorName(std::string& name,
                                             std::string& error) {
    if (!database_) {
        error = "Database is not open.";
        lastError_ = error;
        return false;
    }
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_,
                           "SELECT setting_value FROM app_settings "
                           "WHERE setting_key = 'current_operator';",
                           -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(database_);
        lastError_ = error;
        return false;
    }
    const int result = sqlite3_step(statement);
    if (result != SQLITE_ROW) {
        error = result == SQLITE_DONE
            ? "Current operator setting is missing."
            : sqlite3_errmsg(database_);
        lastError_ = error;
        sqlite3_finalize(statement);
        return false;
    }
    const unsigned char* text = sqlite3_column_text(statement, 0);
    name = text ? reinterpret_cast<const char*>(text) : "";
    sqlite3_finalize(statement);
    return true;
}

bool DatabaseManager::setCurrentOperatorName(const std::string& name,
                                             std::string& error) {
    if (!database_) {
        error = "Database is not open.";
        lastError_ = error;
        return false;
    }
    if (name.find_first_not_of(" \t\r\n") == std::string::npos) {
        error = "Operator name cannot be empty.";
        return false;
    }
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            database_,
            "INSERT INTO app_settings(setting_key, setting_value) "
            "VALUES('current_operator', ?) "
            "ON CONFLICT(setting_key) DO UPDATE SET setting_value=excluded.setting_value;",
            -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(database_);
        lastError_ = error;
        return false;
    }
    sqlite3_bind_text(statement, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    const int result = sqlite3_step(statement);
    sqlite3_finalize(statement);
    if (result != SQLITE_DONE) {
        error = sqlite3_errmsg(database_);
        lastError_ = error;
        return false;
    }
    return true;
}
