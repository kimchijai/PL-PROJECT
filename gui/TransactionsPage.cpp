#include "TransactionsPage.h"

#include "DisplayFormatting.h"
#include "ExportService.h"

#include <QAbstractItemView>
#include <QColor>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QInputDialog>

#include <limits>

namespace {
QFrame* makeTransactionCard(QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setObjectName("contentCard");
    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(18);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(89, 67, 74, 24));
    card->setGraphicsEffect(shadow);
    return card;
}

QLabel* makeTransactionTitle(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setStyleSheet("font-size: 13pt; font-weight: 750; color: #59434A;");
    return label;
}

void styleTransactionTable(QTableWidget* table) {
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->hide();
}
}

TransactionsPage::TransactionsPage(PosController& controller, QWidget* parent)
    : QWidget(parent),
      controller_(controller),
      searchInput_(nullptr),
      historyTable_(nullptr),
      itemsTable_(nullptr),
      receiptView_(nullptr),
      messageLabel_(nullptr),
      pdfButton_(nullptr),
      printButton_(nullptr),
      fullRefundButton_(nullptr),
      partialRefundButton_(nullptr),
      voidButton_(nullptr),
      selectedTransaction_{} {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* historyCard = makeTransactionCard(this);
    auto* historyLayout = new QVBoxLayout(historyCard);
    historyLayout->setContentsMargins(18, 15, 18, 15);
    historyLayout->setSpacing(9);
    auto* historyHeader = new QHBoxLayout();
    historyHeader->addWidget(makeTransactionTitle("Transaction history", historyCard));
    historyHeader->addStretch();
    searchInput_ = new QLineEdit(historyCard);
    searchInput_->setObjectName("transactionSearchInput");
    searchInput_->setPlaceholderText("Transaction ID");
    searchInput_->setValidator(new QIntValidator(1, std::numeric_limits<int>::max(),
                                                  searchInput_));
    searchInput_->setMaximumWidth(170);
    auto* searchButton = new QPushButton("Find transaction", historyCard);
    searchButton->setObjectName("searchTransactionButton");
    auto* refreshButton = new QPushButton("Refresh", historyCard);
    refreshButton->setObjectName("refreshTransactionsButton");
    historyHeader->addWidget(searchInput_);
    historyHeader->addWidget(searchButton);
    historyHeader->addWidget(refreshButton);
    historyLayout->addLayout(historyHeader);

    historyTable_ = new QTableWidget(historyCard);
    historyTable_->setObjectName("transactionsTable");
    historyTable_->setColumnCount(8);
    historyTable_->setHorizontalHeaderLabels(
        {"Transaction ID", "Date", "Total", "Cash received", "Change", "Status",
         "Refunded", "Operator"});
    styleTransactionTable(historyTable_);
    historyTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    historyTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int column = 2; column < 8; ++column)
        historyTable_->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    historyLayout->addWidget(historyTable_, 1);
    layout->addWidget(historyCard, 2);

    auto* detailsSplitter = new QSplitter(Qt::Horizontal, this);
    detailsSplitter->setObjectName("transactionDetailsSplitter");
    detailsSplitter->setChildrenCollapsible(false);
    detailsSplitter->setHandleWidth(10);

    auto* itemsCard = makeTransactionCard(detailsSplitter);
    auto* itemsLayout = new QVBoxLayout(itemsCard);
    itemsLayout->setContentsMargins(16, 14, 16, 14);
    itemsLayout->setSpacing(8);
    itemsLayout->addWidget(makeTransactionTitle("Transaction details", itemsCard));
    itemsTable_ = new QTableWidget(itemsCard);
    itemsTable_->setObjectName("transactionItemsTable");
    itemsTable_->setColumnCount(5);
    itemsTable_->setHorizontalHeaderLabels(
        {"Product ID", "Product snapshot", "Unit price", "Qty", "Subtotal"});
    styleTransactionTable(itemsTable_);
    itemsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    itemsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int column = 2; column < 5; ++column)
        itemsTable_->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    itemsLayout->addWidget(itemsTable_, 1);

    auto* receiptCard = makeTransactionCard(detailsSplitter);
    auto* receiptLayout = new QVBoxLayout(receiptCard);
    receiptLayout->setContentsMargins(16, 14, 16, 14);
    receiptLayout->setSpacing(8);
    receiptLayout->addWidget(makeTransactionTitle("Receipt", receiptCard));
    receiptView_ = new QPlainTextEdit(receiptCard);
    receiptView_->setObjectName("transactionReceipt");
    receiptView_->setReadOnly(true);
    receiptView_->setPlaceholderText("Select or search for a transaction to view its receipt.");
    receiptLayout->addWidget(receiptView_, 1);
    auto* receiptActions = new QHBoxLayout();
    pdfButton_ = new QPushButton("Export PDF", receiptCard);
    pdfButton_->setObjectName("exportReceiptPdfButton");
    printButton_ = new QPushButton("Print receipt", receiptCard);
    printButton_->setObjectName("printReceiptButton");
    partialRefundButton_ = new QPushButton("Partial refund", receiptCard);
    partialRefundButton_->setObjectName("partialRefundButton");
    fullRefundButton_ = new QPushButton("Full refund", receiptCard);
    fullRefundButton_->setObjectName("fullRefundButton");
    voidButton_ = new QPushButton("Void", receiptCard);
    voidButton_->setObjectName("voidTransactionButton");
    pdfButton_->setEnabled(false);
    printButton_->setEnabled(false);
    partialRefundButton_->setEnabled(false);
    fullRefundButton_->setEnabled(false);
    voidButton_->setEnabled(false);
    receiptActions->addStretch();
    receiptActions->addWidget(pdfButton_);
    receiptActions->addWidget(printButton_);
    receiptActions->addWidget(partialRefundButton_);
    receiptActions->addWidget(fullRefundButton_);
    receiptActions->addWidget(voidButton_);
    receiptLayout->addLayout(receiptActions);
    detailsSplitter->addWidget(itemsCard);
    detailsSplitter->addWidget(receiptCard);
    detailsSplitter->setStretchFactor(0, 3);
    detailsSplitter->setStretchFactor(1, 2);
    layout->addWidget(detailsSplitter, 3);

    messageLabel_ = new QLabel(this);
    messageLabel_->setObjectName("transactionsMessage");
    messageLabel_->setWordWrap(true);
    messageLabel_->setMinimumHeight(24);
    layout->addWidget(messageLabel_);

    connect(searchButton, &QPushButton::clicked,
            this, [this]() { searchTransaction(); });
    connect(searchInput_, &QLineEdit::returnPressed,
            this, [this]() { searchTransaction(); });
    connect(refreshButton, &QPushButton::clicked,
            this, [this]() { refreshHistory(); });
    connect(historyTable_, &QTableWidget::itemSelectionChanged,
            this, [this]() { loadSelectedTransaction(); });
    connect(pdfButton_, &QPushButton::clicked,
            this, [this]() { exportSelectedPdf(); });
    connect(printButton_, &QPushButton::clicked,
            this, [this]() { printSelectedReceipt(); });
    connect(fullRefundButton_, &QPushButton::clicked,
            this, [this]() { fullRefundSelected(); });
    connect(partialRefundButton_, &QPushButton::clicked,
            this, [this]() { partialRefundSelected(); });
    connect(voidButton_, &QPushButton::clicked,
            this, [this]() { voidSelected(); });

    clearDetails();
    refreshHistory();
}

