#include "PosController.h"

namespace {
constexpr int LOW_STOCK_THRESHOLD = 30;
}

PosController::PosController(const std::string& databasePath)
    : pos_(databasePath) {}

bool PosController::isReady() const {
    return pos_.isDatabaseReady();
}

std::string PosController::errorMessage() const {
    return pos_.databaseError();
}

std::string PosController::currentOperatorName() const {
    return pos_.currentOperatorName();
}

std::string PosController::currentOperatorRole() const {
    return pos_.currentOperatorRole();
}

int PosController::currentOperatorAccountId() const {
    return pos_.currentOperatorAccountId();
}

bool PosController::isAuthenticated() const {
    return pos_.isAuthenticated();
}

bool PosController::isAdministrator() const {
    return pos_.isAdministrator();
}

bool PosController::hasAdministrator(bool& exists, std::string& error) const {
    return pos_.hasAdministrator(exists, error);
}

bool PosController::createInitialAdministrator(
    const std::string& username, const std::string& password,
    std::string& error) {
    return pos_.createInitialAdministrator(username, password, error);
}

bool PosController::login(const std::string& username,
                          const std::string& password,
                          std::string& error) {
    return pos_.login(username, password, error);
}

void PosController::logout() {
    pos_.logout();
}

bool PosController::listOperatorAccounts(
    std::vector<OperatorAccount>& accounts, std::string& error) const {
    return pos_.listOperatorAccounts(accounts, error);
}

bool PosController::createOperatorAccount(
    const std::string& username, const std::string& password,
    const std::string& role, std::string& error) {
    return pos_.createOperatorAccount(username, password, role, error);
}

bool PosController::updateOperatorAccount(
    int accountId, const std::string& username, const std::string& role,
    const std::string& newPassword, std::string& error) {
    return pos_.updateOperatorAccount(accountId, username, role,
                                      newPassword, error);
}

bool PosController::deactivateOperatorAccount(int accountId,
                                             std::string& error) {
    return pos_.deactivateOperatorAccount(accountId, error);
}

bool PosController::activeProducts(std::vector<ProductRecord>& records,
                                   std::string& error) const {
    return pos_.getActiveProducts(records, error);
}

bool PosController::searchProducts(const std::string& term,
                                   std::vector<ProductRecord>& records,
                                   std::string& error) const {
    return pos_.searchProducts(term, records, error);
}

bool PosController::stock(const std::string& productId, int& quantity,
                          std::string& error) const {
    return pos_.getStock(productId, quantity, error);
}

bool PosController::latestInventoryLog(const std::string& productId,
                                       InventoryLog& log,
                                       std::string& error) const {
    return pos_.getLatestInventoryLog(productId, log, error);
}

bool PosController::createProduct(const std::string& productId,
                                  const std::string& name,
                                  int priceCents, int initialStock,
                                  std::string& error) {
    return pos_.createProduct(productId, name, priceCents, initialStock, error);
}

bool PosController::createProduct(const std::string& productId,
                                  const std::string& name,
                                  int priceCents, int initialStock,
                                  const std::string& imagePath,
                                  std::string& error) {
    return pos_.createProduct(productId, name, priceCents,
                              initialStock, imagePath, error);
}

bool PosController::editProduct(const std::string& productId,
                                const std::string& name, int priceCents,
                                std::string& error) {
    return pos_.editProduct(productId, name, priceCents, error);
}

bool PosController::editProduct(const std::string& productId,
                                const std::string& name, int priceCents,
                                const std::string& imagePath,
                                std::string& error) {
    return pos_.editProduct(productId, name, priceCents, imagePath, error);
}

bool PosController::deactivateProduct(const std::string& productId,
                                      std::string& error) {
    return pos_.deactivateProduct(productId, error);
}

bool PosController::deleteProduct(const std::string& productId,
                                 ProductRecord& deletedRecord,
                                 bool& imagePathUnreferenced,
                                 std::string& error) {
    return pos_.deleteProduct(productId, deletedRecord,
                              imagePathUnreferenced, error);
}

bool PosController::restock(const std::string& productId, int quantity,
                            const std::string& reason, std::string& error) {
    return pos_.restockProduct(productId, quantity, reason, error);
}

bool PosController::adjustStock(const std::string& productId,
                                int quantityChange, const std::string& reason,
                                std::string& error) {
    return pos_.adjustStock(productId, quantityChange, reason, error);
}

bool PosController::addToCart(const std::string& productId, int quantity,
                              std::string& error) {
    return pos_.addProductToCart(productId, quantity, error);
}

bool PosController::updateCart(const std::string& productId, int quantity,
                               std::string& error) {
    return pos_.updateCartQuantity(productId, quantity, error);
}

