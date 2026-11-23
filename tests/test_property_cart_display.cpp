/**
 * test_property_cart_display.cpp
 * ------------------------------
 * Property-based test for cart display completeness.
 *
 * Property 8: Cart display completeness
 * For any non-empty cart:
 *   (a) getItems() contains every added product (matched by Product_ID)
 *   (b) Each item in getItems() has the correct accumulated quantity
 *   (c) getOrderTotal() equals the sum of all CartItem subtotals
 *
 * // Feature: bakery-pos-system, Property 8: Cart display completeness
 *
 * Validates: Requirements 4.1
 *
 * Framework: doctest (header-only), random testing loop (>=100 iterations)
 *
 * Compile:
 *   g++ -std=c++17 -I. -Itests tests/test_property_cart_display.cpp Product.cpp CartItem.cpp ShoppingCart.cpp -o tests/test_property_cart_display
 */

#include "doctest.h"
#include "Product.h"
#include "CartItem.h"
#include "ShoppingCart.h"

#include <string>
#include <random>
#include <vector>
#include <numeric>
#include <cmath>

// ---------------------------------------------------------------------------
// Catalog — the eight seeded bakery products
// ---------------------------------------------------------------------------

static const Product CATALOG[] = {
    Product("P001", "Chocolate Chip Cookie",   65.00),
    Product("P002", "Double Chocolate Cookie", 70.00),
    Product("P003", "Red Velvet Cookie",       70.00),
    Product("P004", "S'mores Cookie",          75.00),
    Product("P005", "Brookies",                75.00),
    Product("P006", "Nutella",                 65.00),
    Product("P007", "Dubai Chewy Cookie (1 Piece)", 110.00),
    Product("P008", "Dubai Chewy Cookie (4 Pieces)", 420.00)
};
static constexpr int CATALOG_SIZE = 8;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Returns a random positive quantity in [1, 10]. */
static int randomQty(std::mt19937& rng) {
    std::uniform_int_distribution<int> d(1, 10);
    return d(rng);
}

/** Returns a random catalog index in [0, CATALOG_SIZE). */
static int randomCatalogIdx(std::mt19937& rng) {
    std::uniform_int_distribution<int> d(0, CATALOG_SIZE - 1);
    return d(rng);
}

/**
 * Finds the CartItem in `items` whose Product_ID matches `id`.
 * Returns nullptr if not found.
 */