void TransactionsPage::refreshHistory() {
    std::vector<TransactionRecord> transactions;
    std::string error;
    if (!controller_.recentTransactions(transactions, 100, error)) {
        displayedTransactions_.clear();
        historyTable_->setRowCount(0);
        clearDetails();
        setMessage(QString::fromStdString(error), true);
        return;
    }

    displayedTransactions_ = transactions;
    historyTable_->setRowCount(static_cast<int>(transactions.size()));
    for (int row = 0; row < static_cast<int>(transactions.size()); ++row) {
        const auto& transaction = transactions[static_cast<std::size_t>(row)];
        auto* idItem = new QTableWidgetItem(QString::number(transaction.transactionId));
        idItem->setData(Qt::UserRole, transaction.transactionId);
        historyTable_->setItem(row, 0, idItem);
        historyTable_->setItem(row, 1, new QTableWidgetItem(
            guiFormatTimestamp(transaction.transactionDate)));
        historyTable_->setItem(row, 2, new QTableWidgetItem(
            guiFormatMoney(transaction.orderTotalCents)));
        historyTable_->setItem(row, 3, new QTableWidgetItem(
            guiFormatMoney(transaction.cashReceivedCents)));
        historyTable_->setItem(row, 4, new QTableWidgetItem(
            guiFormatMoney(transaction.changeCents)));
        historyTable_->setItem(row, 5, new QTableWidgetItem(
            QString::fromStdString(transaction.status)));
        historyTable_->setItem(row, 6, new QTableWidgetItem(
            guiFormatMoney(transaction.refundedCents)));
        historyTable_->setItem(row, 7, new QTableWidgetItem(
            QString::fromStdString(transaction.operatorName)));
    }
    if (transactions.empty()) {
        clearDetails();
        setMessage("No completed transactions have been recorded.", false);
    } else {
        setMessage(QStringLiteral("%1 recent transaction(s) loaded.")
                       .arg(static_cast<qulonglong>(transactions.size())), false);
    }
}

