#include "POS.h"
#include <iostream>
#include <limits>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cmath>
#ifdef _WIN32
#include <conio.h>
#include <io.h>
#endif

POS::POS() : POS("bakery_pos.db") {}

POS::POS(const std::string& databasePath)
    : transactionId_(1),
      operatorAccountRepository_(database_),
      productRepository_(database_),
      inventoryRepository_(database_),
      transactionRepository_(database_),
      databaseReady_(false),
      operatorAccountId_(0) {
    databaseReady_ = database_.open(databasePath) && database_.initializeSchema();
    if (databaseReady_) refreshCatalog();
}

bool POS::isDatabaseReady() const {
    return databaseReady_;
}

std::string POS::databaseError() const {
    return database_.getLastError();
}

std::string POS::currentOperatorName() const {
    return operatorName_;
}

std::string POS::currentOperatorRole() const {
    return operatorRole_;
}

int POS::currentOperatorAccountId() const {
    return operatorAccountId_;
}

bool POS::hasAdministrator(bool& exists, std::string& error) const {
    if (!databaseReady_) {
        error = database_.getLastError();
        return false;
    }
    return operatorAccountRepository_.hasAdministrator(exists, error);
}

bool POS::createInitialAdministrator(const std::string& username,
                                     const std::string& password,
                                     std::string& error) {
    if (!databaseReady_) {
        error = database_.getLastError();
        return false;
    }
    OperatorAccount account{};
    return operatorAccountRepository_.createInitialAdministrator(
        username, password, account, error);
}

bool POS::login(const std::string& username, const std::string& password,
                std::string& error) {
    if (!databaseReady_) {
        error = database_.getLastError();
        return false;
    }
    OperatorAccount account{};
    if (!operatorAccountRepository_.authenticate(
            username, password, account, error))
        return false;
    operatorAccountId_ = account.accountId;
    operatorName_ = account.username;
    operatorRole_ = account.role;
    return true;
}

void POS::logout() {
    operatorAccountId_ = 0;
    operatorName_.clear();
    operatorRole_.clear();
    cart_.clear();
}

bool POS::isAuthenticated() const {
    return !operatorName_.empty() && operatorAccountId_ > 0;
}

bool POS::isAdministrator() const {
    return isAuthenticated() && operatorRole_ == "administrator";
}

bool POS::listOperatorAccounts(
    std::vector<OperatorAccount>& accounts, std::string& error) const {
    if (!requireAdministrator(error))
        return false;
    return operatorAccountRepository_.listAccounts(accounts, error);
}

bool POS::createOperatorAccount(const std::string& username,
                                const std::string& password,
                                const std::string& role,
                                std::string& error) {
    if (!requireAdministrator(error))
        return false;
    OperatorAccount account{};
    return operatorAccountRepository_.createAccount(
        username, password, role, account, error);
}

bool POS::updateOperatorAccount(int accountId, const std::string& username,
                                const std::string& role,
                                const std::string& newPassword,
                                std::string& error) {
    if (!requireAdministrator(error))
        return false;
    if (accountId == operatorAccountId_ && role != "administrator") {
        error = "An administrator cannot change their own role.";
        return false;
    }
    if (!operatorAccountRepository_.updateAccount(
            accountId, username, role, newPassword, error))
        return false;
    if (accountId == operatorAccountId_) {
        OperatorAccount updated{};
        if (!operatorAccountRepository_.getActiveAccount(
                accountId, updated, error))
            return false;
        operatorName_ = updated.username;
        operatorRole_ = updated.role;
    }
    return true;
}

bool POS::deactivateOperatorAccount(int accountId, std::string& error) {
    if (!requireAdministrator(error))
        return false;
    if (accountId == operatorAccountId_) {
        error = "Log out before deactivating your own account.";
        return false;
    }
    return operatorAccountRepository_.deactivateAccount(accountId, error);
}

bool POS::requireOperator(std::string& error) const {
    if (!isAuthenticated()) {
        error = "Log in before recording this action.";
        return false;
    }
    OperatorAccount active{};
    if (!operatorAccountRepository_.getActiveAccount(
            operatorAccountId_, active, error))
        return false;
    if (active.username != operatorName_ || active.role != operatorRole_) {
        error = "The logged-in account changed. Log in again to continue.";
        return false;
    }
    return true;
}

bool POS::requireAdministrator(std::string& error) const {
    if (!requireOperator(error))
        return false;
    if (operatorRole_ != "administrator") {
        error = "Administrator access is required.";
        return false;
    }
    return true;
}

bool POS::getActiveProducts(std::vector<ProductRecord>& records,
                            std::string& error) const {
    return productRepository_.getActive(records, error);
}

bool POS::searchProducts(const std::string& term,
                         std::vector<ProductRecord>& records,
                         std::string& error) const {
    return productRepository_.search(term, records, error);
}

bool POS::getStock(const std::string& productId, int& quantity,
                   std::string& error) const {
    return inventoryRepository_.getStock(productId, quantity, error);
}

