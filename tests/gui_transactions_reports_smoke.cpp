#include "../gui/ReportsPage.h"
#include "../gui/TransactionsPage.h"
#include "../gui/ExportService.h"
#include "../gui/DisplayFormatting.h"
#include "../gui/ApplicationFont.h"
#include "../DatabaseManager.h"

#include <QApplication>
#include <QByteArray>
#include <QComboBox>
#include <QDate>
#include <QDateTime>
#include <QDateEdit>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPrintPreviewDialog>
#include <QPrintPreviewWidget>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QFileInfo>

#include <iostream>
#include <functional>
#include <string>
#include <vector>

namespace {
int transactionRow(QTableWidget* table, int transactionId) {
    for (int row = 0; row < table->rowCount(); ++row) {
        if (table->item(row, 0)
            && table->item(row, 0)->text().toInt() == transactionId) {
            return row;
        }
    }
    return -1;
}

bool verifyPrintPreviewCanBeCancelled(
    const QString& objectName,
    const std::function<bool(std::string&)>& printWorkflow,
    std::string& error,
    int minimumPageCount = 1) {
    bool previewOpened = false;
    bool enoughPages = false;
    QTimer::singleShot(0, [&previewOpened, &enoughPages, objectName,
                           minimumPageCount]() {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            auto* preview = qobject_cast<QPrintPreviewDialog*>(widget);
            if (!preview || preview->objectName() != objectName)
                continue;
            auto* previewWidget = preview->findChild<QPrintPreviewWidget*>();
            previewOpened = previewWidget != nullptr;
            if (!previewWidget) {
                preview->reject();
                return;
            }
            QTimer::singleShot(150, preview, [preview, previewWidget,
                                               &enoughPages, minimumPageCount]() {
                enoughPages = previewWidget->pageCount() >= minimumPageCount;
                preview->reject();
            });
            return;
        }
    });

    const bool printed = printWorkflow(error);
    return !printed && previewOpened && enoughPages
        && error == "Printing cancelled.";
}

