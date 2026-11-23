#ifndef TRANSACTIONREPOSITORY_H
#define TRANSACTIONREPOSITORY_H

#include "DatabaseManager.h"
#include <string>
#include <vector>

struct TransactionItemInput {
    std::string productId;
    int quantity;
};

struct TransactionItemRecord {
    std::string productId;
    std::string productName;
    int unitPriceCents;
    int quantity;
    int subtotalCents;
};

struct TransactionRecord {
    int transactionId;
    std::string transactionDate;
    int orderTotalCents;
    int cashReceivedCents;
    int changeCents;
    std::string status;
    int refundedCents;
    std::string operatorName;
};

struct TransactionRefundItemInput {
    std::string productId;
    int quantity;
};

struct TransactionRefundRecord {
    int refundId;
    int transactionId;
    std::string refundDate;
    std::string refundType;
    std::string reason;
    int refundCents;
    std::string operatorName;
};

struct DailySalesRecord {
    int transactionCount;
    int productsSold;
    int grossSalesCents;
};

struct SalesReportRecord {
    int transactionCount;
    int productsSold;
    int grossSalesCents;
    int refundCount;
    int refundedCents;
    int voidCount;
    int voidedCents;
    int netSalesCents;
};

struct ProductSalesRecord {
    std::string productId;
    std::string productName;
    int quantitySold;
    int salesCents;
};

struct OperatorSalesRecord {
    std::string operatorName;
    int transactionCount;
    int salesCents;
};

class TransactionRepository {
public:
    explicit TransactionRepository(DatabaseManager& database);

    bool createAtomic(const std::vector<TransactionItemInput>& items,
                      int cashReceivedCents,
                      TransactionRecord& transaction,
                      std::vector<TransactionItemRecord>& savedItems,
                      std::string& error,
                      const std::string& operatorName = "Unknown");
    bool getById(int transactionId, TransactionRecord& transaction,
                 std::string& error) const;
    bool getItems(int transactionId,
                  std::vector<TransactionItemRecord>& items,
                  std::string& error) const;
    bool getRefunds(int transactionId,
                    std::vector<TransactionRefundRecord>& refunds,
                    std::string& error) const;
    bool listRecent(std::vector<TransactionRecord>& transactions,
                    int limit, std::string& error) const;
    bool listForDate(const std::string& date,
                     std::vector<TransactionRecord>& transactions,
                     std::string& error) const;
    bool getDailySales(const std::string& date,
                       DailySalesRecord& report,
                       std::vector<ProductSalesRecord>& productSales,
                       std::string& error) const;
    bool getDailyOperatorSales(const std::string& date,
                               std::vector<OperatorSalesRecord>& operatorSales,
                               std::string& error) const;
    bool getSalesReport(
        const std::string& startDate, const std::string& endDate,
        SalesReportRecord& report,
        std::vector<ProductSalesRecord>& productSales,
        std::vector<OperatorSalesRecord>& operatorSales,
        std::vector<TransactionRefundRecord>& refunds,
        std::string& error) const;
    bool fullRefund(int transactionId, const std::string& reason,
                    TransactionRefundRecord& refund, std::string& error,
                    const std::string& operatorName = "Unknown");
    bool partialRefund(int transactionId,
                       const std::vector<TransactionRefundItemInput>& items,
                       const std::string& reason,
                       TransactionRefundRecord& refund, std::string& error,
                       const std::string& operatorName = "Unknown");
    bool voidTransaction(int transactionId, const std::string& reason,
                         TransactionRefundRecord& refund, std::string& error,
                         const std::string& operatorName = "Unknown");

private:
    DatabaseManager& database_;
};

#endif
