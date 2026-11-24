#include "ExportService.h"

#include "DisplayFormatting.h"

#include <QPrinter>
#include <QPagedPaintDevice>
#include <QPdfWriter>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QTextDocument>

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

static QString escapeHtml(const QString& value) {
    QString escaped = value;
    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    escaped.replace("\"", "&quot;");
    return escaped;
}

static QString csvField(const QString& value) {
    QString escaped = value;
    escaped.replace("\"", "\"\"");
    if (escaped.contains(',') || escaped.contains('"')
        || escaped.contains('\n') || escaped.contains('\r'))
        return "\"" + escaped + "\"";
    return escaped;
}

static QString reportHtml(const std::string& date,
                          const DailySalesRecord& report,
                          const std::vector<ProductSalesRecord>& products,
                          const std::vector<OperatorSalesRecord>& operatorSales) {
    QString html;
    html += "<html><head><style>"
            "body{font-family:Arial,sans-serif;color:#59434A;padding:24px}"
            "h1{color:#59434A;border-bottom:2px solid #F6C9D5;padding-bottom:10px}"
            ".summary{margin:18px 0}.summary td{padding:8px 18px 8px 0}"
            "table{border-collapse:collapse;width:100%;margin-top:12px}"
            "th{background:#F6C9D5;color:#59434A;text-align:left;padding:9px;border:1px solid #E3BFC8}"
            "td{padding:8px;border:1px solid #E3BFC8}"
            "</style></head><body>";
    html += "<h1>Crombi Cookies - Daily Sales Report</h1>";
    html += "<p><b>Report date:</b> " + escapeHtml(QString::fromStdString(date)) + "</p>";
    html += "<table class=\"summary\"><tr>";
    html += "<td><b>Completed transactions</b><br>" +
            QString::number(report.transactionCount) + "</td>";
    html += "<td><b>Products sold</b><br>" +
            QString::number(report.productsSold) + "</td>";
    html += "<td><b>Gross sales</b><br>" +
            escapeHtml(guiFormatMoney(report.grossSalesCents)) + "</td>";
    html += "</tr></table>";
    html += "<table><tr><th>Product ID</th><th>Product snapshot</th>"
            "<th>Quantity sold</th><th>Sales</th></tr>";
    for (const auto& product : products) {
        html += "<tr>";
        html += "<td>" + escapeHtml(QString::fromStdString(product.productId)) + "</td>";
        html += "<td>" + escapeHtml(QString::fromStdString(product.productName)) + "</td>";
        html += "<td>" + QString::number(product.quantitySold) + "</td>";
        html += "<td>" + escapeHtml(guiFormatMoney(product.salesCents)) + "</td>";
        html += "</tr>";
    }
    html += "</table><h2>Sales by operator</h2>";
    html += "<table><tr><th>Operator</th><th>Transactions</th><th>Sales</th></tr>";
    for (const auto& operatorRecord : operatorSales) {
        html += "<tr><td>" + escapeHtml(
                    QString::fromStdString(operatorRecord.operatorName)) + "</td>";
        html += "<td>" + QString::number(operatorRecord.transactionCount) + "</td>";
        html += "<td>" + escapeHtml(guiFormatMoney(operatorRecord.salesCents)) + "</td></tr>";
    }
    html += "</table></body></html>";
    return html;
}

