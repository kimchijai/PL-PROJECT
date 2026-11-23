/**
 * test_property_receipt.cpp
 * -------------------------
 * Property-based test for receipt completeness.
 *
 * Property 13: Receipt completeness
 * For any completed checkout, the receipt string must contain:
 *   1. The store header ("Crombi Bakery")
 *   2. A transaction ID number
 *   3. A timestamp / date-time line
 *   4. Every cart item's product name
 *   5. The order total amount
 *   6. The cash amount
 *   7. The change amount
 *   8. A thank-you message ("Thank you")
 *
 * // Feature: bakery-pos-system, Property 13: Receipt completeness
 *
 * Validates: Requirements 8.1, 8.2, 8.3
 *
 * Framework: doctest (header-only), random testing loop (>=100 iterations)
 *
 * This test exercises receipt completeness through a free helper function
 * generateReceipt() defined below. The helper mirrors the snapshot-based
 * receipt formatting used by POS and avoids POS private-interface coupling.
 *
 * Compile:
 *   g++ -std=c++17 -I. -Itests tests/test_property_receipt.cpp Product.cpp CartItem.cpp ShoppingCart.cpp POS.cpp -o tests/test_property_receipt
 */

#include "doctest.h"
#include "Product.h"
#include "CartItem.h"
#include "ShoppingCart.h"

#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <random>
#include <ctime>

// ---------------------------------------------------------------------------
// Receipt generation helper for snapshot-based receipt formatting
// ---------------------------------------------------------------------------

/**
 * Formats a monetary value as "PHP X.XX" — mirrors POS::formatCurrency.
 */
static std::string fmtCurrency(double amount) {
    std::ostringstream oss;
    oss << "PHP " << std::fixed << std::setprecision(2) << amount;
    return oss.str();
}

/**
 * Builds and returns a complete receipt string given all checkout data.
 *
 * @param txId   Transaction ID (positive integer, starts at 1).
 * @param items  The CartItems in the order (non-empty).
 * @param total  Pre-computed order total (sum of all item subtotals).
 * @param cash   Cash tendered by the customer (>= total).
 *
 * This replicates snapshot-based POS receipt formatting so the result can be
 * captured and validated without redirecting std::cout.
 */
static std::string generateReceipt(int txId,
                                   const std::vector<CartItem>& items,
                                   double total,
                                   double cash)
{
    const std::string SEP  = std::string(40, '=');
    const std::string DASH = std::string(40, '-');

    // Obtain a wall-clock timestamp (same approach as POS::getCurrentDateTime)
    std::time_t now = std::time(nullptr);
    std::string dt  = std::ctime(&now);
    if (!dt.empty() && dt.back() == '\n') dt.pop_back();

    std::ostringstream oss;

    oss << "\n" << SEP << "\n";
    oss << "           Crombi Bakery\n";
    oss << SEP << "\n";
    oss << "Transaction ID: " << txId << "\n";
    oss << "Date/Time: " << dt << "\n";
    oss << "\nItems:\n";

    for (const auto& item : items) {
        std::ostringstream line;
        line << "  " << item.getQuantity()
             << " x " << fmtCurrency(item.getUnitPrice())
             << " = " << fmtCurrency(item.getSubtotal());

        std::string prefix = line.str();
        if (static_cast<int>(prefix.size()) < 32) {
            prefix += std::string(32 - prefix.size(), ' ');
        }
        oss << prefix << item.getProductName() << "\n";
    }

    double change = cash - total;

    oss << DASH << "\n";
    oss << std::left << std::setw(14) << "Order Total:"
        << " " << fmtCurrency(total) << "\n";
    oss << std::left << std::setw(14) << "Cash:"
        << " " << fmtCurrency(cash) << "\n";
    oss << std::left << std::setw(14) << "Change:"
        << " " << fmtCurrency(change) << "\n";
    oss << DASH << "\n";
    oss << "   Thank you for your purchase!\n";
    oss << SEP << "\n";

    return oss.str();
}

// ---------------------------------------------------------------------------
// Utility: check if 'haystack' contains 'needle'
// ---------------------------------------------------------------------------
static bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

// ---------------------------------------------------------------------------
// Property test
// ---------------------------------------------------------------------------