bool POS::getLatestInventoryLog(const std::string& productId, InventoryLog& log,
                                std::string& error) const {
    return inventoryRepository_.getLatestLog(productId, log, error);
}

bool POS::createProduct(const std::string& productId, const std::string& name,
                        int priceCents, int initialStock, std::string& error) {
    return createProduct(productId, name, priceCents, initialStock, "", error);
}

bool POS::createProduct(const std::string& productId, const std::string& name,
                        int priceCents, int initialStock,
                        const std::string& imagePath, std::string& error) {
    if (!databaseReady_) {
        error = database_.getLastError();
        return false;
    }
    if (!productRepository_.create(productId, 1, name, priceCents,
                                   initialStock, imagePath, error))
        return false;
    refreshCatalog();
    return true;
}

bool POS::editProduct(const std::string& productId, const std::string& name,
                      int priceCents, std::string& error) {
    ProductRecord record;
    if (!productRepository_.findById(productId, record, error) || !record.active)
        return false;
    return editProduct(productId, name, priceCents, record.imagePath, error);
}

bool POS::editProduct(const std::string& productId, const std::string& name,
                      int priceCents, const std::string& imagePath,
                      std::string& error) {
    ProductRecord record;
    if (!productRepository_.findById(productId, record, error) || !record.active)
        return false;
    if (!productRepository_.update(productId, record.categoryId, name,
                                   priceCents, imagePath, error))
        return false;
    refreshCatalog();
    return true;
}

bool POS::deactivateProduct(const std::string& productId, std::string& error) {
    if (!productRepository_.deactivate(productId, error)) return false;
    refreshCatalog();
    return true;
}

bool POS::deleteProduct(const std::string& productId,
                        ProductRecord& deletedRecord,
                        bool& imagePathUnreferenced,
                        std::string& error) {
    if (!databaseReady_) {
        error = database_.getLastError();
        return false;
    }
    if (!requireOperator(error))
        return false;
    if (cart_.containsProduct(productId)) {
        error = "Remove the product from the current order before deleting it.";
        return false;
    }
    if (!productRepository_.deleteProduct(productId, deletedRecord,
                                          imagePathUnreferenced, error,
                                          operatorName_))
        return false;
    refreshCatalog();
    return true;
}

bool POS::restockProduct(const std::string& productId, int quantity,
                         const std::string& reason, std::string& error) {
    if (!databaseReady_) {
        error = database_.getLastError();
        return false;
    }
    if (!requireOperator(error)) return false;
    ProductRecord record;
    if (!productRepository_.findById(productId, record, error) || !record.active) {
        error = "Product not found or inactive.";
        return false;
    }
    return inventoryRepository_.restock(productId, quantity, reason, error,
                                        operatorName_);
}

bool POS::adjustStock(const std::string& productId, int quantityChange,
                      const std::string& reason, std::string& error) {
    if (!databaseReady_) {
        error = database_.getLastError();
        return false;
    }
    if (!requireOperator(error)) return false;
    ProductRecord record;
    if (!productRepository_.findById(productId, record, error) || !record.active) {
        error = "Product not found or inactive.";
        return false;
    }
    return inventoryRepository_.adjust(productId, quantityChange, reason, error,
                                       operatorName_);
}

bool POS::addProductToCart(const std::string& productId, int quantity,
                           std::string& error) {
    if (!databaseReady_) {
        error = database_.getLastError();
        return false;
    }
    if (quantity <= 0) {
        error = "Quantity must be positive.";
        return false;
    }
    ProductRecord record;
    if (!productRepository_.findById(productId, record, error) || !record.active) {
        error = "Product not found or inactive.";
        return false;
    }
    const Product* product = findProductById(productId);
    if (!product) {
        error = "Product is not available in the catalog.";
        return false;
    }
    int stock = 0;
    if (!inventoryRepository_.getStock(productId, stock, error)) return false;
    int currentQuantity = 0;
    for (const auto& item : cart_.getItems()) {
        if (item.getProductId() == productId) {
            currentQuantity = item.getQuantity();
            break;
        }
    }
    if (quantity > stock - currentQuantity) {
        error = "Insufficient stock. Available: " + std::to_string(stock - currentQuantity);
        return false;
    }
    cart_.addItem(*product, quantity);
    return true;
}

bool POS::updateCartQuantity(const std::string& productId, int quantity,
                             std::string& error) {
    if (quantity <= 0) {
        error = "Quantity must be positive.";
        return false;
    }
    if (!cart_.containsProduct(productId)) {
        error = "Product not in cart.";
        return false;
    }
    ProductRecord record;
    if (!productRepository_.findById(productId, record, error) || !record.active) {
        error = "Product not found or inactive.";
        return false;
    }
    int stock = 0;
    if (!inventoryRepository_.getStock(productId, stock, error)) return false;
    if (quantity > stock) {
        error = "Insufficient stock. Available: " + std::to_string(stock);
        return false;
    }
    return cart_.updateItem(productId, quantity);
}

