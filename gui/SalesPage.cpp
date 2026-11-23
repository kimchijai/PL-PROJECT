#include "SalesPage.h"
#include "ProductImagePaths.h"

#include <QAbstractItemView>
#include <QColor>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QDir>

#include <limits>
#include <algorithm>

namespace {
QPixmap productPixmap(const std::string& productId,
                      const std::string& imagePath) {
    QPixmap image = ProductImagePaths::loadProductImage(
        QString::fromStdString(productId), QString::fromStdString(imagePath));
    if (!image.isNull())
        return image.scaled(88, 88, Qt::KeepAspectRatioByExpanding,
                            Qt::SmoothTransformation);
    QPixmap placeholder(88, 88);
    placeholder.fill(QColor("#F5EDE3"));
    return placeholder;
}

QFrame* makeCard(QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setObjectName("contentCard");
    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(16);
    shadow->setOffset(0, 3);
    shadow->setColor(QColor(91, 67, 59, 20));
    card->setGraphicsEffect(shadow);
    return card;
}

QLabel* makeSectionTitle(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setStyleSheet("font-size: 13pt; font-weight: 750; color: #59434A;");
    return label;
}
}

SalesPage::SalesPage(PosController& controller, QWidget* parent)
    : QWidget(parent),
      controller_(controller),
      productTable_(nullptr),
      cartTable_(nullptr),
      addQuantity_(nullptr),
      cartQuantity_(nullptr),
      cashInput_(nullptr),
      totalLabel_(nullptr),
      messageLabel_(nullptr),
      receiptView_(nullptr),
      addButton_(nullptr),
      updateButton_(nullptr),
      removeButton_(nullptr),
      checkoutButton_(nullptr) {
    auto* pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(12);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(12);

    auto* productsCard = makeCard(splitter);
    auto* productsLayout = new QVBoxLayout(productsCard);
    productsLayout->setContentsMargins(18, 16, 18, 16);
    productsLayout->setSpacing(10);
    auto* productsHeader = new QHBoxLayout();
    productsHeader->addWidget(makeSectionTitle("Available products", productsCard));
    productsHeader->addStretch();
    auto* refreshButton = new QPushButton("Refresh", productsCard);
    refreshButton->setObjectName("refreshProductsButton");
    productsHeader->addWidget(refreshButton);
    productsLayout->addLayout(productsHeader);

    productTable_ = new QTableWidget(productsCard);
    productTable_->setObjectName("productTable");
    productTable_->setColumnCount(5);
    productTable_->setHorizontalHeaderLabels({"Photo", "ID", "Product", "Price", "Stock"});
    productTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    productTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    productTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    productTable_->setAlternatingRowColors(true);
    productTable_->verticalHeader()->hide();
    productTable_->setIconSize(QSize(88, 88));
    productTable_->verticalHeader()->setDefaultSectionSize(104);
    productTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    productTable_->setColumnWidth(0, 96);
    productTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    productTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    productTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    productTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    productsLayout->addWidget(productTable_, 1);

    auto* addControls = new QHBoxLayout();
    addQuantity_ = new QSpinBox(productsCard);
    addQuantity_->setObjectName("addQuantity");
    addQuantity_->setRange(1, 1000000);
    addQuantity_->setValue(1);
    addButton_ = new QPushButton("Add to cart", productsCard);
    addButton_->setObjectName("addToCartButton");
    addControls->addWidget(new QLabel("Quantity:", productsCard));
    addControls->addWidget(addQuantity_);
    addControls->addStretch();
    addControls->addWidget(addButton_);
    productsLayout->addLayout(addControls);

    auto* cartCard = makeCard(splitter);
    auto* cartLayout = new QVBoxLayout(cartCard);
    cartLayout->setContentsMargins(18, 16, 18, 16);
    cartLayout->setSpacing(10);
    cartLayout->addWidget(makeSectionTitle("Current order", cartCard));

    cartTable_ = new QTableWidget(cartCard);
    cartTable_->setObjectName("cartTable");
    cartTable_->setColumnCount(5);
    cartTable_->setHorizontalHeaderLabels({"ID", "Product", "Unit", "Qty", "Subtotal"});
    cartTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    cartTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    cartTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    cartTable_->setAlternatingRowColors(true);
    cartTable_->verticalHeader()->hide();
    cartTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    cartTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    cartTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    cartTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    cartTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    cartLayout->addWidget(cartTable_, 1);

    auto* cartControls = new QHBoxLayout();
    cartQuantity_ = new QSpinBox(cartCard);
    cartQuantity_->setObjectName("cartQuantity");
    cartQuantity_->setRange(1, 1000000);
    updateButton_ = new QPushButton("Update", cartCard);
    updateButton_->setObjectName("updateCartButton");
    removeButton_ = new QPushButton("Remove", cartCard);
    removeButton_->setObjectName("removeCartButton");
    cartControls->addWidget(new QLabel("Selected quantity:", cartCard));
    cartControls->addWidget(cartQuantity_);
    cartControls->addStretch();
    cartControls->addWidget(updateButton_);
    cartControls->addWidget(removeButton_);
    cartLayout->addLayout(cartControls);

    totalLabel_ = new QLabel("Total: PHP 0.00", cartCard);
    totalLabel_->setObjectName("orderTotal");
    totalLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    totalLabel_->setStyleSheet("font-size: 16pt; font-weight: 800; color: #59434A;");
    cartLayout->addWidget(totalLabel_);

    auto* paymentRow = new QHBoxLayout();
    paymentRow->addWidget(new QLabel("Cash (PHP):", cartCard));
    cashInput_ = new QLineEdit(cartCard);
    cashInput_->setObjectName("cashInput");
    cashInput_->setPlaceholderText("e.g. 250.00");
    cashInput_->setMaxLength(12);
    paymentRow->addWidget(cashInput_, 1);
    cartLayout->addLayout(paymentRow);

    checkoutButton_ = new QPushButton("Complete sale", cartCard);
    checkoutButton_->setObjectName("checkoutButton");
    cartLayout->addWidget(checkoutButton_);

    splitter->addWidget(productsCard);
    splitter->addWidget(cartCard);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 4);
    pageLayout->addWidget(splitter, 1);

    messageLabel_ = new QLabel(this);
    messageLabel_->setObjectName("salesMessage");
    messageLabel_->setWordWrap(true);
    messageLabel_->setMinimumHeight(24);
    pageLayout->addWidget(messageLabel_);

    receiptView_ = new QPlainTextEdit(this);
    receiptView_->setObjectName("receiptView");
    receiptView_->setReadOnly(true);
    receiptView_->setMaximumHeight(150);
    receiptView_->setPlaceholderText("A completed sale receipt will appear here.");
    receiptView_->hide();
    pageLayout->addWidget(receiptView_);

    connect(refreshButton, &QPushButton::clicked, this, [this]() { refreshProducts(); });
    connect(addButton_, &QPushButton::clicked, this, [this]() { addSelectedProduct(); });
    connect(updateButton_, &QPushButton::clicked, this, [this]() { updateSelectedQuantity(); });
    connect(removeButton_, &QPushButton::clicked, this, [this]() { removeSelectedProduct(); });
    connect(checkoutButton_, &QPushButton::clicked, this, [this]() { checkout(); });
    connect(cartTable_, &QTableWidget::itemSelectionChanged,
            this, [this]() { updateCartControls(); });

    std::string error;
    if (!loadProducts(error))
        setMessage(QString::fromStdString(error), true);
    else if (productTable_->rowCount() == 0)
        setMessage("No active products are available.", false);
    refreshCart();
}

