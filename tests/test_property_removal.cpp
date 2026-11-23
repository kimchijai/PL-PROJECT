/**
 * test_property_removal.cpp
 * -------------------------
 * Property-based test for ShoppingCart item removal correctness.
 *
 * Property 9: Removal correctness
 * For any cart item present by Product_ID, after removeItem that
 * product must no longer appear and all other items must be unchanged
 * (same Product_IDs, same quantities, in the same order).
 *
 * // Feature: bakery-pos-system, Property 9: Removal correctness
 *
 * Validates: Requirements 5.3
 *
 * Framework: doctest (header-only), random testing loop (>=100 iterations)
 *
 * Compile:
 *   g++ -std=c++17 -I. -Itests tests/test_property_removal.cpp Product.cpp CartItem.cpp ShoppingCart.cpp -o tests/test_property_removal
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

/**
 * Builds a snapshot of {productId, quantity} pairs from a cart.
 * Used to compare cart contents before and after removeItem.
 */
struct ItemSnapshot {
    std::string productId;
    int         quantity;
};

static std::vector<ItemSnapshot> snapshot(const ShoppingCart& cart) {
    std::vector<ItemSnapshot> snaps;
    for (const CartItem& item : cart.getItems()) {
        snaps.push_back({item.getProductId(), item.getQuantity()});
    }
    return snaps;
}

// ---------------------------------------------------------------------------
// Property 9: Removal correctness
// ---------------------------------------------------------------------------

TEST_CASE("Property 9: Removed product is no longer present in the cart") {
    // Feature: bakery-pos-system, Property 9: Removal correctness
    //
    // For any cart that contains at least one item, pick one item at random
    // and call removeItem. The property asserts:
    //   (a) removeItem returns true (the item was found and removed).
    //   (b) containsProduct returns false for the removed Product_ID.
    //   (c) No CartItem in getItems() has the removed Product_ID.

    constexpr int NUM_ITERATIONS = 150;
    std::mt19937 rng(42); // Fixed seed for reproducibility

    std::uniform_int_distribution<int> sizeDist(1, CATALOG_SIZE);
    std::uniform_int_distribution<int> catalogDist(0, CATALOG_SIZE - 1);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        ShoppingCart cart;

        // Add 1–5 distinct catalog products in random order
        // Use a shuffled subset so each iteration can vary
        int numItems = sizeDist(rng);

        // Build a shuffled index array and pick the first numItems entries
        std::vector<int> indices = {0, 1, 2, 3, 4, 5, 6, 7};
        std::shuffle(indices.begin(), indices.end(), rng);

        for (int i = 0; i < numItems; ++i) {
            cart.addItem(CATALOG[indices[i]], randomQty(rng));
        }

        // Pick a random item currently in the cart to remove
        std::uniform_int_distribution<int> pickDist(0, numItems - 1);
        int chosenIndex = pickDist(rng);
        const std::string targetId = CATALOG[indices[chosenIndex]].getId();

        // --- (a) removeItem must return true ---
        bool removed = cart.removeItem(targetId);
        REQUIRE_MESSAGE(
            removed == true,
            "removeItem returned false for a product that was in the cart."
            " Iteration: " << iter << ", Product_ID: " << targetId
        );

        // --- (b) containsProduct must return false for the removed ID ---
        REQUIRE_MESSAGE(
            cart.containsProduct(targetId) == false,
            "containsProduct still true after removeItem."
            " Iteration: " << iter << ", Product_ID: " << targetId
        );

        // --- (c) No item in getItems() has the removed Product_ID ---
        for (const CartItem& item : cart.getItems()) {
            REQUIRE_MESSAGE(
                item.getProductId() != targetId,
                "getItems() still contains removed Product_ID."
                " Iteration: " << iter << ", Product_ID: " << targetId
                << ", found: " << item.getProductId()
            );
        }
    }
}