bool POS::removeCartProduct(const std::string& productId, std::string& error) {
    if (!cart_.removeItem(productId)) {
        error = "Product not in cart.";
        return false;
    }
    return true;
}

const std::vector<CartItem>& POS::cartItems() const {
    return cart_.getItems();
}

double POS::cartTotal() const {
    return cart_.getOrderTotal();
}

bool POS::checkoutWithCash(int cashCents, TransactionRecord& transaction,
                           std::vector<TransactionItemRecord>& items,
                           std::string& error) {
    if (!databaseReady_) {
        error = database_.getLastError();
        return false;
    }
    if (!requireOperator(error)) return false;
    if (cashCents < 0) {
        error = "Cash amount cannot be negative.";
        return false;
    }
    std::vector<TransactionItemInput> checkoutItems;
    for (const auto& item : cart_.getItems())
        checkoutItems.push_back({item.getProductId(), item.getQuantity()});
    if (!transactionRepository_.createAtomic(
            checkoutItems, cashCents, transaction, items, error, operatorName_))
        return false;
    cart_.clear();
    transactionId_ = transaction.transactionId + 1;
    return true;
}

bool POS::listRecentTransactions(std::vector<TransactionRecord>& transactions,
                                 int limit, std::string& error) const {
    return transactionRepository_.listRecent(transactions, limit, error);
}

bool POS::listTransactionsForDate(
    const std::string& date,
    std::vector<TransactionRecord>& transactions,
    std::string& error) const {
    return transactionRepository_.listForDate(date, transactions, error);
}

bool POS::getTransaction(int transactionId, TransactionRecord& transaction,
                         std::string& error) const {
    return transactionRepository_.getById(transactionId, transaction, error);
}

bool POS::getTransactionItems(int transactionId,
                              std::vector<TransactionItemRecord>& items,
                              std::string& error) const {
    return transactionRepository_.getItems(transactionId, items, error);
}

bool POS::listTransactionRefunds(
    int transactionId,
    std::vector<TransactionRefundRecord>& refunds,
    std::string& error) const {
    return transactionRepository_.getRefunds(transactionId, refunds, error);
}

bool POS::getDailySales(const std::string& date, DailySalesRecord& report,
                        std::vector<ProductSalesRecord>& products,
                        std::string& error) const {
    return transactionRepository_.getDailySales(date, report, products, error);
}

bool POS::getDailyOperatorSales(
    const std::string& date,
    std::vector<OperatorSalesRecord>& operatorSales,
    std::string& error) const {
    return transactionRepository_.getDailyOperatorSales(date, operatorSales, error);
}

bool POS::getSalesReport(
    const std::string& startDate,
    const std::string& endDate,
    SalesReportRecord& report,
    std::vector<ProductSalesRecord>& productSales,
    std::vector<OperatorSalesRecord>& operatorSales,
    std::vector<TransactionRefundRecord>& refunds,
    std::string& error) const {
    if (!databaseReady_) {
        error = database_.getLastError();
        return false;
    }
    if (!isValidDate(startDate) || !isValidDate(endDate)) {
        error = "Report dates must use YYYY-MM-DD.";
        return false;
    }
    if (startDate > endDate) {
        error = "Report start date must not be after the end date.";
        return false;
    }
    return transactionRepository_.getSalesReport(
        startDate, endDate, report, productSales, operatorSales, refunds, error);
}

bool POS::backupDatabase(const std::string& destinationPath, std::string& error) {
    if (!databaseReady_) {
        error = database_.getLastError();
        return false;
    }
    return database_.backupDatabase(destinationPath, error);
}

bool POS::validateDatabaseBackup(const std::string& sourcePath,
                                 std::string& error) const {
    if (!databaseReady_) {
        error = database_.getLastError();
        return false;
    }
    return database_.validateDatabaseFile(sourcePath, error);
}

bool POS::restoreDatabase(const std::string& sourcePath, std::string& error) {
    if (!databaseReady_) {
        error = database_.getLastError();
        return false;
    }
    if (!database_.restoreDatabase(sourcePath, error))
        return false;
    logout();
    refreshCatalog();
    return true;
}

bool POS::fullRefund(int transactionId, const std::string& reason,
                     TransactionRefundRecord& refund, std::string& error) {
    if (!databaseReady_) {
        error = database_.getLastError();
        return false;
    }
    if (!requireOperator(error)) return false;
    return transactionRepository_.fullRefund(transactionId, reason, refund,
                                             error, operatorName_);
}

bool POS::partialRefund(int transactionId,
                        const std::vector<TransactionRefundItemInput>& items,
                        const std::string& reason,
                        TransactionRefundRecord& refund, std::string& error) {
    if (!databaseReady_) {
        error = database_.getLastError();
        return false;
    }
    if (!requireOperator(error)) return false;
    return transactionRepository_.partialRefund(transactionId, items, reason,
                                                refund, error, operatorName_);
}