bool SalesPage::loadProducts(std::string& error) {
    std::vector<ProductRecord> products;
    if (!controller_.activeProducts(products, error)) return false;

    productTable_->setRowCount(static_cast<int>(products.size()));
    bool stockReadFailed = false;
    for (int row = 0; row < static_cast<int>(products.size()); ++row) {
        const auto& product = products[static_cast<std::size_t>(row)];
        int stock = 0;
        std::string stockError;
        const bool stockAvailable = controller_.stock(product.productId, stock, stockError);
        if (!stockAvailable && !stockReadFailed) {
            error = "Unable to load stock for " + product.productId + ": " + stockError;
            stockReadFailed = true;
        }

        auto* idItem = new QTableWidgetItem(QString::fromStdString(product.productId));
        auto* nameItem = new QTableWidgetItem(QString::fromStdString(product.name));
        auto* priceItem = new QTableWidgetItem(formatMoney(product.priceCents));
        auto* stockItem = new QTableWidgetItem(
            stockAvailable ? QString::number(stock) : QStringLiteral("Unavailable"));
        stockItem->setData(Qt::UserRole, stockAvailable ? stock : -1);
        if (stockAvailable && stock == 0)
            stockItem->setForeground(QColor("#A75C6A"));

        auto* photoItem = new QTableWidgetItem;
        photoItem->setIcon(QIcon(productPixmap(product.productId,
                                              product.imagePath)));
        photoItem->setTextAlignment(Qt::AlignCenter);
        productTable_->setItem(row, 0, photoItem);
        productTable_->setItem(row, 1, idItem);
        productTable_->setItem(row, 2, nameItem);
        productTable_->setItem(row, 3, priceItem);
        productTable_->setItem(row, 4, stockItem);
    }

    if (products.empty())
        error = "There are no active products available.";
    return !stockReadFailed;
}

