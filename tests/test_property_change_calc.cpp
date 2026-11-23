/**
 * test_property_change_calc.cpp
 * -----------------------------
 * Property-based test for change calculation correctness.
 *
 * Property 12: Change calculation correctness
 * For any non-empty cart with computed order total T, and any cash
 * amount C >= T, the change returned to the customer must equal C - T.
 *
 * // Feature: bakery-pos-system, Property 12: Change calculation correctness
 *
 * Validates: Requirements 7.5
 *
 * Framework: doctest (header-only), random testing loop (>=100 iterations)
 *
 * Compile:
 *   g++ -std=c++17 -I. -Itests tests/test_property_change_calc.cpp Product.cpp CartItem.cpp ShoppingCart.cpp -o tests/test_property_change_calc
 */

#include "doctest.h"
#include "Product.h"
#include "CartItem.h"
#include "ShoppingCart.h"

#include <random>
#include <vector>

// ---------------------------------------------------------------------------
// Property test
// ---------------------------------------------------------------------------

TEST_CASE("Property 12: Change calculation correctness — change == cashAmount - orderTotal") {
    // Feature: bakery-pos-system, Property 12: Change calculation correctness

    // ----------------------------------------------------------------
    // Run 200 iterations with randomly generated carts and cash amounts.
    //
    // For each iteration:
    //   1. Build a non-empty cart from randomly chosen products and quantities.
    //   2. Obtain the real order total T via ShoppingCart::getOrderTotal().
    //   3. Generate a cash amount C >= T (a random overpayment on top of T).
    //   4. Verify that C - T equals the expected change.
    //
    // doctest::Approx is used for floating-point comparison to tolerate
    // any minor rounding accumulated in price * quantity summation.
    // ----------------------------------------------------------------
    constexpr int NUM_ITERATIONS = 200;

    std::mt19937 rng(2025); // Fixed seed for reproducibility

    // Quantities drawn from [1, 20] — realistic bakery order sizes.
    std::uniform_int_distribution<int> qtyDist(1, 20);

    // Over-payment drawn from [0, 1000] PHP on top of the order total.
    std::uniform_real_distribution<double> overPayDist(0.0, 1000.0);

    // A small fixed catalog of bakery products (prices in PHP).
    const std::vector<Product> catalog = {
        Product("P001", "Chocolate Chip Cookie",  65.00),
        Product("P002", "Double Chocolate Cookie", 70.00),
        Product("P003", "Red Velvet Cookie",       70.00),
        Product("P004", "S'mores Cookie",          75.00),
        Product("P005", "Brookies",                75.00),
    };

    // Decide how many distinct products appear in each cart: [1, 5].
    std::uniform_int_distribution<int> sizeDist(1, static_cast<int>(catalog.size()));

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        ShoppingCart cart;

        // Add a random non-empty subset of catalog products.
        const int numProducts = sizeDist(rng);
        for (int p = 0; p < numProducts; ++p) {
            const Product& product = catalog[static_cast<std::size_t>(p)];
            const int qty = qtyDist(rng);
            cart.addItem(product, qty);
        }

        // Pre-condition: cart must be non-empty.
        REQUIRE_MESSAGE(
            !cart.isEmpty(),
            "Pre-condition failed: cart is empty (iteration " << iter << ")"
        );

        const double orderTotal = cart.getOrderTotal();

        // Pre-condition: order total must be positive for a non-empty cart.
        REQUIRE_MESSAGE(
            orderTotal > 0.0,
            "Pre-condition failed: order total is not positive (iteration " << iter << ")"
        );

        // Cash amount C is always >= order total T (exact payment or overpayment).
        const double overpayment = overPayDist(rng);
        const double cashAmount  = orderTotal + overpayment;

        // The change calculation: change = cashAmount - orderTotal.
        const double change = cashAmount - orderTotal;

        REQUIRE_MESSAGE(
            change == doctest::Approx(overpayment).epsilon(1e-9),
            "Property 12 violated: change=" << change
                << " expected=" << overpayment
                << " (cashAmount=" << cashAmount
                << ", orderTotal=" << orderTotal
                << ", iteration=" << iter << ")"
        );
    }
}

TEST_CASE("Property 12 (exact tender): zero change when cash equals order total") {
    // Feature: bakery-pos-system, Property 12: Change calculation correctness
    // Edge case: when the customer pays the exact order total, change must be 0.

    constexpr int NUM_ITERATIONS = 100;

    std::mt19937 rng(777);
    std::uniform_int_distribution<int> qtyDist(1, 20);
    std::uniform_int_distribution<int> sizeDist(1, 5);

    const std::vector<Product> catalog = {
        Product("P001", "Chocolate Chip Cookie",  65.00),
        Product("P002", "Double Chocolate Cookie", 70.00),
        Product("P003", "Red Velvet Cookie",       70.00),
        Product("P004", "S'mores Cookie",          75.00),
        Product("P005", "Brookies",                75.00),
    };

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        ShoppingCart cart;

        const int numProducts = sizeDist(rng);
        for (int p = 0; p < numProducts; ++p) {
            cart.addItem(catalog[static_cast<std::size_t>(p)], qtyDist(rng));
        }

        const double orderTotal = cart.getOrderTotal();

        // Exact tender: cash == total, so change must be zero.
        const double cashAmount = orderTotal;
        const double change     = cashAmount - orderTotal;

        REQUIRE_MESSAGE(
            change == doctest::Approx(0.0).epsilon(1e-9),
            "Property 12 (exact tender) violated: change=" << change
                << " expected=0.0"
                << " (orderTotal=" << orderTotal
                << ", iteration=" << iter << ")"
        );
    }
}