bool verifyProductionDatabase(const std::string& path) {
    PosController controller(path);
    if (!controller.isReady()) {
        std::cerr << "Could not open database: " << controller.errorMessage() << "\n";
        return false;
    }

    std::string error;
    std::vector<TransactionRecord> expectedTransactions;
    if (!controller.recentTransactions(expectedTransactions, 100, error)) {
        std::cerr << "Could not read recent transactions: " << error << "\n";
        return false;
    }
    TransactionsPage transactionsPage(controller);
    ReportsPage reportsPage(controller);
    auto* history = transactionsPage.findChild<QTableWidget*>("transactionsTable");
    auto* transactionItems =
        transactionsPage.findChild<QTableWidget*>("transactionItemsTable");
    auto* receipt = transactionsPage.findChild<QPlainTextEdit*>("transactionReceipt");
    auto* dateInput = reportsPage.findChild<QDateEdit*>("reportDateInput");
    auto* reportButton = reportsPage.findChild<QPushButton*>("generateReportButton");
    auto* transactionCount = reportsPage.findChild<QLabel*>("reportTransactionCount");
    auto* productsSold = reportsPage.findChild<QLabel*>("reportProductsSold");
    auto* grossSales = reportsPage.findChild<QLabel*>("reportGrossSales");
    auto* refundTotal = reportsPage.findChild<QLabel*>("reportRefundTotal");
    auto* netSales = reportsPage.findChild<QLabel*>("reportNetSales");
    auto* productTable = reportsPage.findChild<QTableWidget*>("productSalesTable");
    auto* operatorTable = reportsPage.findChild<QTableWidget*>("operatorSalesTable");
    auto* refundTable = reportsPage.findChild<QTableWidget*>("reportRefundsTable");
    if (!history || !transactionItems || !receipt || !dateInput || !reportButton || !transactionCount
        || !productsSold || !grossSales || !refundTotal || !netSales || !productTable
        || !operatorTable || !refundTable
        || history->rowCount() != static_cast<int>(expectedTransactions.size())) {
        std::cerr << "Transaction history page does not match the database records.\n";
        return false;
    }

    for (int row = 0; row < static_cast<int>(expectedTransactions.size()); ++row) {
        const auto& expected = expectedTransactions[static_cast<std::size_t>(row)];
        const QString expectedDate = QDateTime::fromString(
            QString::fromStdString(expected.transactionDate), Qt::ISODate)
                .toString("yyyy-MM-dd HH:mm:ss");
        if (!history->item(row, 0)
            || history->item(row, 0)->text().toInt() != expected.transactionId
            || history->item(row, 1)->text() != expectedDate
            || history->item(row, 2)->text()
                != QStringLiteral("PHP %1")
                       .arg(static_cast<double>(expected.orderTotalCents) / 100.0, 0, 'f', 2)
            || history->item(row, 3)->text()
                != QStringLiteral("PHP %1")
                       .arg(static_cast<double>(expected.cashReceivedCents) / 100.0, 0, 'f', 2)
            || history->item(row, 4)->text()
                != QStringLiteral("PHP %1")
                       .arg(static_cast<double>(expected.changeCents) / 100.0, 0, 'f', 2)
            || history->item(row, 7)->text()
                != QString::fromStdString(expected.operatorName)) {
            std::cerr << "Transaction history totals do not match repository values.\n";
            return false;
        }
    }

    if (!expectedTransactions.empty()) {
        const auto& latest = expectedTransactions.front();
        const int row = transactionRow(history, latest.transactionId);
        history->selectRow(row);
        std::vector<TransactionItemRecord> expectedItems;
        if (!controller.transactionItems(latest.transactionId, expectedItems, error)
            || receipt->toPlainText().isEmpty()
            || transactionItems->rowCount() != static_cast<int>(expectedItems.size())
            || !receipt->toPlainText().contains(
                QStringLiteral("Transaction #%1").arg(latest.transactionId))) {
            std::cerr << "Receipt view did not load from saved transaction data: "
                      << error << "\n";
            return false;
        }
        for (int row = 0; row < static_cast<int>(expectedItems.size()); ++row) {
            const auto& item = expectedItems[static_cast<std::size_t>(row)];
            if (!receipt->toPlainText().contains(QString::fromStdString(item.productName))
                || !receipt->toPlainText().contains(QStringLiteral("PHP %1")
                    .arg(static_cast<double>(item.unitPriceCents) / 100.0, 0, 'f', 2))
                || transactionItems->item(row, 0)->text()
                    != QString::fromStdString(item.productId)
                || transactionItems->item(row, 1)->text()
                    != QString::fromStdString(item.productName)
                || transactionItems->item(row, 2)->text()
                    != QStringLiteral("PHP %1")
                           .arg(static_cast<double>(item.unitPriceCents) / 100.0, 0, 'f', 2)
                || transactionItems->item(row, 3)->text() != QString::number(item.quantity)
                || transactionItems->item(row, 4)->text()
                    != QStringLiteral("PHP %1")
                           .arg(static_cast<double>(item.subtotalCents) / 100.0, 0, 'f', 2)) {
                std::cerr << "Receipt does not preserve the saved product snapshot.\n";
                return false;
            }
        }
        dateInput->setDate(QDate::fromString(
            QString::fromStdString(latest.transactionDate.substr(0, 10)), "yyyy-MM-dd"));
    } else {
        dateInput->setDate(QDate::currentDate());
    }
    reportButton->click();

    SalesReportRecord expectedReport{};
    std::vector<ProductSalesRecord> expectedProducts;
    std::vector<OperatorSalesRecord> expectedOperators;
    std::vector<TransactionRefundRecord> expectedRefunds;
    const std::string reportDate = dateInput->date().toString("yyyy-MM-dd").toStdString();
    if (!controller.salesReport(reportDate, reportDate, expectedReport,
                                expectedProducts, expectedOperators,
                                expectedRefunds, error)) {
        std::cerr << "Could not read daily report: " << error << "\n";
        return false;
    }
    if (transactionCount->text().toInt() != expectedReport.transactionCount
        || productsSold->text().toInt() != expectedReport.productsSold
        || grossSales->text() != QStringLiteral("PHP %1")
               .arg(static_cast<double>(expectedReport.grossSalesCents) / 100.0, 0, 'f', 2)
        || refundTotal->text() != QStringLiteral("%1 (%2)")
               .arg(guiFormatMoney(expectedReport.refundedCents))
               .arg(expectedReport.refundCount)
        || netSales->text() != guiFormatMoney(expectedReport.netSalesCents)
        || productTable->rowCount() != static_cast<int>(expectedProducts.size())
        || operatorTable->rowCount() != static_cast<int>(expectedOperators.size())
        || refundTable->rowCount() != static_cast<int>(expectedRefunds.size())) {
        std::cerr << "Reports page does not match repository report values.\n";
        return false;
    }
    for (int row = 0; row < static_cast<int>(expectedProducts.size()); ++row) {
        const auto& expected = expectedProducts[static_cast<std::size_t>(row)];
        if (!productTable->item(row, 0)
            || productTable->item(row, 0)->text() != QString::fromStdString(expected.productId)
            || productTable->item(row, 1)->text()
                != QString::fromStdString(expected.productName)
            || productTable->item(row, 2)->text() != QString::number(expected.quantitySold)
            || productTable->item(row, 3)->text()
                != QStringLiteral("PHP %1")
                       .arg(static_cast<double>(expected.salesCents) / 100.0, 0, 'f', 2)) {
            std::cerr << "Product sales table does not match repository report values.\n";
            return false;
        }
    }

    std::cout << "Actual database verified: " << expectedTransactions.size()
              << " recent transaction(s); report for " << reportDate
              << " includes " << expectedReport.transactionCount << " transaction(s), "
              << expectedReport.productsSold << " products sold, gross PHP "
              << expectedReport.grossSalesCents / 100 << ".\n";
    return true;
}