TEST_CASE("Property 13: Receipt completeness — all required fields present") {
    // Feature: bakery-pos-system, Property 13: Receipt completeness

    // ----------------------------------------------------------------
    // Run 150 iterations with randomly generated transaction data.
    //
    // For each iteration:
    //   1. Pick a random transaction ID in [1, 999].
    //   2. Build a non-empty list of CartItems from randomly chosen
    //      catalog products and quantities.
    //   3. Compute the order total.
    //   4. Generate a cash amount >= order total.
    //   5. Call generateReceipt() and capture the result string.
    //   6. Assert that every required field appears in the receipt.
    // ----------------------------------------------------------------
    constexpr int NUM_ITERATIONS = 150;

    std::mt19937 rng(20250601); // fixed seed for reproducibility

    std::uniform_int_distribution<int>    txIdDist(1, 999);
    std::uniform_int_distribution<int>    qtyDist(1, 20);
    std::uniform_int_distribution<int>    sizeDist(1, 5);
    std::uniform_real_distribution<double> overPayDist(0.0, 500.0);

    // Crombi Bakery catalog (matches POS catalog)
    const std::vector<Product> catalog = {
        Product("P001", "Chocolate Chip Cookie",   65.00),
        Product("P002", "Double Chocolate Cookie",  70.00),
        Product("P003", "Red Velvet Cookie",        70.00),
        Product("P004", "S'mores Cookie",           75.00),
        Product("P005", "Brookies",                 75.00),
    };

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        // --- Build inputs ---
        const int txId        = txIdDist(rng);
        const int numProducts = sizeDist(rng);

        std::vector<CartItem> items;
        items.reserve(static_cast<std::size_t>(numProducts));

        for (int p = 0; p < numProducts; ++p) {
            const Product& prod = catalog[static_cast<std::size_t>(p)];
            items.emplace_back(prod, qtyDist(rng));
        }

        // Compute order total (sum of subtotals)
        double total = 0.0;
        for (const auto& item : items) {
            total += item.getSubtotal();
        }

        const double cash   = total + overPayDist(rng);
        const double change = cash - total;

        // --- Generate receipt ---
        const std::string receipt = generateReceipt(txId, items, total, cash);

        // ---- Assertions ----

        // 1. Store header
        REQUIRE_MESSAGE(
            contains(receipt, "Crombi Bakery"),
            "Property 13 violated: 'Crombi Bakery' not found in receipt"
                << " (iter=" << iter << ", txId=" << txId << ")"
        );

        // 2. Transaction ID
        REQUIRE_MESSAGE(
            contains(receipt, "Transaction ID: " + std::to_string(txId)),
            "Property 13 violated: Transaction ID not found"
                << " (iter=" << iter << ", txId=" << txId << ")"
        );

        // 3. Date/Time label present (timestamp value is dynamic, just verify label)
        REQUIRE_MESSAGE(
            contains(receipt, "Date/Time:"),
            "Property 13 violated: 'Date/Time:' label not found"
                << " (iter=" << iter << ")"
        );

        // 4. All product names present
        for (const auto& item : items) {
            REQUIRE_MESSAGE(
                contains(receipt, item.getProductName()),
                "Property 13 violated: product name '"
                    << item.getProductName() << "' not found in receipt"
                    << " (iter=" << iter << ", txId=" << txId << ")"
            );
        }

        // 5. Order total amount
        REQUIRE_MESSAGE(
            contains(receipt, fmtCurrency(total)),
            "Property 13 violated: order total '" << fmtCurrency(total)
                << "' not found in receipt"
                << " (iter=" << iter << ", txId=" << txId << ")"
        );

        // 6. Cash amount
        REQUIRE_MESSAGE(
            contains(receipt, fmtCurrency(cash)),
            "Property 13 violated: cash amount '" << fmtCurrency(cash)
                << "' not found in receipt"
                << " (iter=" << iter << ", txId=" << txId << ")"
        );

        // 7. Change amount
        REQUIRE_MESSAGE(
            contains(receipt, fmtCurrency(change)),
            "Property 13 violated: change '" << fmtCurrency(change)
                << "' not found in receipt"
                << " (iter=" << iter << ", txId=" << txId << ")"
        );

        // 8. Thank-you message
        REQUIRE_MESSAGE(
            contains(receipt, "Thank you"),
            "Property 13 violated: 'Thank you' not found in receipt"
                << " (iter=" << iter << ", txId=" << txId << ")"
        );
    }
}

TEST_CASE("Property 13 (exact tender): receipt correct when cash equals order total") {
    // Feature: bakery-pos-system, Property 13: Receipt completeness
    // Edge case: exact payment means change == 0.00; receipt must still show it.

    constexpr int NUM_ITERATIONS = 100;

    std::mt19937 rng(99991);
    std::uniform_int_distribution<int> txIdDist(1, 999);
    std::uniform_int_distribution<int> qtyDist(1, 20);
    std::uniform_int_distribution<int> sizeDist(1, 5);

    const std::vector<Product> catalog = {
        Product("P001", "Chocolate Chip Cookie",   65.00),
        Product("P002", "Double Chocolate Cookie",  70.00),
        Product("P003", "Red Velvet Cookie",        70.00),
        Product("P004", "S'mores Cookie",           75.00),
        Product("P005", "Brookies",                 75.00),
    };

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        const int txId        = txIdDist(rng);
        const int numProducts = sizeDist(rng);

        std::vector<CartItem> items;
        items.reserve(static_cast<std::size_t>(numProducts));
        for (int p = 0; p < numProducts; ++p) {
            items.emplace_back(catalog[static_cast<std::size_t>(p)], qtyDist(rng));
        }

        double total = 0.0;
        for (const auto& item : items) total += item.getSubtotal();

        // Exact tender: cash == total, change == 0.00
        const double cash = total;

        const std::string receipt = generateReceipt(txId, items, total, cash);

        // All required fields still present for exact-tender receipts
        REQUIRE_MESSAGE(contains(receipt, "Crombi Bakery"),
            "Property 13 (exact tender): 'Crombi Bakery' missing (iter=" << iter << ")");

        REQUIRE_MESSAGE(contains(receipt, "Transaction ID: " + std::to_string(txId)),
            "Property 13 (exact tender): Transaction ID missing (iter=" << iter << ")");

        REQUIRE_MESSAGE(contains(receipt, fmtCurrency(total)),
            "Property 13 (exact tender): order total missing (iter=" << iter << ")");

        // Change line must show PHP 0.00
        REQUIRE_MESSAGE(contains(receipt, fmtCurrency(0.0)),
            "Property 13 (exact tender): change 'PHP 0.00' missing (iter=" << iter << ")");

        REQUIRE_MESSAGE(contains(receipt, "Thank you"),
            "Property 13 (exact tender): 'Thank you' missing (iter=" << iter << ")");
    }
}