bool POS::voidTransaction(int transactionId, const std::string& reason,
                          TransactionRefundRecord& refund, std::string& error) {
    if (!databaseReady_) {
        error = database_.getLastError();
        return false;
    }
    if (!requireOperator(error)) return false;
    return transactionRepository_.voidTransaction(transactionId, reason, refund,
                                                  error, operatorName_);
}

void POS::run() {
    if (!databaseReady_) {
        std::cout << "Database initialization failed: "
                  << database_.getLastError() << "\n";
        std::cout << "Goodbye!\n";
        return;
    }
    std::string error;
    bool administratorExists = false;
    if (!hasAdministrator(administratorExists, error)) {
        std::cout << "Could not check operator accounts: " << error << "\n";
        std::cout << "Goodbye!\n";
        return;
    }
    if (!administratorExists) {
        std::cout << "First launch: create an administrator account.\n";
        const std::string username = readLine("Username: ");
        const std::string password = readPassword("Password: ");
        const std::string confirmation = readPassword("Confirm password: ");
        if (password != confirmation
            || !createInitialAdministrator(username, password, error)) {
            std::cout << (password != confirmation
                              ? "Passwords do not match."
                              : "Administrator setup failed: " + error)
                      << "\nGoodbye!\n";
            return;
        }
        std::cout << "Administrator account created. Please log in.\n";
    }
    if (!isAuthenticated()) {
        const std::string username = readLine("Username: ");
        const std::string password = readPassword("Password: ");
        if (!login(username, password, error)) {
            std::cout << "Login failed: " << error << "\n";
            std::cout << "Goodbye!\n";
            return;
        }
    }
    while (true) {
        std::cout << "\n=== Crombi Bakery POS ===\n";
        std::cout << "Logged in as: " << currentOperatorName() << "\n";
        std::cout << "1. View Products\n";
        std::cout << "2. Add Product to Cart\n";
        std::cout << "3. View Shopping Cart\n";
        std::cout << "4. Remove Product from Cart\n";
        std::cout << "5. Update Product Quantity\n";
        std::cout << "6. Checkout\n";
        std::cout << "7. Exit\n";
        std::cout << "8. Product Management\n";
        std::cout << "9. Inventory Management\n";
        std::cout << "10. Transaction History and Reports\n";
        std::cout << "11. Logout\n";
        std::cout << "Enter choice: ";

        int choice = readMenuChoice();
        switch (choice) {
            case 1: viewProducts();   break;
            case 2: addToCart();      break;
            case 3: viewCart();       break;
            case 4: removeFromCart(); break;
            case 5: updateQuantity(); break;
            case 6: checkout();       break;
            case 7: std::cout << "Goodbye!\n"; return;
            case 8: productManagementMenu(); break;
            case 9: inventoryManagementMenu(); break;
            case 10: historyAndReportsMenu(); break;
            case 11:
                logout();
                std::cout << "Logged out. Goodbye!\n";
                return;
            default: std::cout << "Invalid choice. Please select 1-10.\n"; break;
        }
    }
}

