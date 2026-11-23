#ifndef EXPORTSERVICE_H
#define EXPORTSERVICE_H

#include "../TransactionRepository.h"

#include <QString>
#include <QWidget>

#include <string>
#include <vector>

class ExportService final {
public:
    static bool exportReportCsv(const std::string& path,
                                const std::string& date,
                                const DailySalesRecord& report,
                                const std::vector<ProductSalesRecord>& products,
                                const std::vector<OperatorSalesRecord>& operatorSales,
                                std::string& error);
    static bool exportReportPdf(const std::string& path,
                                const std::string& date,
                                const DailySalesRecord& report,
                                const std::vector<ProductSalesRecord>& products,
                                const std::vector<OperatorSalesRecord>& operatorSales,
                                std::string& error);
    static bool printReport(const std::string& date,
                            const DailySalesRecord& report,
                            const std::vector<ProductSalesRecord>& products,
                            const std::vector<OperatorSalesRecord>& operatorSales,
                            QWidget* parent,
                            std::string& error);
    static bool exportReportCsv(
        const std::string& path, const std::string& startDate,
        const std::string& endDate, const SalesReportRecord& report,
        const std::vector<ProductSalesRecord>& products,
        const std::vector<OperatorSalesRecord>& operatorSales,
        const std::vector<TransactionRefundRecord>& refunds,
        std::string& error);
    static bool exportReportPdf(
        const std::string& path, const std::string& startDate,
        const std::string& endDate, const SalesReportRecord& report,
        const std::vector<ProductSalesRecord>& products,
        const std::vector<OperatorSalesRecord>& operatorSales,
        const std::vector<TransactionRefundRecord>& refunds,
        std::string& error);
    static bool printReport(
        const std::string& startDate, const std::string& endDate,
        const SalesReportRecord& report,
        const std::vector<ProductSalesRecord>& products,
        const std::vector<OperatorSalesRecord>& operatorSales,
        const std::vector<TransactionRefundRecord>& refunds,
        QWidget* parent,
        std::string& error);
    static bool exportReceiptPdf(const std::string& path,
                                 const TransactionRecord& transaction,
                                 const std::vector<TransactionItemRecord>& items,
                                 std::string& error);
    static bool printReceipt(const TransactionRecord& transaction,
                             const std::vector<TransactionItemRecord>& items,
                             QWidget* parent,
                             std::string& error);
};

#endif