void TransactionsPage::loadSelectedTransaction() {
    const int row = historyTable_->currentRow();
    if (row < 0 || row >= static_cast<int>(displayedTransactions_.size())) {
        clearDetails();
        return;
    }
    showTransaction(displayedTransactions_[static_cast<std::size_t>(row)].transactionId);
}

void TransactionsPage::searchTransaction() {
    bool valid = false;
    const int transactionId = searchInput_->text().trimmed().toInt(&valid);
    if (!valid || transactionId <= 0) {
        clearDetails();
        setMessage("Enter a valid positive transaction ID.", true);
        return;
    }
    showTransaction(transactionId);
}

bool TransactionsPage::showTransaction(int transactionId) {
    TransactionRecord transaction{};
    std::string error;
    if (!controller_.transaction(transactionId, transaction, error)) {
        clearDetails();
        setMessage(QString::fromStdString(error), true);
        return false;
    }

    std::vector<TransactionItemRecord> items;
    if (!controller_.transactionItems(transactionId, items, error)) {
        clearDetails();
        setMessage(QString::fromStdString(error), true);
        return false;
    }
    std::vector<TransactionRefundRecord> refunds;
    if (!controller_.transactionRefunds(transactionId, refunds, error)) {
        clearDetails();
        setMessage(QString::fromStdString(error), true);
        return false;
    }
    selectedTransaction_ = transaction;
    selectedItems_ = items;
    const bool eligible = transaction.status == "completed"
        || transaction.status == "partial_refund";
    pdfButton_->setEnabled(true);
    printButton_->setEnabled(true);
    partialRefundButton_->setEnabled(eligible);
    fullRefundButton_->setEnabled(eligible);
    voidButton_->setEnabled(eligible);

    itemsTable_->setRowCount(static_cast<int>(items.size()));
    QStringList receiptLines;
    receiptLines << "Crombi Bakery"
                 << QStringLiteral("Transaction #%1").arg(transaction.transactionId)
                 << QStringLiteral("Date: %1")
                        .arg(guiFormatTimestamp(transaction.transactionDate))
                 << QStringLiteral("Status: %1")
                        .arg(QString::fromStdString(transaction.status))
                 << QStringLiteral("Operator: %1")
                        .arg(QString::fromStdString(transaction.operatorName))
                 << QStringLiteral("Refunded: %1")
                        .arg(guiFormatMoney(transaction.refundedCents))
                 << QString();
    for (int row = 0; row < static_cast<int>(items.size()); ++row) {
        const auto& item = items[static_cast<std::size_t>(row)];
        itemsTable_->setItem(row, 0,
            new QTableWidgetItem(QString::fromStdString(item.productId)));
        itemsTable_->setItem(row, 1,
            new QTableWidgetItem(QString::fromStdString(item.productName)));
        itemsTable_->setItem(row, 2,
            new QTableWidgetItem(guiFormatMoney(item.unitPriceCents)));
        itemsTable_->setItem(row, 3,
            new QTableWidgetItem(QString::number(item.quantity)));
        itemsTable_->setItem(row, 4,
            new QTableWidgetItem(guiFormatMoney(item.subtotalCents)));
        receiptLines << QStringLiteral("%1 × %2")
                            .arg(QString::fromStdString(item.productName))
                            .arg(item.quantity)
                     << QStringLiteral("  %1 each   %2")
                            .arg(guiFormatMoney(item.unitPriceCents),
                                 guiFormatMoney(item.subtotalCents));
    }
    if (items.empty()) receiptLines << "No transaction items recorded.";
    if (!refunds.empty()) {
        receiptLines << QString() << "Refund / void history:";
        for (const auto& refund : refunds) {
            receiptLines << QStringLiteral("%1 - %2 - %3 by %4")
                                .arg(QString::fromStdString(refund.refundType),
                                     guiFormatMoney(refund.refundCents),
                                     guiFormatTimestamp(refund.refundDate),
                                     QString::fromStdString(refund.operatorName))
                         << QStringLiteral("  Reason: %1")
                                .arg(QString::fromStdString(refund.reason));
        }
    }
    receiptLines << QString()
                 << QStringLiteral("Total: %1").arg(guiFormatMoney(transaction.orderTotalCents))
                 << QStringLiteral("Cash: %1").arg(guiFormatMoney(transaction.cashReceivedCents))
                 << QStringLiteral("Change: %1").arg(guiFormatMoney(transaction.changeCents));
    receiptView_->setPlainText(receiptLines.join('\n'));

    setMessage(items.empty()
                   ? "Transaction loaded, but no item snapshots were found."
                   : "Transaction details and saved receipt loaded.",
               items.empty());
    return true;
}