void POS::viewProducts() const {
    std::cout << "\n=== Product Catalog ===\n";
    std::cout << std::left
              << std::setw(12) << "Product ID"
              << "| "
              << std::setw(25) << "Name"
              << "| Price\n";
    std::cout << std::string(46, '-') << "\n";
    for (const auto& p : catalog_) {
        std::cout << std::left
                  << std::setw(12) << p.getId()
                  << "| "
                  << std::setw(25) << p.getName()
                  << "| " << formatCurrency(p.getPrice()) << "\n";
    }
}
void POS::addToCart() {
    std::string id = readProductId("Enter Product ID: ");
    const Product* product = findProductById(id);
    if (product == nullptr) {
        std::cout << "Product not found or inactive.\n";
        return;
    }
    int qty = readPositiveInt("Enter quantity: ");
    std::string error;
    if (!addProductToCart(id, qty, error)) {
        std::cout << error << "\n";
        return;
    }
    std::cout << "Added " << product->getName()
              << " (x" << qty << ") to cart.\n";
}
void POS::viewCart() const {
    std::cout << "\n=== Shopping Cart ===\n";
    if (cart_.isEmpty()) {
        std::cout << "Cart is empty.\n";
        return;
    }

    // Column widths
    const int W_ID       = 11;
    const int W_NAME     = 24;
    const int W_PRICE    = 12;
    const int W_QTY      =  4;

    // Header
    std::cout << std::left
              << std::setw(W_ID)       << "Product ID"
              << "| " << std::setw(W_NAME)  << "Name"
              << "| " << std::setw(W_PRICE) << "Unit Price"
              << "| " << std::setw(W_QTY)   << "Qty"
              << "| Subtotal\n";
    std::cout << std::string(69, '-') << "\n";

    // Rows
    for (const auto& item : cart_.getItems()) {
        std::cout << std::left
                  << std::setw(W_ID)       << item.getProductId()
                  << "| " << std::setw(W_NAME)  << item.getProductName()
                  << "| " << std::setw(W_PRICE) << formatCurrency(item.getUnitPrice())
                  << "| " << std::right << std::setw(W_QTY) << item.getQuantity()
                  << " | " << formatCurrency(item.getSubtotal()) << "\n";
    }

    std::cout << std::string(69, '-') << "\n";
    std::cout << "Order Total: " << formatCurrency(cart_.getOrderTotal()) << "\n";
}
void POS::removeFromCart() {
    std::string id = readProductId("Enter Product ID to remove: ");
    std::string error;
    if (!removeCartProduct(id, error)) {
        std::cout << error << "\n";
        return;
    }
    std::cout << "Removed " << id << " from cart.\n";
}
void POS::updateQuantity() {
    std::string id = readProductId("Enter Product ID to update: ");
    int qty = readPositiveInt("Enter new quantity: ");
    std::string error;
    if (!updateCartQuantity(id, qty, error)) {
        std::cout << error << "\n";
        return;
    }
    std::cout << "Updated " << id << " quantity to " << qty << ".\n";
}
void POS::checkout() {
    if (cart_.isEmpty()) {
        std::cout << "Cart is empty. Cannot checkout.\n";
        return;
    }

    // ── Order Summary ──────────────────────────────────────────────
    std::cout << "\n=== Order Summary ===\n";

    const int W_ID       = 11;
    const int W_NAME     = 24;
    const int W_PRICE    = 12;
    const int W_QTY      =  4;

    std::cout << std::left
              << std::setw(W_ID)       << "Product ID"
              << "| " << std::setw(W_NAME)  << "Name"
              << "| " << std::setw(W_PRICE) << "Unit Price"
              << "| " << std::setw(W_QTY)   << "Qty"
              << "| Subtotal\n";
    std::cout << std::string(69, '-') << "\n";

    for (const auto& item : cart_.getItems()) {
        std::cout << std::left
                  << std::setw(W_ID)       << item.getProductId()
                  << "| " << std::setw(W_NAME)  << item.getProductName()
                  << "| " << std::setw(W_PRICE) << formatCurrency(item.getUnitPrice())
                  << "| " << std::right << std::setw(W_QTY) << item.getQuantity()
                  << " | " << formatCurrency(item.getSubtotal()) << "\n";
    }

    std::cout << std::string(69, '-') << "\n";

    double orderTotal = cart_.getOrderTotal();
    std::cout << "Order Total: " << formatCurrency(orderTotal) << "\n";

    double cashAmount = readCashAmount(orderTotal);
    int cashReceivedCents = static_cast<int>(std::llround(cashAmount * 100.0));

    TransactionRecord transaction;
    std::vector<TransactionItemRecord> savedItems;
    std::string error;
    if (!checkoutWithCash(cashReceivedCents, transaction, savedItems, error)) {
        std::cout << "Checkout failed: " << error << "\n";
        std::cout << "The shopping cart was retained.\n";
        return;
    }

    std::cout << "Change: "
              << formatCurrency(static_cast<double>(transaction.changeCents) / 100.0)
              << "\n";
    printSavedReceipt(transaction, savedItems);
}

void POS::printSavedReceipt(
    const TransactionRecord& transaction,
    const std::vector<TransactionItemRecord>& items) {
    const std::string SEP = std::string(40, '=');
    const std::string DASH = std::string(40, '-');

    std::cout << "\n" << SEP << "\n";
    std::cout << "           Crombi Bakery\n";
    std::cout << SEP << "\n";
    std::cout << "Transaction ID: " << transaction.transactionId << "\n";
    std::cout << "Date/Time: " << transaction.transactionDate << "\n";
    std::cout << "\nItems:\n";

    for (const auto& item : items) {
        std::ostringstream line;
        line << "  " << item.quantity
             << " x " << formatCurrency(static_cast<double>(item.unitPriceCents) / 100.0)
             << " = " << formatCurrency(static_cast<double>(item.subtotalCents) / 100.0);
        std::string prefix = line.str();
        if (static_cast<int>(prefix.size()) < 32)
            prefix += std::string(32 - prefix.size(), ' ');
        std::cout << prefix << item.productName << "\n";
    }

    std::cout << DASH << "\n";
    std::cout << std::left << std::setw(14) << "Order Total:"
              << " " << formatCurrency(static_cast<double>(transaction.orderTotalCents) / 100.0) << "\n";
    std::cout << std::left << std::setw(14) << "Cash:"
              << " " << formatCurrency(static_cast<double>(transaction.cashReceivedCents) / 100.0) << "\n";
    std::cout << std::left << std::setw(14) << "Change:"
              << " " << formatCurrency(static_cast<double>(transaction.changeCents) / 100.0) << "\n";
    std::cout << DASH << "\n";
    std::cout << "   Thank you for your purchase!\n";
    std::cout << SEP << "\n";
}

