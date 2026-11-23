#include "ReportsPage.h"

#include "DisplayFormatting.h"
#include "ExportService.h"

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QFileDialog>

namespace {
QFrame* makeReportCard(QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setObjectName("contentCard");
    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(18);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(89, 67, 74, 24));
    card->setGraphicsEffect(shadow);
    return card;
}

QLabel* makeReportTitle(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setStyleSheet("font-size: 13pt; font-weight: 750; color: #59434A;");
    return label;
}

QFrame* makeMetricCard(const QString& titleText, QLabel*& valueLabel, const QString& objectName,
                       QWidget* parent) {
    auto* card = makeReportCard(parent);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(6);
    auto* title = new QLabel(titleText, card);
    title->setStyleSheet("color: #59434A; font-weight: 700;");
    valueLabel = new QLabel("—", card);
    valueLabel->setObjectName(objectName);
    valueLabel->setStyleSheet("font-size: 20pt; font-weight: 800; color: #59434A;");
    layout->addWidget(title);
    layout->addWidget(valueLabel);
    return card;
}
}

ReportsPage::ReportsPage(PosController& controller, QWidget* parent)
    : QWidget(parent),
      controller_(controller),
      periodSelector_(nullptr),
      reportDate_(nullptr),
      customStartDate_(nullptr),
      customEndDate_(nullptr),
      reportDateLabel_(nullptr),
      customStartLabel_(nullptr),
      customEndLabel_(nullptr),
      reportRangeLabel_(nullptr),
      transactionCount_(nullptr),
      productsSold_(nullptr),
      grossSales_(nullptr),
      refundTotal_(nullptr),
      netSales_(nullptr),
      productSalesTable_(nullptr),
      operatorSalesTable_(nullptr),
      refundsTable_(nullptr),
      messageLabel_(nullptr),
      csvButton_(nullptr),
      pdfButton_(nullptr),
      printButton_(nullptr),
      currentReport_{} {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* dateCard = makeReportCard(this);
    auto* dateLayout = new QVBoxLayout(dateCard);
    dateLayout->setContentsMargins(18, 12, 18, 12);
    dateLayout->setSpacing(8);
    auto* filterLayout = new QHBoxLayout();
    filterLayout->addWidget(makeReportTitle("Sales report", dateCard));
    filterLayout->addStretch();
    filterLayout->addWidget(new QLabel("Period:", dateCard));
    periodSelector_ = new QComboBox(dateCard);
    periodSelector_->setObjectName("reportPeriodInput");
    periodSelector_->addItems(
        {"Daily", "Weekly", "Monthly", "Custom date range"});
    filterLayout->addWidget(periodSelector_);
    reportDateLabel_ = new QLabel("Date in period:", dateCard);
    filterLayout->addWidget(reportDateLabel_);
    reportDate_ = new QDateEdit(QDate::currentDate(), dateCard);
    reportDate_->setObjectName("reportDateInput");
    reportDate_->setCalendarPopup(true);
    reportDate_->setDisplayFormat("yyyy-MM-dd");
    customStartLabel_ = new QLabel("From:", dateCard);
    customStartDate_ = new QDateEdit(QDate::currentDate(), dateCard);
    customStartDate_->setObjectName("reportStartDateInput");
    customStartDate_->setCalendarPopup(true);
    customStartDate_->setDisplayFormat("yyyy-MM-dd");
    customEndLabel_ = new QLabel("To:", dateCard);
    customEndDate_ = new QDateEdit(QDate::currentDate(), dateCard);
    customEndDate_->setObjectName("reportEndDateInput");
    customEndDate_->setCalendarPopup(true);
    customEndDate_->setDisplayFormat("yyyy-MM-dd");
    customStartLabel_->hide();
    customStartDate_->hide();
    customEndLabel_->hide();
    customEndDate_->hide();
    filterLayout->addWidget(reportDate_);
    dateLayout->addLayout(filterLayout);
    auto* actionLayout = new QHBoxLayout();
    actionLayout->addWidget(customStartLabel_);
    actionLayout->addWidget(customStartDate_);
    actionLayout->addWidget(customEndLabel_);
    actionLayout->addWidget(customEndDate_);
    actionLayout->addStretch();
    auto* generateButton = new QPushButton("Generate", dateCard);
    generateButton->setObjectName("generateReportButton");
    actionLayout->addWidget(generateButton);
    csvButton_ = new QPushButton("Export CSV", dateCard);
    csvButton_->setObjectName("exportReportCsvButton");
    pdfButton_ = new QPushButton("Export PDF", dateCard);
    pdfButton_->setObjectName("exportReportPdfButton");
    printButton_ = new QPushButton("Print report", dateCard);
    printButton_->setObjectName("printReportButton");
    actionLayout->addWidget(csvButton_);
    actionLayout->addWidget(pdfButton_);
    actionLayout->addWidget(printButton_);
    dateLayout->addLayout(actionLayout);
    layout->addWidget(dateCard);
    reportRangeLabel_ = new QLabel(this);
    reportRangeLabel_->setObjectName("reportRangeLabel");
    reportRangeLabel_->setStyleSheet("color: #76636A; font-weight: 600;");
    layout->addWidget(reportRangeLabel_);

    auto* metrics = new QHBoxLayout();
    metrics->setSpacing(12);
    metrics->addWidget(makeMetricCard("Sales transactions", transactionCount_,
                                      "reportTransactionCount", this), 1);
    metrics->addWidget(makeMetricCard("Products sold", productsSold_,
                                      "reportProductsSold", this), 1);
    metrics->addWidget(makeMetricCard("Gross sales", grossSales_,
                                      "reportGrossSales", this), 1);
    metrics->addWidget(makeMetricCard("Refunds", refundTotal_,
                                      "reportRefundTotal", this), 1);
    metrics->addWidget(makeMetricCard("Net sales", netSales_,
                                      "reportNetSales", this), 1);
    layout->addLayout(metrics);

    auto* productsCard = makeReportCard(this);
    auto* productsLayout = new QVBoxLayout(productsCard);
    productsLayout->setContentsMargins(18, 15, 18, 15);
    productsLayout->setSpacing(9);
    productsLayout->addWidget(makeReportTitle("Product sales summary", productsCard));
    productSalesTable_ = new QTableWidget(productsCard);
    productSalesTable_->setObjectName("productSalesTable");
    productSalesTable_->setColumnCount(4);
    productSalesTable_->setHorizontalHeaderLabels(
        {"Product ID", "Product snapshot", "Quantity sold", "Sales"});
    productSalesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    productSalesTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    productSalesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    productSalesTable_->setAlternatingRowColors(true);
    productSalesTable_->verticalHeader()->hide();
    productSalesTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    productSalesTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    productSalesTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    productSalesTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    productsLayout->addWidget(productSalesTable_, 1);
    layout->addWidget(productsCard, 2);

    auto* operatorsCard = makeReportCard(this);
    auto* operatorsLayout = new QVBoxLayout(operatorsCard);
    operatorsLayout->setContentsMargins(18, 15, 18, 15);
    operatorsLayout->setSpacing(9);
    operatorsLayout->addWidget(makeReportTitle("Sales by operator", operatorsCard));
    operatorSalesTable_ = new QTableWidget(operatorsCard);
    operatorSalesTable_->setObjectName("operatorSalesTable");
    operatorSalesTable_->setColumnCount(3);
    operatorSalesTable_->setHorizontalHeaderLabels(
        {"Operator", "Transactions", "Sales"});
    operatorSalesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    operatorSalesTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    operatorSalesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    operatorSalesTable_->setAlternatingRowColors(true);
    operatorSalesTable_->verticalHeader()->hide();
    operatorSalesTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    operatorSalesTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    operatorSalesTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    operatorsLayout->addWidget(operatorSalesTable_, 1);
    layout->addWidget(operatorsCard, 1);

    auto* refundsCard = makeReportCard(this);
    auto* refundsLayout = new QVBoxLayout(refundsCard);
    refundsLayout->setContentsMargins(18, 15, 18, 15);
    refundsLayout->setSpacing(9);
    refundsLayout->addWidget(makeReportTitle("Refund and void activity", refundsCard));
    refundsTable_ = new QTableWidget(refundsCard);
    refundsTable_->setObjectName("reportRefundsTable");
    refundsTable_->setColumnCount(7);
    refundsTable_->setHorizontalHeaderLabels(
        {"Refund ID", "Transaction", "Date", "Type", "Reason", "Amount", "Operator"});
    refundsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    refundsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    refundsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    refundsTable_->setAlternatingRowColors(true);
    refundsTable_->verticalHeader()->hide();
    refundsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    refundsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    refundsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    refundsTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    refundsTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    refundsTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    refundsTable_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    refundsLayout->addWidget(refundsTable_, 1);
    layout->addWidget(refundsCard, 1);

    messageLabel_ = new QLabel(this);
    messageLabel_->setObjectName("reportsMessage");
    messageLabel_->setWordWrap(true);
    messageLabel_->setMinimumHeight(24);
    layout->addWidget(messageLabel_);

    connect(generateButton, &QPushButton::clicked,
            this, [this]() { generateReport(); });
    connect(periodSelector_, &QComboBox::currentTextChanged,
            this, [this]() { updateDateControls(); });
    connect(reportDate_, &QDateEdit::dateChanged,
            this, [this]() { generateReport(); });
    connect(customStartDate_, &QDateEdit::dateChanged,
            this, [this]() { generateReport(); });
    connect(customEndDate_, &QDateEdit::dateChanged,
            this, [this]() { generateReport(); });
    connect(csvButton_, &QPushButton::clicked,
            this, [this]() { exportCsv(); });
    connect(pdfButton_, &QPushButton::clicked,
            this, [this]() { exportPdf(); });
    connect(printButton_, &QPushButton::clicked,
            this, [this]() { printReport(); });
    updateDateControls();
}

