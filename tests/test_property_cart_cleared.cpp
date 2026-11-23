/**
 * test_property_cart_cleared.cpp
 * ------------------------------
 * Property-based test verifying that the cart is emptied after checkout.
 *
 * Property 15: Cart cleared after checkout
 * For any non-empty cart, after clear() is called (which POS::checkout()
 * invokes on a successful transaction), the cart must be empty:
 *   - isEmpty() returns true
 *   - getItems() returns an empty collection
 *   - getOrderTotal() returns 0.0
 *
 * // Feature: bakery-pos-system, Property 15: Cart cleared after checkout
 *
 * Validates: Requirements 8.5
 *
 * Framework: doctest (header-only), random testing loop (>=100 iterations)
 *
 * Compile:
 *   g++ -std=c++17 -I. -Itests tests/test_property_cart_cleared.cpp Product.cpp CartItem.cpp ShoppingCart.cpp -o tests/test_property_cart_cleared
 */

#include "doctest.h"
#include "Product.h"
#include "CartItem.h"
#include "ShoppingCart.h"

#include <string>
#include <vector>
#include <random>
#include <algorithm>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** The eight seeded catalog products used throughout these tests. */
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

/** Picks a random quantity in [1, 20]. */
static int randomQty(std::mt19937& rng) {
    std::uniform_int_distribution<int> d(1, 20);
    return d(rng);
}

// ---------------------------------------------------------------------------
// Property 15: Cart cleared after checkout
// ---------------------------------------------------------------------------

TEST_CASE("Property 15: isEmpty() is true after clear() on any non-empty cart") {
    // Feature: bakery-pos-system, Property 15: Cart cleared after checkout
    //
    // For any non-empty cart (1 to 8 distinct items with random quantities),
    // calling clear() must make isEmpty() return true.

    constexpr int NUM_ITERATIONS = 150;
    std::mt19937 rng(42); // Fixed seed for reproducibility

    std::uniform_int_distribution<int> sizeDist(1, CATALOG_SIZE);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        ShoppingCart cart;

        int numItems = sizeDist(rng);

        // Shuffle catalog indices to vary which products appear each iteration
        std::vector<int> indices = {0, 1, 2, 3, 4, 5, 6, 7};
        std::shuffle(indices.begin(), indices.end(), rng);

        for (int i = 0; i < numItems; ++i) {
            cart.addItem(CATALOG[indices[i]], randomQty(rng));
        }

        // Pre-condition: cart is not empty before checkout
        REQUIRE_MESSAGE(
            cart.isEmpty() == false,
            "Pre-condition failed: cart should not be empty before clear()."
            " Iteration: " << iter
        );

        // Simulate POS::checkout() clearing the cart
        cart.clear();

        // Post-condition: cart must be empty
        REQUIRE_MESSAGE(
            cart.isEmpty() == true,
            "isEmpty() returned false after clear()."
            " Iteration: " << iter << ", items added: " << numItems
        );
    }
}

TEST_CASE("Property 15: getItems() is empty after clear() on any non-empty cart") {
    // Feature: bakery-pos-system, Property 15: Cart cleared after checkout
    //
    // For any non-empty cart, calling clear() must leave getItems()
    // returning an empty vector (size == 0).

    constexpr int NUM_ITERATIONS = 150;
    std::mt19937 rng(77);

    std::uniform_int_distribution<int> sizeDist(1, CATALOG_SIZE);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        ShoppingCart cart;

        int numItems = sizeDist(rng);

        std::vector<int> indices = {0, 1, 2, 3, 4, 5, 6, 7};
        std::shuffle(indices.begin(), indices.end(), rng);

        for (int i = 0; i < numItems; ++i) {
            cart.addItem(CATALOG[indices[i]], randomQty(rng));
        }

        cart.clear();

        REQUIRE_MESSAGE(
            cart.getItems().empty() == true,
            "getItems() is not empty after clear()."
            " Iteration: " << iter
            << ", items added: " << numItems
            << ", items remaining: " << cart.getItems().size()
        );

        REQUIRE_MESSAGE(
            cart.getItems().size() == 0u,
            "getItems().size() != 0 after clear()."
            " Iteration: " << iter
            << ", actual size: " << cart.getItems().size()
        );
    }
}

TEST_CASE("Property 15: getOrderTotal() is 0 after clear() on any non-empty cart") {
    // Feature: bakery-pos-system, Property 15: Cart cleared after checkout
    //
    // After clear(), the order total must be 0.0 because there are no
    // items left to sum.

    constexpr int NUM_ITERATIONS = 150;
    std::mt19937 rng(13);

    std::uniform_int_distribution<int> sizeDist(1, CATALOG_SIZE);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        ShoppingCart cart;

        int numItems = sizeDist(rng);

        std::vector<int> indices = {0, 1, 2, 3, 4, 5, 6, 7};
        std::shuffle(indices.begin(), indices.end(), rng);

        for (int i = 0; i < numItems; ++i) {
            cart.addItem(CATALOG[indices[i]], randomQty(rng));
        }

        // Confirm the cart had a positive total before clearing
        REQUIRE_MESSAGE(
            cart.getOrderTotal() > 0.0,
            "Pre-condition failed: order total should be > 0 before clear()."
            " Iteration: " << iter
        );

        cart.clear();

        REQUIRE_MESSAGE(
            cart.getOrderTotal() == doctest::Approx(0.0),
            "getOrderTotal() != 0.0 after clear()."
            " Iteration: " << iter
            << ", remaining total: " << cart.getOrderTotal()
        );
    }
}

TEST_CASE("Property 15 (all sizes): clear() empties carts of every possible size 1-8") {
    // Feature: bakery-pos-system, Property 15: Cart cleared after checkout
    //
    // Run at least 100 iterations across all cart sizes (1–8 items) to
    // confirm the property holds regardless of how many line items are present.

    constexpr int NUM_ITERATIONS = 120;
    std::mt19937 rng(99);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        // Cycle through all cart sizes (1 through 8)
        int numItems = (iter % CATALOG_SIZE) + 1;

        ShoppingCart cart;

        std::vector<int> indices = {0, 1, 2, 3, 4, 5, 6, 7};
        std::shuffle(indices.begin(), indices.end(), rng);

        for (int i = 0; i < numItems; ++i) {
            cart.addItem(CATALOG[indices[i]], randomQty(rng));
        }

        cart.clear();

        REQUIRE_MESSAGE(
            cart.isEmpty() == true,
            "isEmpty() not true after clear()."
            " Iteration: " << iter << ", cart size: " << numItems
        );
        REQUIRE_MESSAGE(
            cart.getItems().size() == 0u,
            "getItems() not empty after clear()."
            " Iteration: " << iter << ", cart size: " << numItems
        );
        REQUIRE_MESSAGE(
            cart.getOrderTotal() == doctest::Approx(0.0),
            "getOrderTotal() != 0.0 after clear()."
            " Iteration: " << iter << ", cart size: " << numItems
        );
    }
}
