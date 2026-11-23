#include "doctest.h"
#include "../gui/PosController.h"

#include <cstdio>
#include <ctime>

TEST_CASE("POS controller exposes reusable product, cart, checkout, and history operations") {
    const char* databasePath = "tests/phase3_controller_test.db";
    std::remove(databasePath);

    {
        PosController controller(databasePath);
        REQUIRE(controller.isReady());

        std::string error;
        int stock = 0;
        REQUIRE(controller.stock("P001", stock, error));
        CHECK(stock == 100);

        REQUIRE(controller.addToCart("P001", 2, error));
        CHECK(controller.cartTotal() == 130.00);
        REQUIRE(controller.updateCart("P001", 3, error));
        CHECK(controller.cartTotal() == 195.00);
        CHECK_FALSE(controller.addToCart("UNKNOWN", 1, error));
        TransactionRecord failedTransaction;
        std::vector<TransactionItemRecord> failedItems;
        CHECK_FALSE(controller.checkout(20000, failedTransaction, failedItems, error));
        CHECK(controller.cartItems().size() == 1);
        CHECK(error.find("Log in") != std::string::npos);
        REQUIRE(controller.createInitialAdministrator(
            "controller_admin", "ControllerTestPassword!23", error));
        REQUIRE(controller.login("controller_admin",
                                "ControllerTestPassword!23", error));
        CHECK(controller.currentOperatorName() == "controller_admin");
        CHECK_FALSE(controller.login("controller_admin", "wrong password", error));
        CHECK_FALSE(controller.checkout(100, failedTransaction, failedItems, error));
        CHECK(error.find("Insufficient payment") != std::string::npos);

        TransactionRecord transaction;
        std::vector<TransactionItemRecord> savedItems;
        REQUIRE(controller.checkout(20000, transaction, savedItems, error));
        CHECK(transaction.operatorName == "controller_admin");
        CHECK(controller.cartItems().empty());
        REQUIRE(savedItems.size() == 1);
        CHECK(savedItems[0].productName == "Chocolate Chip Cookie");

        std::time_t now = std::time(nullptr);
        std::tm localDate{};
#ifdef _WIN32
        localtime_s(&localDate, &now);
#else
        localtime_r(&now, &localDate);
#endif
        char todayText[11]{};
        REQUIRE(std::strftime(todayText, sizeof(todayText), "%Y-%m-%d",
                              &localDate) == 10);
        DashboardSummary todaySummary{};
        std::vector<TransactionRecord> todayTransactions;
        std::vector<LowStockSummary> todayLowStock;
        REQUIRE(controller.dashboard(todayText, todaySummary, todayTransactions,
                                     todayLowStock, error));
        CHECK(todaySummary.transactionCount == 1);
        REQUIRE(todayTransactions.size()
                == static_cast<std::size_t>(todaySummary.transactionCount));
        CHECK(todayTransactions[0].transactionId == transaction.transactionId);

        REQUIRE(controller.stock("P001", stock, error));
        CHECK(stock == 97);

        REQUIRE(controller.adjustStock("P001", -67, "Set stock to 30", error));
        DashboardSummary summaryAtThreshold{};
        std::vector<TransactionRecord> recentAtThreshold;
        std::vector<LowStockSummary> lowStockAtThreshold;
        REQUIRE(controller.dashboard("2020-01-01", summaryAtThreshold,
                                     recentAtThreshold, lowStockAtThreshold, error));
        CHECK(summaryAtThreshold.lowStockCount == 1);
        CHECK(summaryAtThreshold.transactionCount == 0);
        CHECK(recentAtThreshold.empty());

        REQUIRE(controller.restock("P001", 1, "Set stock above threshold", error));
        DashboardSummary summaryAboveThreshold{};
        std::vector<TransactionRecord> recentAboveThreshold;
        std::vector<LowStockSummary> lowStockAboveThreshold;
        REQUIRE(controller.dashboard("2020-01-01", summaryAboveThreshold,
                                     recentAboveThreshold, lowStockAboveThreshold, error));
        CHECK(summaryAboveThreshold.lowStockCount == 0);

        DashboardSummary summary{};
        std::vector<TransactionRecord> recent;
        std::vector<LowStockSummary> lowStock;
        REQUIRE(controller.dashboard("2020-01-01", summary, recent, lowStock, error));
        CHECK(summary.lowStockCount == 0);
        CHECK(summary.transactionCount == static_cast<int>(recent.size()));
        CHECK(recent.empty());

        TransactionRecord retrieved;
        REQUIRE(controller.transaction(transaction.transactionId, retrieved, error));
        CHECK(retrieved.orderTotalCents == 19500);
        std::vector<TransactionItemRecord> retrievedItems;
        REQUIRE(controller.transactionItems(transaction.transactionId, retrievedItems, error));
        REQUIRE(retrievedItems.size() == 1);
        CHECK(retrievedItems[0].productName == "Chocolate Chip Cookie");

        std::vector<OperatorSalesRecord> operatorSales;
        REQUIRE(controller.dailyOperatorSales(todayText, operatorSales, error));
        REQUIRE(operatorSales.size() == 1);
        CHECK(operatorSales[0].operatorName == "controller_admin");
        CHECK(operatorSales[0].transactionCount == 1);
        CHECK(operatorSales[0].salesCents == 19500);

        InventoryLog latestLog{};
        REQUIRE(controller.latestInventoryLog("P001", latestLog, error));
        CHECK(latestLog.operatorName == "controller_admin");

        TransactionRefundRecord refund{};
        REQUIRE(controller.fullRefund(transaction.transactionId,
                                      "Controller attribution test", refund, error));
        CHECK(refund.operatorName == "controller_admin");
        std::vector<TransactionRefundRecord> refunds;
        REQUIRE(controller.transactionRefunds(transaction.transactionId, refunds, error));
        REQUIRE(refunds.size() == 1);
        CHECK(refunds[0].operatorName == "controller_admin");

        REQUIRE(controller.addToCart("P002", 1, error));
        TransactionRecord voidTarget{};
        REQUIRE(controller.checkout(10000, voidTarget, savedItems, error));
        TransactionRefundRecord voidRecord{};
        REQUIRE(controller.voidTransaction(voidTarget.transactionId,
                                           "Controller void attribution test",
                                           voidRecord, error));
        CHECK(voidRecord.refundType == "void");
        CHECK(voidRecord.operatorName == "controller_admin");

        REQUIRE(controller.addToCart("P003", 2, error));
        TransactionRecord partialTarget{};
        REQUIRE(controller.checkout(20000, partialTarget, savedItems, error));
        TransactionRefundRecord partialRecord{};
        REQUIRE(controller.partialRefund(
            partialTarget.transactionId, {{"P003", 1}},
            "Controller partial refund attribution test", partialRecord, error));
        CHECK(partialRecord.operatorName == "controller_admin");
    }

    {
        PosController restoredController(databasePath);
        REQUIRE(restoredController.isReady());
        CHECK(restoredController.currentOperatorName().empty());
        CHECK_FALSE(restoredController.isAuthenticated());
        std::string error;
        REQUIRE(restoredController.login(
            "controller_admin", "ControllerTestPassword!23", error));
        CHECK(restoredController.currentOperatorName() == "controller_admin");
    }

    std::remove(databasePath);
}