bool ReportsPage::selectedDateRange(std::string& startDate,
                                    std::string& endDate) const {
    const QString period = periodSelector_->currentText();
    if (period == "Custom date range") {
        startDate = customStartDate_->date().toString("yyyy-MM-dd").toStdString();
        endDate = customEndDate_->date().toString("yyyy-MM-dd").toStdString();
        return startDate <= endDate;
    }

    QDate start = reportDate_->date();
    QDate end = start;
    if (period == "Weekly") {
        start = start.addDays(1 - start.dayOfWeek());
        end = start.addDays(6);
    } else if (period == "Monthly") {
        start = QDate(start.year(), start.month(), 1);
        end = start.addMonths(1).addDays(-1);
    }
    startDate = start.toString("yyyy-MM-dd").toStdString();
    endDate = end.toString("yyyy-MM-dd").toStdString();
    return true;
}

void ReportsPage::updateDateControls() {
    const bool customRange = periodSelector_->currentText() == "Custom date range";
    reportDateLabel_->setVisible(!customRange);
    reportDate_->setVisible(!customRange);
    customStartLabel_->setVisible(customRange);
    customStartDate_->setVisible(customRange);
    customEndLabel_->setVisible(customRange);
    customEndDate_->setVisible(customRange);
    reportDateLabel_->setText(periodSelector_->currentText() == "Daily"
                                  ? QStringLiteral("Report date:")
                                  : QStringLiteral("Date in period:"));
    generateReport();
}