void SalesPage::refreshProducts() {
    std::string error;
    if (!loadProducts(error)) {
        setMessage(QString::fromStdString(error), true);
        return;
    }
    if (productTable_->rowCount() == 0) {
        setMessage("No active products are available.", false);
        return;
    }
    setMessage("Product catalog and stock refreshed.", false);
}

void SalesPage::refreshCart(int selectedRow) {
    const auto& items = controller_.cartItems();
    cartTable_->setRowCount(static_cast<int>(items.size()));
    for (int row = 0; row < static_cast<int>(items.size()); ++row) {
        const auto& item = items[static_cast<std::size_t>(row)];
        cartTable_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(item.getProductId())));
        cartTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(item.getProductName())));
        cartTable_->setItem(row, 2, new QTableWidgetItem(formatMoney(item.getUnitPrice())));
        cartTable_->setItem(row, 3, new QTableWidgetItem(QString::number(item.getQuantity())));
        cartTable_->setItem(row, 4, new QTableWidgetItem(formatMoney(item.getSubtotal())));
    }
    totalLabel_->setText(QStringLiteral("Total: %1").arg(formatMoney(controller_.cartTotal())));
    if (selectedRow >= 0 && selectedRow < cartTable_->rowCount())
        cartTable_->selectRow(selectedRow);
    else
        cartTable_->clearSelection();
    updateCartControls();
}

void SalesPage::addSelectedProduct() {
    const int row = productTable_->currentRow();
    if (row < 0 || !productTable_->item(row, 1)) {
        setMessage("Select a product to add to the order.", true);
        return;
    }
    auto* stockItem = productTable_->item(row, 4);
    if (!stockItem || stockItem->data(Qt::UserRole).toInt() < 0) {
        setMessage("Stock information is unavailable; refresh the catalog and try again.", true);
        return;
    }

    const std::string productId = productTable_->item(row, 1)->text().toStdString();
    std::string error;
    if (!controller_.addToCart(productId, addQuantity_->value(), error)) {
        setMessage(QString::fromStdString(error), true);
        return;
    }
    receiptView_->clear();
    receiptView_->hide();
    setMessage("Product added to the order.", false);
    refreshCart();
}

void SalesPage::updateSelectedQuantity() {
    const int row = cartTable_->currentRow();
    if (row < 0 || !cartTable_->item(row, 0)) {
        setMessage("Select an order item to update.", true);
        return;
    }
    const std::string productId = cartTable_->item(row, 0)->text().toStdString();
    std::string error;
    if (!controller_.updateCart(productId, cartQuantity_->value(), error)) {
        setMessage(QString::fromStdString(error), true);
        return;
    }
    receiptView_->clear();
    receiptView_->hide();
    setMessage("Order quantity updated.", false);
    refreshCart(row);
}

void SalesPage::removeSelectedProduct() {
    const int row = cartTable_->currentRow();
    if (row < 0 || !cartTable_->item(row, 0)) {
        setMessage("Select an order item to remove.", true);
        return;
    }
    const std::string productId = cartTable_->item(row, 0)->text().toStdString();
    std::string error;
    if (!controller_.removeFromCart(productId, error)) {
        setMessage(QString::fromStdString(error), true);
        return;
    }
    receiptView_->clear();
    receiptView_->hide();
    setMessage("Order item removed.", false);
    refreshCart(std::min(row, cartTable_->rowCount() - 1));
}