static QString rangeReportHtml(
    const std::string& startDate,
    const std::string& endDate,
    const SalesReportRecord& report,
    const std::vector<ProductSalesRecord>& products,
    const std::vector<OperatorSalesRecord>& operatorSales,
    const std::vector<TransactionRefundRecord>& refunds) {
    QString html;
    html += "<html><head><style>"
            "body{font-family:Arial,sans-serif;color:#59434A;padding:24px}"
            "h1{color:#59434A;border-bottom:2px solid #F6C9D5;padding-bottom:10px}"
            "h2{margin-top:24px}"
            "table{border-collapse:collapse;width:100%;margin-top:12px}"
            "th{background:#F6C9D5;color:#59434A;text-align:left;padding:9px;border:1px solid #E3BFC8}"
            "td{padding:8px;border:1px solid #E3BFC8}"
            ".note{color:#76636A;font-size:10pt}"
            "</style></head><body>";
    html += "<h1>Crombi Cookies - Sales Report</h1>";
    html += "<p><b>Date range:</b> " + escapeHtml(
        QString::fromStdString(startDate + " to " + endDate)) + "</p>";
    html += "<p class=\"note\">Sales are grouped by transaction date; refunds and voids "
            "are grouped by processing date.</p>";
    html += "<table><tr><th>Measure</th><th>Amount / count</th></tr>";
    html += "<tr><td>Sales transactions</td><td>"
        + QString::number(report.transactionCount) + "</td></tr>";
    html += "<tr><td>Products sold</td><td>"
        + QString::number(report.productsSold) + "</td></tr>";
    html += "<tr><td>Gross sales</td><td>"
        + escapeHtml(guiFormatMoney(report.grossSalesCents)) + "</td></tr>";
    html += "<tr><td>Refunds processed</td><td>"
        + QString::number(report.refundCount) + " / "
        + escapeHtml(guiFormatMoney(report.refundedCents)) + "</td></tr>";
    html += "<tr><td>Net sales</td><td>"
        + escapeHtml(guiFormatMoney(report.netSalesCents)) + "</td></tr>";
    html += "<tr><td>Voids</td><td>"
        + QString::number(report.voidCount) + " / "
        + escapeHtml(guiFormatMoney(report.voidedCents)) + "</td></tr></table>";
    html += "<h2>Product sales</h2>"
            "<table><tr><th>Product ID</th><th>Product snapshot</th>"
            "<th>Quantity sold</th><th>Gross sales</th></tr>";
    for (const auto& product : products) {
        html += "<tr><td>" + escapeHtml(
            QString::fromStdString(product.productId)) + "</td>";
        html += "<td>" + escapeHtml(
            QString::fromStdString(product.productName)) + "</td>";
        html += "<td>" + QString::number(product.quantitySold) + "</td>";
        html += "<td>" + escapeHtml(guiFormatMoney(product.salesCents)) + "</td></tr>";
    }
    html += "</table><h2>Sales by operator</h2>"
            "<table><tr><th>Operator</th><th>Transactions</th><th>Gross sales</th></tr>";
    for (const auto& operatorRecord : operatorSales) {
        html += "<tr><td>" + escapeHtml(
            QString::fromStdString(operatorRecord.operatorName)) + "</td>";
        html += "<td>" + QString::number(operatorRecord.transactionCount) + "</td>";
        html += "<td>" + escapeHtml(guiFormatMoney(operatorRecord.salesCents)) + "</td></tr>";
    }
    html += "</table><h2>Refund and void activity</h2>"
            "<table><tr><th>Date</th><th>Transaction</th><th>Type</th>"
            "<th>Reason</th><th>Amount</th><th>Operator</th></tr>";
    for (const auto& refund : refunds) {
        html += "<tr><td>" + escapeHtml(
            guiFormatTimestamp(refund.refundDate)) + "</td>";
        html += "<td>" + QString::number(refund.transactionId) + "</td>";
        html += "<td>" + escapeHtml(
            QString::fromStdString(refund.refundType)) + "</td>";
        html += "<td>" + escapeHtml(
            QString::fromStdString(refund.reason)) + "</td>";
        html += "<td>" + escapeHtml(guiFormatMoney(refund.refundCents)) + "</td>";
        html += "<td>" + escapeHtml(
            QString::fromStdString(refund.operatorName)) + "</td></tr>";
    }
    html += "</table></body></html>";
    return html;
}

static QString receiptHtml(const TransactionRecord& transaction,
                           const std::vector<TransactionItemRecord>& items) {
    QString html;
    html += "<html><head><style>"
            "body{font-family:Arial,sans-serif;color:#59434A;width:480px;margin:20px auto}"
            "h1{text-align:center;margin-bottom:4px}.meta{text-align:center}"
            "table{border-collapse:collapse;width:100%;margin-top:18px}"
            "th{border-bottom:2px solid #59434A;text-align:left;padding:7px}"
            "td{border-bottom:1px solid #E3BFC8;padding:7px}"
            ".totals{text-align:right;margin-top:16px;font-size:14px}"
            "</style></head><body>";
    html += "<h1>Crombi Cookies</h1>";
    html += "<div class=\"meta\">Transaction #" +
            QString::number(transaction.transactionId) + "<br>" +
            escapeHtml(guiFormatTimestamp(transaction.transactionDate)) + "</div>";
    html += "<div class=\"meta\">Status: " +
            escapeHtml(QString::fromStdString(transaction.status)) +
            "<br>Operator: " +
            escapeHtml(QString::fromStdString(transaction.operatorName)) +
            "<br>Refunded: " +
            escapeHtml(guiFormatMoney(transaction.refundedCents)) + "</div>";
    html += "<table><tr><th>Item</th><th>Qty</th><th>Unit</th><th>Subtotal</th></tr>";
    for (const auto& item : items) {
        html += "<tr>";
        html += "<td>" + escapeHtml(QString::fromStdString(item.productName)) + "</td>";
        html += "<td>" + QString::number(item.quantity) + "</td>";
        html += "<td>" + escapeHtml(guiFormatMoney(item.unitPriceCents)) + "</td>";
        html += "<td>" + escapeHtml(guiFormatMoney(item.subtotalCents)) + "</td>";
        html += "</tr>";
    }
    html += "</table><div class=\"totals\">";
    html += "<b>Total:</b> " + escapeHtml(guiFormatMoney(transaction.orderTotalCents)) + "<br>";
    html += "<b>Cash:</b> " + escapeHtml(guiFormatMoney(transaction.cashReceivedCents)) + "<br>";
    html += "<b>Change:</b> " + escapeHtml(guiFormatMoney(transaction.changeCents));
    html += "</div></body></html>";
    return html;
}

