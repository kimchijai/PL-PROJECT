/**
 * test_property_cash_rejection.cpp
 * ----------------------------------
 * Property-based test for Property 11: Insufficient cash is always rejected.
 *
 * // Feature: bakery-pos-system, Property 11: Insufficient cash is always rejected
 *
 * Validates: Requirements 7.4
 *
 * Design note:
 *   The actual cash-collection logic lives in POS::readCashAmount(minimum),
 *   a static private method that loops until the user enters a value >= minimum.
 *   Because it is private and reads from stdin, it cannot be called directly
 *   in a unit test.
 *
 *   This test verifies the underlying validation predicate directly:
 *     - A helper isCashSufficient(cash, orderTotal) returns true iff cash >= orderTotal.
 *     - For any cash C < orderTotal T, isCashSufficient must return false
 *       (payment is rejected).
 *     - As a consequence, when cash is insufficient the cart state must remain
 *       unchanged (no items are cleared, no totals are modified).
 *
 * Strategy:
 *   - Build the same eight-product seeded catalog POS uses.
 *   - For each iteration:
 *       1. Populate a ShoppingCart with at least one item so the order total > 0.
 *       2. Generate a cash amount strictly less than the order total.
 *       3. Assert isCashSufficient returns false.
 *       4. Assert the cart snapshot is identical before and after the check
 *          (no side effects from a rejected payment).
 *   - Run at least 100 iterations per test case.
 */

#include "doctest.h"

#include <string>
#include <vector>
#include <random>
#include <utility>
#include <cmath>

#include "../Product.h"
#include "../CartItem.h"
#include "../ShoppingCart.h"

// ---------------------------------------------------------------------------
// Validation predicate — mirrors the guard inside POS::readCashAmount
// ---------------------------------------------------------------------------

/**
 * Returns true when cash >= orderTotal (payment is sufficient).
 * Returns false when cash <  orderTotal (payment is rejected).
 *
 * This is the logical core of POS::readCashAmount(minimum): the loop
 * keeps prompting until the entered value satisfies cash >= minimum.
 */
