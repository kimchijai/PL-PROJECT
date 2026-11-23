#include "InventoryPage.h"

#include <QAbstractItemView>
#include <QColor>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
constexpr int LOW_STOCK_THRESHOLD = 30;
}

namespace {
QFrame* makeInventoryCard(QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setObjectName("contentCard");
    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(18);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(89, 67, 74, 24));
    card->setGraphicsEffect(shadow);
    return card;
}
}

InventoryPage::InventoryPage(PosController& controller, QWidget* parent)
    : QWidget(parent),
      controller_(controller),
      searchInput_(nullptr),
      inventoryTable_(nullptr),
      restockQuantity_(nullptr),
      restockReason_(nullptr),
      adjustmentAmount_(nullptr),
      adjustmentReason_(nullptr),
      restockButton_(nullptr),
      adjustButton_(nullptr),
      selectedLabel_(nullptr),
      messageLabel_(nullptr) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* listCard = makeInventoryCard(this);
    auto* listLayout = new QVBoxLayout(listCard);
    listLayout->setContentsMargins(18, 16, 18, 16);
    listLayout->setSpacing(10);
    auto* header = new QHBoxLayout();
    auto* title = new QLabel("Inventory levels", listCard);
    title->setStyleSheet("font-size: 15pt; font-weight: 750; color: #59434A;");
    header->addWidget(title);
    header->addStretch();
    searchInput_ = new QLineEdit(listCard);
    searchInput_->setObjectName("inventorySearchInput");
    searchInput_->setPlaceholderText("Search by product ID or name");
    searchInput_->setMinimumWidth(220);
    auto* searchButton = new QPushButton("Search", listCard);
    searchButton->setObjectName("inventorySearchButton");
    auto* refreshButton = new QPushButton("Show all", listCard);
    refreshButton->setObjectName("inventoryRefreshButton");
    header->addWidget(searchInput_);
    header->addWidget(searchButton);
    header->addWidget(refreshButton);
    listLayout->addLayout(header);

    inventoryTable_ = new QTableWidget(listCard);
    inventoryTable_->setObjectName("inventoryTable");
    inventoryTable_->setColumnCount(6);
    inventoryTable_->setHorizontalHeaderLabels(
        {"Product ID", "Name", "Stock", "Stock status", "Product status",
         "Last movement by"});
    inventoryTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    inventoryTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    inventoryTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    inventoryTable_->setAlternatingRowColors(true);
    inventoryTable_->verticalHeader()->hide();
    inventoryTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    inventoryTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    inventoryTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    inventoryTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    inventoryTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    inventoryTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    listLayout->addWidget(inventoryTable_, 1);
    layout->addWidget(listCard, 3);

    auto* actionsCard = makeInventoryCard(this);
    auto* actionsLayout = new QVBoxLayout(actionsCard);
    actionsLayout->setContentsMargins(18, 14, 18, 14);
    actionsLayout->setSpacing(8);
    selectedLabel_ = new QLabel("Select an active product to manage its stock.", actionsCard);
    selectedLabel_->setObjectName("selectedInventoryProduct");
    selectedLabel_->setStyleSheet("font-weight: 700; color: #59434A;");
    actionsLayout->addWidget(selectedLabel_);

    auto* columns = new QHBoxLayout();
    auto* restockForm = new QFormLayout();
    restockQuantity_ = new QSpinBox(actionsCard);
    restockQuantity_->setObjectName("restockQuantity");
    restockQuantity_->setRange(1, 1000000);
    restockQuantity_->setValue(1);
    restockReason_ = new QLineEdit(actionsCard);
    restockReason_->setObjectName("restockReason");
    restockReason_->setPlaceholderText("Required");
    restockForm->addRow("Restock quantity", restockQuantity_);
    restockForm->addRow("Reason", restockReason_);
    restockButton_ = new QPushButton("Record restock", actionsCard);
    restockButton_->setObjectName("restockButton");
    restockForm->addRow(restockButton_);
    columns->addLayout(restockForm, 1);

    auto* adjustmentForm = new QFormLayout();
    adjustmentAmount_ = new QSpinBox(actionsCard);
    adjustmentAmount_->setObjectName("adjustmentAmount");
    adjustmentAmount_->setRange(-1000000, 1000000);
    adjustmentAmount_->setValue(-1);
    adjustmentReason_ = new QLineEdit(actionsCard);
    adjustmentReason_->setObjectName("adjustmentReason");
    adjustmentReason_->setPlaceholderText("Required");
    adjustmentForm->addRow("Stock change (+/-)", adjustmentAmount_);
    adjustmentForm->addRow("Reason", adjustmentReason_);
    adjustButton_ = new QPushButton("Apply adjustment", actionsCard);
    adjustButton_->setObjectName("adjustStockButton");
    adjustmentForm->addRow(adjustButton_);
    columns->addLayout(adjustmentForm, 1);
    actionsLayout->addLayout(columns);
    layout->addWidget(actionsCard);

    messageLabel_ = new QLabel(this);
    messageLabel_->setObjectName("inventoryMessage");
    messageLabel_->setWordWrap(true);
    messageLabel_->setMinimumHeight(24);
    layout->addWidget(messageLabel_);

    connect(searchButton, &QPushButton::clicked,
            this, [this]() { searchInventory(); });
    connect(searchInput_, &QLineEdit::returnPressed,
            this, [this]() { searchInventory(); });
    connect(refreshButton, &QPushButton::clicked, this, [this]() {
        searchInput_->clear();
        searchInventory();
    });
    connect(inventoryTable_, &QTableWidget::itemSelectionChanged,
            this, [this]() { selectProduct(); });
    connect(restockButton_, &QPushButton::clicked,
            this, [this]() { restockSelected(); });
    connect(adjustButton_, &QPushButton::clicked,
            this, [this]() { adjustSelected(); });

    searchInventory();
    updateActionControls();
}

