/**
 * test_property_cartitem_accumulate.cpp
 * --------------------------------------
 * Property-based test for CartItem quantity accumulation.
 *
 * Property 6: Duplicate item quantity accumulation
 * For any CartItem with quantity Q and any positive delta, after
 * calling addQuantity(delta) the quantity must equal Q + delta.
 *
 * // Feature: bakery-pos-system, Property 6: Duplicate item quantity accumulation
 *
 * Validates: Requirements 3.6
 *
 * Framework: doctest (header-only), random testing loop (>=100 iterations)
 *
 * Compile:
 *   g++ -std=c++17 -I. -Itests tests/test_property_cartitem_accumulate.cpp Product.cpp CartItem.cpp -o tests/test_property_cartitem_accumulate
 */

#include "doctest.h"
#include "Product.h"
#include "CartItem.h"

#include <random>

// ---------------------------------------------------------------------------
// Property test
// ---------------------------------------------------------------------------

TEST_CASE("Property 6: Duplicate item quantity accumulation — addQuantity(delta) yields Q + delta") {
    // Feature: bakery-pos-system, Property 6: Duplicate item quantity accumulation

    // ----------------------------------------------------------------
    // Run 200 iterations with randomly generated (Q, delta) pairs to
    // satisfy the minimum-100-iteration requirement.
    // Q (initial quantity) and delta are both drawn from [1, 1000] so
    // that every generated CartItem is in a valid, positive-quantity
    // state before and after the call.
    // ----------------------------------------------------------------
    constexpr int NUM_ITERATIONS = 200;

    std::mt19937 rng(2024); // Fixed seed for reproducibility

    // Both Q and delta must be positive (> 0)
    std::uniform_int_distribution<int> qtyDist(1, 1000);

    // A representative product — price and identity are irrelevant to
    // the quantity-accumulation property, so a single fixed product is used.
    const Product product("P001", "Chocolate Chip Cookie", 65.00);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        const int initialQty = qtyDist(rng);
        const int delta      = qtyDist(rng);

        CartItem item(product, initialQty);

        // Pre-condition: quantity matches what was passed to the constructor
        REQUIRE_MESSAGE(
            item.getQuantity() == initialQty,
            "Pre-condition failed: initial quantity mismatch (iteration " << iter << ")"
        );

        item.addQuantity(delta);

        const int expected = initialQty + delta;
        const int actual   = item.getQuantity();

        REQUIRE_MESSAGE(
            actual == expected,
            "Property 6 violated: expected " << expected
                << " but got " << actual
                << " (Q=" << initialQty << ", delta=" << delta
                << ", iteration=" << iter << ")"
        );
    }
}

TEST_CASE("Property 6 (edge): addQuantity accumulates correctly across multiple calls") {
    // Feature: bakery-pos-system, Property 6: Duplicate item quantity accumulation
    // Extends the core property by verifying that repeated addQuantity calls
    // accumulate correctly — each call adds to whatever the current quantity is.

    constexpr int NUM_ITERATIONS = 100;
    constexpr int CALLS_PER_ITER = 5;

    std::mt19937 rng(99);
    std::uniform_int_distribution<int> qtyDist(1, 500);

    const Product product("P002", "Double Chocolate Cookie", 70.00);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        const int initialQty = qtyDist(rng);
        CartItem item(product, initialQty);

        int runningTotal = initialQty;

        for (int call = 0; call < CALLS_PER_ITER; ++call) {
            const int delta = qtyDist(rng);
            item.addQuantity(delta);
            runningTotal += delta;

            REQUIRE_MESSAGE(
                item.getQuantity() == runningTotal,
                "Property 6 (multi-call) violated at call " << call
                    << ": expected " << runningTotal
                    << " but got " << item.getQuantity()
                    << " (iteration=" << iter << ")"
            );
        }
    }
}
