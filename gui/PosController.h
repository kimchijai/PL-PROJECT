#ifndef POSCONTROLLER_H
#define POSCONTROLLER_H

#include "../POS.h"

struct LowStockSummary {
    std::string productId;
    std::string name;
    int stock;
};

struct DashboardSummary {
    int todaySalesCents;
    int transactionCount;
    int totalProducts;
    int lowStockCount;
};

class PosController final {
public:
    explicit PosController(const std::string& databasePath = "bakery_pos.db");

    bool isReady() const;
    std::string errorMessage() const;
    std::string currentOperatorName() const;
    std::string currentOperatorRole() const;
    int currentOperatorAccountId() const;
    bool isAuthenticated() const;
    bool isAdministrator() const;
    bool hasAdministrator(bool& exists, std::string& error) const;
    bool createInitialAdministrator(const std::string& username,
                                    const std::string& password,
                                    std::string& error);
    bool login(const std::string& username, const std::string& password,
               std::string& error);
    void logout();
    bool listOperatorAccounts(std::vector<OperatorAccount>& accounts,
                              std::string& error) const;
    bool createOperatorAccount(const std::string& username,
                               const std::string& password,
                               const std::string& role,
                               std::string& error);
    bool updateOperatorAccount(int accountId, const std::string& username,
                               const std::string& role,
                               const std::string& newPassword,
                               std::string& error);
    bool deactivateOperatorAccount(int accountId, std::string& error);
    bool activeProducts(std::vector<ProductRecord>& records, std::string& error) const;
    bool searchProducts(const std::string& term, std::vector<ProductRecord>& records,
                        std::string& error) const;
    bool stock(const std::string& productId, int& quantity, std::string& error) const;
    bool latestInventoryLog(const std::string& productId, InventoryLog& log,
                            std::string& error) const;
    bool createProduct(const std::string& productId, const std::string& name,
                       int priceCents, int initialStock, std::string& error);
    bool createProduct(const std::string& productId, const std::string& name,
                       int priceCents, int initialStock,
                       const std::string& imagePath, std::string& error);
    bool editProduct(const std::string& productId, const std::string& name,
                     int priceCents, std::string& error);
    bool editProduct(const std::string& productId, const std::string& name,
                     int priceCents, const std::string& imagePath,
                     std::string& error);
    bool deactivateProduct(const std::string& productId, std::string& error);
    bool deleteProduct(const std::string& productId,
                       ProductRecord& deletedRecord,
                       bool& imagePathUnreferenced,
                       std::string& error);
    bool restock(const std::string& productId, int quantity,
                 const std::string& reason, std::string& error);
    bool adjustStock(const std::string& productId, int quantityChange,
                     const std::string& reason, std::string& error);

    bool addToCart(const std::string& productId, int quantity, std::string& error);
    bool updateCart(const std::string& productId, int quantity, std::string& error);
    bool removeFromCart(const std::string& productId, std::string& error);
    const std::vector<CartItem>& cartItems() const;
    double cartTotal() const;
    bool checkout(int cashCents, TransactionRecord& transaction,
                  std::vector<TransactionItemRecord>& items, std::string& error);

    bool recentTransactions(std::vector<TransactionRecord>& transactions,
                            int limit, std::string& error) const;
    bool transaction(int transactionId, TransactionRecord& record,
                     std::string& error) const;
    bool transactionItems(int transactionId,
                          std::vector<TransactionItemRecord>& items,
                          std::string& error) const;
    bool transactionRefunds(
        int transactionId, std::vector<TransactionRefundRecord>& refunds,
        std::string& error) const;
    bool dailySales(const std::string& date, DailySalesRecord& report,
                    std::vector<ProductSalesRecord>& products,
                    std::string& error) const;
    bool dailyOperatorSales(
        const std::string& date,
        std::vector<OperatorSalesRecord>& operatorSales,
        std::string& error) const;
    bool salesReport(
        const std::string& startDate, const std::string& endDate,
        SalesReportRecord& report,
        std::vector<ProductSalesRecord>& products,
        std::vector<OperatorSalesRecord>& operatorSales,
        std::vector<TransactionRefundRecord>& refunds,
        std::string& error) const;
    bool dashboard(const std::string& date, DashboardSummary& summary,
                   std::vector<TransactionRecord>& recentTransactions,
                   std::vector<LowStockSummary>& lowStock,
                   std::string& error) const;
    bool backupDatabase(const std::string& destinationPath, std::string& error);
    bool validateDatabaseBackup(const std::string& sourcePath,
                                std::string& error) const;
    bool restoreDatabase(const std::string& sourcePath, std::string& error);
    bool fullRefund(int transactionId, const std::string& reason,
                    TransactionRefundRecord& refund, std::string& error);
    bool partialRefund(int transactionId,
                       const std::vector<TransactionRefundItemInput>& items,
                       const std::string& reason,
                       TransactionRefundRecord& refund, std::string& error);
    bool voidTransaction(int transactionId, const std::string& reason,
                         TransactionRefundRecord& refund, std::string& error);

private:
    POS pos_;
};

#endif