bool ExportService::exportReportCsv(const std::string& path,
                                    const std::string& date,
                                    const DailySalesRecord& report,
                                    const std::vector<ProductSalesRecord>& products,
                                    const std::vector<OperatorSalesRecord>& operatorSales,
                                    std::string& error) {
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        error = "Unable to create CSV file.";
        return false;
    }
    QTextStream stream(&file);
    stream << "Crombi Cookies Daily Sales Report\n";
    stream << "Report date," << csvField(QString::fromStdString(date)) << "\n";
    stream << "Completed transactions," << report.transactionCount << "\n";
    stream << "Products sold," << report.productsSold << "\n";
    stream << "Gross sales," << csvField(guiFormatMoney(report.grossSalesCents)) << "\n";
    stream << "\nProduct ID,Product snapshot,Quantity sold,Sales\n";
    for (const auto& product : products) {
        stream << csvField(QString::fromStdString(product.productId)) << ","
               << csvField(QString::fromStdString(product.productName)) << ","
               << product.quantitySold << ","
               << csvField(guiFormatMoney(product.salesCents)) << "\n";
    }
    stream << "\nOperator,Transactions,Sales\n";
    for (const auto& operatorRecord : operatorSales) {
        stream << csvField(QString::fromStdString(operatorRecord.operatorName)) << ","
               << operatorRecord.transactionCount << ","
               << csvField(guiFormatMoney(operatorRecord.salesCents)) << "\n";
    }
    file.close();
    return true;
}

bool ExportService::exportReportPdf(const std::string& path,
                                    const std::string& date,
                                    const DailySalesRecord& report,
                                    const std::vector<ProductSalesRecord>& products,
                                    const std::vector<OperatorSalesRecord>& operatorSales,
                                    std::string& error) {
    QTextDocument document;
    document.setHtml(reportHtml(date, report, products, operatorSales));
    QPdfWriter writer(QString::fromStdString(path));
    writer.setResolution(300);
    document.print(&writer);
    QFileInfo info(QString::fromStdString(path));
    if (!info.exists() || info.size() == 0) {
        error = "Unable to create PDF report.";
        return false;
    }
    return true;
}

bool ExportService::printReport(const std::string& date,
                                const DailySalesRecord& report,
                                const std::vector<ProductSalesRecord>& products,
const std::vector<OperatorSalesRecord>& operatorSales,
                                QWidget* parent,
                                std::string& error) {
    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName("Crombi-Cookies-Daily-Sales");
    QPrintDialog dialog(&printer, parent);
    if (dialog.exec() != QDialog::Accepted) {
        error = "Printing cancelled.";
        return false;
    }
    QTextDocument document;
    document.setHtml(reportHtml(date, report, products, operatorSales));
    document.print(&printer);
    return true;
}

