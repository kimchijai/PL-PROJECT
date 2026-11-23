#include "../gui/InventoryPage.h"
#include "../gui/ProductsPage.h"
#include "../gui/ApplicationFont.h"

#include <QApplication>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QTemporaryDir>
#include <QStandardPaths>

#include "../DatabaseManager.h"
#include "../InventoryRepository.h"
#include "../gui/ProductImagePaths.h"

#include <iostream>

namespace {
int findProductRow(QTableWidget* table, int idColumn, const QString& productId) {
    for (int row = 0; row < table->rowCount(); ++row) {
        if (table->item(row, idColumn)
            && table->item(row, idColumn)->text() == productId) {
            return row;
        }
    }
    return -1;
}
}

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QString fontError;
    if (!GuiApplicationFont::load(fontError)) {
        std::cerr << fontError.toStdString() << "\n";
        return 1;
    }
    QCoreApplication::setOrganizationName("Crombi Tests");
    QCoreApplication::setApplicationName("Product Inventory Smoke");
    QStandardPaths::setTestModeEnabled(true);
    const char* productIds[] = {
        "P001", "P002", "P003", "P004", "P005", "P006", "P007", "P008"
    };
    for (const char* productId : productIds) {
        if (ProductImagePaths::loadProductImage(
                QString::fromLatin1(productId),
                "assets/products/missing-product-image.jpg").isNull()) {
            std::cerr << "Embedded image fallback failed for "
                      << productId << ".\n";
            return 1;
        }
    }

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        std::cerr << "Could not create temporary directory.\n";
        return 1;
    }

    const std::string databasePath =
        temporaryDirectory.filePath("products-inventory-test.db").toStdString();
    QString copiedProductImagePath;
    {
        PosController controller(databasePath);
        if (!controller.isReady()) {
            std::cerr << "Test database initialization failed: "
                      << controller.errorMessage() << "\n";
            return 1;
        }
        std::string operatorError;
        if (!controller.createInitialAdministrator(
                "inventory_smoke", "InventorySmokePassword!23", operatorError)
            || !controller.login("inventory_smoke",
                                 "InventorySmokePassword!23", operatorError)) {
            std::cerr << "Could not create or log in test operator: "
                      << operatorError << "\n";
            return 1;
        }

        {
            ProductsPage productsPage(controller);
            auto* productTable = productsPage.findChild<QTableWidget*>("productsTable");
            auto* searchInput =
                productsPage.findChild<QLineEdit*>("productsSearchInput");
            auto* searchButton =
                productsPage.findChild<QPushButton*>("productsSearchButton");
            auto* refreshButton =
                productsPage.findChild<QPushButton*>("productsRefreshButton");
            auto* priceInput = productsPage.findChild<QLineEdit*>("productPriceInput");
            auto* saveButton = productsPage.findChild<QPushButton*>("saveProductButton");
            auto* addButton = productsPage.findChild<QPushButton*>("addProductButton");
            auto* deactivateButton =
                productsPage.findChild<QPushButton*>("deactivateProductButton");
            auto* deleteButton =
                productsPage.findChild<QPushButton*>("deleteProductButton");
            auto* productIdInput = productsPage.findChild<QLineEdit*>("productIdInput");
            auto* productNameInput = productsPage.findChild<QLineEdit*>("productNameInput");
            auto* initialStockInput = productsPage.findChild<QSpinBox*>("initialStockInput");
            auto* chooseImageButton =
                productsPage.findChild<QPushButton*>("chooseProductImageButton");
            auto* imagePathInput =
                productsPage.findChild<QLineEdit*>("productImagePath");
            auto* productsMessage = productsPage.findChild<QLabel*>("productsMessage");
            if (!productTable || !searchInput || !searchButton || !refreshButton
                || !priceInput || !saveButton || !addButton
                || !deactivateButton || !deleteButton
                || !productIdInput || !productNameInput
                || !initialStockInput || !chooseImageButton || !imagePathInput
                || !productsMessage || productTable->rowCount() != 8) {
                std::cerr << "Products page did not load the seeded catalog and controls.\n";
                return 1;
            }

            searchInput->setText("Nutella");
            searchButton->click();
            if (productTable->rowCount() != 1
                || findProductRow(productTable, 1, "P006") < 0) {
                std::cerr << "Product search did not filter the seeded catalog.\n";
                return 1;
            }
            refreshButton->click();
            if (productTable->rowCount() != 8) {
                std::cerr << "Showing all products did not restore the catalog.\n";
                return 1;
            }

            int nutellaRow = findProductRow(productTable, 1, "P006");
            if (nutellaRow < 0
                || productTable->item(nutellaRow, 0)->icon().isNull()
                || productTable->item(nutellaRow, 2)->text() != "Nutella"
                || productTable->item(nutellaRow, 4)->text() != "PHP 65.00") {
                std::cerr << "Products page did not display the seeded Nutella record.\n";
                return 1;
            }
            productTable->selectRow(nutellaRow);
            priceInput->setText("66.00");
            saveButton->click();
            nutellaRow = findProductRow(productTable, 1, "P006");
            if (nutellaRow < 0
                || productTable->item(nutellaRow, 4)->text() != "PHP 66.00"
                || !productsMessage->text().contains("updated successfully")) {
                std::cerr << "Product edit was not applied or reported successfully.\n";
                return 1;
            }

            auto* clearButton =
                productsPage.findChild<QPushButton*>("clearProductFormButton");
            if (!clearButton) {
                std::cerr << "Products page is missing its clear-form control.\n";
                return 1;
            }
            clearButton->click();
            productIdInput->setText("P900");
            productNameInput->setText("Temporary Test Product");
            priceInput->setText("-1");
            initialStockInput->setValue(4);
            const QString sourceImagePath =
                temporaryDirectory.filePath("product-upload.png");
            QImage sourceImage(16, 16, QImage::Format_RGB32);
            sourceImage.fill(Qt::magenta);
            if (!sourceImage.save(sourceImagePath)) {
                std::cerr << "Could not create an image upload fixture.\n";
                return 1;
            }
            QTimer::singleShot(0, [sourceImagePath]() {
                auto* dialog = qobject_cast<QFileDialog*>(
                    QApplication::activeModalWidget());
                if (!dialog) {
                    for (QWidget* widget : QApplication::topLevelWidgets()) {
                        dialog = qobject_cast<QFileDialog*>(widget);
                        if (dialog) break;
                    }
                }
                if (dialog) {
                    dialog->selectFile(sourceImagePath);
                    QMetaObject::invokeMethod(
                        dialog, "accept", Qt::DirectConnection);
                }
            });
            chooseImageButton->click();
            if (imagePathInput->text().isEmpty()) {
                std::cerr << "Selecting a product image did not update the form.\n";
                return 1;
            }
            addButton->click();
            if (findProductRow(productTable, 1, "P900") >= 0
                || !productsMessage->text().contains("valid non-negative price")) {
                std::cerr << "Invalid product price was not rejected by the form.\n";
                return 1;
            }
            priceInput->setText("9.99");
            addButton->click();
            int temporaryRow = findProductRow(productTable, 1, "P900");
            if (temporaryRow < 0
                || productTable->item(temporaryRow, 0)->icon().isNull()
                || productTable->item(temporaryRow, 5)->text() != "4"
                || !productsMessage->text().contains("added successfully")) {
                std::cerr << "Adding a product did not create its initial stock.\n";
                return 1;
            }
            std::vector<ProductRecord> uploadedProduct;
            std::string imageError;
            if (!controller.searchProducts("P900", uploadedProduct, imageError)
                || uploadedProduct.size() != 1
                || uploadedProduct[0].imagePath.find(
                       "assets/products/uploads/P900_") != 0
                || uploadedProduct[0].imagePath.size() < 4
                || uploadedProduct[0].imagePath.substr(
                       uploadedProduct[0].imagePath.size() - 4) != ".png") {
                std::cerr << "Uploaded image path was not persisted relatively: "
                          << imageError << " (records=" << uploadedProduct.size()
                          << (uploadedProduct.empty()
                                  ? std::string()
                                  : ", path=" + uploadedProduct[0].imagePath)
                          << ")\n";
                return 1;
            }
            copiedProductImagePath = ProductImagePaths::resolve(
                QString::fromStdString(uploadedProduct[0].imagePath));
            if (!QFileInfo::exists(copiedProductImagePath)) {
                std::cerr << "Uploaded image was not copied into the assets directory"
                          << " (stored path=" << uploadedProduct[0].imagePath
                          << ", resolved path="
                          << copiedProductImagePath.toStdString()
                          << ", data root="
                          << ProductImagePaths::applicationDataRoot().toStdString()
                          << ").\n";
                return 1;
            }
            productTable->selectRow(temporaryRow);
            deactivateButton->click();
            temporaryRow = findProductRow(productTable, 1, "P900");
            if (temporaryRow < 0
                || productTable->item(temporaryRow, 6)->text() != "Inactive"
                || !productsMessage->text().contains("deactivated")) {
                std::cerr << "Product deactivation was not applied or reported.\n";
                return 1;
            }

            auto confirmDelete = [deleteButton](QMessageBox::StandardButton button) {
                QTimer::singleShot(0, [button]() {
                    auto* dialog = qobject_cast<QMessageBox*>(
                        QApplication::activeModalWidget());
                    if (!dialog) {
                        for (QWidget* widget : QApplication::topLevelWidgets()) {
                            dialog = qobject_cast<QMessageBox*>(widget);
                            if (dialog) break;
                        }
                    }
                    if (dialog && dialog->button(button))
                        dialog->button(button)->click();
                });
                deleteButton->click();
            };
            confirmDelete(QMessageBox::Yes);
            if (findProductRow(productTable, 1, "P900") >= 0
                || !productsMessage->text().contains("permanently deleted")) {
                std::cerr << "Product with remaining stock was not deleted.\n";
                return 1;
            }

            clearButton->click();
            productIdInput->setText("P901");
            productNameInput->setText("Deletable Test Product");
            priceInput->setText("1.00");
            initialStockInput->setValue(0);
            addButton->click();
            int deletableRow = findProductRow(productTable, 1, "P901");
            if (deletableRow < 0) {
                std::cerr << "Could not create an eligible product for deletion.\n";
                return 1;
            }
            productTable->selectRow(deletableRow);
            confirmDelete(QMessageBox::No);
            if (findProductRow(productTable, 1, "P901") < 0) {
                std::cerr << "Declining deletion did not preserve the product.\n";
                return 1;
            }
            confirmDelete(QMessageBox::Yes);
            if (findProductRow(productTable, 1, "P901") >= 0
                || !productsMessage->text().contains("permanently deleted")) {
                std::cerr << "Confirmed product deletion did not update the catalog.\n";
                return 1;
            }
        }

        {
            InventoryPage inventoryPage(controller);
            auto* inventoryTable =
                inventoryPage.findChild<QTableWidget*>("inventoryTable");
            auto* restockQuantity =
                inventoryPage.findChild<QSpinBox*>("restockQuantity");
            auto* restockReason =
                inventoryPage.findChild<QLineEdit*>("restockReason");
            auto* restockButton =
                inventoryPage.findChild<QPushButton*>("restockButton");
            auto* adjustmentAmount =
                inventoryPage.findChild<QSpinBox*>("adjustmentAmount");
            auto* adjustmentReason =
                inventoryPage.findChild<QLineEdit*>("adjustmentReason");
            auto* adjustmentButton =
                inventoryPage.findChild<QPushButton*>("adjustStockButton");
            auto* inventoryMessage =
                inventoryPage.findChild<QLabel*>("inventoryMessage");
            if (!inventoryTable || !restockQuantity || !restockReason || !restockButton
                || !adjustmentAmount || !adjustmentReason || !adjustmentButton
                || !inventoryMessage || inventoryTable->rowCount() != 8) {
                std::cerr << "Inventory page did not load product stock and controls.\n";
                return 1;
            }
            if (findProductRow(inventoryTable, 0, "P900") >= 0) {
                std::cerr << "Deleted product remained in the inventory listing.\n";
                return 1;
            }

            int nutellaRow = findProductRow(inventoryTable, 0, "P006");
            if (nutellaRow < 0 || inventoryTable->item(nutellaRow, 2)->text() != "100") {
                std::cerr << "Inventory page did not show the current seeded stock.\n";
                return 1;
            }
            inventoryTable->selectRow(nutellaRow);

            restockButton->click();
            if (!inventoryMessage->text().contains("Enter a reason")
                || inventoryTable->item(nutellaRow, 2)->text() != "100") {
                std::cerr << "A restock without a reason was not rejected.\n";
                return 1;
            }

            adjustmentAmount->setValue(-98);
            adjustmentReason->setText("Prepare low-stock test");
            adjustmentButton->click();
            nutellaRow = findProductRow(inventoryTable, 0, "P006");
            if (nutellaRow < 0 || inventoryTable->item(nutellaRow, 2)->text() != "2"
                || inventoryTable->item(nutellaRow, 3)->text() != "Low stock"
                || inventoryTable->item(nutellaRow, 5)->text()
                    != "inventory_smoke") {
                std::cerr << "Stock adjustment or low-stock status failed.\n";
                return 1;
            }

            adjustmentAmount->setValue(-2);
            adjustmentReason->setText("Sell remaining stock");
            adjustmentButton->click();
            nutellaRow = findProductRow(inventoryTable, 0, "P006");
            if (nutellaRow < 0 || inventoryTable->item(nutellaRow, 2)->text() != "0"
                || inventoryTable->item(nutellaRow, 3)->text() != "Out of stock") {
                std::cerr << "Out-of-stock status was not displayed.\n";
                return 1;
            }

            restockQuantity->setValue(7);
            restockReason->setText("Test delivery");
            restockButton->click();
            nutellaRow = findProductRow(inventoryTable, 0, "P006");
            if (nutellaRow < 0 || inventoryTable->item(nutellaRow, 2)->text() != "7"
                || !inventoryMessage->text().contains("Restock recorded")) {
                std::cerr << "Restock was not applied or reported successfully.\n";
                return 1;
            }

            adjustmentAmount->setValue(0);
            adjustmentReason->setText("Zero adjustment");
            adjustmentButton->click();
            if (inventoryTable->item(nutellaRow, 2)->text() != "7"
                || !inventoryMessage->text().contains("cannot be zero")) {
                std::cerr << "Zero stock adjustment was not rejected.\n";
                return 1;
            }

            adjustmentAmount->setValue(-2);
            adjustmentReason->setText("Damaged stock");
            adjustmentButton->click();
            nutellaRow = findProductRow(inventoryTable, 0, "P006");
            if (nutellaRow < 0 || inventoryTable->item(nutellaRow, 2)->text() != "5") {
                std::cerr << "Signed inventory adjustment failed.\n";
                return 1;
            }

            adjustmentAmount->setValue(-1000);
            adjustmentReason->setText("Invalid overdraw");
            adjustmentButton->click();
            nutellaRow = findProductRow(inventoryTable, 0, "P006");
            if (nutellaRow < 0 || inventoryTable->item(nutellaRow, 2)->text() != "5"
                || !inventoryMessage->text().contains("negative")) {
                std::cerr << "Invalid stock adjustment changed stock or lacked feedback.\n";
                return 1;
            }
        }
    }

    {
        PosController reopened(databasePath);
        std::string error;
        std::vector<ProductRecord> records;
        if (!reopened.searchProducts("P006", records, error) || records.size() != 1
            || records[0].priceCents != 6600 || records[0].name != "Nutella") {
            std::cerr << "Product edits did not persist after reopening: " << error << "\n";
            return 1;
        }
        int stock = -1;
        if (!reopened.stock("P006", stock, error) || stock != 5) {
            std::cerr << "Inventory changes did not persist after reopening: " << error << "\n";
            return 1;
        }
        records.clear();
        if (!reopened.searchProducts("P900", records, error) || !records.empty()) {
            std::cerr << "Product deletion did not persist after reopening.\n";
            return 1;
        }
    }

    {
        DatabaseManager database;
        if (!database.open(databasePath) || !database.initializeSchema()) {
            std::cerr << "Could not reopen database to verify inventory log.\n";
            return 1;
        }
        InventoryRepository inventory(database);
        InventoryLog log{};
        std::string error;
        if (!inventory.getLatestLog("P006", log, error)
            || log.quantityChange != -2
            || log.resultingQuantity != 5
            || log.reason != "Damaged stock") {
            std::cerr << "Inventory movement log did not persist: " << error << "\n";
            return 1;
        }
        if (!inventory.getLatestLog("P900", log, error)
            || log.quantityChange != -4
            || log.resultingQuantity != 0
            || log.reason.find("written off") == std::string::npos) {
            std::cerr << "Deleted product stock write-off was not retained: "
                      << error << "\n";
            return 1;
        }
    }
    if (!copiedProductImagePath.isEmpty())
        QFile::remove(copiedProductImagePath);

    std::cout << "Products, inventory updates, and restart persistence passed.\n";
    return 0;
}