const Product* POS::findProductById(const std::string& id) const {
    for (const auto& p : catalog_) {
        if (p.getId() == id) return &p;
    }
    return nullptr;
}

void POS::refreshCatalog() {
    catalog_.clear();
    std::vector<ProductRecord> records;
    std::string error;
    if (!productRepository_.getActive(records, error)) {
        std::cout << "Unable to load product catalog: " << error << "\n";
        return;
    }
    for (const auto& record : records) {
        catalog_.emplace_back(record.productId, record.name,
                              static_cast<double>(record.priceCents) / 100.0);
    }
}

void POS::productManagementMenu() {
    if (!databaseReady_) return;
    while (true) {
        std::cout << "\n=== Product Management ===\n";
        std::cout << "1. View Products\n";
        std::cout << "2. Add Product\n";
        std::cout << "3. Edit Product\n";
        std::cout << "4. Deactivate Product\n";
        std::cout << "5. Search Products\n";
        std::cout << "6. Back\n";
        std::cout << "Enter choice: ";
        int choice = readMenuChoice();
        if (choice == 1) viewManagedProducts();
        else if (choice == 2) addProduct();
        else if (choice == 3) editProduct();
        else if (choice == 4) deactivateProduct();
        else if (choice == 5) searchProducts();
        else if (choice == 6) return;
        else std::cout << "Invalid choice. Please select 1-6.\n";
    }
}

void POS::viewManagedProducts() {
    std::vector<ProductRecord> records;
    std::string error;
    if (!productRepository_.getActive(records, error)) {
        std::cout << "Unable to view products: " << error << "\n";
        return;
    }
    std::cout << "\n=== Active Products ===\n";
    std::cout << std::left << std::setw(12) << "Product ID"
              << "| " << std::setw(25) << "Name"
              << "| " << std::setw(12) << "Price" << "| Stock\n";
    for (const auto& record : records) {
        int stock = 0;
        inventoryRepository_.getStock(record.productId, stock, error);
        std::cout << std::left << std::setw(12) << record.productId
                  << "| " << std::setw(25) << record.name
                  << "| " << std::setw(12)
                  << formatCurrency(static_cast<double>(record.priceCents) / 100.0)
                  << "| " << stock << "\n";
    }
    if (records.empty()) std::cout << "No active products.\n";
}

void POS::searchProducts() {
    std::string term = readLine("Search ID or name: ");
    if (term.empty()) {
        std::cout << "Search term cannot be empty.\n";
        return;
    }
    std::vector<ProductRecord> records;
    std::string error;
    if (!searchProducts(term, records, error)) {
        std::cout << "Search failed: " << error << "\n";
        return;
    }
    std::cout << "\n=== Search Results ===\n";
    for (const auto& record : records) {
        std::cout << record.productId << " | " << record.name << " | "
                  << formatCurrency(static_cast<double>(record.priceCents) / 100.0)
                  << " | " << (record.active ? "Active" : "Inactive") << "\n";
    }
    if (records.empty()) std::cout << "No matching products.\n";
}

void POS::addProduct() {
    std::string id = readLine("Enter new Product ID: ");
    std::string name = readLine("Enter product name: ");
    double price = readNonNegativeDouble("Enter unit price: ");
    int stock = readPositiveInt("Enter initial stock: ");
    std::string error;
    int cents = static_cast<int>(std::llround(price * 100.0));
    if (createProduct(id, name, cents, stock, error)) {
        std::cout << "Product " << id << " added successfully.\n";
    } else {
        std::cout << "Unable to add product: " << error << "\n";
    }
}

void POS::editProduct() {
    std::string id = readLine("Enter Product ID to edit: ");
    std::string name = readLine("Enter new product name: ");
    double price = readNonNegativeDouble("Enter new unit price: ");
    int cents = static_cast<int>(std::llround(price * 100.0));
    std::string error;
    if (editProduct(id, name, cents, error)) {
        std::cout << "Product " << id << " updated successfully.\n";
    } else {
        std::cout << "Unable to update product: " << error << "\n";
    }
}

void POS::deactivateProduct() {
    std::string id = readLine("Enter Product ID to deactivate: ");
    std::string error;
    if (deactivateProduct(id, error)) {
        std::cout << "Product " << id << " deactivated successfully.\n";
    } else {
        std::cout << "Unable to deactivate product: " << error << "\n";
    }
}

void POS::inventoryManagementMenu() {
    if (!databaseReady_) return;
    while (true) {
        std::cout << "\n=== Inventory Management ===\n";
        std::cout << "1. View Stock\n";
        std::cout << "2. Restock Product\n";
        std::cout << "3. Adjust Stock\n";
        std::cout << "4. Back\n";
        std::cout << "Enter choice: ";
        int choice = readMenuChoice();
        if (choice == 1) viewInventory();
        else if (choice == 2) restockProduct();
        else if (choice == 3) adjustStock();
        else if (choice == 4) return;
        else std::cout << "Invalid choice. Please select 1-4.\n";
    }
}

