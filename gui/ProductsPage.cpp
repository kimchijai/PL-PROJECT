#include "ProductsPage.h"
#include "ProductImagePaths.h"

#include <QAbstractItemView>
#include <QColor>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QPixmap>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QRegularExpression>
#include <QUuid>

#include <limits>

namespace {
QFrame* makeProductsCard(QWidget* parent) {
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

ProductsPage::ProductsPage(PosController& controller, QWidget* parent)
    : QWidget(parent),
      controller_(controller),
      searchInput_(nullptr),
      productIdInput_(nullptr),
      nameInput_(nullptr),
      priceInput_(nullptr),
      initialStockInput_(nullptr),
      productTable_(nullptr),
      messageLabel_(nullptr),
      addButton_(nullptr),
      saveButton_(nullptr),
      deactivateButton_(nullptr),
      deleteButton_(nullptr),
      imagePreview_(nullptr),
      imagePathInput_(nullptr) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* listCard = makeProductsCard(this);
    auto* listLayout = new QVBoxLayout(listCard);
    listLayout->setContentsMargins(18, 16, 18, 16);
    listLayout->setSpacing(10);

    auto* header = new QHBoxLayout();
    auto* title = new QLabel("Product catalog", listCard);
    title->setStyleSheet("font-size: 15pt; font-weight: 750; color: #59434A;");
    header->addWidget(title);
    header->addStretch();
    searchInput_ = new QLineEdit(listCard);
    searchInput_->setObjectName("productsSearchInput");
    searchInput_->setPlaceholderText("Search by product ID or name");
    searchInput_->setMinimumWidth(220);
    auto* searchButton = new QPushButton("Search", listCard);
    searchButton->setObjectName("productsSearchButton");
    auto* refreshButton = new QPushButton("Show all", listCard);
    refreshButton->setObjectName("productsRefreshButton");
    header->addWidget(searchInput_);
    header->addWidget(searchButton);
    header->addWidget(refreshButton);
    listLayout->addLayout(header);

    productTable_ = new QTableWidget(listCard);
    productTable_->setObjectName("productsTable");
    productTable_->setColumnCount(7);
    productTable_->setHorizontalHeaderLabels(
        {"Image", "Product ID", "Name", "Category ID", "Price", "Stock", "Status"});
    productTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    productTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    productTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    productTable_->setAlternatingRowColors(true);
    productTable_->verticalHeader()->hide();
    productTable_->setIconSize(QSize(72, 72));
    productTable_->verticalHeader()->setDefaultSectionSize(80);
    productTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    productTable_->setColumnWidth(0, 88);
    productTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    productTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    for (int column = 3; column < 7; ++column)
        productTable_->horizontalHeader()->setSectionResizeMode(
            column, QHeaderView::ResizeToContents);
    listLayout->addWidget(productTable_, 1);
    layout->addWidget(listCard, 3);

    auto* editCard = makeProductsCard(this);
    auto* editLayout = new QVBoxLayout(editCard);
    editLayout->setContentsMargins(18, 14, 18, 14);
    editLayout->setSpacing(8);
    auto* editTitle = new QLabel("Add or edit product", editCard);
    editTitle->setStyleSheet("font-size: 13pt; font-weight: 750; color: #59434A;");
    editLayout->addWidget(editTitle);

    auto* form = new QFormLayout();
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);
    productIdInput_ = new QLineEdit(editCard);
    productIdInput_->setObjectName("productIdInput");
    nameInput_ = new QLineEdit(editCard);
    nameInput_->setObjectName("productNameInput");
    priceInput_ = new QLineEdit(editCard);
    priceInput_->setObjectName("productPriceInput");
    priceInput_->setPlaceholderText("e.g. 65.00");
    initialStockInput_ = new QSpinBox(editCard);
    initialStockInput_->setObjectName("initialStockInput");
    initialStockInput_->setRange(0, 1000000);
    form->addRow("Product ID", productIdInput_);
    form->addRow("Name", nameInput_);
    form->addRow("Unit price (PHP)", priceInput_);
    form->addRow("Initial stock (new products)", initialStockInput_);
    editLayout->addLayout(form);