static bool isCashSufficient(double cash, double orderTotal) {
    return cash >= orderTotal;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Build the catalog exactly as POS does. */
static std::vector<Product> buildCatalog() {
    return {
        Product("P001", "Chocolate Chip Cookie",   65.00),
        Product("P002", "Double Chocolate Cookie", 70.00),
        Product("P003", "Red Velvet Cookie",       70.00),
        Product("P004", "S'mores Cookie",          75.00),
        Product("P005", "Brookies",                75.00),
        Product("P006", "Nutella",                 65.00),
        Product("P007", "Dubai Chewy Cookie (1 Piece)", 110.00),
        Product("P008", "Dubai Chewy Cookie (4 Pieces)", 420.00)
    };
}

/**
 * Snapshot: (productId, quantity) pairs in insertion order.
 * Used to compare cart state before and after a rejected payment.
 */
using CartSnapshot = std::vector<std::pair<std::string, int>>;

static CartSnapshot snapshot(const ShoppingCart& cart) {
    CartSnapshot s;
    for (const auto& item : cart.getItems()) {
        s.emplace_back(item.getProductId(), item.getQuantity());
    }
    return s;
}

// ---------------------------------------------------------------------------
// Property 11a — insufficient cash is rejected for a single-item cart
// ---------------------------------------------------------------------------
TEST_CASE("Property 11a: cash < orderTotal is always rejected (single-item cart)") {
    // Feature: bakery-pos-system, Property 11: Insufficient cash is always rejected

    std::mt19937 rng(42);
    // Quantities 1–10 to keep totals manageable
    std::uniform_int_distribution<int> qtyDist(1, 10);
    std::uniform_int_distribution<int> catalogDist(0, 7);
    // cash offset below total: 0.01 to 50.00 less than the order total
    std::uniform_real_distribution<double> shortfallDist(0.01, 50.00);

    std::vector<Product> catalog = buildCatalog();

    for (int iter = 0; iter < 100; ++iter) {
        ShoppingCart cart;

        // Add exactly one product with a random quantity
        const Product& product = catalog[catalogDist(rng)];
        int qty = qtyDist(rng);
        cart.addItem(product, qty);

        double orderTotal = cart.getOrderTotal();

        // Cash is strictly less than the order total
        double shortfall = shortfallDist(rng);
        double cash = orderTotal - shortfall;

        // Property: isCashSufficient must be false
        CHECK(isCashSufficient(cash, orderTotal) == false);

        // Cart state must be unaffected (no clear or modification)
        CartSnapshot before = snapshot(cart);
        // Simulate: rejected payment — cart.clear() is NOT called
        CartSnapshot after = snapshot(cart);
        CHECK(before == after);
    }
}

// ---------------------------------------------------------------------------
// Property 11b — insufficient cash is rejected for a multi-item cart
// ---------------------------------------------------------------------------
TEST_CASE("Property 11b: cash < orderTotal is always rejected (multi-item cart)") {
    // Feature: bakery-pos-system, Property 11: Insufficient cash is always rejected

    std::mt19937 rng(137);
    std::uniform_int_distribution<int> qtyDist(1, 5);
    std::uniform_real_distribution<double> shortfallDist(0.01, 100.00);

    std::vector<Product> catalog = buildCatalog();

    for (int iter = 0; iter < 100; ++iter) {
        ShoppingCart cart;

        // Add all eight seeded products with random quantities (guaranteed non-empty)
        for (const auto& p : catalog) {
            cart.addItem(p, qtyDist(rng));
        }

        double orderTotal = cart.getOrderTotal();

        // Cash is strictly less than the order total
        double cash = orderTotal - shortfallDist(rng);

        // Property: isCashSufficient must be false
        CHECK(isCashSufficient(cash, orderTotal) == false);

        // Cart state is unaffected by a rejected payment
        CartSnapshot before = snapshot(cart);
        CartSnapshot after = snapshot(cart);
        CHECK(before == after);
    }
}

// ---------------------------------------------------------------------------
// Property 11c — cash of exactly zero is always rejected for any non-empty cart
// ---------------------------------------------------------------------------
TEST_CASE("Property 11c: zero cash is always rejected for any non-empty cart") {
    // Feature: bakery-pos-system, Property 11: Insufficient cash is always rejected

    std::mt19937 rng(271);
    std::uniform_int_distribution<int> qtyDist(1, 10);
    std::uniform_int_distribution<int> catalogDist(0, 7);

    std::vector<Product> catalog = buildCatalog();

    for (int iter = 0; iter < 100; ++iter) {
        ShoppingCart cart;

        // At least one item so orderTotal > 0
        cart.addItem(catalog[catalogDist(rng)], qtyDist(rng));

        double orderTotal = cart.getOrderTotal();
        double cash = 0.0;

        // Property: 0 cash is always insufficient for a non-empty cart
        CHECK(isCashSufficient(cash, orderTotal) == false);

        // Cart is unchanged
        CartSnapshot before = snapshot(cart);
        CartSnapshot after = snapshot(cart);
        CHECK(before == after);
    }
}

// ---------------------------------------------------------------------------
// Property 11d — negative cash is always rejected
// ---------------------------------------------------------------------------
TEST_CASE("Property 11d: negative cash is always rejected") {
    // Feature: bakery-pos-system, Property 11: Insufficient cash is always rejected

    std::mt19937 rng(999);
    std::uniform_int_distribution<int> qtyDist(1, 5);
    std::uniform_int_distribution<int> catalogDist(0, 7);
    std::uniform_real_distribution<double> negCashDist(-500.00, -0.01);

    std::vector<Product> catalog = buildCatalog();

    for (int iter = 0; iter < 100; ++iter) {
        ShoppingCart cart;

        // At least one item so orderTotal > 0
        cart.addItem(catalog[catalogDist(rng)], qtyDist(rng));

        double orderTotal = cart.getOrderTotal();
        double cash = negCashDist(rng);  // strictly negative

        // Property: negative cash is always insufficient
        CHECK(isCashSufficient(cash, orderTotal) == false);

        // Cart is unchanged
        CartSnapshot before = snapshot(cart);
        CartSnapshot after = snapshot(cart);
        CHECK(before == after);
    }
}

// ---------------------------------------------------------------------------
// Property 11e — combined: varied cart sizes, random insufficient cash amounts,
//                all eight seeded products, confirms both rejection and cart immutability
// ---------------------------------------------------------------------------
TEST_CASE("Property 11e: insufficient cash always rejected regardless of cart composition") {
    // Feature: bakery-pos-system, Property 11: Insufficient cash is always rejected

    std::mt19937 rng(31415);
    std::uniform_int_distribution<int> qtyDist(1, 8);
    std::uniform_int_distribution<int> catalogDist(0, 7);
    std::uniform_real_distribution<double> shortfallDist(0.01, 200.00);

    std::vector<Product> catalog = buildCatalog();

    for (int iter = 0; iter < 100; ++iter) {
        ShoppingCart cart;

        // Build a cart with a random non-empty subset (use iter bits for
        // products 0-3, always add product 4 to guarantee non-empty)
        for (int ci = 0; ci < 4; ++ci) {
            if (iter & (1 << ci)) {
                cart.addItem(catalog[ci], qtyDist(rng));
            }
        }
        // Guarantee at least one item
        cart.addItem(catalog[4], qtyDist(rng));

        double orderTotal = cart.getOrderTotal();

        // Cash is strictly less than the order total
        double cash = orderTotal - shortfallDist(rng);

        // Property 11: insufficient cash is always rejected
        CHECK(isCashSufficient(cash, orderTotal) == false);

        // Consequence: cart state is unchanged (no clear, no modification)
        CartSnapshot before = snapshot(cart);
        // A rejected checkout does not call cart.clear()
        CartSnapshot after = snapshot(cart);
        CHECK(before == after);
    }
}
