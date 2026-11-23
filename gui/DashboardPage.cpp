#include "DashboardPage.h"

#include "DisplayFormatting.h"

#include <QAbstractItemView>
#include <QColor>
#include <QDate>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
QFrame* makeDashboardCard(QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setObjectName("contentCard");
    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(18);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(89, 67, 74, 28));
    card->setGraphicsEffect(shadow);
    return card;
}

QFrame* makeKpiCard(const QString& title, QLabel*& value,
                    const QString& objectName, QWidget* parent) {
    auto* card = makeDashboardCard(parent);
    card->setObjectName("kpiCard");
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 13, 16, 13);
    layout->setSpacing(5);
    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("kpiTitle");
    titleLabel->setStyleSheet("color: #59434A; font-size: 10pt; font-weight: 700;");
    value = new QLabel("0", card);
    value->setObjectName(objectName);
    value->setStyleSheet("color: #59434A; font-size: 22pt; font-weight: 800;");
    layout->addWidget(titleLabel);
    layout->addWidget(value);
    return card;
}

QLabel* makeDashboardTitle(const QString& text, QWidget* parent) {
    auto* title = new QLabel(text, parent);
    title->setStyleSheet("color: #59434A; font-size: 13pt; font-weight: 750;");
    return title;
}

void styleDashboardTable(QTableWidget* table) {
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->hide();
    table->setMinimumHeight(150);
}
}

DashboardPage::DashboardPage(PosController& controller, QWidget* parent)
    : QWidget(parent),
      controller_(controller),
      salesValue_(nullptr),
      transactionValue_(nullptr),
      productValue_(nullptr),
      lowStockValue_(nullptr),
      recentTable_(nullptr),
      lowStockTable_(nullptr),
      messageLabel_(nullptr) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* kpiRow = new QHBoxLayout();
    kpiRow->setSpacing(12);
    kpiRow->addWidget(makeKpiCard("Today's Sales", salesValue_,
                                  "dashboardSalesValue", this), 1);
    kpiRow->addWidget(makeKpiCard("Transaction Count", transactionValue_,
                                  "dashboardTransactionCount", this), 1);
    kpiRow->addWidget(makeKpiCard("Total Products", productValue_,
                                  "dashboardTotalProducts", this), 1);
    kpiRow->addWidget(makeKpiCard("Low Stock Alerts", lowStockValue_,
                                  "dashboardLowStockCount", this), 1);
    layout->addLayout(kpiRow);

    auto* detailsRow = new QHBoxLayout();
    detailsRow->setSpacing(12);

    auto* recentCard = makeDashboardCard(this);
    auto* recentLayout = new QVBoxLayout(recentCard);
    recentLayout->setContentsMargins(16, 14, 16, 14);
    recentLayout->setSpacing(8);
    auto* recentHeader = new QHBoxLayout();
    recentHeader->addWidget(makeDashboardTitle("Recent Transactions", recentCard));
    recentHeader->addStretch();
    auto* refreshButton = new QPushButton("Refresh", recentCard);
    refreshButton->setObjectName("dashboardRefreshButton");
    recentHeader->addWidget(refreshButton);
    recentLayout->addLayout(recentHeader);
    recentTable_ = new QTableWidget(recentCard);
    recentTable_->setObjectName("dashboardRecentTransactions");
    recentTable_->setColumnCount(4);
    recentTable_->setHorizontalHeaderLabels(
        {"Transaction", "Date", "Total", "Change"});
    styleDashboardTable(recentTable_);
    recentTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    recentTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    recentTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    recentTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    recentLayout->addWidget(recentTable_, 1);
    detailsRow->addWidget(recentCard, 3);

    auto* lowStockCard = makeDashboardCard(this);
    auto* lowStockLayout = new QVBoxLayout(lowStockCard);
    lowStockLayout->setContentsMargins(16, 14, 16, 14);
    lowStockLayout->setSpacing(8);
    lowStockLayout->addWidget(makeDashboardTitle("Low Stock Products", lowStockCard));
    lowStockTable_ = new QTableWidget(lowStockCard);
    lowStockTable_->setObjectName("dashboardLowStockProducts");
    lowStockTable_->setColumnCount(3);
    lowStockTable_->setHorizontalHeaderLabels(
        {"Product ID", "Product", "Stock"});
    styleDashboardTable(lowStockTable_);
    lowStockTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    lowStockTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    lowStockTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    lowStockLayout->addWidget(lowStockTable_, 1);
    detailsRow->addWidget(lowStockCard, 2);

    layout->addLayout(detailsRow, 1);
    messageLabel_ = new QLabel(this);
    messageLabel_->setObjectName("dashboardMessage");
    messageLabel_->setWordWrap(true);
    messageLabel_->setMinimumHeight(24);
    layout->addWidget(messageLabel_);

    connect(refreshButton, &QPushButton::clicked, this, [this]() { refresh(); });
    refresh();
}

void DashboardPage::refresh() {
    DashboardSummary summary{};
    std::vector<TransactionRecord> recent;
    std::vector<LowStockSummary> lowStock;
    std::string error;
    const std::string date =
        QDate::currentDate().toString("yyyy-MM-dd").toStdString();
    if (!controller_.dashboard(date, summary, recent, lowStock, error)) {
        salesValue_->setText("—");
        transactionValue_->setText("—");
        productValue_->setText("—");
        lowStockValue_->setText("—");
        recentTable_->setRowCount(0);
        lowStockTable_->setRowCount(0);
        setMessage(QString::fromStdString(error), true);
        return;
    }

    salesValue_->setText(formatMoney(summary.todaySalesCents));
    transactionValue_->setText(QString::number(summary.transactionCount));
    productValue_->setText(QString::number(summary.totalProducts));
    lowStockValue_->setText(QString::number(summary.lowStockCount));

    recentTable_->setRowCount(static_cast<int>(recent.size()));
    for (int row = 0; row < static_cast<int>(recent.size()); ++row) {
        const auto& transaction = recent[static_cast<std::size_t>(row)];
        recentTable_->setItem(row, 0,
            new QTableWidgetItem(QString::number(transaction.transactionId)));
        recentTable_->setItem(row, 1,
            new QTableWidgetItem(guiFormatTimestamp(transaction.transactionDate)));
        recentTable_->setItem(row, 2,
            new QTableWidgetItem(formatMoney(transaction.orderTotalCents)));
        recentTable_->setItem(row, 3,
            new QTableWidgetItem(formatMoney(transaction.changeCents)));
    }

    lowStockTable_->setRowCount(static_cast<int>(lowStock.size()));
    for (int row = 0; row < static_cast<int>(lowStock.size()); ++row) {
        const auto& product = lowStock[static_cast<std::size_t>(row)];
        lowStockTable_->setItem(row, 0,
            new QTableWidgetItem(QString::fromStdString(product.productId)));
        lowStockTable_->setItem(row, 1,
            new QTableWidgetItem(QString::fromStdString(product.name)));
        lowStockTable_->setItem(row, 2,
            new QTableWidgetItem(QString::number(product.stock)));
    }

    if (recent.empty())
        setMessage("No transactions have been recorded today.", false);
    else if (lowStock.empty())
        setMessage("No low-stock products.", false);
    else
        setMessage("Dashboard is showing current database information.", false);
}

void DashboardPage::setMessage(const QString& message, bool isError) {
    messageLabel_->setText(message);
    messageLabel_->setStyleSheet(isError
        ? "color: #A3485A; font-weight: 700;"
        : "color: #58765D; font-weight: 650;");
}

QString DashboardPage::formatMoney(int cents) {
    return guiFormatMoney(cents);
}