bool ExportService::exportReportCsv(
    const std::string& path,
    const std::string& startDate,
    const std::string& endDate,
    const SalesReportRecord& report,
    const std::vector<ProductSalesRecord>& products,
    const std::vector<OperatorSalesRecord>& operatorSales,
    const std::vector<TransactionRefundRecord>& refunds,
    std::string& error) {
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        error = "Unable to create CSV file.";
        return false;
    }
    QTextStream stream(&file);
    stream << "Crombi Cookies Sales Report\n";
    stream << "Start date," << csvField(QString::fromStdString(startDate)) << "\n";
    stream << "End date," << csvField(QString::fromStdString(endDate)) << "\n";
    stream << "Sales transactions," << report.transactionCount << "\n";
    stream << "Products sold," << report.productsSold << "\n";
    stream << "Gross sales," << csvField(guiFormatMoney(report.grossSalesCents)) << "\n";
    stream << "Refunds processed," << report.refundCount << "\n";
    stream << "Refund total," << csvField(guiFormatMoney(report.refundedCents)) << "\n";
    stream << "Net sales," << csvField(guiFormatMoney(report.netSalesCents)) << "\n";
    stream << "Voids," << report.voidCount << "\n";
    stream << "Void total," << csvField(guiFormatMoney(report.voidedCents)) << "\n";
    stream << "\nProduct ID,Product snapshot,Quantity sold,Gross sales\n";
    for (const auto& product : products) {
        stream << csvField(QString::fromStdString(product.productId)) << ","
               << csvField(QString::fromStdString(product.productName)) << ","
               << product.quantitySold << ","
               << csvField(guiFormatMoney(product.salesCents)) << "\n";
    }
    stream << "\nOperator,Transactions,Gross sales\n";
    for (const auto& operatorRecord : operatorSales) {
        stream << csvField(QString::fromStdString(operatorRecord.operatorName)) << ","
               << operatorRecord.transactionCount << ","
               << csvField(guiFormatMoney(operatorRecord.salesCents)) << "\n";
    }
    stream << "\nRefund ID,Transaction ID,Date,Type,Reason,Amount,Operator\n";
    for (const auto& refund : refunds) {
        stream << refund.refundId << ","
               << refund.transactionId << ","
               << csvField(QString::fromStdString(refund.refundDate)) << ","
               << csvField(QString::fromStdString(refund.refundType)) << ","
               << csvField(QString::fromStdString(refund.reason)) << ","
               << csvField(guiFormatMoney(refund.refundCents)) << ","
               << csvField(QString::fromStdString(refund.operatorName)) << "\n";
    }
    file.close();
    return true;
}

bool ExportService::exportReportPdf(
    const std::string& path,
    const std::string& startDate,
    const std::string& endDate,
    const SalesReportRecord& report,
    const std::vector<ProductSalesRecord>& products,
    const std::vector<OperatorSalesRecord>& operatorSales,
    const std::vector<TransactionRefundRecord>& refunds,
    std::string& error) {
    QTextDocument document;
    document.setHtml(rangeReportHtml(startDate, endDate, report, products,
                                     operatorSales, refunds));
    QPdfWriter writer(QString::fromStdString(path));
    writer.setResolution(300);
    document.print(&writer);
    QFileInfo info(QString::fromStdString(path));
    if (!info.exists() || info.size() == 0) {
        error = "Unable to create PDF report.";
        return false;
    }
    return true;
}

bool ExportService::printReport(
    const std::string& startDate,
    const std::string& endDate,
    const SalesReportRecord& report,
    const std::vector<ProductSalesRecord>& products,
    const std::vector<OperatorSalesRecord>& operatorSales,
    const std::vector<TransactionRefundRecord>& refunds,
    QWidget* parent,
    std::string& error) {
    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName("Crombi-Cookies-Sales-Report");
    QPrintDialog dialog(&printer, parent);
    if (dialog.exec() != QDialog::Accepted) {
        error = "Printing cancelled.";
        return false;
    }
    QTextDocument document;
    document.setHtml(rangeReportHtml(startDate, endDate, report, products,
                                     operatorSales, refunds));
    document.print(&printer);
    return true;
}

bool ExportService::exportReceiptPdf(const std::string& path,
                                     const TransactionRecord& transaction,
                                     const std::vector<TransactionItemRecord>& items,
                                     std::string& error) {
    QTextDocument document;
    document.setHtml(receiptHtml(transaction, items));
    QPdfWriter writer(QString::fromStdString(path));
    writer.setResolution(300);
    document.print(&writer);
    QFileInfo info(QString::fromStdString(path));
    if (!info.exists() || info.size() == 0) {
        error = "Unable to create receipt PDF.";
        return false;
    }
    return true;
}

bool ExportService::printReceipt(const TransactionRecord& transaction,
                                 const std::vector<TransactionItemRecord>& items,
                                 QWidget* parent,
                                 std::string& error) {
    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(QString("Crombi-Receipt-%1").arg(transaction.transactionId));
    QPrintPreviewDialog dialog(&printer, parent);
    dialog.setObjectName("receiptPrintPreviewDialog");
    dialog.setWindowTitle(
        QString("Receipt preview - Transaction #%1")
            .arg(transaction.transactionId));
    QTextDocument document;
    document.setHtml(receiptHtml(transaction, items));
    QObject::connect(&dialog, &QPrintPreviewDialog::paintRequested,
                     &dialog, [&document](QPrinter* previewPrinter) {
        document.print(previewPrinter);
    });
    if (dialog.exec() != QDialog::Accepted) {
        error = "Printing cancelled.";
        return false;
    }
    return true;
}
