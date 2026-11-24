#include "ProductRepository.h"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace {
std::string currentTimestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm local;
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%dT%H:%M:%S");
    return output.str();
}
}

ProductRepository::ProductRepository(DatabaseManager& database)
    : database_(database) {}

bool ProductRepository::validateProduct(const std::string& productId,
                                        int categoryId,
                                        const std::string& name,
                                        int priceCents,
                                        int initialStock,
                                        std::string& error) {
    if (productId.empty()) {
        error = "Product ID cannot be empty.";
        return false;
    }
    if (name.empty()) {
        error = "Product name cannot be empty.";
        return false;
    }
    if (categoryId <= 0) {
        error = "Category ID must be positive.";
        return false;
    }
    if (priceCents < 0) {
        error = "Product price cannot be negative.";
        return false;
    }
    if (initialStock < 0) {
        error = "Initial stock cannot be negative.";
        return false;
    }
    return true;
}

bool ProductRepository::create(const std::string& productId, int categoryId,
                               const std::string& name, int priceCents,
                               int initialStock, std::string& error) {
    return create(productId, categoryId, name, priceCents, initialStock, "", error);
}

bool ProductRepository::create(const std::string& productId, int categoryId,
                               const std::string& name, int priceCents,
                               int initialStock, const std::string& imagePath,
                               std::string& error) {
    if (!validateProduct(productId, categoryId, name, priceCents, initialStock, error))
        return false;

    sqlite3* db = database_.getHandle();
    if (!db) {
        error = "Database is not open.";
        return false;
    }

    if (!database_.beginTransaction()) {
        error = database_.getLastError();
        return false;
    }

    sqlite3_stmt* insertProduct = nullptr;
    const char* sql =
        "INSERT INTO products(product_id, category_id, name, price_cents, active, image_path) "
        "VALUES(?, ?, ?, ?, 1, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &insertProduct, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_bind_text(insertProduct, 1, productId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(insertProduct, 2, categoryId);
    sqlite3_bind_text(insertProduct, 3, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(insertProduct, 4, priceCents);
    sqlite3_bind_text(insertProduct, 5, imagePath.c_str(), -1, SQLITE_TRANSIENT);
    int result = sqlite3_step(insertProduct);
    if (result != SQLITE_DONE) {
        error = result == SQLITE_CONSTRAINT
            ? "A product ID already exists or category is invalid."
            : sqlite3_errmsg(db);
        sqlite3_finalize(insertProduct);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_finalize(insertProduct);

    sqlite3_stmt* insertInventory = nullptr;
    if (sqlite3_prepare_v2(db,
                           "INSERT INTO inventory(product_id, quantity) VALUES(?, ?);",
                           -1, &insertInventory, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_bind_text(insertInventory, 1, productId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(insertInventory, 2, initialStock);
    result = sqlite3_step(insertInventory);
    sqlite3_finalize(insertInventory);
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

bool ProductRepository::findById(const std::string& productId,
                                 ProductRecord& record,
                                 std::string& error) const {
    sqlite3* db = database_.getHandle();
    sqlite3_stmt* statement = nullptr;
    if (!db) {
        error = "Database is not open.";
        return false;
    }
    if (sqlite3_prepare_v2(db,
                           "SELECT product_id, category_id, name, price_cents, active, image_path "
                           "FROM products WHERE product_id = ?;",
                           -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_text(statement, 1, productId.c_str(), -1, SQLITE_TRANSIENT);
    int result = sqlite3_step(statement);
    if (result != SQLITE_ROW) {
        error = "Product not found.";
        sqlite3_finalize(statement);
        return false;
    }
    record.productId = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
    record.categoryId = sqlite3_column_int(statement, 1);
    record.name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
    record.priceCents = sqlite3_column_int(statement, 3);
    record.active = sqlite3_column_int(statement, 4) != 0;
    const unsigned char* imageText =
        sqlite3_column_text(statement, 5);
    record.imagePath = imageText ? reinterpret_cast<const char*>(imageText) : "";
    sqlite3_finalize(statement);
    return true;
}

bool ProductRepository::search(const std::string& term,
                               std::vector<ProductRecord>& records,
                               std::string& error) const {
    sqlite3* db = database_.getHandle();
    sqlite3_stmt* statement = nullptr;
    if (!db) {
        error = "Database is not open.";
        return false;
    }
    if (sqlite3_prepare_v2(db,
                           "SELECT product_id, category_id, name, price_cents, active, image_path "
                           "FROM products "
                           "WHERE product_id LIKE ? COLLATE NOCASE OR name LIKE ? COLLATE NOCASE "
                           "ORDER BY product_id;",
                           -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    std::string pattern = "%" + term + "%";
    sqlite3_bind_text(statement, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, pattern.c_str(), -1, SQLITE_TRANSIENT);
    records.clear();
    while (sqlite3_step(statement) == SQLITE_ROW) {
        ProductRecord record;
        record.productId = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        record.categoryId = sqlite3_column_int(statement, 1);
        record.name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
        record.priceCents = sqlite3_column_int(statement, 3);
        record.active = sqlite3_column_int(statement, 4) != 0;
        const unsigned char* imageText =
            sqlite3_column_text(statement, 5);
        record.imagePath = imageText ? reinterpret_cast<const char*>(imageText) : "";
        records.push_back(record);
    }
    sqlite3_finalize(statement);
    return true;
}

bool ProductRepository::getActive(std::vector<ProductRecord>& records,
                                  std::string& error) const {
    sqlite3* db = database_.getHandle();
    sqlite3_stmt* statement = nullptr;
    if (!db) {
        error = "Database is not open.";
        return false;
    }
    if (sqlite3_prepare_v2(db,
                           "SELECT product_id, category_id, name, price_cents, active, image_path "
                           "FROM products WHERE active = 1 ORDER BY product_id;",
                           -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    records.clear();
    while (sqlite3_step(statement) == SQLITE_ROW) {
        ProductRecord record;
        record.productId = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        record.categoryId = sqlite3_column_int(statement, 1);
        record.name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
        record.priceCents = sqlite3_column_int(statement, 3);
        record.active = true;
        const unsigned char* imageText =
            sqlite3_column_text(statement, 5);
        record.imagePath = imageText ? reinterpret_cast<const char*>(imageText) : "";
        records.push_back(record);
    }
    sqlite3_finalize(statement);
    return true;
}

bool ProductRepository::update(const std::string& productId, int categoryId,
                               const std::string& name, int priceCents,
                               std::string& error) {
    ProductRecord existing;
    if (!findById(productId, existing, error))
        return false;
    return update(productId, categoryId, name, priceCents,
                  existing.imagePath, error);
}

bool ProductRepository::update(const std::string& productId, int categoryId,
                               const std::string& name, int priceCents,
                               const std::string& imagePath, std::string& error) {
    if (!validateProduct(productId, categoryId, name, priceCents, 0, error))
        return false;
    sqlite3* db = database_.getHandle();
    sqlite3_stmt* statement = nullptr;
    if (!db) {
        error = "Database is not open.";
        return false;
    }
    ProductRecord existing;
    if (!findById(productId, existing, error))
        return false;
    if (!existing.active) {
        error = "Inactive products cannot be edited.";
        return false;
    }
    if (sqlite3_prepare_v2(db,
                           "UPDATE products SET category_id = ?, name = ?, price_cents = ?, "
                           "image_path = ? "
                           "WHERE product_id = ?;",
                           -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_int(statement, 1, categoryId);
    sqlite3_bind_text(statement, 2, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 3, priceCents);
    sqlite3_bind_text(statement, 4, imagePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 5, productId.c_str(), -1, SQLITE_TRANSIENT);
    int result = sqlite3_step(statement);
    bool changed = sqlite3_changes(db) > 0;
    sqlite3_finalize(statement);
    if (result != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        return false;
    }
    if (!changed) {
        error = "Product not found.";
        return false;
    }
    return true;
}

bool ProductRepository::deactivate(const std::string& productId,
                                   std::string& error) {
    sqlite3* db = database_.getHandle();
    sqlite3_stmt* statement = nullptr;
    if (!db) {
        error = "Database is not open.";
        return false;
    }
    if (sqlite3_prepare_v2(db,
                           "UPDATE products SET active = 0 WHERE product_id = ?;",
                           -1, &statement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    sqlite3_bind_text(statement, 1, productId.c_str(), -1, SQLITE_TRANSIENT);
    int result = sqlite3_step(statement);
    bool changed = sqlite3_changes(db) > 0;
    sqlite3_finalize(statement);
    if (result != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        return false;
    }
    if (!changed) {
        error = "Product not found.";
        return false;
    }
    return true;
}

bool ProductRepository::deleteProduct(const std::string& productId,
                                      ProductRecord& deletedRecord,
                                      bool& imagePathUnreferenced,
                                      std::string& error,
                                      const std::string& operatorName) {
    imagePathUnreferenced = false;
    sqlite3* db = database_.getHandle();
    if (!db) {
        error = "Database is not open.";
        return false;
    }
    if (!database_.beginTransaction()) {
        error = database_.getLastError();
        return false;
    }

    ProductRecord existing;
    if (!findById(productId, existing, error)) {
        database_.rollbackTransaction();
        return false;
    }

    sqlite3_stmt* stockStatement = nullptr;
    if (sqlite3_prepare_v2(db,
                           "SELECT quantity FROM inventory WHERE product_id = ?;",
                           -1, &stockStatement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_bind_text(stockStatement, 1, productId.c_str(), -1, SQLITE_TRANSIENT);
    const int stockResult = sqlite3_step(stockStatement);
    const int stockQuantity = stockResult == SQLITE_ROW
        ? sqlite3_column_int(stockStatement, 0) : 0;
    if (stockResult != SQLITE_ROW && stockResult != SQLITE_DONE)
        error = sqlite3_errmsg(db);
    sqlite3_finalize(stockStatement);
    if (stockResult != SQLITE_ROW && stockResult != SQLITE_DONE) {
        database_.rollbackTransaction();
        return false;
    }
    if (!existing.imagePath.empty()) {
        sqlite3_stmt* imageReferences = nullptr;
        if (sqlite3_prepare_v2(
                db,
                "SELECT COUNT(*) FROM products "
                "WHERE image_path = ? AND product_id <> ?;",
                -1, &imageReferences, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            database_.rollbackTransaction();
            return false;
        }
        sqlite3_bind_text(imageReferences, 1, existing.imagePath.c_str(),
                          -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(imageReferences, 2, productId.c_str(),
                          -1, SQLITE_TRANSIENT);
        const int imageResult = sqlite3_step(imageReferences);
        if (imageResult == SQLITE_ROW)
            imagePathUnreferenced = sqlite3_column_int(imageReferences, 0) == 0;
        else
            error = sqlite3_errmsg(db);
        sqlite3_finalize(imageReferences);
        if (imageResult != SQLITE_ROW) {
            database_.rollbackTransaction();
            return false;
        }
    }

    sqlite3_stmt* deleteInventory = nullptr;
    if (sqlite3_prepare_v2(db,
                           "DELETE FROM inventory WHERE product_id = ?;",
                           -1, &deleteInventory, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_bind_text(deleteInventory, 1, productId.c_str(), -1, SQLITE_TRANSIENT);
    int result = sqlite3_step(deleteInventory);
    sqlite3_finalize(deleteInventory);
    if (result != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }

    if (stockQuantity > 0) {
        sqlite3_stmt* log = nullptr;
        if (sqlite3_prepare_v2(
                db,
                "INSERT INTO inventory_logs"
                "(product_id, quantity_change, reason, resulting_quantity, "
                "logged_at, operator_name) VALUES(?, ?, ?, 0, ?, ?);",
                -1, &log, nullptr) != SQLITE_OK) {
            error = sqlite3_errmsg(db);
            database_.rollbackTransaction();
            return false;
        }
        const std::string reason =
            "Product permanently deleted; remaining stock written off.";
        const std::string timestamp = currentTimestamp();
        sqlite3_bind_text(log, 1, productId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(log, 2, -stockQuantity);
        sqlite3_bind_text(log, 3, reason.c_str(), -1, SQLITE_TRANSIENT);
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

    sqlite3_stmt* deleteProductStatement = nullptr;
    if (sqlite3_prepare_v2(db,
                           "DELETE FROM products WHERE product_id = ?;",
                           -1, &deleteProductStatement, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_bind_text(deleteProductStatement, 1, productId.c_str(),
                      -1, SQLITE_TRANSIENT);
    result = sqlite3_step(deleteProductStatement);
    const bool deleted = sqlite3_changes(db) == 1;
    sqlite3_finalize(deleteProductStatement);
    if (result != SQLITE_DONE || !deleted) {
        error = result == SQLITE_DONE ? "Product not found." : sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }

    sqlite3_stmt* suppressSeed = nullptr;
    if (sqlite3_prepare_v2(
            db,
            "INSERT OR REPLACE INTO product_seed_suppressions"
            "(product_id, product_name) VALUES(?, ?);",
            -1, &suppressSeed, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }
    sqlite3_bind_text(suppressSeed, 1, productId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(suppressSeed, 2, existing.name.c_str(), -1, SQLITE_TRANSIENT);
    result = sqlite3_step(suppressSeed);
    sqlite3_finalize(suppressSeed);
    if (result != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        database_.rollbackTransaction();
        return false;
    }

    if (!database_.commitTransaction()) {
        error = database_.getLastError();
        return false;
    }
    deletedRecord = existing;
    return true;
}