    auto* imageRow = new QHBoxLayout();
    imagePreview_ = new QLabel(editCard);
    imagePreview_->setObjectName("productImagePreview");
    imagePreview_->setFixedSize(96, 96);
    imagePreview_->setAlignment(Qt::AlignCenter);
    imagePathInput_ = new QLineEdit(editCard);
    imagePathInput_->setObjectName("productImagePath");
    imagePathInput_->setReadOnly(true);
    imagePathInput_->setPlaceholderText("No product image");
    auto* chooseImageButton = new QPushButton("Choose image", editCard);
    chooseImageButton->setObjectName("chooseProductImageButton");
    auto* removeImageButton = new QPushButton("Remove image", editCard);
    removeImageButton->setObjectName("removeProductImageButton");
    imageRow->addWidget(imagePreview_);
    imageRow->addWidget(imagePathInput_, 1);
    imageRow->addWidget(chooseImageButton);
    imageRow->addWidget(removeImageButton);
    editLayout->addLayout(imageRow);

    auto* actions = new QHBoxLayout();
    actions->addStretch();
    addButton_ = new QPushButton("Add product", editCard);
    addButton_->setObjectName("addProductButton");
    saveButton_ = new QPushButton("Save edits", editCard);
    saveButton_->setObjectName("saveProductButton");
    deactivateButton_ = new QPushButton("Deactivate", editCard);
    deactivateButton_->setObjectName("deactivateProductButton");
    deleteButton_ = new QPushButton("Delete product", editCard);
    deleteButton_->setObjectName("deleteProductButton");
    auto* clearButton = new QPushButton("Clear form", editCard);
    clearButton->setObjectName("clearProductFormButton");
    actions->addWidget(addButton_);
    actions->addWidget(saveButton_);
    actions->addWidget(deactivateButton_);
    actions->addWidget(deleteButton_);
    actions->addWidget(clearButton);
    editLayout->addLayout(actions);
    layout->addWidget(editCard);

    messageLabel_ = new QLabel(this);
    messageLabel_->setObjectName("productsMessage");
    messageLabel_->setWordWrap(true);
    messageLabel_->setMinimumHeight(24);
    layout->addWidget(messageLabel_);

    connect(searchButton, &QPushButton::clicked,
            this, [this]() { searchProducts(); });
    connect(searchInput_, &QLineEdit::returnPressed,
            this, [this]() { searchProducts(); });
    connect(refreshButton, &QPushButton::clicked, this, [this]() {
        searchInput_->clear();
        searchProducts();
    });
    connect(productTable_, &QTableWidget::itemSelectionChanged,
            this, [this]() { selectProduct(); });
    connect(addButton_, &QPushButton::clicked, this, [this]() { addProduct(); });
    connect(saveButton_, &QPushButton::clicked, this, [this]() { saveProduct(); });
    connect(deactivateButton_, &QPushButton::clicked,
            this, [this]() { deactivateProduct(); });
    connect(deleteButton_, &QPushButton::clicked,
            this, [this]() { deleteProduct(); });
    connect(clearButton, &QPushButton::clicked, this, [this]() { clearForm(); });
    connect(chooseImageButton, &QPushButton::clicked,
            this, [this]() { chooseImage(); });
    connect(removeImageButton, &QPushButton::clicked, this, [this]() {
        selectedImagePath_.clear();
        imagePathInput_->clear();
        imagePreview_->clear();
        setMessage("Product image removed from the form.", false);
    });

    clearForm();
    searchProducts();
}