void ReportsPage::generateReport() {
    clearReport();
    std::string startDate;
    std::string endDate;
    if (!selectedDateRange(startDate, endDate)) {
        currentStartDate_.clear();
        currentEndDate_.clear();
        reportRangeLabel_->setText("Choose a start date on or before the end date.");
        setMessage("The custom report start date must not be after its end date.", true);
        return;
    }
    currentStartDate_ = startDate;
    currentEndDate_ = endDate;
    reportRangeLabel_->setText(
        QStringLiteral("Sales by transaction date; refunds and voids by processing date: "
                       "%1 to %2")
            .arg(QString::fromStdString(startDate),
                 QString::fromStdString(endDate)));

    SalesReportRecord report{};
    std::vector<ProductSalesRecord> products;
    std::vector<OperatorSalesRecord> operatorSales;
    std::vector<TransactionRefundRecord> refunds;
    std::string error;
    if (!controller_.salesReport(startDate, endDate, report, products,
                                 operatorSales, refunds, error)) {
        setMessage(QString::fromStdString(error), true);
        return;
    }
    currentReport_ = report;
    currentProducts_ = products;
    currentOperatorSales_ = operatorSales;
    currentRefunds_ = refunds;
    csvButton_->setEnabled(true);
    pdfButton_->setEnabled(true);
    printButton_->setEnabled(true);

    transactionCount_->setText(QString::number(report.transactionCount));
    productsSold_->setText(QString::number(report.productsSold));
    grossSales_->setText(guiFormatMoney(report.grossSalesCents));
    refundTotal_->setText(QStringLiteral("%1 (%2)")
        .arg(guiFormatMoney(report.refundedCents))
        .arg(report.refundCount));
    netSales_->setText(guiFormatMoney(report.netSalesCents));
    productSalesTable_->setRowCount(static_cast<int>(products.size()));
    for (int row = 0; row < static_cast<int>(products.size()); ++row) {
        const auto& product = products[static_cast<std::size_t>(row)];
        productSalesTable_->setItem(row, 0,
            new QTableWidgetItem(QString::fromStdString(product.productId)));
        productSalesTable_->setItem(row, 1,
            new QTableWidgetItem(QString::fromStdString(product.productName)));
        productSalesTable_->setItem(row, 2,
            new QTableWidgetItem(QString::number(product.quantitySold)));
        productSalesTable_->setItem(row, 3,
            new QTableWidgetItem(guiFormatMoney(product.salesCents)));
    }
    operatorSalesTable_->setRowCount(static_cast<int>(operatorSales.size()));
    for (int row = 0; row < static_cast<int>(operatorSales.size()); ++row) {
        const auto& operatorRecord = operatorSales[static_cast<std::size_t>(row)];
        operatorSalesTable_->setItem(row, 0, new QTableWidgetItem(
            QString::fromStdString(operatorRecord.operatorName)));
        operatorSalesTable_->setItem(row, 1, new QTableWidgetItem(
            QString::number(operatorRecord.transactionCount)));
        operatorSalesTable_->setItem(row, 2, new QTableWidgetItem(
            guiFormatMoney(operatorRecord.salesCents)));
    }
    refundsTable_->setRowCount(static_cast<int>(refunds.size()));
    for (int row = 0; row < static_cast<int>(refunds.size()); ++row) {
        const auto& refund = refunds[static_cast<std::size_t>(row)];
        refundsTable_->setItem(row, 0,
            new QTableWidgetItem(QString::number(refund.refundId)));
        refundsTable_->setItem(row, 1,
            new QTableWidgetItem(QString::number(refund.transactionId)));
        refundsTable_->setItem(row, 2,
            new QTableWidgetItem(guiFormatTimestamp(refund.refundDate)));
        refundsTable_->setItem(row, 3,
            new QTableWidgetItem(QString::fromStdString(refund.refundType)));
        refundsTable_->setItem(row, 4,
            new QTableWidgetItem(QString::fromStdString(refund.reason)));
        refundsTable_->setItem(row, 5,
            new QTableWidgetItem(guiFormatMoney(refund.refundCents)));
        refundsTable_->setItem(row, 6,
            new QTableWidgetItem(QString::fromStdString(refund.operatorName)));
    }

    if (report.transactionCount == 0 && report.refundCount == 0
        && report.voidCount == 0) {
        setMessage(QStringLiteral("No sales, refunds, or voids for %1 to %2.")
                       .arg(QString::fromStdString(startDate),
                            QString::fromStdString(endDate)),
                   false);
    } else {
        setMessage(QStringLiteral("Report generated for %1 to %2.")
                       .arg(QString::fromStdString(startDate),
                            QString::fromStdString(endDate)),
                   false);
    }
}