void TransactionsPage::clearDetails() {
    itemsTable_->setRowCount(0);
    receiptView_->clear();
    selectedTransaction_ = {};
    selectedItems_.clear();
    if (pdfButton_) pdfButton_->setEnabled(false);
    if (printButton_) printButton_->setEnabled(false);
    if (fullRefundButton_) fullRefundButton_->setEnabled(false);
    if (partialRefundButton_) partialRefundButton_->setEnabled(false);
    if (voidButton_) voidButton_->setEnabled(false);
}

void TransactionsPage::fullRefundSelected() {
    if (selectedTransaction_.transactionId <= 0) {
        setMessage("Select a transaction first.", true);
        return;
    }
    bool ok = false;
    const QString reason = QInputDialog::getText(
        this, "Full refund", "Refund reason:",
        QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok) return;
    if (reason.isEmpty()) {
        setMessage("A refund reason is required.", true);
        return;
    }
    TransactionRefundRecord refund{};
    std::string error;
    if (!controller_.fullRefund(selectedTransaction_.transactionId,
                                reason.toStdString(), refund, error)) {
        setMessage(QString::fromStdString(error), true);
        return;
    }
    refreshHistory();
    showTransaction(selectedTransaction_.transactionId);
    setMessage(QStringLiteral("Full refund of %1 recorded.")
                   .arg(guiFormatMoney(refund.refundCents)), false);
}

