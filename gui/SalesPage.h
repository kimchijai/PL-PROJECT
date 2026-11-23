#ifndef SALESPAGE_H
#define SALESPAGE_H

#include "PosController.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

class SalesPage final : public QWidget {
public:
    explicit SalesPage(PosController& controller, QWidget* parent = nullptr);

private:
    bool loadProducts(std::string& error);
    void refreshProducts();
    void refreshCart(int selectedRow = -1);
    void addSelectedProduct();
    void updateSelectedQuantity();
    void removeSelectedProduct();
    void checkout();
    void updateCartControls();
    void setMessage(const QString& message, bool isError);
    QString buildReceipt(const TransactionRecord& transaction,
                         const std::vector<TransactionItemRecord>& items) const;
    static bool parseCashCents(const QString& text, int& cents);
    static QString formatMoney(int cents);
    static QString formatMoney(double amount);

    PosController& controller_;
    QTableWidget* productTable_;
    QTableWidget* cartTable_;
    QSpinBox* addQuantity_;
    QSpinBox* cartQuantity_;
    QLineEdit* cashInput_;
    QLabel* totalLabel_;
    QLabel* messageLabel_;
    QPlainTextEdit* receiptView_;
    QPushButton* addButton_;
    QPushButton* updateButton_;
    QPushButton* removeButton_;
    QPushButton* checkoutButton_;
};

#endif