void ReportsPage::exportCsv() {
    const QString path = QFileDialog::getSaveFileName(
        this, "Export Sales Report CSV",
        QString("sales-report-%1-to-%2.csv")
            .arg(QString::fromStdString(currentStartDate_),
                 QString::fromStdString(currentEndDate_)),
        "CSV files (*.csv)");
    if (path.isEmpty()) return;
    std::string error;
    if (!ExportService::exportReportCsv(
            path.toStdString(), currentStartDate_, currentEndDate_,
            currentReport_, currentProducts_, currentOperatorSales_,
            currentRefunds_, error))
        setMessage(QString::fromStdString(error), true);
    else
        setMessage("CSV report exported successfully.", false);
}

void ReportsPage::exportPdf() {
    const QString path = QFileDialog::getSaveFileName(
        this, "Export Sales Report PDF",
        QString("sales-report-%1-to-%2.pdf")
            .arg(QString::fromStdString(currentStartDate_),
                 QString::fromStdString(currentEndDate_)),
        "PDF files (*.pdf)");
    if (path.isEmpty()) return;
    std::string error;
    if (!ExportService::exportReportPdf(
            path.toStdString(), currentStartDate_, currentEndDate_,
            currentReport_, currentProducts_, currentOperatorSales_,
            currentRefunds_, error))
        setMessage(QString::fromStdString(error), true);
    else
        setMessage("PDF report exported successfully.", false);
}

void ReportsPage::printReport() {
    std::string error;
    if (!ExportService::printReport(
            currentStartDate_, currentEndDate_, currentReport_,
            currentProducts_, currentOperatorSales_, currentRefunds_,
            this, error))
        setMessage(QString::fromStdString(error),
                   error != "Printing cancelled.");
    else
        setMessage("Report sent to printer.", false);
}

void ReportsPage::setMessage(const QString& message, bool isError) {
    messageLabel_->setText(message);
    messageLabel_->setStyleSheet(isError
        ? "color: #A3485A; font-weight: 600;"
        : "color: #58765D; font-weight: 600;");
}

void ReportsPage::clearReport() {
    currentReport_ = {};
    currentProducts_.clear();
    currentOperatorSales_.clear();
    currentRefunds_.clear();
    transactionCount_->setText("0");
    productsSold_->setText("0");
    grossSales_->setText(guiFormatMoney(0));
    refundTotal_->setText(QStringLiteral("%1 (0)").arg(guiFormatMoney(0)));
    netSales_->setText(guiFormatMoney(0));
    productSalesTable_->setRowCount(0);
    operatorSalesTable_->setRowCount(0);
    refundsTable_->setRowCount(0);
    csvButton_->setEnabled(false);
    pdfButton_->setEnabled(false);
    printButton_->setEnabled(false);
}