void POS::viewInventory() {
    std::vector<ProductRecord> records;
    std::string error;
    if (!productRepository_.getActive(records, error)) {
        std::cout << "Unable to view inventory: " << error << "\n";
        return;
    }
    std::cout << "\n=== Current Stock ===\n";
    std::cout << std::left << std::setw(12) << "Product ID"
              << "| " << std::setw(25) << "Name" << "| Stock | Status\n";
    for (const auto& record : records) {
        int stock = 0;
        if (!inventoryRepository_.getStock(record.productId, stock, error)) {
            std::cout << "Unable to read stock for " << record.productId
                      << ": " << error << "\n";
            continue;
        }
        std::string status = stock == 0 ? "OUT OF STOCK"
                            : (stock <= 30 ? "LOW STOCK" : "IN STOCK");
        std::cout << std::left << std::setw(12) << record.productId
                  << "| " << std::setw(25) << record.name
                  << "| " << std::setw(6) << stock << "| " << status << "\n";
    }
    if (records.empty()) std::cout << "No active products.\n";
}

void POS::restockProduct() {
    std::string id = readLine("Enter Product ID to restock: ");
    std::string error;
    int quantity = readPositiveInt("Enter restock quantity: ");
    std::string reason = readLine("Enter restock reason: ");
    if (POS::restockProduct(id, quantity, reason, error)) {
        std::cout << "Restocked " << id << " by " << quantity << ".\n";
    } else {
        std::cout << "Unable to restock: " << error << "\n";
    }
}

void POS::adjustStock() {
    std::string id = readLine("Enter Product ID to adjust: ");
    std::string error;
    int change = readSignedInt("Enter quantity change (negative to reduce): ");
    if (change == 0) {
        std::cout << "Stock adjustment cannot be zero.\n";
        return;
    }
    std::string reason = readLine("Enter adjustment reason: ");
    if (POS::adjustStock(id, change, reason, error)) {
        std::cout << "Adjusted stock for " << id << " by " << change << ".\n";
    } else {
        std::cout << "Unable to adjust stock: " << error << "\n";
    }
}

void POS::historyAndReportsMenu() {
    if (!databaseReady_) return;
    while (true) {
        std::cout << "\n=== Transaction History and Reports ===\n";
        std::cout << "1. View Recent Transactions\n";
        std::cout << "2. Search Transaction by ID\n";
        std::cout << "3. Daily Sales Report\n";
        std::cout << "4. Back\n";
        std::cout << "Enter choice: ";
        int choice = readMenuChoice();
        if (choice == 1) viewTransactionHistory();
        else if (choice == 2) viewTransactionDetails();
        else if (choice == 3) viewDailySalesReport();
        else if (choice == 4) return;
        else std::cout << "Invalid choice. Please select 1-4.\n";
    }
}

void POS::viewTransactionHistory() {
    std::vector<TransactionRecord> transactions;
    std::string error;
    if (!listRecentTransactions(transactions, 50, error)) {
        std::cout << "Unable to view transaction history: " << error << "\n";
        return;
    }
    if (transactions.empty()) {
        std::cout << "No completed transactions.\n";
        return;
    }
    std::cout << "\n=== Recent Transactions ===\n";
    std::cout << std::left << std::setw(16) << "Transaction ID"
              << "| " << std::setw(20) << "Date/Time"
              << "| " << std::setw(14) << "Total"
              << "| " << std::setw(14) << "Cash"
              << "| Change\n";
    for (const auto& transaction : transactions) {
        std::cout << std::left << std::setw(16) << transaction.transactionId
                  << "| " << std::setw(20) << transaction.transactionDate
                  << "| " << std::setw(14)
                  << formatCurrency(static_cast<double>(transaction.orderTotalCents) / 100.0)
                  << "| " << std::setw(14)
                  << formatCurrency(static_cast<double>(transaction.cashReceivedCents) / 100.0)
                  << "| " << formatCurrency(static_cast<double>(transaction.changeCents) / 100.0)
                  << "\n";
    }
}

void POS::viewTransactionDetails() {
    int transactionId = readPositiveInt("Enter Transaction ID: ");
    TransactionRecord transaction;
    std::string error;
    if (!getTransaction(transactionId, transaction, error)) {
        std::cout << "Unable to find transaction: " << error << "\n";
        return;
    }
    std::vector<TransactionItemRecord> items;
    if (!getTransactionItems(transactionId, items, error)) {
        std::cout << "Unable to load transaction items: " << error << "\n";
        return;
    }
    printSavedReceipt(transaction, items);
}