TEST_CASE("Property 9: All remaining items are unchanged after removeItem") {
    // Feature: bakery-pos-system, Property 9: Removal correctness
    //
    // After removing one item, every other item must be present with the
    // same Product_ID and the same quantity, in the same relative order.

    constexpr int NUM_ITERATIONS = 150;
    std::mt19937 rng(99); // Different seed for variety

    std::uniform_int_distribution<int> sizeDist(2, CATALOG_SIZE); // Need >=2 to have survivors

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        ShoppingCart cart;

        int numItems = sizeDist(rng);

        // Shuffle catalog indices and add distinct products
        std::vector<int> indices = {0, 1, 2, 3, 4, 5, 6, 7};
        std::shuffle(indices.begin(), indices.end(), rng);

        for (int i = 0; i < numItems; ++i) {
            cart.addItem(CATALOG[indices[i]], randomQty(rng));
        }

        // Snapshot the full cart before removal
        std::vector<ItemSnapshot> before = snapshot(cart);

        // Pick a random item to remove
        std::uniform_int_distribution<int> pickDist(0, numItems - 1);
        int chosenIndex = pickDist(rng);
        const std::string targetId = CATALOG[indices[chosenIndex]].getId();

        // Build the expected survivor list (all entries except the removed one,
        // in the same relative order)
        std::vector<ItemSnapshot> expected;
        for (const ItemSnapshot& s : before) {
            if (s.productId != targetId) {
                expected.push_back(s);
            }
        }

        cart.removeItem(targetId);

        // Snapshot after removal
        std::vector<ItemSnapshot> after = snapshot(cart);

        // The two snapshots must match element-by-element
        REQUIRE_MESSAGE(
            after.size() == expected.size(),
            "Cart size mismatch after removeItem."
            " Iteration: " << iter
            << ", expected size: " << expected.size()
            << ", actual size: " << after.size()
        );

        for (std::size_t i = 0; i < expected.size(); ++i) {
            REQUIRE_MESSAGE(
                after[i].productId == expected[i].productId,
                "Product_ID mismatch in remaining items after removeItem."
                " Iteration: " << iter
                << ", position: " << i
                << ", expected: " << expected[i].productId
                << ", got: " << after[i].productId
            );
            REQUIRE_MESSAGE(
                after[i].quantity == expected[i].quantity,
                "Quantity mismatch in remaining items after removeItem."
                " Iteration: " << iter
                << ", Product_ID: " << after[i].productId
                << ", expected qty: " << expected[i].quantity
                << ", got qty: " << after[i].quantity
            );
        }
    }
}

TEST_CASE("Property 9 (boundary): removeItem on a single-item cart leaves the cart empty") {
    // Feature: bakery-pos-system, Property 9: Removal correctness
    //
    // Edge case: removing the only item in a one-item cart must result
    // in an empty cart.

    constexpr int NUM_ITERATIONS = 100;
    std::mt19937 rng(7);
    std::uniform_int_distribution<int> catalogDist(0, CATALOG_SIZE - 1);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        ShoppingCart cart;

        const Product& p = CATALOG[catalogDist(rng)];
        cart.addItem(p, randomQty(rng));

        bool removed = cart.removeItem(p.getId());

        REQUIRE_MESSAGE(
            removed == true,
            "removeItem returned false for single-item cart."
            " Iteration: " << iter << ", Product_ID: " << p.getId()
        );
        REQUIRE_MESSAGE(
            cart.isEmpty() == true,
            "Cart is not empty after removing the only item."
            " Iteration: " << iter << ", Product_ID: " << p.getId()
        );
        REQUIRE_MESSAGE(
            cart.getItems().size() == 0u,
            "getItems() not empty after removing the only item."
            " Iteration: " << iter
        );
    }
}

TEST_CASE("Property 9 (non-member): removeItem for an absent Product_ID returns false and leaves cart unchanged") {
    // Feature: bakery-pos-system, Property 9: Removal correctness
    //
    // Attempting to remove a product that is NOT in the cart must:
    //   (a) return false
    //   (b) leave all existing items completely unchanged

    constexpr int NUM_ITERATIONS = 100;
    std::mt19937 rng(55);

    std::uniform_int_distribution<int> sizeDist(0, CATALOG_SIZE - 1);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        ShoppingCart cart;

        // Add 0–4 catalog products
        int numItems = sizeDist(rng);
        std::vector<int> indices = {0, 1, 2, 3, 4, 5, 6, 7};
        std::shuffle(indices.begin(), indices.end(), rng);
        for (int i = 0; i < numItems; ++i) {
            cart.addItem(CATALOG[indices[i]], randomQty(rng));
        }

        // Snapshot before the failed removal
        std::vector<ItemSnapshot> before = snapshot(cart);

        // Pick a product ID that is definitely NOT in the cart
        // (use the first catalog entry not among the added ones)
        std::string absentId;
        for (int i = 0; i < CATALOG_SIZE; ++i) {
            const std::string& cid = CATALOG[i].getId();
            bool present = false;
            for (const ItemSnapshot& s : before) {
                if (s.productId == cid) { present = true; break; }
            }
            if (!present) { absentId = cid; break; }
        }

        // If the cart is full (all 8 seeded products present), use a completely
        // unknown ID to test the not-found path
        if (absentId.empty()) {
            absentId = "P999";
        }

        // --- (a) removeItem must return false ---
        bool removed = cart.removeItem(absentId);
        REQUIRE_MESSAGE(
            removed == false,
            "removeItem returned true for a product not in the cart."
            " Iteration: " << iter << ", absent Product_ID: " << absentId
        );

        // --- (b) cart contents must be unchanged ---
        std::vector<ItemSnapshot> after = snapshot(cart);

        REQUIRE_MESSAGE(
            after.size() == before.size(),
            "Cart size changed after failed removeItem."
            " Iteration: " << iter
        );

        for (std::size_t i = 0; i < before.size(); ++i) {
            REQUIRE_MESSAGE(
                after[i].productId == before[i].productId,
                "Product_ID changed after failed removeItem."
                " Iteration: " << iter << ", position: " << i
            );
            REQUIRE_MESSAGE(
                after[i].quantity == before[i].quantity,
                "Quantity changed after failed removeItem."
                " Iteration: " << iter << ", position: " << i
            );
        }
    }
}