bool InventoryPage::loadInventory(const std::string& term, std::string& error) {
    std::vector<ProductRecord> products;
    if (!controller_.searchProducts(term, products, error)) return false;
    displayedProducts_ = products;
    inventoryTable_->setRowCount(static_cast<int>(products.size()));

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

        inventoryTable_->setItem(row, 0,
            new QTableWidgetItem(QString::fromStdString(product.productId)));
        inventoryTable_->setItem(row, 1,
            new QTableWidgetItem(QString::fromStdString(product.name)));
        auto* stockItem = new QTableWidgetItem(
            stockAvailable ? QString::number(stock) : QStringLiteral("Unavailable"));
        stockItem->setData(Qt::UserRole, product.active);
        inventoryTable_->setItem(row, 2, stockItem);

        const QString stockStatus = !stockAvailable ? QStringLiteral("Unavailable")
            : stock == 0 ? QStringLiteral("Out of stock")
            : stock <= LOW_STOCK_THRESHOLD ? QStringLiteral("Low stock")
                          : QStringLiteral("In stock");
        auto* stockStatusItem = new QTableWidgetItem(stockStatus);
        if (stockAvailable && stock == 0)
            stockStatusItem->setForeground(QColor("#A3485A"));
        else if (stockAvailable && stock > 0 && stock <= LOW_STOCK_THRESHOLD)
            stockStatusItem->setForeground(QColor("#A87943"));
        inventoryTable_->setItem(row, 3, stockStatusItem);
        auto* productStatus = new QTableWidgetItem(product.active ? "Active" : "Inactive");
        productStatus->setData(Qt::UserRole, product.active);
        inventoryTable_->setItem(row, 4, productStatus);

        InventoryLog latestLog{};
        std::string logError;
        if (controller_.latestInventoryLog(product.productId, latestLog, logError)) {
            inventoryTable_->setItem(row, 5,
                new QTableWidgetItem(QString::fromStdString(latestLog.operatorName)));
        } else if (logError == "Inventory log not found.") {
            inventoryTable_->setItem(row, 5, new QTableWidgetItem("—"));
        } else {
            if (!stockReadFailed) {
                error = "Unable to load latest inventory movement for "
                    + product.productId + ": " + logError;
                stockReadFailed = true;
            }
            inventoryTable_->setItem(row, 5, new QTableWidgetItem("Unavailable"));
        }
    }
    return !stockReadFailed;
}

