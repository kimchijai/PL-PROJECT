#ifndef TRANSACTIONSPAGE_H
#define TRANSACTIONSPAGE_H

#include "PosController.h"

#include <QWidget>

#include <vector>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;
class QPushButton;

class TransactionsPage final : public QWidget {
public:
    explicit TransactionsPage(PosController& controller, QWidget* parent = nullptr);

private:
    void refreshHistory();
    void loadSelectedTransaction();
    void searchTransaction();
    bool showTransaction(int transactionId);
    void clearDetails();
    void setMessage(const QString& message, bool isError);
    void exportSelectedPdf();
    void printSelectedReceipt();
    void fullRefundSelected();
    void partialRefundSelected();
    void voidSelected();

    PosController& controller_;
    QLineEdit* searchInput_;
    QTableWidget* historyTable_;
    QTableWidget* itemsTable_;
    QPlainTextEdit* receiptView_;
    QLabel* messageLabel_;
    std::vector<TransactionRecord> displayedTransactions_;
    QPushButton* pdfButton_;
    QPushButton* printButton_;
    QPushButton* fullRefundButton_;
    QPushButton* partialRefundButton_;
    QPushButton* voidButton_;
    TransactionRecord selectedTransaction_;
    std::vector<TransactionItemRecord> selectedItems_;
};

#endif
