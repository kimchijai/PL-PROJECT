#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

#include "PosController.h"

#include <QWidget>

class QLabel;
class QTableWidget;

class DashboardPage final : public QWidget {
public:
    explicit DashboardPage(PosController& controller, QWidget* parent = nullptr);

private:
    void refresh();
    void setMessage(const QString& message, bool isError);
    static QString formatMoney(int cents);

    PosController& controller_;
    QLabel* salesValue_;
    QLabel* transactionValue_;
    QLabel* productValue_;
    QLabel* lowStockValue_;
    QTableWidget* recentTable_;
    QTableWidget* lowStockTable_;
    QLabel* messageLabel_;
};

#endif