bool ProductsPage::loadProducts(const std::string& term, std::string& error) {
    std::vector<ProductRecord> records;
    if (!controller_.searchProducts(term, records, error)) return false;
    displayedProducts_ = records;
    productTable_->setRowCount(static_cast<int>(records.size()));

    bool stockReadFailed = false;
    for (int row = 0; row < static_cast<int>(records.size()); ++row) {
        const auto& product = records[static_cast<std::size_t>(row)];
        int stock = 0;
        std::string stockError;
        const bool stockAvailable = controller_.stock(product.productId, stock, stockError);
        if (!stockAvailable && !stockReadFailed) {
            error = "Unable to load stock for " + product.productId + ": " + stockError;
            stockReadFailed = true;
        }

        auto* imageItem = new QTableWidgetItem;
        QPixmap image = ProductImagePaths::loadProductImage(
            QString::fromStdString(product.productId),
            QString::fromStdString(product.imagePath));
        if (image.isNull()) {
            image = QPixmap(72, 72);
            image.fill(QColor("#F5EDE3"));
        } else {
            image = image.scaled(72, 72, Qt::KeepAspectRatioByExpanding,
                                 Qt::SmoothTransformation);
        }
        imageItem->setIcon(QIcon(image));
        imageItem->setTextAlignment(Qt::AlignCenter);
        productTable_->setItem(row, 0, imageItem);
        productTable_->setItem(row, 1,
            new QTableWidgetItem(QString::fromStdString(product.productId)));
        productTable_->setItem(row, 2,
            new QTableWidgetItem(QString::fromStdString(product.name)));
        productTable_->setItem(row, 3,
            new QTableWidgetItem(QString::number(product.categoryId)));
        productTable_->setItem(row, 4,
            new QTableWidgetItem(formatMoney(product.priceCents)));
        productTable_->setItem(row, 5,
            new QTableWidgetItem(stockAvailable
                ? QString::number(stock) : QStringLiteral("Unavailable")));
        auto* status = new QTableWidgetItem(product.active ? "Active" : "Inactive");
        status->setData(Qt::UserRole, product.active);
        productTable_->setItem(row, 6, status);
    }
    return !stockReadFailed;
}

void ProductsPage::searchProducts() {
    std::string error;
    const std::string term = searchInput_->text().trimmed().toStdString();
    if (!loadProducts(term, error)) {
        setMessage(QString::fromStdString(error), true);
        return;
    }
    setMessage(displayedProducts_.empty()
                   ? QStringLiteral("No products match this search.")
                   : QStringLiteral("%1 product(s) shown.")
                         .arg(static_cast<qulonglong>(displayedProducts_.size())),
               false);
}