void InventoryPage::searchInventory() {
    std::string error;
    if (!loadInventory(searchInput_->text().trimmed().toStdString(), error)) {
        setMessage(QString::fromStdString(error), true);
        return;
    }
    setMessage(displayedProducts_.empty()
                   ? QStringLiteral("No products match this search.")
                   : QStringLiteral("%1 product(s) shown.")
                         .arg(static_cast<qulonglong>(displayedProducts_.size())),
               false);
    updateActionControls();
}

void InventoryPage::selectProduct() {
    const int row = inventoryTable_->currentRow();
    if (row < 0 || row >= static_cast<int>(displayedProducts_.size())) {
        selectedProductId_.clear();
        selectedLabel_->setText("Select an active product to manage its stock.");
    } else {
        const auto& product = displayedProducts_[static_cast<std::size_t>(row)];
        selectedProductId_ = product.productId;
        selectedLabel_->setText(QStringLiteral("%1 - %2")
            .arg(QString::fromStdString(product.productId),
                 QString::fromStdString(product.name)));
    }
    updateActionControls();
}

void InventoryPage::restockSelected() {
    if (selectedProductId_.empty()) {
        setMessage("Select an active product first.", true);
        return;
    }
    const QString reason = restockReason_->text().trimmed();
    if (reason.isEmpty()) {
        setMessage("Enter a reason for the restock.", true);
        return;
    }
    const std::string productId = selectedProductId_;
    std::string error;
    if (!controller_.restock(productId, restockQuantity_->value(),
                             reason.toStdString(), error)) {
        setMessage(QString::fromStdString(error), true);
        return;
    }
    restockReason_->clear();
    if (!refreshAfterMovement(productId)) return;
    setMessage("Restock recorded successfully.", false);
}

void InventoryPage::adjustSelected() {
    if (selectedProductId_.empty()) {
        setMessage("Select an active product first.", true);
        return;
    }
    const int quantityChange = adjustmentAmount_->value();
    if (quantityChange == 0) {
        setMessage("Stock adjustment cannot be zero.", true);
        return;
    }
    const QString reason = adjustmentReason_->text().trimmed();
    if (reason.isEmpty()) {
        setMessage("Enter a reason for the stock adjustment.", true);
        return;
    }
    const std::string productId = selectedProductId_;
    std::string error;
    if (!controller_.adjustStock(productId, quantityChange,
                                 reason.toStdString(), error)) {
        setMessage(QString::fromStdString(error), true);
        return;
    }
    adjustmentReason_->clear();
    if (!refreshAfterMovement(productId)) return;
    setMessage("Stock adjustment recorded successfully.", false);
}

void InventoryPage::setMessage(const QString& message, bool isError) {
    messageLabel_->setText(message);
    messageLabel_->setStyleSheet(isError
        ? "color: #A3485A; font-weight: 600;"
        : "color: #58765D; font-weight: 600;");
}

bool InventoryPage::refreshAfterMovement(const std::string& productId) {
    std::string error;
    if (!loadInventory(searchInput_->text().trimmed().toStdString(), error)) {
        setMessage(QStringLiteral("Inventory was updated, but the view could not be refreshed: %1")
                       .arg(QString::fromStdString(error)), true);
        return false;
    }
    for (int row = 0; row < static_cast<int>(displayedProducts_.size()); ++row) {
        if (displayedProducts_[static_cast<std::size_t>(row)].productId == productId) {
            inventoryTable_->selectRow(row);
            return true;
        }
    }
    selectedProductId_.clear();
    updateActionControls();
    return true;
}

void InventoryPage::updateActionControls() {
    bool active = false;
    const int row = inventoryTable_->currentRow();
    if (row >= 0 && row < static_cast<int>(displayedProducts_.size()))
        active = displayedProducts_[static_cast<std::size_t>(row)].active;
    restockButton_->setEnabled(active);
    adjustButton_->setEnabled(active);
    restockQuantity_->setEnabled(active);
    restockReason_->setEnabled(active);
    adjustmentAmount_->setEnabled(active);
    adjustmentReason_->setEnabled(active);
}
