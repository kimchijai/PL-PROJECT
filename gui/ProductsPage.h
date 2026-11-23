#ifndef PRODUCTSPAGE_H
#define PRODUCTSPAGE_H

#include "PosController.h"

#include <QWidget>

#include <vector>

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QLabel;

class ProductsPage final : public QWidget {
public:
    explicit ProductsPage(PosController& controller, QWidget* parent = nullptr);

private:
    bool loadProducts(const std::string& term, std::string& error);
    void searchProducts();
    void selectProduct();
    void clearForm();
    void addProduct();
    void saveProduct();
    void deactivateProduct();
    void deleteProduct();
    void chooseImage();
    bool refreshAfterChange(const std::string& selectedId, bool resetSearch);
    void setMessage(const QString& message, bool isError);
    static bool parsePriceCents(const QString& text, int& cents);
    static QString formatMoney(int cents);

    PosController& controller_;
    QLineEdit* searchInput_;
    QLineEdit* productIdInput_;
    QLineEdit* nameInput_;
    QLineEdit* priceInput_;
    QSpinBox* initialStockInput_;
    QTableWidget* productTable_;
    QLabel* messageLabel_;
    QPushButton* addButton_;
    QPushButton* saveButton_;
    QPushButton* deactivateButton_;
    QPushButton* deleteButton_;
    std::vector<ProductRecord> displayedProducts_;
    std::string selectedProductId_;
    std::string selectedImagePath_;
    QLabel* imagePreview_;
    QLineEdit* imagePathInput_;
};

#endif
