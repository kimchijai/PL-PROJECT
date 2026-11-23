#include "../gui/SalesPage.h"

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>

#include <iostream>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        std::cerr << "Could not create temporary directory.\n";
        return 1;
    }

    const std::string databasePath =
        temporaryDirectory.filePath("sales-test.db").toStdString();
    PosController controller(databasePath);
    std::string error;
    if (!controller.isReady()
        || !controller.createInitialAdministrator(
            "sales_smoke", "SalesSmokePassword!23", error)
        || !controller.login("sales_smoke", "SalesSmokePassword!23", error)
        || !controller.adjustStock("P001", -98, "Prepare isolated sales test stock", error)) {
        std::cerr << "Could not prepare the seeded sales test product: " << error << "\n";
        return 1;
    }

    SalesPage page(controller);
    auto* products = page.findChild<QTableWidget*>("productTable");
    auto* cart = page.findChild<QTableWidget*>("cartTable");
    auto* quantity = page.findChild<QSpinBox*>("addQuantity");
    auto* cartQuantity = page.findChild<QSpinBox*>("cartQuantity");
    auto* cash = page.findChild<QLineEdit*>("cashInput");
    auto* addButton = page.findChild<QPushButton*>("addToCartButton");
    auto* updateButton = page.findChild<QPushButton*>("updateCartButton");
    auto* removeButton = page.findChild<QPushButton*>("removeCartButton");
    auto* checkoutButton = page.findChild<QPushButton*>("checkoutButton");
    auto* message = page.findChild<QLabel*>("salesMessage");
    auto* receipt = page.findChild<QPlainTextEdit*>("receiptView");
    if (!products || !cart || !quantity || !cartQuantity || !cash || !addButton
        || !updateButton || !removeButton || !checkoutButton || !message || !receipt) {
        std::cerr << "A required sales page control is missing.\n";
        return 1;
    }

    int saleRow = -1;
    for (int row = 0; row < products->rowCount(); ++row) {
        if (products->item(row, 1)->text() == "P001") {
            saleRow = row;
            break;
        }
    }
    if (saleRow < 0 || products->item(saleRow, 2)->text() != "Chocolate Chip Cookie"
        || products->item(saleRow, 3)->text() != "PHP 65.00"
        || products->item(saleRow, 4)->text() != "2") {
        std::cerr << "Product name, price, or stock was not displayed correctly.\n";
        return 1;
    }

    products->selectRow(saleRow);
    quantity->setValue(3);
    addButton->click();
    if (cart->rowCount() != 0 || !message->text().contains("Insufficient stock")) {
        std::cerr << "Insufficient stock was not handled.\n";
        return 1;
    }

    quantity->setValue(2);
    addButton->click();
    if (cart->rowCount() != 1
        || cart->item(0, 4)->text() != "PHP 130.00"
        || !page.findChild<QLabel*>("orderTotal")->text().contains("PHP 130.00")) {
        std::cerr << "Adding to cart or calculating the order total failed.\n";
        return 1;
    }

    cart->selectRow(0);
    cartQuantity->setValue(1);
    updateButton->click();
    if (cart->rowCount() != 1 || cart->item(0, 3)->text() != "1"
        || cart->item(0, 4)->text() != "PHP 65.00") {
        std::cerr << "Updating the cart quantity failed.\n";
        return 1;
    }
    cartQuantity->setValue(2);
    updateButton->click();
    removeButton->click();
    if (cart->rowCount() != 0) {
        std::cerr << "Removing an item from the cart failed.\n";
        return 1;
    }
    addButton->click();
    if (cart->rowCount() != 1 || cart->item(0, 3)->text() != "2") {
        std::cerr << "Re-adding the product to the cart failed.\n";
        return 1;
    }

    cash->setText("not money");
    checkoutButton->click();
    if (cart->rowCount() != 1 || !message->text().contains("valid cash amount")) {
        std::cerr << "Invalid cash input was not rejected or the cart was cleared.\n";
        return 1;
    }

    cash->setText("1.00");
    checkoutButton->click();
    if (cart->rowCount() != 1 || !message->text().contains("Insufficient payment")) {
        std::cerr << "Insufficient payment was not handled or the cart was cleared.\n";
        return 1;
    }

    if (!controller.adjustStock("P001", -1, "Simulate concurrent sale", error)) {
        std::cerr << "Could not simulate a stock change before checkout: " << error << "\n";
        return 1;
    }
    cash->setText("140.00");
    checkoutButton->click();
    if (cart->rowCount() != 1 || !message->text().contains("stock", Qt::CaseInsensitive)) {
        std::cerr << "Checkout stock revalidation failed or cleared the cart.\n";
        return 1;
    }
    if (!controller.restock("P001", 1, "Restore test stock", error)) {
        std::cerr << "Could not restore stock after the failure test: " << error << "\n";
        return 1;
    }

    cash->setText("140.00");
    checkoutButton->click();
    if (cart->rowCount() != 0 || receipt->isHidden()
        || !receipt->toPlainText().contains("Chocolate Chip Cookie")
        || !receipt->toPlainText().contains("Transaction #")
        || !receipt->toPlainText().contains("Change: PHP 10.00")) {
        std::cerr << "Successful checkout did not show a snapshot-based receipt.\n";
        return 1;
    }

    int remainingStock = -1;
    if (!controller.stock("P001", remainingStock, error) || remainingStock != 0) {
        std::cerr << "Successful checkout did not deduct product stock.\n";
        return 1;
    }

    products->selectRow(saleRow);
    quantity->setValue(1);
    addButton->click();
    if (cart->rowCount() != 0 || !message->text().contains("Insufficient stock")) {
        std::cerr << "Out-of-stock product was allowed into the cart.\n";
        return 1;
    }

    std::vector<TransactionRecord> transactions;
    if (!controller.recentTransactions(transactions, 10, error)
        || transactions.size() != 1) {
        std::cerr << "Successful transaction was not persisted: " << error << "\n";
        return 1;
    }

    std::cout << "Sales workflow, validation, checkout, and receipt passed.\n";
    return 0;
}