void SalesPage::checkout() {
    receiptView_->clear();
    receiptView_->hide();

    int cashCents = 0;
    if (!parseCashCents(cashInput_->text(), cashCents)) {
        setMessage("Enter a valid cash amount with up to two decimal places.", true);
        return;
    }

    TransactionRecord transaction{};
    std::vector<TransactionItemRecord> savedItems;
    std::string error;
    if (!controller_.checkout(cashCents, transaction, savedItems, error)) {
        setMessage(QString::fromStdString(error), true);
        return;
    }

    refreshCart();
    const bool productsRefreshed = loadProducts(error);
    receiptView_->setPlainText(buildReceipt(transaction, savedItems));
    receiptView_->show();
    cashInput_->clear();
    setMessage(productsRefreshed
                   ? QStringLiteral("Sale completed successfully.")
                   : QStringLiteral("Sale completed; product refresh failed: %1")
                         .arg(QString::fromStdString(error)),
               !productsRefreshed);
}

void SalesPage::updateCartControls() {
    const int row = cartTable_->currentRow();
    const bool selected = row >= 0 && row < cartTable_->rowCount();
    updateButton_->setEnabled(selected);
    removeButton_->setEnabled(selected);
    cartQuantity_->setEnabled(selected);
    if (selected && row < static_cast<int>(controller_.cartItems().size()))
        cartQuantity_->setValue(controller_.cartItems()[static_cast<std::size_t>(row)].getQuantity());
}

void SalesPage::setMessage(const QString& message, bool isError) {
    messageLabel_->setText(message);
    messageLabel_->setStyleSheet(isError
        ? "color: #A3485A; font-weight: 600;"
        : "color: #58765D; font-weight: 600;");
}

QString SalesPage::buildReceipt(
    const TransactionRecord& transaction,
    const std::vector<TransactionItemRecord>& items) const {
    QStringList lines;
    lines << "Crombi Bakery - Receipt"
          << QStringLiteral("Transaction #%1").arg(transaction.transactionId)
          << QStringLiteral("Date: %1").arg(QString::fromStdString(transaction.transactionDate))
          << "";
    for (const auto& item : items) {
        lines << QStringLiteral("%1 x %2 @ %3 = %4")
                     .arg(item.quantity)
                     .arg(QString::fromStdString(item.productName))
                     .arg(formatMoney(item.unitPriceCents))
                     .arg(formatMoney(item.subtotalCents));
    }
    lines << ""
          << QStringLiteral("Total: %1").arg(formatMoney(transaction.orderTotalCents))
          << QStringLiteral("Cash: %1").arg(formatMoney(transaction.cashReceivedCents))
          << QStringLiteral("Change: %1").arg(formatMoney(transaction.changeCents));
    return lines.join('\n');
}

bool SalesPage::parseCashCents(const QString& text, int& cents) {
    const QString value = text.trimmed();
    if (value.isEmpty()) return false;
    const int decimalPoint = value.indexOf('.');
    if (decimalPoint != value.lastIndexOf('.')) return false;

    const QString wholePart = decimalPoint < 0 ? value : value.left(decimalPoint);
    QString fractionPart = decimalPoint < 0 ? QString() : value.mid(decimalPoint + 1);
    if (wholePart.isEmpty() || fractionPart.size() > 2) return false;
    for (const QChar character : wholePart) {
        if (character < QLatin1Char('0') || character > QLatin1Char('9')) return false;
    }
    for (const QChar character : fractionPart) {
        if (character < QLatin1Char('0') || character > QLatin1Char('9')) return false;
    }
    if (decimalPoint >= 0 && fractionPart.isEmpty()) fractionPart = "0";
    bool wholeOk = false;
    const qlonglong whole = wholePart.toLongLong(&wholeOk);
    if (!wholeOk || whole > std::numeric_limits<int>::max() / 100) return false;

    if (fractionPart.isEmpty()) fractionPart = "00";
    else if (fractionPart.size() == 1) fractionPart.append('0');
    bool fractionOk = false;
    const int fraction = fractionPart.toInt(&fractionOk);
    if (!fractionOk) return false;

    const qlonglong totalCents = whole * 100 + fraction;
    if (totalCents > std::numeric_limits<int>::max()) return false;
    cents = static_cast<int>(totalCents);
    return true;
}

QString SalesPage::formatMoney(int cents) {
    return QStringLiteral("PHP %1").arg(static_cast<double>(cents) / 100.0, 0, 'f', 2);
}

QString SalesPage::formatMoney(double amount) {
    return QStringLiteral("PHP %1").arg(amount, 0, 'f', 2);
}