void POS::viewDailySalesReport() {
    std::string date = readLine("Enter date (YYYY-MM-DD): ");
    if (!isValidDate(date)) {
        std::cout << "Invalid date. Use YYYY-MM-DD.\n";
        return;
    }
    DailySalesRecord report;
    std::vector<ProductSalesRecord> productSales;
    std::string error;
    if (!getDailySales(date, report, productSales, error)) {
        std::cout << "Unable to generate sales report: " << error << "\n";
        return;
    }
    std::cout << "\n=== Daily Sales Report: " << date << " ===\n";
    std::cout << "Completed transactions: " << report.transactionCount << "\n";
    std::cout << "Total products sold: " << report.productsSold << "\n";
    std::cout << "Gross sales: "
              << formatCurrency(static_cast<double>(report.grossSalesCents) / 100.0)
              << "\n";
    if (productSales.empty()) {
        std::cout << "No product sales for this date.\n";
        return;
    }
    std::cout << "\n=== Product Sales Summary ===\n";
    std::cout << std::left << std::setw(12) << "Product ID"
              << "| " << std::setw(25) << "Product Name"
              << "| " << std::setw(12) << "Quantity"
              << "| Sales\n";
    for (const auto& product : productSales) {
        std::cout << std::left << std::setw(12) << product.productId
                  << "| " << std::setw(25) << product.productName
                  << "| " << std::setw(12) << product.quantitySold
                  << "| " << formatCurrency(static_cast<double>(product.salesCents) / 100.0)
                  << "\n";
    }
    std::cout << "Best-selling product: " << productSales.front().productName
              << " (" << productSales.front().quantitySold << " sold)\n";
}

bool POS::isValidDate(const std::string& date) {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-')
        return false;
    for (std::size_t i = 0; i < date.size(); ++i) {
        if (i == 4 || i == 7) continue;
        if (date[i] < '0' || date[i] > '9') return false;
    }
    int year = std::stoi(date.substr(0, 4));
    int month = std::stoi(date.substr(5, 2));
    int day = std::stoi(date.substr(8, 2));
    if (year < 1 || month < 1 || month > 12 || day < 1)
        return false;
    static const int days[] = {31, 28, 31, 30, 31, 30,
                               31, 31, 30, 31, 30, 31};
    int maxDay = days[month - 1];
    if (month == 2 &&
        (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)))
        maxDay = 29;
    return day <= maxDay;
}

std::string POS::formatCurrency(double amount) {
    std::ostringstream oss;
    oss << "PHP " << std::fixed << std::setprecision(2) << amount;
    return oss.str();
}

std::string POS::getCurrentDateTime() {
    std::time_t now = std::time(nullptr);
    std::string dt = std::ctime(&now);
    // ctime appends a trailing newline — remove it
    if (!dt.empty() && dt.back() == '\n') {
        dt.pop_back();
    }
    return dt;
}

double POS::readNonNegativeDouble(const std::string& prompt) {
    double value = 0.0;
    while (true) {
        std::cout << prompt;
        std::cin >> value;
        if (!std::cin.fail() && value >= 0.0 && std::isfinite(value)) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return value;
        }
        std::cout << "Invalid input. Please enter a non-negative number.\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

std::string POS::readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string value;
    std::getline(std::cin, value);
    return value;
}

std::string POS::readPassword(const std::string& prompt) {
    std::cout << prompt;
    std::string value;
#ifdef _WIN32
    if (_isatty(_fileno(stdin))) {
        while (true) {
            const int character = _getch();
            if (character == '\r' || character == '\n')
                break;
            if (character == 0 || character == 224) {
                _getch();
                continue;
            }
            if (character == '\b') {
                if (!value.empty()) {
                    value.pop_back();
                    std::cout << "\b \b";
                }
                continue;
            }
            if (character >= 32 && character <= 126) {
                value.push_back(static_cast<char>(character));
                std::cout << '*';
            }
        }
        std::cout << '\n';
        return value;
    }
#endif
    std::getline(std::cin, value);
    return value;
}

int POS::readSignedInt(const std::string& prompt) {
    int value = 0;
    while (true) {
        std::cout << prompt;
        std::cin >> value;
        if (!std::cin.fail()) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return value;
        }
        std::cout << "Invalid input. Please enter a whole number.\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

int POS::readMenuChoice() {
    int choice = 0;
    std::cin >> choice;
    if (std::cin.fail() || choice < 1 || choice > 11) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        return -1;
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return choice;
}

int POS::readPositiveInt(const std::string& prompt) {
    int value = 0;
    while (true) {
        std::cout << prompt;
        std::cin >> value;
        if (!std::cin.fail() && value > 0) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return value;
        }
        std::cout << "Invalid input. Please enter a positive integer.\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

double POS::readCashAmount(double minimum) {
    double amount = 0.0;
    while (true) {
        std::cout << "Enter cash amount: ";
        std::cin >> amount;
        if (!std::cin.fail() && amount >= minimum) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return amount;
        }
        if (std::cin.fail()) {
            std::cout << "Invalid input. Please enter a valid amount.\n";
        } else {
                std::cout << "Amount must be at least " << formatCurrency(minimum) << ". Try again.\n";
        }
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

std::string POS::readProductId(const std::string& prompt) {
    std::cout << prompt;
    std::string id;
    std::cin >> id;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return id;
}
