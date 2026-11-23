#include "../gui/MainWindow.h"
#include "../gui/AppDataPaths.h"
#include "../gui/ApplicationFont.h"
#include "../gui/ProductImagePaths.h"

#include <QApplication>
#include <QFile>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QFontInfo>
#include <QFontMetrics>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QTemporaryDir>
#include <QWidget>

#include "../DatabaseManager.h"
#include "../InventoryRepository.h"

#include <iostream>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QString fontError;
    if (!GuiApplicationFont::load(fontError)) {
        std::cerr << fontError.toStdString() << "\n";
        return 1;
    }
    QFont boldFont = application.font();
    boldFont.setWeight(QFont::Bold);
    const QFontInfo boldFontInfo(boldFont);
    const QFontMetrics fontMetrics(application.font());
    for (const QChar character : QStringLiteral("0123456789+-\u2212")) {
        if (!fontMetrics.inFont(character)) {
            std::cerr << "Google Sans Flex is missing a numeric or quantity glyph.\n";
            return 1;
        }
    }
    if (boldFontInfo.family() != QStringLiteral("Google Sans Flex")
        || boldFontInfo.weight() < QFont::DemiBold) {
        std::cerr << "Google Sans Flex bold text could not be resolved.\n";
        return 1;
    }
    const QString captureDirectory =
        qEnvironmentVariable("CROMBI_THEME_CAPTURE_DIR");
    if (!captureDirectory.isEmpty() && !QDir().mkpath(captureDirectory)) {
        std::cerr << "Could not create the theme capture directory.\n";
        return 1;
    }
    const auto capture = [captureDirectory](QWidget* widget,
                                             const QString& name) {
        if (captureDirectory.isEmpty())
            return true;
        const QString path =
            QDir(captureDirectory).filePath(name + QStringLiteral(".png"));
        if (!widget || !widget->grab().save(path)) {
            std::cerr << "Could not capture theme preview: "
                      << path.toStdString() << "\n";
            return false;
        }
        return true;
    };
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        std::cerr << "Could not create temporary directory.\n";
        return 1;
    }

    const QString portableApplicationDirectory =
        temporaryDirectory.filePath("portable-install");
    if (!QDir().mkpath(portableApplicationDirectory)) {
        std::cerr << "Could not create portable path fixture.\n";
        return 1;
    }
    const QString fallbackDataDirectory =
        temporaryDirectory.filePath("profile-data");
    if (ProductImagePaths::usesPortableData(portableApplicationDirectory)
        || ProductImagePaths::applicationDataRoot(
               portableApplicationDirectory, fallbackDataDirectory)
               != fallbackDataDirectory) {
        std::cerr << "A regular installation did not retain per-user storage.\n";
        return 1;
    }
    QFile portableMarker(
        QDir(portableApplicationDirectory).filePath("portable.ini"));
    if (!portableMarker.open(QIODevice::WriteOnly)
        || portableMarker.write("[Application]\nStorage=portable\n") < 0) {
        std::cerr << "Could not create portable marker fixture.\n";
        return 1;
    }
    portableMarker.close();
    const QString expectedPortableDataDirectory =
        QDir(portableApplicationDirectory).filePath("data");
    if (!ProductImagePaths::usesPortableData(portableApplicationDirectory)
        || ProductImagePaths::applicationDataRoot(
               portableApplicationDirectory, fallbackDataDirectory)
               != expectedPortableDataDirectory) {
        std::cerr << "Portable data directory selection failed.\n";
        return 1;
    }

    const QString legacyRoot =
        temporaryDirectory.filePath("development-workspace");
    const QString legacyUploads =
        QDir(legacyRoot).filePath("assets/products/uploads");
    if (!QDir().mkpath(legacyUploads)) {
        std::cerr << "Could not create the database migration fixture.\n";
        return 1;
    }
    const QString legacyDatabasePath =
        QDir(legacyRoot).filePath("bakery_pos.db");
    DatabaseManager legacyDatabase;
    if (!legacyDatabase.open(legacyDatabasePath.toStdString())
        || !legacyDatabase.initializeSchema()) {
        std::cerr << "Could not initialize the legacy database fixture.\n";
        return 1;
    }
    std::string migrationError;
    InventoryRepository legacyInventory(legacyDatabase);
    if (!legacyInventory.adjust("P001", -27, "Migration smoke fixture",
                                migrationError)) {
        std::cerr << "Could not update legacy fixture stock: "
                  << migrationError << "\n";
        return 1;
    }
    legacyDatabase.close();
    const QString legacyImagePath = QDir(legacyUploads).filePath("legacy.png");
    QFile legacyImage(legacyImagePath);
    if (!legacyImage.open(QIODevice::WriteOnly)
        || legacyImage.write("legacy image") < 0) {
        std::cerr << "Could not create the uploaded image migration fixture.\n";
        return 1;
    }
    legacyImage.close();

    const QString applicationDataDirectory =
        temporaryDirectory.filePath("installed-user-data");
    QString migratedDatabasePath;
    if (!AppDataPaths::prepareDatabasePath(
            applicationDataDirectory, {legacyRoot}, {legacyRoot},
            migratedDatabasePath, migrationError)) {
        std::cerr << "Legacy database migration failed: "
                  << migrationError << "\n";
        return 1;
    }
    const QString migratedImagePath = QDir(applicationDataDirectory)
        .filePath("assets/products/uploads/legacy.png");
    DatabaseManager migratedDatabase;
    if (migratedDatabasePath
            != QDir(applicationDataDirectory).filePath("bakery_pos.db")
        || !QFileInfo::exists(legacyDatabasePath)
        || !QFileInfo::exists(migratedImagePath)
        || !migratedDatabase.open(migratedDatabasePath.toStdString())) {
        std::cerr << "Migration did not preserve the database and uploaded image.\n";
        return 1;
    }
    int migratedStock = 0;
    InventoryRepository migratedInventory(migratedDatabase);
    if (!migratedInventory.getStock("P001", migratedStock, migrationError)
        || migratedStock != 73
        || ProductImagePaths::resolve(
               "assets/products/uploads/legacy.png",
               applicationDataDirectory, legacyRoot)
               != migratedImagePath) {
        std::cerr << "Migrated data paths or stock were not preserved (stock="
                  << migratedStock << ", expected image="
                  << migratedImagePath.toStdString() << ", resolved image="
                  << ProductImagePaths::resolve(
                         "assets/products/uploads/legacy.png",
                         applicationDataDirectory, legacyRoot).toStdString()
                  << ", error=" << migrationError << ").\n";
        return 1;
    }
    migratedDatabase.close();

    const QString existingPortableDataDirectory =
        temporaryDirectory.filePath("existing-portable-data");
    if (!QDir().mkpath(existingPortableDataDirectory)) {
        std::cerr << "Could not create existing portable database fixture.\n";
        return 1;
    }
    const QString existingPortableDatabasePath =
        QDir(existingPortableDataDirectory).filePath("bakery_pos.db");
    DatabaseManager existingPortableDatabase;
    if (!existingPortableDatabase.open(
            existingPortableDatabasePath.toStdString())
        || !existingPortableDatabase.initializeSchema()) {
        std::cerr << "Could not initialize existing portable database fixture.\n";
        return 1;
    }
    InventoryRepository existingPortableInventory(existingPortableDatabase);
    if (!existingPortableInventory.adjust(
            "P001", -31, "Portable database preservation fixture",
            migrationError)) {
        std::cerr << "Could not update existing portable database fixture: "
                  << migrationError << "\n";
        return 1;
    }
    existingPortableDatabase.close();

    QString selectedPortableDatabasePath;
    if (!AppDataPaths::prepareDatabasePath(
            existingPortableDataDirectory, {legacyRoot}, {},
            selectedPortableDatabasePath, migrationError)
        || selectedPortableDatabasePath != existingPortableDatabasePath
        || !existingPortableDatabase.open(
            selectedPortableDatabasePath.toStdString())) {
        std::cerr << "Existing portable database was not retained: "
                  << migrationError << "\n";
        return 1;
    }
    int preservedPortableStock = 0;
    InventoryRepository preservedPortableInventory(existingPortableDatabase);
    if (!preservedPortableInventory.getStock(
            "P001", preservedPortableStock, migrationError)
        || preservedPortableStock != 69) {
        std::cerr << "Existing portable database contents were overwritten.\n";
        return 1;
    }
    existingPortableDatabase.close();

    const std::string databasePath =
        temporaryDirectory.filePath("navigation-test.db").toStdString();
    MainWindow window(databasePath);
    const QString sharedStyle = application.styleSheet();
    if (application.font().family() != QStringLiteral("Google Sans Flex")
        || !sharedStyle.contains("font-family: \"Google Sans Flex\"")) {
        std::cerr << "Google Sans Flex is not applied as the shared UI font.\n";
        return 1;
    }
    QString quantityButtonStyle;
    const int quantityButtonStart =
        sharedStyle.indexOf("QSpinBox::up-button, QSpinBox::down-button");
    if (quantityButtonStart >= 0) {
        const int quantityButtonEnd =
            sharedStyle.indexOf("QLineEdit:disabled", quantityButtonStart);
        if (quantityButtonEnd > quantityButtonStart) {
            quantityButtonStyle = sharedStyle.mid(
                quantityButtonStart, quantityButtonEnd - quantityButtonStart);
        }
    }
    if (!sharedStyle.contains("#FFF9F2")
        || !sharedStyle.contains("#F3CDD5")
        || !sharedStyle.contains("stop:0 #F8D9E1")
        || !sharedStyle.contains("#C97995")
        || !sharedStyle.contains("QPushButton#sidebarToggle")
        || !sharedStyle.contains("QDialog, QMessageBox")
        || !sharedStyle.contains("QWidget#loginPage")
        || !sharedStyle.contains("QStackedWidget#pageStack")
        || !sharedStyle.contains("QScrollArea QWidget#pageViewport")
        || !sharedStyle.contains("QToolButton")
        || !sharedStyle.contains("QSpinBox::up-button")
        || !sharedStyle.contains("QTableWidget::item:hover")
        || !sharedStyle.contains("QHeaderView::section")
        || !sharedStyle.contains("font-weight: bold")
        || !sharedStyle.contains("QLabel#productImagePreview")
        || !sharedStyle.contains("QPlainTextEdit#transactionReceipt")
        || !sharedStyle.contains("QSplitter::handle:hover")
        || !sharedStyle.contains("QFrame#contentCard")
        || !sharedStyle.contains("qlineargradient")
        || !quantityButtonStyle.contains("stop:0 #F5F1EB, stop:1 #E9E2D9")
        || !quantityButtonStyle.contains("QSpinBox::up-button:hover")
        || !quantityButtonStyle.contains("QSpinBox::up-button:pressed")
        || !quantityButtonStyle.contains("QSpinBox::up-button:disabled")
        || quantityButtonStyle.contains("#F8D9E1")
        || quantityButtonStyle.contains("#EDA8BA")) {
        std::cerr << "The shared theme or neutral quantity button states are missing.\n";
        return 1;
    }
    for (QPushButton* button : window.findChildren<QPushButton*>()) {
        if (!button->styleSheet().isEmpty()) {
            std::cerr << "A button bypasses the shared pink button styling.\n";
            return 1;
        }
    }
    auto* pageStack = window.findChild<QStackedWidget*>("pageStack");
    auto* applicationStateStack =
        window.findChild<QStackedWidget*>("applicationStateStack");
    auto* operatorButton = window.findChild<QPushButton*>("currentOperatorButton");
    if (!pageStack || pageStack->count() != 6 || !applicationStateStack
        || window.currentPageIndex() != 0) {
        std::cerr << "Main window did not initialize all six navigation pages.\n";
        return 1;
    }
    if (!operatorButton || operatorButton->text() != "Not logged in"
        || !window.findChild<QWidget*>("administratorSetupPage")) {
        std::cerr << "First-launch administrator setup was not shown.\n";
        return 1;
    }
    if (!window.findChild<QWidget*>("salesPage")) {
        std::cerr << "The point-of-sale page is not integrated into the main window.\n";
        return 1;
    }
    if (!window.findChild<QWidget*>("dashboardPage")
        || !window.findChild<QLabel*>("dashboardSalesValue")
        || !window.findChild<QTableWidget*>("dashboardRecentTransactions")
        || !window.findChild<QTableWidget*>("dashboardLowStockProducts")) {
        std::cerr << "The dashboard and its data sections are not integrated.\n";
        return 1;
    }
    auto* salesValue = window.findChild<QLabel*>("dashboardSalesValue");
    auto* transactionCount =
        window.findChild<QLabel*>("dashboardTransactionCount");
    auto* productCount = window.findChild<QLabel*>("dashboardTotalProducts");
    auto* lowStockCount = window.findChild<QLabel*>("dashboardLowStockCount");
    if (!salesValue || !transactionCount || !productCount || !lowStockCount
        || salesValue->text() != "PHP 0.00"
        || transactionCount->text() != "0"
        || productCount->text() != "8"
        || lowStockCount->text() != "0") {
        std::cerr << "Dashboard KPI values do not match the temporary database.\n";
        return 1;
    }
    if (!window.findChild<QWidget*>("productsPage")
        || !window.findChild<QWidget*>("inventoryPage")) {
        std::cerr << "Product and inventory pages are not integrated into the main window.\n";
        return 1;
    }
    if (!window.findChild<QWidget*>("transactionsPage")
        || !window.findChild<QWidget*>("reportsPage")) {
        std::cerr << "Transaction and reports pages are not integrated into the main window.\n";
        return 1;
    }

    auto* sidebarToggle =
        window.findChild<QPushButton*>("sidebarToggle");
    auto* sidebar =
        window.findChild<QWidget*>("sidebar");
    if (!sidebarToggle || !sidebar) {
        std::cerr << "Collapsible sidebar controls are missing.\n";
        return 1;
    }
    window.show();
    QCoreApplication::processEvents();
    auto* pageTitle = window.findChild<QLabel*>("pageTitle");
    if (!capture(&window, "administrator-setup")) {
        return 1;
    }
    auto* setupUsername = window.findChild<QLineEdit*>("setupUsernameInput");
    auto* setupPassword = window.findChild<QLineEdit*>("setupPasswordInput");
    auto* setupConfirmation =
        window.findChild<QLineEdit*>("setupConfirmPasswordInput");
    auto* createAdministrator =
        window.findChild<QPushButton*>("createAdministratorButton");
    if (!setupUsername || !setupPassword || !setupConfirmation
        || !createAdministrator) {
        std::cerr << "Administrator setup form controls are missing.\n";
        return 1;
    }
    setupUsername->setText("test_admin");
    setupPassword->setText("NavigationTestPassword!23");
    setupConfirmation->setText("mismatch");
    createAdministrator->click();
    QCoreApplication::processEvents();
    if (applicationStateStack->currentWidget()
            != window.findChild<QWidget*>("administratorSetupPage")
        || window.findChild<QLabel*>("administratorSetupMessage")->text().isEmpty()) {
        std::cerr << "Mismatched first-admin passwords were not rejected.\n";
        return 1;
    }
    setupConfirmation->setText("NavigationTestPassword!23");
    createAdministrator->click();
    QCoreApplication::processEvents();
    if (applicationStateStack->currentWidget()
        != window.findChild<QWidget*>("loginPage")) {
        std::cerr << "Successful administrator setup did not redirect to login.\n";
        return 1;
    }
    if (!capture(&window, "login")) {
        return 1;
    }
    auto* loginUsername = window.findChild<QLineEdit*>("loginUsernameInput");
    auto* loginPassword = window.findChild<QLineEdit*>("loginPasswordInput");
    auto* loginButton = window.findChild<QPushButton*>("loginButton");
    if (!loginUsername || !loginPassword || !loginButton) {
        std::cerr << "Login form controls are missing.\n";
        return 1;
    }
    loginUsername->setText("test_admin");
    loginPassword->setText("invalid-password");
    loginButton->click();
    QCoreApplication::processEvents();
    if (applicationStateStack->currentWidget()
            != window.findChild<QWidget*>("loginPage")
        || window.findChild<QLabel*>("loginMessage")->text().isEmpty()) {
        std::cerr << "Invalid login was not rejected.\n";
        return 1;
    }
    loginPassword->setText("NavigationTestPassword!23");
    loginButton->click();
    QCoreApplication::processEvents();
    if (applicationStateStack->currentWidget()
            != window.findChild<QWidget*>("root")
        || operatorButton->text() != "Operator: test_admin") {
        std::cerr << "Valid login did not open the POS as its operator.\n";
        return 1;
    }
    if (!pageTitle || pageTitle->width() < pageTitle->sizeHint().width()) {
        std::cerr << "The page title is clipped by the application header.\n";
        return 1;
    }
    const QList<QPushButton*> headerButtons = {
        operatorButton,
        window.findChild<QPushButton*>("manageOperatorsButton"),
        window.findChild<QPushButton*>("backupDatabaseButton"),
        window.findChild<QPushButton*>("restoreDatabaseButton"),
        window.findChild<QPushButton*>("logoutButton")
    };
    for (QPushButton* button : headerButtons) {
        if (button && button->isVisible()
            && button->width() < button->sizeHint().width()) {
            std::cerr << "An application header action is clipped.\n";
            return 1;
        }
    }
    if (!capture(&window, "dashboard")) {
        return 1;
    }
    auto* manageOperators =
        window.findChild<QPushButton*>("manageOperatorsButton");
    bool managementDialogOpened = false;
    bool managementDialogUsesSharedTheme = false;
    if (!manageOperators || !manageOperators->isVisible()) {
        std::cerr << "Administrator account management control is missing.\n";
        return 1;
    }
    QTimer::singleShot(0, [&managementDialogOpened,
                            &managementDialogUsesSharedTheme,
                            &capture]() {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            auto* dialog = qobject_cast<QDialog*>(widget);
            if (!dialog || dialog->windowTitle() != "Manage operator accounts")
                continue;
            auto* accounts = dialog->findChild<QTableWidget*>(
                "operatorAccountsTable");
            managementDialogOpened = accounts && accounts->rowCount() == 1
                && accounts->item(0, 0)
                && accounts->item(0, 0)->text() == "test_admin";
            managementDialogUsesSharedTheme = dialog->styleSheet().isEmpty();
            for (QPushButton* button : dialog->findChildren<QPushButton*>()) {
                managementDialogUsesSharedTheme =
                    managementDialogUsesSharedTheme
                    && button->styleSheet().isEmpty();
            }
            managementDialogUsesSharedTheme =
                managementDialogUsesSharedTheme
                && capture(dialog, "operator-settings-dialog");
            dialog->accept();
            return;
        }
    });
    manageOperators->click();
    if (!managementDialogOpened || !managementDialogUsesSharedTheme) {
        std::cerr << "Administrator dialog failed to load or bypassed the shared theme.\n";
        return 1;
    }
    sidebarToggle->click();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    auto* reopenToggle =
        window.findChild<QPushButton*>("sidebarReopenToggle");
    if (sidebar->isVisible()
        || !reopenToggle
        || !reopenToggle->isVisible()
        || reopenToggle->icon().isNull()) {
        std::cerr << "Sidebar did not disappear or expose the reopen toggle.\n";
        return 1;
    }
    reopenToggle->click();
    QCoreApplication::processEvents();
    if (!sidebar->isVisible()
        || reopenToggle->isVisible()
        || sidebarToggle->icon().isNull()) {
        std::cerr << "Sidebar did not expand to text navigation.\n";
        return 1;
    }

    for (int pageIndex = 0; pageIndex < 6; ++pageIndex) {
        auto* scrollArea = window.findChild<QScrollArea*>(
            QStringLiteral("pageScrollArea%1").arg(pageIndex));
        if (!scrollArea || pageStack->widget(pageIndex) != scrollArea
            || !scrollArea->widget() || !scrollArea->widgetResizable()
            || scrollArea->verticalScrollBarPolicy() != Qt::ScrollBarAsNeeded
            || !scrollArea->styleSheet().isEmpty()
            || !scrollArea->viewport()->styleSheet().isEmpty()
            || scrollArea->viewport()->objectName() != "pageViewport"
            || scrollArea->isAncestorOf(pageTitle)) {
            std::cerr << "Page " << pageIndex
                      << " is not scrollable or the global header moved into it.\n";
            return 1;
        }
    }
    auto* firstPageScroll = window.findChild<QScrollArea*>("pageScrollArea0");
    QWidget* firstPage = firstPageScroll->widget();
    firstPage->setMinimumHeight(firstPageScroll->viewport()->height() + 100);
    QCoreApplication::processEvents();
    if (firstPageScroll->verticalScrollBar()->maximum() <= 0) {
        std::cerr << "An oversized page did not expose a vertical scrollbar.\n";
        return 1;
    }
    firstPage->setMinimumHeight(0);

    for (int pageIndex = 0; pageIndex < 6; ++pageIndex) {
        const auto buttonName = QStringLiteral("nav%1").arg(pageIndex);
        auto* button = window.findChild<QPushButton*>(buttonName);
        if (!button || !button->isEnabled()) {
            std::cerr << "A navigation button is missing or disabled.\n";
            return 1;
        }
        button->click();
        QCoreApplication::processEvents();
        if (window.currentPageIndex() != pageIndex
            || pageStack->currentIndex() != pageIndex
            || !button->isChecked()
            || pageTitle->width() < pageTitle->sizeHint().width()) {
            std::cerr << "Navigation failed for page index " << pageIndex << ".\n";
            return 1;
        }
        static const char* pageCaptureNames[] = {
            "dashboard", "pos", "products", "inventory", "transactions", "reports"
        };
        if (!capture(&window,
                     QString::fromLatin1(pageCaptureNames[pageIndex]))) {
            return 1;
        }
    }

    window.navigateTo(-1);
    if (window.currentPageIndex() != 5) {
        std::cerr << "Invalid navigation changed the current page.\n";
        return 1;
    }
    auto* logoutButton = window.findChild<QPushButton*>("logoutButton");
    if (!logoutButton || !logoutButton->isVisible()) {
        std::cerr << "Authenticated logout control is missing.\n";
        return 1;
    }
    logoutButton->click();
    QCoreApplication::processEvents();
    if (applicationStateStack->currentWidget()
        != window.findChild<QWidget*>("loginPage")) {
        std::cerr << "Logout did not return to the login page.\n";
        return 1;
    }
    PosController controller(
        temporaryDirectory.filePath("controller-test.db").toStdString());
    std::string error;
    if (!controller.isReady()) {
        std::cerr << "Controller database initialization failed: " << error << "\n";
        return 1;
    }
    if (!controller.addToCart("P001", 2, error)
        || !controller.updateCart("P001", 3, error)
        || controller.cartItems().size() != 1
        || controller.cartTotal() != 195.0
        || !controller.removeFromCart("P001", error)
        || !controller.cartItems().empty()) {
        std::cerr << "Controller cart operations failed: " << error << "\n";
        return 1;
    }

    std::cout << "All six navigation pages passed.\n";
    return 0;
}
