#ifndef REPORTSPAGE_H
#define REPORTSPAGE_H

#include "PosController.h"

#include <QWidget>
#include <string>
#include <vector>

class QDateEdit;
class QComboBox;
class QLabel;
class QTableWidget;
class QPushButton;

class ReportsPage final : public QWidget {
public:
    explicit ReportsPage(PosController& controller, QWidget* parent = nullptr);

private:
    void generateReport();
    void setMessage(const QString& message, bool isError);
    void clearReport();
    void exportCsv();
    void exportPdf();
    void printReport();
    void updateDateControls();
    bool selectedDateRange(std::string& startDate, std::string& endDate) const;

    PosController& controller_;
    QComboBox* periodSelector_;
    QDateEdit* reportDate_;
    QDateEdit* customStartDate_;
    QDateEdit* customEndDate_;
    QLabel* reportDateLabel_;
    QLabel* customStartLabel_;
    QLabel* customEndLabel_;
    QLabel* reportRangeLabel_;
    QLabel* transactionCount_;
    QLabel* productsSold_;
    QLabel* grossSales_;
    QLabel* refundTotal_;
    QLabel* netSales_;
    QTableWidget* productSalesTable_;
    QTableWidget* operatorSalesTable_;
    QTableWidget* refundsTable_;
    QLabel* messageLabel_;
    QPushButton* csvButton_;
    QPushButton* pdfButton_;
    QPushButton* printButton_;
    std::string currentStartDate_;
    std::string currentEndDate_;
    SalesReportRecord currentReport_;
    std::vector<ProductSalesRecord> currentProducts_;
    std::vector<OperatorSalesRecord> currentOperatorSales_;
    std::vector<TransactionRefundRecord> currentRefunds_;
};

#endif