void ProductsPage::selectProduct() {
    const int row = productTable_->currentRow();
    if (row < 0 || row >= static_cast<int>(displayedProducts_.size())) {
        clearForm();
        return;
    }
    const auto& product = displayedProducts_[static_cast<std::size_t>(row)];
    selectedProductId_ = product.productId;
    productIdInput_->setText(QString::fromStdString(product.productId));
    productIdInput_->setEnabled(false);
    nameInput_->setText(QString::fromStdString(product.name));
    priceInput_->setText(QString::number(
        static_cast<double>(product.priceCents) / 100.0, 'f', 2));
    selectedImagePath_ = product.imagePath;
    imagePathInput_->setText(QString::fromStdString(product.imagePath));
    QPixmap preview = ProductImagePaths::loadProductImage(
        QString::fromStdString(product.productId),
        QString::fromStdString(product.imagePath));
    if (!preview.isNull()) {
        imagePreview_->setPixmap(preview.scaled(
            88, 88, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        imagePreview_->clear();
    }
    initialStockInput_->setEnabled(false);
    saveButton_->setEnabled(product.active);
    deactivateButton_->setEnabled(product.active);
    deleteButton_->setEnabled(true);
    addButton_->setEnabled(false);
}

void ProductsPage::clearForm() {
    selectedProductId_.clear();
    selectedImagePath_.clear();
    if (imagePathInput_) imagePathInput_->clear();
    if (imagePreview_) imagePreview_->clear();
    productTable_->clearSelection();
    productIdInput_->clear();
    productIdInput_->setEnabled(true);
    nameInput_->clear();
    priceInput_->clear();
    initialStockInput_->setValue(0);
    initialStockInput_->setEnabled(true);
    addButton_->setEnabled(true);
    saveButton_->setEnabled(false);
    deactivateButton_->setEnabled(false);
    deleteButton_->setEnabled(false);
}

void ProductsPage::addProduct() {
    const QString productId = productIdInput_->text().trimmed();
    const QString name = nameInput_->text().trimmed();
    int priceCents = 0;
    if (productId.isEmpty() || name.isEmpty()) {
        setMessage("Enter both a product ID and product name.", true);
        return;
    }
    if (!parsePriceCents(priceInput_->text(), priceCents)) {
        setMessage("Enter a valid non-negative price with up to two decimals.", true);
        return;
    }

    std::string error;
    if (!controller_.createProduct(productId.toStdString(), name.toStdString(),
                                   priceCents, initialStockInput_->value(),
                                   selectedImagePath_, error)) {
        setMessage(QString::fromStdString(error), true);
        return;
    }
    searchInput_->clear();
    clearForm();
    std::string refreshError;
    if (!loadProducts("", refreshError)) {
        setMessage(QStringLiteral("Product was added, but the catalog could not be refreshed: %1")
                       .arg(QString::fromStdString(refreshError)), true);
        return;
    }
    setMessage("Product added successfully.", false);
}

void ProductsPage::saveProduct() {
    if (selectedProductId_.empty()) {
        setMessage("Select an active product to edit.", true);
        return;
    }
    const QString name = nameInput_->text().trimmed();
    int priceCents = 0;
    if (name.isEmpty()) {
        setMessage("Product name cannot be empty.", true);
        return;
    }
    if (!parsePriceCents(priceInput_->text(), priceCents)) {
        setMessage("Enter a valid non-negative price with up to two decimals.", true);
        return;
    }

    const std::string selectedId = selectedProductId_;
    std::string error;
    if (!controller_.editProduct(selectedId, name.toStdString(), priceCents,
                                 selectedImagePath_, error)) {
        setMessage(QString::fromStdString(error), true);
        return;
    }
    if (!refreshAfterChange(selectedId, false)) return;
    setMessage("Product updated successfully.", false);
}

void ProductsPage::deactivateProduct() {
    if (selectedProductId_.empty()) {
        setMessage("Select an active product to deactivate.", true);
        return;
    }
    const std::string selectedId = selectedProductId_;
    std::string error;
    if (!controller_.deactivateProduct(selectedId, error)) {
        setMessage(QString::fromStdString(error), true);
        return;
    }
    if (!refreshAfterChange(selectedId, false)) return;
    setMessage("Product deactivated. Its transaction history is preserved.", false);
}

void ProductsPage::deleteProduct() {
    if (selectedProductId_.empty()) {
        setMessage("Select a product to delete.", true);
        return;
    }

    const std::string selectedId = selectedProductId_;
    int currentStock = 0;
    std::string error;
    if (!controller_.stock(selectedId, currentStock, error)) {
        setMessage(QString::fromStdString(error), true);
        return;
    }
    const QString stockWarning = currentStock > 0
        ? QStringLiteral(" The remaining %1 item(s) will be written off and "
                         "recorded in inventory history.")
              .arg(currentStock)
        : QString();
    const int confirmation = QMessageBox::warning(
        this, "Delete product",
        QStringLiteral("Permanently delete product %1? Existing transaction "
                       "and inventory history will be preserved.%2")
            .arg(QString::fromStdString(selectedId), stockWarning),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (confirmation != QMessageBox::Yes)
        return;

    ProductRecord deletedRecord{};
    bool imagePathUnreferenced = false;
    if (!controller_.deleteProduct(selectedId, deletedRecord,
                                   imagePathUnreferenced, error)) {
        setMessage(QString::fromStdString(error), true);
        return;
    }

    QString imageError;
    const bool imageRemoved = !imagePathUnreferenced
        || ProductImagePaths::removeUnreferencedUpload(
            QString::fromStdString(deletedRecord.imagePath), imageError);
    if (!refreshAfterChange(selectedId, false))
        return;
    if (!imageRemoved) {
        setMessage(QStringLiteral("Product deleted, but its image could not be removed: %1")
                       .arg(imageError), true);
        return;
    }
    setMessage("Product permanently deleted.", false);
}

bool ProductsPage::refreshAfterChange(const std::string& selectedId, bool resetSearch) {
    if (resetSearch) searchInput_->clear();
    std::string error;
    if (!loadProducts(searchInput_->text().trimmed().toStdString(), error)) {
        setMessage(QString::fromStdString(error), true);
        return false;
    }
    for (int row = 0; row < static_cast<int>(displayedProducts_.size()); ++row) {
        if (displayedProducts_[static_cast<std::size_t>(row)].productId == selectedId) {
            productTable_->selectRow(row);
            return true;
        }
    }
    clearForm();
    return true;
}

void ProductsPage::chooseImage() {
    const QString sourcePath = QFileDialog::getOpenFileName(
        this, "Choose product image", QString(),
        "Product images (*.png *.jpg *.jpeg)");
    if (sourcePath.isEmpty()) return;

    const QFileInfo sourceInfo(sourcePath);
    const QString extension = sourceInfo.suffix().toLower();
    if (extension != "png" && extension != "jpg" && extension != "jpeg") {
        setMessage("Only PNG, JPG, and JPEG images are supported.", true);
        return;
    }

    QImageReader reader(sourcePath);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        setMessage("The selected file is not a readable PNG, JPG, or JPEG image.", true);
        return;
    }

    const QString productId = productIdInput_->text().trimmed();
    if (productId.isEmpty()) {
        setMessage("Enter or select a product before choosing an image.", true);
        return;
    }

    const QString root = ProductImagePaths::applicationDataRoot();
    const QString uploadDirectory = ProductImagePaths::uploadDirectory();
    if (root.isEmpty() || !QDir().mkpath(uploadDirectory)) {
        setMessage("Could not create the product image data directory.", true);
        return;
    }

    QString safeId = productId;
    safeId.replace(QRegularExpression("[^A-Za-z0-9_-]"), "_");
    const QString destinationRelative =
        QString("assets/products/uploads/%1_%2.%3")
            .arg(safeId, QUuid::createUuid().toString(QUuid::WithoutBraces), extension);
    const QString destinationAbsolute = QDir(root).filePath(destinationRelative);
    if (!QFile::copy(sourcePath, destinationAbsolute)) {
        setMessage("Could not update the product image.", true);
        return;
    }

    selectedImagePath_ = destinationRelative.toStdString();
    imagePathInput_->setText(destinationRelative);
    imagePreview_->setPixmap(
        QPixmap::fromImage(image).scaled(
            88, 88, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    setMessage("Product image selected.", false);
}

void ProductsPage::setMessage(const QString& message, bool isError) {
    messageLabel_->setText(message);
    messageLabel_->setStyleSheet(isError
        ? "color: #A3485A; font-weight: 600;"
        : "color: #58765D; font-weight: 600;");
}

bool ProductsPage::parsePriceCents(const QString& text, int& cents) {
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
    if (fractionPart.isEmpty()) fractionPart = "00";
    else if (fractionPart.size() == 1) fractionPart.append('0');

    bool wholeOk = false;
    const qlonglong whole = wholePart.toLongLong(&wholeOk);
    if (!wholeOk || whole > std::numeric_limits<int>::max() / 100) return false;
    bool fractionOk = false;
    const int fraction = fractionPart.toInt(&fractionOk);
    if (!fractionOk) return false;

    const qlonglong totalCents = whole * 100 + fraction;
    if (totalCents > std::numeric_limits<int>::max()) return false;
    cents = static_cast<int>(totalCents);
    return true;
}

QString ProductsPage::formatMoney(int cents) {
    return QStringLiteral("PHP %1").arg(static_cast<double>(cents) / 100.0, 0, 'f', 2);
}