void TransactionsPage::partialRefundSelected() {
    if (selectedTransaction_.transactionId <= 0) {
        setMessage("Select a transaction first.", true);
        return;
    }
    std::vector<TransactionRefundItemInput> refundItems;
    for (const auto& item : selectedItems_) {
        bool ok = false;
        const int quantity = QInputDialog::getInt(
            this, "Partial refund",
            QStringLiteral("Refund quantity for %1")
                .arg(QString::fromStdString(item.productName)),
            0, 0, item.quantity, 1, &ok);
        if (!ok) return;
        if (quantity > 0)
            refundItems.push_back({item.productId, quantity});
    }
    if (refundItems.empty()) {
        setMessage("Select at least one item quantity to refund.", true);
        return;
    }
    bool reasonOk = false;
    const QString reason = QInputDialog::getText(
        this, "Partial refund", "Refund reason:",
        QLineEdit::Normal, QString(), &reasonOk).trimmed();
    if (!reasonOk) return;
    if (reason.isEmpty()) {
        setMessage("A refund reason is required.", true);
        return;
    }

    TransactionRefundRecord refund{};
    std::string error;
    if (!controller_.partialRefund(selectedTransaction_.transactionId,
                                   refundItems, reason.toStdString(),
                                   refund, error)) {
        setMessage(QString::fromStdString(error), true);
        return;
    }
    refreshHistory();
    showTransaction(selectedTransaction_.transactionId);
    setMessage(QStringLiteral("Partial refund of %1 recorded.")
                   .arg(guiFormatMoney(refund.refundCents)), false);
}

void TransactionsPage::voidSelected() {
    if (selectedTransaction_.transactionId <= 0) {
        setMessage("Select a transaction first.", true);
        return;
    }
    bool ok = false;
    const QString reason = QInputDialog::getText(
        this, "Void transaction", "Void reason:",
        QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok) return;
    if (reason.isEmpty()) {
        setMessage("A void reason is required.", true);
        return;
    }
    TransactionRefundRecord refund{};
    std::string error;
    if (!controller_.voidTransaction(selectedTransaction_.transactionId,
                                     reason.toStdString(), refund, error)) {
        setMessage(QString::fromStdString(error), true);
        return;
    }
    refreshHistory();
    showTransaction(selectedTransaction_.transactionId);
    setMessage(QStringLiteral("Transaction voided. %1 restored as refund.")
                   .arg(guiFormatMoney(refund.refundCents)), false);
}

void TransactionsPage::exportSelectedPdf() {
    if (selectedTransaction_.transactionId <= 0) {
        setMessage("Select a transaction before exporting.", true);
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, "Export Receipt PDF",
        QString("crombi-receipt-%1.pdf").arg(selectedTransaction_.transactionId),
        "PDF files (*.pdf)");
    if (path.isEmpty()) return;
    std::string error;
    if (!ExportService::exportReceiptPdf(path.toStdString(),
                                         selectedTransaction_,
                                         selectedItems_, error))
        setMessage(QString::fromStdString(error), true);
    else
        setMessage("Receipt PDF exported successfully.", false);
}

void TransactionsPage::printSelectedReceipt() {
    if (selectedTransaction_.transactionId <= 0) {
        setMessage("Select a transaction before printing.", true);
        return;
    }
    std::string error;
    if (!ExportService::printReceipt(selectedTransaction_, selectedItems_,
                                     this, error))
        setMessage(QString::fromStdString(error),
                   error != "Printing cancelled.");
    else
        setMessage("Receipt sent to printer.", false);
}

void TransactionsPage::setMessage(const QString& message, bool isError) {
    messageLabel_->setText(message);
    messageLabel_->setStyleSheet(isError
        ? "color: #A3485A; font-weight: 600;"
        : "color: #58765D; font-weight: 600;");
}
