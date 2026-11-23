#ifndef INVENTORYPAGE_H
#define INVENTORYPAGE_H

#include "PosController.h"

#include <QWidget>

#include <vector>

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

class InventoryPage final : public QWidget {
public:
    explicit InventoryPage(PosController& controller, QWidget* parent = nullptr);

private:
    bool loadInventory(const std::string& term, std::string& error);
    void searchInventory();
    void selectProduct();
    void restockSelected();
    void adjustSelected();
    void setMessage(const QString& message, bool isError);
    bool refreshAfterMovement(const std::string& productId);
    void updateActionControls();

    PosController& controller_;
    QLineEdit* searchInput_;
    QTableWidget* inventoryTable_;
    QSpinBox* restockQuantity_;
    QLineEdit* restockReason_;
    QSpinBox* adjustmentAmount_;
    QLineEdit* adjustmentReason_;
    QPushButton* restockButton_;
    QPushButton* adjustButton_;
    QLabel* selectedLabel_;
    QLabel* messageLabel_;
    std::vector<ProductRecord> displayedProducts_;
    std::string selectedProductId_;
};

#endif
