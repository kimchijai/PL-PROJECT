#ifndef OPERATORMANAGEMENTDIALOG_H
#define OPERATORMANAGEMENTDIALOG_H

#include "PosController.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

class OperatorManagementDialog final : public QDialog {
public:
    explicit OperatorManagementDialog(PosController& controller,
                                      QWidget* parent = nullptr);

private:
    void refreshAccounts();
    void loadSelectedAccount();
    void clearSelection();
    void saveAccount();
    void deactivateAccount();
    void setMessage(const QString& message, bool isError);

    PosController& controller_;
    QTableWidget* accountsTable_;
    QLineEdit* usernameInput_;
    QLineEdit* passwordInput_;
    QLineEdit* confirmPasswordInput_;
    QComboBox* roleInput_;
    QLabel* messageLabel_;
    QPushButton* saveButton_;
    QPushButton* deactivateButton_;
    int selectedAccountId_;
};

#endif
