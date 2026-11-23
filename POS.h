#ifndef POS_H
#define POS_H

#include "ShoppingCart.h"
#include "DatabaseManager.h"
#include "ProductRepository.h"
#include "InventoryRepository.h"
#include "TransactionRepository.h"
#include "OperatorAccountRepository.h"
#include <vector>
#include <string>

class POS {
public:
    POS();
    explicit POS(const std::string& databasePath);
    void run();  // starts and drives the main menu loop

    bool isDatabaseReady() const;
    std::string databaseError() const;
    std::string currentOperatorName() const;
    std::string currentOperatorRole() const;
    int currentOperatorAccountId() const;
    bool hasAdministrator(bool& exists, std::string& error) const;
    bool createInitialAdministrator(const std::string& username,
                                    const std::string& password,
                                    std::string& error);
    bool login(const std::string& username, const std::string& password,
               std::string& error);
    void logout();
    bool isAuthenticated() const;
    bool isAdministrator() const;
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
    bool getActiveProducts(std::vector<ProductRecord>& records, std::string& error) const;
    bool searchProducts(const std::string& term, std::vector<ProductRecord>& records,
                        std::string& error) const;
    bool getStock(const std::string& productId, int& quantity, std::string& error) const;
    bool getLatestInventoryLog(const std::string& productId, InventoryLog& log,
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
    bool deleteProduct(const std::string& productId, ProductRecord& deletedRecord,
                       bool& imagePathUnreferenced, std::string& error);
    bool restockProduct(const std::string& productId, int quantity,
                        const std::string& reason, std::string& error);
    bool adjustStock(const std::string& productId, int quantityChange,
                     const std::string& reason, std::string& error);
    bool addProductToCart(const std::string& productId, int quantity, std::string& error);
    bool updateCartQuantity(const std::string& productId, int quantity, std::string& error);
    bool removeCartProduct(const std::string& productId, std::string& error);
    const std::vector<CartItem>& cartItems() const;
    double cartTotal() const;
    bool checkoutWithCash(int cashCents, TransactionRecord& transaction,
                          std::vector<TransactionItemRecord>& items, std::string& error);
    bool listRecentTransactions(std::vector<TransactionRecord>& transactions,
                               int limit, std::string& error) const;
    bool listTransactionsForDate(const std::string& date,
                                 std::vector<TransactionRecord>& transactions,
                                 std::string& error) const;
    bool listTransactionRefunds(int transactionId,
                                std::vector<TransactionRefundRecord>& refunds,
                                std::string& error) const;
    bool getTransaction(int transactionId, TransactionRecord& transaction,
                        std::string& error) const;
    bool getTransactionItems(int transactionId,
                             std::vector<TransactionItemRecord>& items,
                             std::string& error) const;
    bool getDailySales(const std::string& date, DailySalesRecord& report,
                       std::vector<ProductSalesRecord>& products,
                       std::string& error) const;
    bool getDailyOperatorSales(
        const std::string& date,
        std::vector<OperatorSalesRecord>& operatorSales,
        std::string& error) const;
    bool getSalesReport(
        const std::string& startDate, const std::string& endDate,
        SalesReportRecord& report,
        std::vector<ProductSalesRecord>& productSales,
        std::vector<OperatorSalesRecord>& operatorSales,
        std::vector<TransactionRefundRecord>& refunds,
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
    std::vector<Product> catalog_;
    ShoppingCart         cart_;
    int                  transactionId_;
    DatabaseManager      database_;
    OperatorAccountRepository operatorAccountRepository_;
    ProductRepository    productRepository_;
    InventoryRepository  inventoryRepository_;
    TransactionRepository transactionRepository_;
    bool                 databaseReady_;
    int                  operatorAccountId_;
    std::string          operatorName_;
    std::string          operatorRole_;

    // Menu action handlers
    void viewProducts()    const;
    void addToCart();
    void viewCart()        const;
    void removeFromCart();
    void updateQuantity();
    void checkout();
    void productManagementMenu();
    void inventoryManagementMenu();
    void viewManagedProducts();
    void searchProducts();
    void addProduct();
    void editProduct();
    void deactivateProduct();
    void viewInventory();
    void restockProduct();
    void adjustStock();
    void refreshCatalog();
    void historyAndReportsMenu();
    void viewTransactionHistory();
    void viewTransactionDetails();
    void viewDailySalesReport();
    void printSavedReceipt(const TransactionRecord& transaction,
                           const std::vector<TransactionItemRecord>& items);

    // Helpers
    const Product* findProductById(const std::string& id) const;
    static std::string formatCurrency(double amount);  // "PHP X.XX"
    static std::string getCurrentDateTime();            // from ctime

    // Input validation helpers
    static int    readMenuChoice();
    static int    readPositiveInt(const std::string& prompt);
    static double readCashAmount(double minimum);
    static std::string readProductId(const std::string& prompt);
    static double readNonNegativeDouble(const std::string& prompt);
    static std::string readLine(const std::string& prompt);
    static std::string readPassword(const std::string& prompt);
    static int readSignedInt(const std::string& prompt);
    static bool isValidDate(const std::string& date);
    bool requireOperator(std::string& error) const;
    bool requireAdministrator(std::string& error) const;
};

#endif // POS_H