bool verifyTemporaryDatabase(const std::string& path) {
    PosController controller(path);
    if (!controller.isReady()) {
        std::cerr << "Temporary database initialization failed: "
                  << controller.errorMessage() << "\n";
        return false;
    }
    std::string error;
    if (!controller.createInitialAdministrator(
            "reports_smoke", "ReportsSmokePassword!23", error)
        || !controller.login("reports_smoke", "ReportsSmokePassword!23", error)) {
        std::cerr << "Could not create or log in test operator: " << error << "\n";
        return false;
    }

    TransactionsPage transactionsPage(controller);
    ReportsPage reportsPage(controller);
    auto* detailsSplitter =
        transactionsPage.findChild<QSplitter*>("transactionDetailsSplitter");
    auto* history = transactionsPage.findChild<QTableWidget*>("transactionsTable");
    auto* historyMessage = transactionsPage.findChild<QLabel*>("transactionsMessage");
    auto* searchInput =
        transactionsPage.findChild<QLineEdit*>("transactionSearchInput");
    auto* searchButton =
        transactionsPage.findChild<QPushButton*>("searchTransactionButton");
    auto* refreshButton =
        transactionsPage.findChild<QPushButton*>("refreshTransactionsButton");
    auto* itemTable =
        transactionsPage.findChild<QTableWidget*>("transactionItemsTable");
    auto* receipt = transactionsPage.findChild<QPlainTextEdit*>("transactionReceipt");
    auto* transactionMessage =
        transactionsPage.findChild<QLabel*>("transactionsMessage");
    auto* dateInput = reportsPage.findChild<QDateEdit*>("reportDateInput");
    auto* periodInput = reportsPage.findChild<QComboBox*>("reportPeriodInput");
    auto* customStart = reportsPage.findChild<QDateEdit*>("reportStartDateInput");
    auto* customEnd = reportsPage.findChild<QDateEdit*>("reportEndDateInput");
    auto* reportRange = reportsPage.findChild<QLabel*>("reportRangeLabel");
    auto* reportButton =
        reportsPage.findChild<QPushButton*>("generateReportButton");
    auto* transactionCount =
        reportsPage.findChild<QLabel*>("reportTransactionCount");
    auto* productsSold = reportsPage.findChild<QLabel*>("reportProductsSold");
    auto* grossSales = reportsPage.findChild<QLabel*>("reportGrossSales");
    auto* refundTotal = reportsPage.findChild<QLabel*>("reportRefundTotal");
    auto* netSales = reportsPage.findChild<QLabel*>("reportNetSales");
    auto* productTable = reportsPage.findChild<QTableWidget*>("productSalesTable");
    auto* operatorTable = reportsPage.findChild<QTableWidget*>("operatorSalesTable");
    auto* refundTable = reportsPage.findChild<QTableWidget*>("reportRefundsTable");
    auto* reportsMessage = reportsPage.findChild<QLabel*>("reportsMessage");
    if (!detailsSplitter || detailsSplitter->handleWidth() != 10
        || detailsSplitter->childrenCollapsible()
        || !detailsSplitter->styleSheet().isEmpty()) {
        std::cerr << "Transaction divider behavior or shared styling changed.\n";
        return false;
    }
    if (!history || !historyMessage || !searchInput || !searchButton || !refreshButton
        || !itemTable || !receipt || !transactionMessage || !dateInput || !periodInput
        || !customStart || !customEnd || !reportRange || !reportButton
        || !transactionCount || !productsSold || !grossSales || !refundTotal
        || !netSales || !productTable || !operatorTable || !refundTable
        || !reportsMessage || history->rowCount() != 0
        || !historyMessage->text().contains("No completed transactions")
        || productTable->rowCount() != 0 || operatorTable->rowCount() != 0
        || transactionCount->text() != "0"
        || productsSold->text() != "0" || grossSales->text() != "PHP 0.00"
        || refundTable->rowCount() != 0
        || !reportsMessage->text().contains("No sales, refunds, or voids")) {
        std::cerr << "Empty transaction and report states were not handled.\n";
        return false;
    }

    if (!controller.addToCart("P001", 2, error)) {
        std::cerr << "Could not prepare first test sale: " << error << "\n";
        return false;
    }
    TransactionRecord first{};
    std::vector<TransactionItemRecord> savedItems;
    if (!controller.checkout(15000, first, savedItems, error)) {
        std::cerr << "Could not save first test sale: " << error << "\n";
        return false;
    }
    if (!controller.editProduct("P001", "Updated Chocolate Chip Cookie", 7000, error)) {
        std::cerr << "Could not edit product after saved sale: " << error << "\n";
        return false;
    }
    if (!controller.addToCart("P002", 1, error)) {
        std::cerr << "Could not prepare second test sale: " << error << "\n";
        return false;
    }
    TransactionRecord second{};
    if (!controller.checkout(10000, second, savedItems, error)) {
        std::cerr << "Could not save second test sale: " << error << "\n";
        return false;
    }

    refreshButton->click();
    if (history->rowCount() != 2 || history->item(0, 0)->text().toInt() != second.transactionId
        || history->item(1, 0)->text().toInt() != first.transactionId
        || history->item(1, 2)->text() != "PHP 130.00") {
        std::cerr << "Recent transaction list does not show saved records correctly.\n";
        return false;
    }

    history->selectRow(1);
    if (!receipt->toPlainText().contains("Chocolate Chip Cookie")
        || receipt->toPlainText().contains("Updated Chocolate Chip Cookie")
        || !receipt->toPlainText().contains("PHP 65.00")
        || !receipt->toPlainText().contains("Total: PHP 130.00")
        || !receipt->toPlainText().contains("Operator: reports_smoke")
        || !receipt->toPlainText().contains("Cash: PHP 150.00")
        || !receipt->toPlainText().contains("Change: PHP 20.00")
        || itemTable->rowCount() != 1
        || itemTable->item(0, 1)->text() != "Chocolate Chip Cookie") {
        std::cerr << "Historical receipt did not use transaction snapshots.\n";
        return false;
    }

    searchInput->setText(QString::number(second.transactionId));
    searchButton->click();
    if (!receipt->toPlainText().contains("Double Chocolate Cookie")
        || !receipt->toPlainText().contains("Total: PHP 70.00")
        || !receipt->toPlainText().contains("Cash: PHP 100.00")
        || !receipt->toPlainText().contains("Change: PHP 30.00")) {
        std::cerr << "Transaction ID search did not display the matching receipt.\n";
        return false;
    }
    searchInput->setText("not-an-id");
    searchButton->click();
    if (!transactionMessage->text().contains("valid positive transaction ID")) {
        std::cerr << "Invalid transaction search input was not rejected.\n";
        return false;
    }
    searchInput->setText("999999");
    searchButton->click();
    if (!transactionMessage->text().contains("Transaction not found")) {
        std::cerr << "Missing transaction search was not handled.\n";
        return false;
    }

    dateInput->setDate(QDate::currentDate());
    reportButton->click();
    if (transactionCount->text() != "2" || productsSold->text() != "3"
        || grossSales->text() != "PHP 200.00" || productTable->rowCount() != 2
        || refundTotal->text() != "PHP 0.00 (0)"
        || netSales->text() != "PHP 200.00"
        || operatorTable->rowCount() != 1
        || operatorTable->item(0, 0)->text() != "reports_smoke"
        || operatorTable->item(0, 1)->text() != "2"
        || operatorTable->item(0, 2)->text() != "PHP 200.00"
        || productTable->item(0, 0)->text() != "P001"
        || productTable->item(0, 1)->text() != "Chocolate Chip Cookie"
        || productTable->item(0, 2)->text() != "2"
        || productTable->item(0, 3)->text() != "PHP 130.00") {
        std::cerr << "Daily sales report does not match committed transaction data.\n";
        return false;
    }

    periodInput->setCurrentText("Weekly");
    const QDate today = QDate::currentDate();
    const QDate weekStart = today.addDays(1 - today.dayOfWeek());
    const QDate weekEnd = weekStart.addDays(6);
    if (transactionCount->text() != "2"
        || !reportRange->text().contains(weekStart.toString("yyyy-MM-dd"))
        || !reportRange->text().contains(weekEnd.toString("yyyy-MM-dd"))) {
        std::cerr << "Weekly report range was not calculated correctly.\n";
        return false;
    }

    periodInput->setCurrentText("Monthly");
    const QDate monthStart(today.year(), today.month(), 1);
    const QDate monthEnd = monthStart.addMonths(1).addDays(-1);
    if (transactionCount->text() != "2"
        || !reportRange->text().contains(monthStart.toString("yyyy-MM-dd"))
        || !reportRange->text().contains(monthEnd.toString("yyyy-MM-dd"))) {
        std::cerr << "Monthly report range was not calculated correctly.\n";
        return false;
    }

    periodInput->setCurrentText("Custom date range");
    customStart->setDate(today);
    customEnd->setDate(today);
    if (transactionCount->text() != "2") {
        std::cerr << "Custom date range report did not include both test sales.\n";
        return false;
    }
    customStart->setDate(QDate(2000, 1, 1));
    customEnd->setDate(QDate(2000, 1, 31));
    if (transactionCount->text() != "0" || productsSold->text() != "0"
        || grossSales->text() != "PHP 0.00" || productTable->rowCount() != 0
        || refundTable->rowCount() != 0 || netSales->text() != "PHP 0.00"
        || !reportsMessage->text().contains("No sales, refunds, or voids")) {
        std::cerr << "Empty custom report range was not handled.\n";
        return false;
    }

    TransactionRefundRecord recordedRefund{};
    if (!controller.fullRefund(first.transactionId, "Range report refund test",
                               recordedRefund, error)) {
        std::cerr << "Could not prepare refund report data: " << error << "\n";
        return false;
    }
    customStart->setDate(today);
    customEnd->setDate(today);
    if (transactionCount->text() != "2"
        || grossSales->text() != "PHP 200.00"
        || refundTotal->text() != "PHP 130.00 (1)"
        || netSales->text() != "PHP 70.00"
        || refundTable->rowCount() != 1
        || refundTable->item(0, 4)->text() != "Range report refund test") {
        std::cerr << "Refund totals and activity did not appear in the report.\n";
        return false;
    }

    const QString exportDirectory =
        QFileInfo(QString::fromStdString(path)).absolutePath();
    const QString csvPath = exportDirectory + "/phase4-sales-report.csv";
    const QString pdfPath = exportDirectory + "/phase4-sales-report.pdf";
    SalesReportRecord expectedReport{};
    std::vector<ProductSalesRecord> expectedProducts;
    std::vector<OperatorSalesRecord> expectedOperatorSales;
    std::vector<TransactionRefundRecord> expectedRefunds;
    std::string exportError;
    const std::string exportDate = today.toString("yyyy-MM-dd").toStdString();
    if (!controller.salesReport(exportDate, exportDate, expectedReport,
                                expectedProducts, expectedOperatorSales,
                                expectedRefunds, exportError)) {
        std::cerr << "Could not prepare report export: " << exportError << "\n";
        return false;
    }
    if (!ExportService::exportReportCsv(csvPath.toStdString(),
                                        exportDate, exportDate, expectedReport,
                                        expectedProducts, expectedOperatorSales,
                                        expectedRefunds, exportError)) {
        std::cerr << "CSV report export failed: " << exportError << "\n";
        return false;
    }
    if (!ExportService::exportReportPdf(pdfPath.toStdString(),
                                        exportDate, exportDate, expectedReport,
                                        expectedProducts, expectedOperatorSales,
                                        expectedRefunds, exportError)) {
        std::cerr << "PDF report export failed: " << exportError << "\n";
        return false;
    }
    QFileInfo csvInfo(csvPath);
    QFileInfo pdfInfo(pdfPath);
    if (!csvInfo.exists() || csvInfo.size() == 0
        || !pdfInfo.exists() || pdfInfo.size() == 0) {
        std::cerr << "Report export files were not created.\n";
        return false;
    }
    QFile csvFile(csvPath);
    if (!csvFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::cerr << "Could not read the exported CSV report.\n";
        return false;
    }
    const QByteArray csvContents = csvFile.readAll();
    if (!csvContents.contains("reports_smoke")
        || !csvContents.contains("Range report refund test")) {
        std::cerr << "CSV export did not include operator or refund details.\n";
        return false;
    }

    std::string previewError;
    if (!verifyPrintPreviewCanBeCancelled(
            QStringLiteral("reportPrintPreviewDialog"),
            [&](std::string& error) {
                return ExportService::printReport(
                    exportDate, exportDate, expectedReport, expectedProducts,
                    expectedOperatorSales, expectedRefunds, nullptr, error);
            }, previewError)) {
        std::cerr << "Range report preview did not open or cancel correctly: "
                  << previewError << "\n";
        return false;
    }

    DailySalesRecord dailyReport{};
    std::vector<ProductSalesRecord> dailyProducts;
    std::vector<OperatorSalesRecord> dailyOperators;
    if (!controller.dailySales(exportDate, dailyReport, dailyProducts, exportError)
        || !controller.dailyOperatorSales(
            exportDate, dailyOperators, exportError)) {
        std::cerr << "Could not prepare daily report preview data: "
                  << exportError << "\n";
        return false;
    }
    if (!verifyPrintPreviewCanBeCancelled(
            QStringLiteral("reportPrintPreviewDialog"),
            [&](std::string& error) {
                return ExportService::printReport(
                    exportDate, dailyReport, dailyProducts,
                    dailyOperators, nullptr, error);
            }, previewError)) {
        std::cerr << "Daily report preview did not open or cancel correctly: "
                  << previewError << "\n";
        return false;
    }

    SalesReportRecord longReport{};
    std::vector<ProductSalesRecord> longProducts;
    std::vector<OperatorSalesRecord> longOperators;
    std::vector<TransactionRefundRecord> longRefunds;
    for (int index = 0; index < 60; ++index) {
        ProductSalesRecord product{};
        product.productId = "P" + std::to_string(index + 1);
        product.productName = "Report pagination product " + std::to_string(index + 1);
        product.quantitySold = index + 1;
        product.salesCents = (index + 1) * 6500;
        longProducts.push_back(product);

        TransactionRefundRecord refund{};
        refund.refundId = index + 1;
        refund.transactionId = index + 1;
        refund.refundDate = exportDate;
        refund.refundType = "refund";
        refund.reason = "Pagination fixture " + std::to_string(index + 1);
        refund.refundCents = (index + 1) * 6500;
        refund.operatorName = "reports_smoke";
        longRefunds.push_back(refund);
    }
    for (int index = 0; index < 12; ++index) {
        OperatorSalesRecord operatorRecord{};
        operatorRecord.operatorName = "report_operator_" + std::to_string(index + 1);
        operatorRecord.transactionCount = index + 1;
        operatorRecord.salesCents = (index + 1) * 10000;
        longOperators.push_back(operatorRecord);
    }
    if (!verifyPrintPreviewCanBeCancelled(
            QStringLiteral("reportPrintPreviewDialog"),
            [&](std::string& error) {
                return ExportService::printReport(
                    exportDate, exportDate, longReport, longProducts,
                    longOperators, longRefunds, nullptr, error);
            }, previewError, 2)) {
        std::cerr << "Long report preview did not paginate across multiple pages: "
                  << previewError << "\n";
        return false;
    }

    const QString receiptPath = exportDirectory + "/phase4-historical-receipt.pdf";
    std::vector<TransactionItemRecord> historicalItems;
    if (!controller.transactionItems(first.transactionId, historicalItems, exportError)) {
        std::cerr << "Could not load historical receipt items: " << exportError << "\n";
        return false;
    }
    if (!ExportService::exportReceiptPdf(receiptPath.toStdString(),
                                         first, historicalItems, exportError)) {
        std::cerr << "Receipt PDF export failed: " << exportError << "\n";
        return false;
    }
    QFileInfo receiptInfo(receiptPath);
    if (!receiptInfo.exists() || receiptInfo.size() == 0) {
        std::cerr << "Historical receipt PDF was not created.\n";
        return false;
    }

    if (!verifyPrintPreviewCanBeCancelled(
            QStringLiteral("receiptPrintPreviewDialog"),
            [&](std::string& error) {
                return ExportService::printReceipt(
                    first, historicalItems, nullptr, error);
            }, previewError)) {
        std::cerr << "Historical receipt printing did not open a cancellable preview.\n";
        return false;
    }

    std::cout << "Transaction history, snapshot receipts, and daily reports passed.\n";
    return true;
}
}

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QString fontError;
    if (!GuiApplicationFont::load(fontError)) {
        std::cerr << fontError.toStdString() << "\n";
        return 1;
    }
    if (argc > 1) {
        QTemporaryDir temporaryDirectory;
        if (!temporaryDirectory.isValid()) {
            std::cerr << "Could not create a temporary database directory.\n";
            return 1;
        }
        DatabaseManager sourceDatabase;
        std::string error;
        if (!sourceDatabase.open(argv[1])) {
            std::cerr << "Could not open database for read-only smoke copy: "
                      << sourceDatabase.getLastError() << "\n";
            return 1;
        }
        const std::string copiedDatabasePath =
            temporaryDirectory.filePath("database-smoke-copy.db").toStdString();
        if (!sourceDatabase.backupDatabase(copiedDatabasePath, error)) {
            std::cerr << "Could not create the disposable database copy: "
                      << error << "\n";
            return 1;
        }
        sourceDatabase.close();
        return verifyProductionDatabase(copiedDatabasePath) ? 0 : 1;
    }

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        std::cerr << "Could not create temporary directory.\n";
        return 1;
    }
    const std::string databasePath =
        temporaryDirectory.filePath("transactions-reports-test.db").toStdString();
    return verifyTemporaryDatabase(databasePath) ? 0 : 1;
}
