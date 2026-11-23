#include "MainWindow.h"
#include "DashboardPage.h"
#include "InventoryPage.h"
#include "OperatorManagementDialog.h"
#include "ProductsPage.h"
#include "ReportsPage.h"
#include "SalesPage.h"
#include "TransactionsPage.h"

#include <QButtonGroup>
#include <QApplication>
#include <QColor>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QFormLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QPixmap>
#include <QImage>
#include <QIcon>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QIcon>
#include <QLineEdit>

#include <vector>

namespace {
const char* PAGE_NAMES[] = {
    "Dashboard",
    "Point of Sale",
    "Products",
    "Inventory",
    "Transactions",
    "Reports"
};

const char* PAGE_ICONS[] = {
    "H", "S", "P", "I", "T", "R"
};

void applyCardShadow(QFrame* card) {
    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(18);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(89, 67, 74, 24));
    card->setGraphicsEffect(shadow);
}
}

MainWindow::MainWindow(const std::string& databasePath, QWidget* parent)
    : QMainWindow(parent),
      controller_(databasePath),
      navigationGroup_(new QButtonGroup(this)),
      pageStack_(new QStackedWidget(this)),
      applicationStateStack_(nullptr),
      pageTitle_(nullptr),
      sidebar_(nullptr),
      sidebarToggle_(nullptr),
      operatorButton_(nullptr),
      logoutButton_(nullptr),
      manageOperatorsButton_(nullptr),
      loginPage_(nullptr),
      administratorSetupPage_(nullptr),
      applicationRoot_(nullptr),
      sidebarCollapsed_(false),
      databasePath_(databasePath) {
    setWindowTitle("Crombi Bakery POS");
    resize(1120, 720);
    setMinimumSize(820, 560);

    qApp->setStyleSheet(R"(
        QMainWindow, QWidget#root {
            background: #FFF9F2;
            color: #59434A;
            font-family: "Inter", "Poppins", "Segoe UI", sans-serif;
            font-size: 10pt;
        }
        QWidget#loginPage, QWidget#administratorSetupPage,
        QWidget#content, QStackedWidget#applicationStateStack,
        QStackedWidget#pageStack {
            background: #FFF9F2;
        }
        QWidget#dashboardPage, QWidget#salesPage, QWidget#productsPage,
        QWidget#inventoryPage, QWidget#transactionsPage, QWidget#reportsPage,
        QWidget#pageViewport {
            background: #FFF9F2;
        }
        QScrollArea {
            background: #FFF9F2;
            border: none;
        }
        QScrollArea QWidget#pageViewport {
            background: #FFF9F2;
        }
        QDialog, QMessageBox {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #FBF1F4, stop:1 #F2DDE4);
            color: #59434A;
            border: 1px solid #E7BEC9;
            border-radius: 14px;
        }
        QLabel { color: #59434A; }
        QToolTip {
            background: #F8E5EA;
            color: #59434A;
            border: 1px solid #D6AEB9;
            border-radius: 8px;
            padding: 5px 8px;
        }
        QWidget#sidebar {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #F3CDD5, stop:1 #E7A6B3);
            border-right: 1px solid #DFCFC5;
        }
        QWidget#sidebar QPushButton {
            text-align: left;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #F8D9E1, stop:1 #EDA8BA);
            color: #59434A;
            border: 1px solid #D39AA7;
            border-radius: 13px;
            padding: 8px 14px;
            min-height: 34px;
            font-size: 11.5pt;
            font-weight: 700;
        }
        QWidget#sidebar QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #F4BCCB, stop:1 #E58FA8);
            color: #59434A;
            border-color: #C98996;
        }
        QWidget#sidebar QPushButton:pressed {
            background: #C97995;
            color: #FFFFFF;
            border-color: #A95D78;
        }
        QWidget#sidebar QPushButton:checked {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #D98AA2, stop:1 #C97995);
            color: #FFFFFF;
            border: 2px solid #A95D78;
        }
        QPushButton#sidebarToggle, QPushButton#sidebarReopenToggle {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #F8D9E1, stop:1 #EDA8BA);
            border: 1px solid #D39AA7;
            border-radius: 21px;
            padding: 4px;
            min-width: 42px;
            max-width: 42px;
            min-height: 42px;
            max-height: 42px;
        }
        QPushButton#sidebarToggle:hover, QPushButton#sidebarReopenToggle:hover {
            background: #F4BCCB;
            border-color: #C98996;
        }
        QPushButton#sidebarToggle:pressed, QPushButton#sidebarReopenToggle:pressed {
            background: #C97995;
        }
        QLabel#brand {
            color: #59434A;
            font-size: 17pt;
            font-weight: 700;
        }
        QLabel#subtitle, QLabel#muted {
            color: #59434A;
        }
        QFrame#contentCard {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 rgba(255,252,248,248),
                stop:0.55 rgba(251,239,243,245),
                stop:1 #F8E5EA);
            border: 1px solid #E7BEC9;
            border-radius: 14px;
        }
        QLabel#pageTitle {
            color: #59434A;
            font-size: 24pt;
            font-weight: 750;
        }
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #F8D9E1, stop:1 #EDA8BA);
            color: #59434A;
            border: 1px solid #D39AA7;
            border-radius: 12px;
            padding: 6px 14px;
            text-align: center;
            font-weight: 700;
            min-height: 30px;
            max-height: 34px;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #F4BCCB, stop:1 #E58FA8);
            border-color: #C98996;
            color: #59434A;
        }
        QPushButton:pressed {
            background: #C97995;
            color: #FFFFFF;
            border-color: #A95D78;
        }
        QPushButton:focus {
            border: 2px solid #C98996;
            outline: none;
        }
        QPushButton:disabled {
            background: #F8E8EC;
            color: #8E7B80;
            border-color: #E5CAD1;
        }
        QToolButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #F8D9E1, stop:1 #EDA8BA);
            color: #59434A;
            border: 1px solid #D39AA7;
            border-radius: 8px;
            padding: 4px;
        }
        QToolButton:hover {
            background: #F4BCCB;
            border-color: #C98996;
        }
        QToolButton:focus {
            border: 2px solid #C98996;
        }
        QToolButton:pressed {
            background: #C97995;
            color: #FFFFFF;
        }
        QToolButton:disabled {
            background: #F8E8EC;
            color: #8E7B80;
            border-color: #E5CAD1;
        }
        QLineEdit, QPlainTextEdit, QSpinBox, QDateEdit, QComboBox {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(255,252,248,248),
                stop:1 rgba(248,232,237,235));
            color: #59434A;
            border: 1px solid #D6AEB9;
            border-radius: 10px;
            padding: 8px 10px;
            selection-background-color: #F6C9D5;
            selection-color: #59434A;
        }
        QLineEdit::placeholder, QPlainTextEdit::placeholder {
            color: #947F85;
        }
        QLineEdit:focus, QPlainTextEdit:focus, QSpinBox:focus,
        QDateEdit:focus, QComboBox:focus {
            border: 2px solid #B98791;
            background: #FFFCF8;
        }
        QComboBox::drop-down, QDateEdit::drop-down {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #F8D9E1, stop:1 #EDA8BA);
            border-left: 1px solid #D6AEB9;
            border-top-right-radius: 9px;
            border-bottom-right-radius: 9px;
            width: 22px;
        }
        QComboBox::drop-down:hover, QDateEdit::drop-down:hover {
            background: #F4BCCB;
        }
        QComboBox::drop-down:disabled, QDateEdit::drop-down:disabled {
            background: #F8E8EC;
        }
        QSpinBox::up-button, QSpinBox::down-button {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #F5F1EB, stop:1 #E9E2D9);
            color: #59434A;
            border: 0;
            border-left: 1px solid #D0C5BA;
            width: 20px;
        }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover {
            background: #E5DCD1;
        }
        QSpinBox::up-button:pressed, QSpinBox::down-button:pressed {
            background: #D5C9BD;
        }
        QSpinBox::up-button:disabled, QSpinBox::down-button:disabled {
            background: #F0ECE6;
            color: #8E7B80;
            border-color: #E0D9D1;
        }
        QLineEdit:disabled, QPlainTextEdit:disabled, QSpinBox:disabled,
        QDateEdit:disabled, QComboBox:disabled {
            background: #F8EDF0;
            color: #8E7B80;
            border-color: #E5CAD1;
        }
        QLabel#productImagePreview {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #FFFCF8, stop:1 #F6DCE4);
            border: 1px solid #E3BFC8;
            border-radius: 12px;
        }
        QPlainTextEdit#transactionReceipt {
            font-family: Consolas, monospace;
        }
        QSplitter::handle {
            background: transparent;
            border: none;
        }
        QSplitter::handle:hover {
            background: #E7A6B3;
            border-radius: 4px;
        }
        QTableWidget {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #FFFCF8, stop:1 #F8E8ED);
            alternate-background-color: #F8E7EC;
            color: #59434A;
            gridline-color: #DFCFC5;
            border: 1px solid #E7BEC9;
            border-radius: 14px;
            selection-background-color: #F6C9D5;
            selection-color: #59434A;
        }
        QTableWidget::item {
            color: #59434A;
            padding: 7px;
        }
        QTableWidget::item:hover {
            background: #F8DDE4;
        }
        QHeaderView::section {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #F3CDD5, stop:1 #E7A6B3);
            color: #59434A;
            border: none;
            border-bottom: 1px solid #C98996;
            padding: 8px;
            font-weight: bold;
        }
        QComboBox QAbstractItemView {
            background: #FFFCF8;
            color: #59434A;
            selection-background-color: #F3CDD5;
            selection-color: #59434A;
        }
        QCalendarWidget QToolButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #F8D9E1, stop:1 #EDA8BA);
            color: #59434A;
            border: 1px solid #D39AA7;
            border-radius: 8px;
            padding: 4px;
        }
        QCalendarWidget QToolButton:hover {
            background: #F4BCCB;
        }
        QCalendarWidget QToolButton:pressed {
            background: #C97995;
            color: #FFFFFF;
        }
        QCalendarWidget QAbstractItemView {
            background: #FFFCF8;
            color: #59434A;
            selection-background-color: #E7A6B3;
            selection-color: #59434A;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 10px;
            margin: 4px 2px;
        }
        QScrollBar:horizontal {
            background: transparent;
            height: 10px;
            margin: 2px 4px;
        }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
            background: #E7A6B3;
            border-radius: 5px;
            min-width: 24px;
            min-height: 24px;
        }
        QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover {
            background: #C98996;
        }
        QScrollBar::add-line, QScrollBar::sub-line {
            width: 0;
            height: 0;
        }
        QPushButton:checked {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #D98AA2, stop:1 #C97995);
            color: #FFFFFF;
            border-color: #A95D78;
            font-weight: 700;
        }
        QPushButton#checkoutButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #D98AA2, stop:1 #C97995);
            color: #FFFFFF;
            font-weight: 700;
        }
        QPushButton#checkoutButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #E89DB2, stop:1 #D9829D);
            color: #FFFFFF;
        }
        QPushButton#checkoutButton:pressed {
            background: #A95D78;
            color: #FFFFFF;
        }
        QLabel#statusBadge {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #F8D9E1, stop:1 #F0C2CE);
            border: 1px solid #E3BFC8;
            border-radius: 12px;
            padding: 5px 10px;
            color: #59434A;
            font-weight: 600;
        }
    )");

    auto* root = new QWidget(this);
    root->setObjectName("root");
    auto* rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    root->setAttribute(Qt::WA_StyledBackground, true);

    sidebar_ = new QWidget(root);
    sidebar_->setObjectName("sidebar");
    sidebar_->setFixedWidth(272);
    sidebar_->setAttribute(Qt::WA_StyledBackground, true);
    auto* sidebar = sidebar_;
    auto* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(20, 26, 20, 20);
    sidebarLayout->setSpacing(10);

    auto* sidebarToggleRow = new QHBoxLayout();
    sidebarToggleRow->addStretch();
    sidebarToggle_ = new QPushButton(sidebar);
    sidebarToggle_->setObjectName("sidebarToggle");
    sidebarToggle_->setIcon(QIcon(":/assets/toggle-sidebar.png"));
    sidebarToggle_->setIconSize(QSize(34, 34));
    sidebarToggle_->setFixedSize(42, 42);
    sidebarToggle_->setToolTip("Collapse sidebar");
    sidebarToggleRow->addWidget(sidebarToggle_, 0, Qt::AlignRight);
    sidebarLayout->addLayout(sidebarToggleRow);
    connect(sidebarToggle_, &QPushButton::clicked, this, [this]() {
        setSidebarCollapsed(!sidebarCollapsed_);
    });

    auto* brand = new QLabel(sidebar);
    brand->setObjectName("brand");
    brand->setAlignment(Qt::AlignCenter);
    brand->setMinimumHeight(132);
    brand->setMaximumHeight(132);
    const QPixmap logo(":/assets/crombi-logo.png");
    if (!logo.isNull()) {
        brand->setPixmap(logo.scaled(
            232, 124, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        brand->setText("CROMBI\nCOOKIES");
        brand->setStyleSheet(
            "color: #8D5965; font-size: 15pt; font-weight: 800; "
            "letter-spacing: 1px;");
    }
    sidebarLayout->addWidget(brand);
    sidebarLayout->addSpacing(12);

    navigationGroup_->setExclusive(true);
    for (int i = 0; i < 6; ++i) {
        auto* button = new QPushButton(QString::fromLatin1(PAGE_NAMES[i]), sidebar);
        button->setObjectName(QStringLiteral("nav%1").arg(i));
        button->setCheckable(true);
        button->setMinimumHeight(38);
        button->setToolTip(QString::fromLatin1(PAGE_NAMES[i]));
        navigationGroup_->addButton(button, i);
        sidebarLayout->addWidget(button);
    }
    connect(navigationGroup_, &QButtonGroup::idClicked,
            this, [this](int pageIndex) { navigateTo(pageIndex); });

    sidebarLayout->addStretch();
    auto* phaseLabel = new QLabel("Crombi Bakery - POS", sidebar);
    phaseLabel->setObjectName("muted");
    sidebarLayout->addWidget(phaseLabel);

    auto* content = new QWidget(root);
    content->setObjectName("content");
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(34, 30, 34, 34);
    contentLayout->setSpacing(18);

    auto* header = new QHBoxLayout();
    auto* reopenToggle = new QPushButton(content);
    reopenToggle->setObjectName("sidebarReopenToggle");
    reopenToggle->setIcon(QIcon(":/assets/toggle-sidebar.png"));
    reopenToggle->setIconSize(QSize(34, 34));
    reopenToggle->setFixedSize(42, 42);
    reopenToggle->setToolTip("Expand sidebar");
    reopenToggle->hide();
    connect(reopenToggle, &QPushButton::clicked, this, [this]() {
        setSidebarCollapsed(false);
    });
    header->addWidget(reopenToggle);
    pageTitle_ = new QLabel(content);
    pageTitle_->setObjectName("pageTitle");
    auto* status = new QLabel(content);
    status->setObjectName("statusBadge");
    std::vector<ProductRecord> activeProducts;
    std::string error;
    bool ready = controller_.isReady();
    if (!ready) {
        error = controller_.errorMessage();
    } else if (!controller_.activeProducts(activeProducts, error)) {
        ready = false;
    }
    status->setText(ready
        ? QStringLiteral("Database ready · %1 products").arg(activeProducts.size())
        : QStringLiteral("Database unavailable"));
    header->addWidget(pageTitle_);
    header->addStretch();
    operatorButton_ = new QPushButton(content);
    operatorButton_->setObjectName("currentOperatorButton");
    operatorButton_->setEnabled(false);
    operatorButton_->setToolTip("Logged-in account recorded on business activity.");
    refreshOperatorControls();
    manageOperatorsButton_ = new QPushButton("Manage operators", content);
    manageOperatorsButton_->setObjectName("manageOperatorsButton");
    connect(manageOperatorsButton_, &QPushButton::clicked,
            this, [this]() { openOperatorManagement(); });
    auto* backupButton = new QPushButton("Backup database", content);
    backupButton->setObjectName("backupDatabaseButton");
    auto* restoreButton = new QPushButton("Restore database", content);
    restoreButton->setObjectName("restoreDatabaseButton");
    connect(backupButton, &QPushButton::clicked,
            this, [this]() { backupDatabase(); });
    connect(restoreButton, &QPushButton::clicked,
            this, [this]() { restoreDatabase(); });
    logoutButton_ = new QPushButton("Log out", content);
    logoutButton_->setObjectName("logoutButton");
    connect(logoutButton_, &QPushButton::clicked,
            this, [this]() { logout(); });
    header->addWidget(operatorButton_);
    header->addWidget(manageOperatorsButton_);
    header->addWidget(backupButton);
    header->addWidget(restoreButton);
    header->addWidget(logoutButton_);
    header->addWidget(status);
    contentLayout->addLayout(header);

    pageStack_->setObjectName("pageStack");
    for (int i = 0; i < 6; ++i) {
        QWidget* page = nullptr;
        if (i == 0) {
            auto* dashboardPage = new DashboardPage(controller_, pageStack_);
            dashboardPage->setObjectName("dashboardPage");
            page = dashboardPage;
        } else if (i == 1) {
            auto* salesPage = new SalesPage(controller_, pageStack_);
            salesPage->setObjectName("salesPage");
            page = salesPage;
        } else if (i == 2) {
            auto* productsPage = new ProductsPage(controller_, pageStack_);
            productsPage->setObjectName("productsPage");
            page = productsPage;
        } else if (i == 3) {
            auto* inventoryPage = new InventoryPage(controller_, pageStack_);
            inventoryPage->setObjectName("inventoryPage");
            page = inventoryPage;
        } else if (i == 4) {
            auto* transactionsPage = new TransactionsPage(controller_, pageStack_);
            transactionsPage->setObjectName("transactionsPage");
            page = transactionsPage;
        } else if (i == 5) {
            auto* reportsPage = new ReportsPage(controller_, pageStack_);
            reportsPage->setObjectName("reportsPage");
            page = reportsPage;
        } else {
            page = createPlaceholderPage(
                i, ready, error, activeProducts.size());
        }
        auto* scrollArea = new QScrollArea(pageStack_);
        scrollArea->setObjectName(QStringLiteral("pageScrollArea%1").arg(i));
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->viewport()->setObjectName("pageViewport");
        scrollArea->viewport()->setAutoFillBackground(false);
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollArea->setWidget(page);
        pageStack_->addWidget(scrollArea);
    }
    contentLayout->addWidget(pageStack_, 1);

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(content, 1);
    applicationRoot_ = root;

    applicationStateStack_ = new QStackedWidget(this);
    applicationStateStack_->setObjectName("applicationStateStack");
    administratorSetupPage_ = createAdministratorSetupPage();
    loginPage_ = createLoginPage();
    applicationStateStack_->addWidget(administratorSetupPage_);
    applicationStateStack_->addWidget(loginPage_);
    applicationStateStack_->addWidget(applicationRoot_);
    setCentralWidget(applicationStateStack_);
    showAuthenticationPage();
    navigateTo(0);
}

QWidget* MainWindow::createPlaceholderPage(int pageIndex, bool databaseReady,
                                           const std::string& databaseError,
                                           std::size_t productCount) {
    auto* page = new QWidget(pageStack_);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* card = new QFrame(page);
    card->setObjectName("contentCard");
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(30, 28, 30, 28);
    cardLayout->setSpacing(14);

    auto* title = new QLabel(QString::fromLatin1(PAGE_NAMES[pageIndex]), card);
    title->setStyleSheet("font-size: 17pt; font-weight: 650; color: #4A3935;");
    auto* description = new QLabel(card);
    description->setObjectName("muted");
    description->setWordWrap(true);
    description->setMaximumWidth(650);

    if (pageIndex == 0) {
        if (databaseReady) {
            description->setText(
                QStringLiteral("The POS is connected to its existing business services. "
                               "%1 active products are available. Use the sidebar for sales, "
                               "product and inventory management, transaction history, and "
                               "daily reports.")
                    .arg(productCount));
        } else {
            description->setText(
                QStringLiteral("The database could not be initialized: %1")
                    .arg(QString::fromStdString(databaseError)));
        }
    } else {
        description->setText("This workspace is ready for the next feature-page implementation step.");
    }

    cardLayout->addWidget(title);
    cardLayout->addWidget(description);
    cardLayout->addStretch();

    applyCardShadow(card);

    layout->addWidget(card, 1);
    return page;
}

void MainWindow::navigateTo(int pageIndex) {
    if (pageIndex < 0 || pageIndex >= pageStack_->count()) return;
    pageStack_->setCurrentIndex(pageIndex);
    if (auto* button = navigationGroup_->button(pageIndex))
        button->setChecked(true);
    pageTitle_->setText(QString::fromLatin1(PAGE_NAMES[pageIndex]));
}

int MainWindow::currentPageIndex() const {
    return pageStack_->currentIndex();
}

QWidget* MainWindow::createLoginPage() {
    auto* page = new QWidget(this);
    page->setObjectName("loginPage");
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->addStretch();
    auto* card = new QFrame(page);
    card->setObjectName("contentCard");
    card->setMaximumWidth(520);
    applyCardShadow(card);
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(32, 28, 32, 28);
    cardLayout->setSpacing(14);
    auto* title = new QLabel("Crombi Bakery POS", card);
    title->setObjectName("loginTitle");
    title->setStyleSheet("font-size: 21pt; font-weight: 750;");
    auto* description = new QLabel("Log in with your operator account.", card);
    auto* form = new QFormLayout();
    auto* username = new QLineEdit(card);
    username->setObjectName("loginUsernameInput");
    username->setMaxLength(32);
    auto* password = new QLineEdit(card);
    password->setObjectName("loginPasswordInput");
    password->setEchoMode(QLineEdit::Password);
    password->setMaxLength(1024);
    form->addRow("Username", username);
    form->addRow("Password", password);
    auto* message = new QLabel(card);
    message->setObjectName("loginMessage");
    message->setWordWrap(true);
    auto* loginButton = new QPushButton("Log in", card);
    loginButton->setObjectName("loginButton");
    loginButton->setDefault(true);
    cardLayout->addWidget(title);
    cardLayout->addWidget(description);
    cardLayout->addLayout(form);
    cardLayout->addWidget(message);
    cardLayout->addWidget(loginButton);
    pageLayout->addWidget(card, 0, Qt::AlignHCenter);
    pageLayout->addStretch();
    connect(loginButton, &QPushButton::clicked,
            this, [this]() { submitLogin(); });
    connect(password, &QLineEdit::returnPressed,
            this, [this]() { submitLogin(); });
    return page;
}

QWidget* MainWindow::createAdministratorSetupPage() {
    auto* page = new QWidget(this);
    page->setObjectName("administratorSetupPage");
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->addStretch();
    auto* card = new QFrame(page);
    card->setObjectName("contentCard");
    card->setMaximumWidth(560);
    applyCardShadow(card);
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(32, 28, 32, 28);
    cardLayout->setSpacing(14);
    auto* title = new QLabel("Create Administrator Account", card);
    title->setObjectName("administratorSetupTitle");
    title->setStyleSheet("font-size: 21pt; font-weight: 750;");
    auto* description = new QLabel(
        "Create the first administrator to secure and manage this POS.", card);
    description->setWordWrap(true);
    auto* form = new QFormLayout();
    auto* username = new QLineEdit(card);
    username->setObjectName("setupUsernameInput");
    username->setMaxLength(32);
    auto* password = new QLineEdit(card);
    password->setObjectName("setupPasswordInput");
    password->setEchoMode(QLineEdit::Password);
    password->setMaxLength(1024);
    auto* confirmation = new QLineEdit(card);
    confirmation->setObjectName("setupConfirmPasswordInput");
    confirmation->setEchoMode(QLineEdit::Password);
    confirmation->setMaxLength(1024);
    form->addRow("Username", username);
    form->addRow("Password (12+ characters)", password);
    form->addRow("Confirm password", confirmation);
    auto* message = new QLabel(card);
    message->setObjectName("administratorSetupMessage");
    message->setWordWrap(true);
    auto* createButton = new QPushButton("Create administrator", card);
    createButton->setObjectName("createAdministratorButton");
    createButton->setDefault(true);
    cardLayout->addWidget(title);
    cardLayout->addWidget(description);
    cardLayout->addLayout(form);
    cardLayout->addWidget(message);
    cardLayout->addWidget(createButton);
    pageLayout->addWidget(card, 0, Qt::AlignHCenter);
    pageLayout->addStretch();
    connect(createButton, &QPushButton::clicked,
            this, [this]() { submitAdministratorSetup(); });
    connect(confirmation, &QLineEdit::returnPressed,
            this, [this]() { submitAdministratorSetup(); });
    return page;
}

void MainWindow::submitLogin() {
    auto* username = loginPage_->findChild<QLineEdit*>("loginUsernameInput");
    auto* password = loginPage_->findChild<QLineEdit*>("loginPasswordInput");
    auto* message = loginPage_->findChild<QLabel*>("loginMessage");
    if (!username || !password || !message)
        return;
    std::string error;
    if (!controller_.login(username->text().trimmed().toStdString(),
                           password->text().toStdString(), error)) {
        message->setText(QString::fromStdString(error));
        message->setStyleSheet("color: #A12626;");
        password->clear();
        return;
    }
    password->clear();
    message->clear();
    applicationStateStack_->setCurrentWidget(applicationRoot_);
    refreshOperatorControls();
    navigateTo(0);
}

void MainWindow::submitAdministratorSetup() {
    auto* username = administratorSetupPage_->findChild<QLineEdit*>(
        "setupUsernameInput");
    auto* password = administratorSetupPage_->findChild<QLineEdit*>(
        "setupPasswordInput");
    auto* confirmation = administratorSetupPage_->findChild<QLineEdit*>(
        "setupConfirmPasswordInput");
    auto* message = administratorSetupPage_->findChild<QLabel*>(
        "administratorSetupMessage");
    if (!username || !password || !confirmation || !message)
        return;
    if (password->text() != confirmation->text()) {
        message->setText("Password confirmation does not match.");
        message->setStyleSheet("color: #A12626;");
        confirmation->clear();
        return;
    }
    std::string error;
    if (!controller_.createInitialAdministrator(
            username->text().trimmed().toStdString(),
            password->text().toStdString(), error)) {
        message->setText(QString::fromStdString(error));
        message->setStyleSheet("color: #A12626;");
        return;
    }
    auto* loginUsername = loginPage_->findChild<QLineEdit*>(
        "loginUsernameInput");
    auto* loginMessage = loginPage_->findChild<QLabel*>("loginMessage");
    if (loginUsername)
        loginUsername->setText(username->text().trimmed());
    if (loginMessage) {
        loginMessage->setText("Administrator account created. Please log in.");
        loginMessage->setStyleSheet("color: #287044;");
    }
    password->clear();
    confirmation->clear();
    applicationStateStack_->setCurrentWidget(loginPage_);
}

void MainWindow::showAuthenticationPage() {
    if (!applicationStateStack_)
        return;
    if (!controller_.isReady()) {
        auto* message = loginPage_->findChild<QLabel*>("loginMessage");
        if (message) {
            message->setText(QString::fromStdString(controller_.errorMessage()));
            message->setStyleSheet("color: #A12626;");
        }
        applicationStateStack_->setCurrentWidget(loginPage_);
        return;
    }
    bool administratorExists = false;
    std::string error;
    if (!controller_.hasAdministrator(administratorExists, error)) {
        auto* message = loginPage_->findChild<QLabel*>("loginMessage");
        if (message) {
            message->setText(QString::fromStdString(error));
            message->setStyleSheet("color: #A12626;");
        }
        applicationStateStack_->setCurrentWidget(loginPage_);
        return;
    }
    applicationStateStack_->setCurrentWidget(
        administratorExists ? loginPage_ : administratorSetupPage_);
    refreshOperatorControls();
}

void MainWindow::refreshOperatorControls() {
    if (!operatorButton_)
        return;
    const std::string name = controller_.currentOperatorName();
    operatorButton_->setText(name.empty()
        ? QStringLiteral("Not logged in")
        : QStringLiteral("Operator: %1").arg(QString::fromStdString(name)));
    operatorButton_->setToolTip(name.empty()
        ? QStringLiteral("Log in to use the POS.")
        : QStringLiteral("Current authenticated operator."));
    const bool authenticated = controller_.isAuthenticated();
    if (logoutButton_)
        logoutButton_->setVisible(authenticated);
    if (manageOperatorsButton_)
        manageOperatorsButton_->setVisible(
            authenticated && controller_.isAdministrator());
}

void MainWindow::openOperatorManagement() {
    if (!controller_.isAdministrator())
        return;
    OperatorManagementDialog dialog(controller_, this);
    dialog.exec();
    refreshOperatorControls();
}

void MainWindow::logout() {
    if (!controller_.isAuthenticated())
        return;
    if (!controller_.cartItems().empty()) {
        const int confirmation = QMessageBox::warning(
            this, "Log out",
            "Logging out will discard the current cart. Continue?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (confirmation != QMessageBox::Yes)
            return;
    }
    controller_.logout();
    if (auto* password = loginPage_->findChild<QLineEdit*>(
            "loginPasswordInput"))
        password->clear();
    if (auto* message = loginPage_->findChild<QLabel*>("loginMessage"))
        message->clear();
    refreshOperatorControls();
    showAuthenticationPage();
}

void MainWindow::backupDatabase() {
    const QString defaultName = QString("CrombiBakery-backup-%1.db")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss"));
    const QString path = QFileDialog::getSaveFileName(
        this, "Create Database Backup", defaultName,
        "SQLite database files (*.db *.sqlite *.sqlite3)");
    if (path.isEmpty()) return;

    std::string error;
    if (!controller_.backupDatabase(path.toStdString(), error)) {
        QMessageBox::critical(this, "Backup failed",
                              QString::fromStdString(error));
        return;
    }
    QMessageBox::information(this, "Backup created",
                             "Database backup created successfully:\n" + path);
}

void MainWindow::restoreDatabase() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Restore Database Backup", QString(),
        "SQLite database files (*.db *.sqlite *.sqlite3);;All files (*.*)");
    if (path.isEmpty()) return;

    std::string error;
    if (!controller_.validateDatabaseBackup(path.toStdString(), error)) {
        QMessageBox::critical(this, "Invalid backup",
                              "The selected file is not a valid Crombi Bakery database:\n"
                              + QString::fromStdString(error));
        return;
    }

    const int confirmation = QMessageBox::warning(
        this, "Restore database",
        "Restore this backup and replace the current database?\n\n"
        "A safety backup of the current database will be created first.",
        QMessageBox::Cancel | QMessageBox::RestoreDefaults,
        QMessageBox::Cancel);
    if (confirmation != QMessageBox::RestoreDefaults) return;

    QFileInfo databaseInfo(QString::fromStdString(databasePath_));
    const QString databaseDir = databaseInfo.absoluteDir().absolutePath();
    const QString safetyName = QString("%1/pre_restore_%2.db")
        .arg(databaseDir,
             QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz"));
    if (!controller_.backupDatabase(safetyName.toStdString(), error)) {
        QMessageBox::critical(this, "Safety backup failed",
                              "Restore was cancelled because the safety backup could not be created:\n"
                              + QString::fromStdString(error));
        return;
    }

    if (!controller_.restoreDatabase(path.toStdString(), error)) {
        QMessageBox::critical(this, "Restore failed",
                              "The database was not restored:\n"
                              + QString::fromStdString(error)
                              + "\n\nThe safety backup is:\n" + safetyName);
        return;
    }

    refreshOperatorControls();
    showAuthenticationPage();
    QMessageBox::information(
        this, "Restore complete",
        "Database restored successfully.\n\nSafety backup:\n" + safetyName);
}

void MainWindow::setSidebarCollapsed(bool collapsed) {
    sidebarCollapsed_ = collapsed;
    sidebar_->setVisible(!collapsed);
    if (auto* reopen =
            findChild<QPushButton*>("sidebarReopenToggle"))
        reopen->setVisible(collapsed);
}