static const CartItem* findInItems(const std::vector<CartItem>& items,
                                   const std::string& id) {
    for (const auto& ci : items) {
        if (ci.getProductId() == id) {
            return &ci;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Property tests
// ---------------------------------------------------------------------------

TEST_CASE("Property 8: Cart display completeness — getItems reflects all added products with correct quantities") {
    // Feature: bakery-pos-system, Property 8: Cart display completeness

    // We generate 100 random carts, each built by adding 1–5 operations
    // that target random catalog products with random positive quantities.
    //
    // For each cart we verify:
    //   (a) Every product that was added at least once appears in getItems().
    //   (b) The quantity stored for each product equals the sum of all
    //       quantities supplied for that product (addItem merges duplicates).

    constexpr int NUM_ITERATIONS = 100;
    std::mt19937 rng(42); // fixed seed for reproducibility

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        ShoppingCart cart;

        // Number of addItem calls this iteration (1–5)
        std::uniform_int_distribution<int> opCountDist(1, 5);
        const int numOps = opCountDist(rng);

        // Track expected quantities per Product_ID
        // Index corresponds to CATALOG index
        int expectedQty[CATALOG_SIZE] = {};

        for (int op = 0; op < numOps; ++op) {
            const int idx = randomCatalogIdx(rng);
            const int qty = randomQty(rng);
            cart.addItem(CATALOG[idx], qty);
            expectedQty[idx] += qty;
        }

        const std::vector<CartItem>& items = cart.getItems();

        // --- (a) All added products appear in getItems() ---
        for (int idx = 0; idx < CATALOG_SIZE; ++idx) {
            if (expectedQty[idx] == 0) {
                continue; // this product was never added, skip
            }
            const CartItem* found = findInItems(items, CATALOG[idx].getId());
            REQUIRE_MESSAGE(
                found != nullptr,
                "Iteration " << iter
                << ": product " << CATALOG[idx].getId()
                << " was added but is missing from getItems()"
            );

            // --- (b) Quantity equals sum of all addItem calls for this product ---
            REQUIRE_MESSAGE(
                found->getQuantity() == expectedQty[idx],
                "Iteration " << iter
                << ": product " << CATALOG[idx].getId()
                << " expected quantity " << expectedQty[idx]
                << " but got " << found->getQuantity()
            );
        }

        // --- No phantom products (items added that were never requested) ---
        for (const auto& ci : items) {
            bool inExpected = false;
            for (int idx = 0; idx < CATALOG_SIZE; ++idx) {
                if (ci.getProductId() == CATALOG[idx].getId() && expectedQty[idx] > 0) {
                    inExpected = true;
                    break;
                }
            }
            REQUIRE_MESSAGE(
                inExpected,
                "Iteration " << iter
                << ": unexpected product " << ci.getProductId()
                << " found in getItems()"
            );
        }
    }
}

TEST_CASE("Property 8: Cart display completeness — getOrderTotal equals sum of all subtotals") {
    // Feature: bakery-pos-system, Property 8: Cart display completeness

    // For each randomly generated non-empty cart, verify that
    // getOrderTotal() equals the manual sum of all CartItem subtotals.

    constexpr int NUM_ITERATIONS = 100;
    std::mt19937 rng(77);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        ShoppingCart cart;

        std::uniform_int_distribution<int> opCountDist(1, 5);
        const int numOps = opCountDist(rng);

        for (int op = 0; op < numOps; ++op) {
            cart.addItem(CATALOG[randomCatalogIdx(rng)], randomQty(rng));
        }

        // Compute expected total manually from getItems()
        double expectedTotal = 0.0;
        for (const auto& ci : cart.getItems()) {
            expectedTotal += ci.getSubtotal();
        }

        REQUIRE_MESSAGE(
            cart.getOrderTotal() == doctest::Approx(expectedTotal),
            "Iteration " << iter
            << ": getOrderTotal() = " << cart.getOrderTotal()
            << " but sum of subtotals = " << expectedTotal
        );
    }
}

TEST_CASE("Property 8 (single product): getItems and getOrderTotal are consistent for a single distinct product") {
    // Feature: bakery-pos-system, Property 8: Cart display completeness
    // Focused check: add one product multiple times and verify that
    // getItems() contains exactly one item with the accumulated quantity
    // and getOrderTotal() matches.

    constexpr int NUM_ITERATIONS = 100;
    std::mt19937 rng(55);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        ShoppingCart cart;

        const int catalogIdx = randomCatalogIdx(rng);
        const Product& p = CATALOG[catalogIdx];

        std::uniform_int_distribution<int> addCountDist(1, 5);
        const int addCount = addCountDist(rng);
        int totalQty = 0;

        for (int i = 0; i < addCount; ++i) {
            const int qty = randomQty(rng);
            cart.addItem(p, qty);
            totalQty += qty;
        }

        const std::vector<CartItem>& items = cart.getItems();

        // Exactly one line item
        REQUIRE_MESSAGE(
            items.size() == 1,
            "Iteration " << iter
            << ": expected 1 item in cart, got " << items.size()
        );

        // Quantity is the accumulated total
        REQUIRE_MESSAGE(
            items[0].getQuantity() == totalQty,
            "Iteration " << iter
            << ": expected quantity " << totalQty
            << ", got " << items[0].getQuantity()
        );

        // getOrderTotal() == unit_price * total_qty
        const double expectedTotal = p.getPrice() * totalQty;
        REQUIRE_MESSAGE(
            cart.getOrderTotal() == doctest::Approx(expectedTotal),
            "Iteration " << iter
            << ": expected order total " << expectedTotal
            << ", got " << cart.getOrderTotal()
        );
    }
}

TEST_CASE("Property 8 (all eight products): full catalog in cart — getItems has all eight and total is correct") {
    // Feature: bakery-pos-system, Property 8: Cart display completeness
    // Add all eight seeded products once each with random quantities.
    // Verify all eight appear in getItems() and getOrderTotal() matches.

    constexpr int NUM_ITERATIONS = 100;
    std::mt19937 rng(31);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        ShoppingCart cart;
        double expectedTotal = 0.0;
        int quantities[CATALOG_SIZE] = {};

        for (int idx = 0; idx < CATALOG_SIZE; ++idx) {
            const int qty = randomQty(rng);
            cart.addItem(CATALOG[idx], qty);
            quantities[idx] = qty;
            expectedTotal += CATALOG[idx].getPrice() * qty;
        }

        const std::vector<CartItem>& items = cart.getItems();

        // All eight products present
        REQUIRE_MESSAGE(
            items.size() == static_cast<size_t>(CATALOG_SIZE),
            "Iteration " << iter
            << ": expected 8 items in cart, got " << items.size()
        );

        // Each product's quantity is correct
        for (int idx = 0; idx < CATALOG_SIZE; ++idx) {
            const CartItem* found = findInItems(items, CATALOG[idx].getId());
            REQUIRE_MESSAGE(
                found != nullptr,
                "Iteration " << iter
                << ": product " << CATALOG[idx].getId()
                << " missing from getItems()"
            );
            REQUIRE_MESSAGE(
                found->getQuantity() == quantities[idx],
                "Iteration " << iter
                << ": product " << CATALOG[idx].getId()
                << " expected qty " << quantities[idx]
                << ", got " << found->getQuantity()
            );
        }

        // Order total matches
        REQUIRE_MESSAGE(
            cart.getOrderTotal() == doctest::Approx(expectedTotal),
            "Iteration " << iter
            << ": expected order total " << expectedTotal
            << ", got " << cart.getOrderTotal()
        );
    }
}