bool PosController::removeFromCart(const std::string& productId,
                                   std::string& error) {
    return pos_.removeCartProduct(productId, error);
}

const std::vector<CartItem>& PosController::cartItems() const {
    return pos_.cartItems();
}

double PosController::cartTotal() const {
    return pos_.cartTotal();
}

bool PosController::checkout(int cashCents, TransactionRecord& transaction,
                             std::vector<TransactionItemRecord>& items,
                             std::string& error) {
    return pos_.checkoutWithCash(cashCents, transaction, items, error);
}

bool PosController::recentTransactions(std::vector<TransactionRecord>& transactions,
                                       int limit, std::string& error) const {
    return pos_.listRecentTransactions(transactions, limit, error);
}

bool PosController::transaction(int transactionId, TransactionRecord& record,
                                std::string& error) const {
    return pos_.getTransaction(transactionId, record, error);
}

bool PosController::transactionItems(
    int transactionId, std::vector<TransactionItemRecord>& items,
    std::string& error) const {
    return pos_.getTransactionItems(transactionId, items, error);
}

bool PosController::transactionRefunds(
    int transactionId,
    std::vector<TransactionRefundRecord>& refunds,
    std::string& error) const {
    return pos_.listTransactionRefunds(transactionId, refunds, error);
}

bool PosController::dailySales(const std::string& date,
                               DailySalesRecord& report,
                               std::vector<ProductSalesRecord>& products,
                               std::string& error) const {
    return pos_.getDailySales(date, report, products, error);
}

bool PosController::dailyOperatorSales(
    const std::string& date,
    std::vector<OperatorSalesRecord>& operatorSales,
    std::string& error) const {
    return pos_.getDailyOperatorSales(date, operatorSales, error);
}

bool PosController::salesReport(
    const std::string& startDate,
    const std::string& endDate,
    SalesReportRecord& report,
    std::vector<ProductSalesRecord>& products,
    std::vector<OperatorSalesRecord>& operatorSales,
    std::vector<TransactionRefundRecord>& refunds,
    std::string& error) const {
    return pos_.getSalesReport(startDate, endDate, report, products,
                               operatorSales, refunds, error);
}

bool PosController::dashboard(const std::string& date, DashboardSummary& summary,
                             std::vector<TransactionRecord>& recentTransactions,
                             std::vector<LowStockSummary>& lowStock,
                             std::string& error) const {
    summary = {};
    recentTransactions.clear();
    lowStock.clear();
    if (!isReady()) {
        error = errorMessage();
        return false;
    }

    DailySalesRecord sales{};
    std::vector<ProductSalesRecord> productSales;
    if (!pos_.getDailySales(date, sales, productSales, error))
        return false;

    std::vector<ProductRecord> activeProducts;
    if (!pos_.getActiveProducts(activeProducts, error))
        return false;
    if (!pos_.listTransactionsForDate(date, recentTransactions, error))
        return false;

    for (const auto& product : activeProducts) {
        int stock = 0;
        if (!pos_.getStock(product.productId, stock, error))
            return false;
        if (stock <= LOW_STOCK_THRESHOLD)
            lowStock.push_back({product.productId, product.name, stock});
    }

    summary.todaySalesCents = sales.grossSalesCents;
    summary.transactionCount = sales.transactionCount;
    summary.totalProducts = static_cast<int>(activeProducts.size());
    summary.lowStockCount = static_cast<int>(lowStock.size());
    return true;
}

bool PosController::backupDatabase(const std::string& destinationPath,
                                   std::string& error) {
    return pos_.backupDatabase(destinationPath, error);
}

bool PosController::validateDatabaseBackup(const std::string& sourcePath,
                                           std::string& error) const {
    return pos_.validateDatabaseBackup(sourcePath, error);
}

bool PosController::restoreDatabase(const std::string& sourcePath,
                                    std::string& error) {
    return pos_.restoreDatabase(sourcePath, error);
}

bool PosController::fullRefund(int transactionId, const std::string& reason,
                               TransactionRefundRecord& refund, std::string& error) {
    return pos_.fullRefund(transactionId, reason, refund, error);
}

bool PosController::partialRefund(
    int transactionId,
    const std::vector<TransactionRefundItemInput>& items,
    const std::string& reason,
    TransactionRefundRecord& refund,
    std::string& error) {
    return pos_.partialRefund(transactionId, items, reason, refund, error);
}

bool PosController::voidTransaction(int transactionId,
                                     const std::string& reason,
                                     TransactionRefundRecord& refund,
                                     std::string& error) {
    return pos_.voidTransaction(transactionId, reason, refund, error);
}
