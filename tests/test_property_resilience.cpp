/**
 * test_property_resilience.cpp
 * ----------------------------
 * Property-based test for system resilience to invalid inputs.
 *
 * Property 16: System survives any invalid input sequence
 * For any sequence of invalid operations on a ShoppingCart, the cart
 * must remain operational and its state must not be corrupted:
 *   (a) removeItem with a non-existent Product_ID returns false and leaves
 *       the cart unchanged.
 *   (b) updateItem with a non-existent Product_ID returns false and leaves
 *       the cart unchanged.
 *   (c) Valid addItem operations still work correctly after any sequence of
 *       invalid operations.
 *   (d) getOrderTotal() always equals the sum of individual item subtotals
 *       (invariant never broken by invalid operations).
 *   (e) getItems().size() is always consistent with the number of distinct
 *       products added to the cart.
 *
 * // Feature: bakery-pos-system, Property 16: System survives any invalid input sequence
 *
 * Validates: Requirements 9.1, 9.2, 9.3
 *
 * Framework: doctest (header-only), random testing loop (>=100 iterations)
 *
 * Compile:
 *   g++ -std=c++17 -I. -Itests tests/test_property_resilience.cpp Product.cpp CartItem.cpp ShoppingCart.cpp -o tests/test_property_resilience
 */

#include "doctest.h"
#include "../Product.h"
#include "../CartItem.h"
#include "../ShoppingCart.h"

#include <string>
#include <vector>
#include <random>
#include <cmath>
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

/** Invalid Product_IDs that are guaranteed not to be in the catalog. */
static const std::string INVALID_IDS[] = {
    "P000", "P009", "P999", "XXXX", "", "0",
    "p001", "P0001", "1001", "P-01", "ABC"
};
static constexpr int INVALID_IDS_SIZE = 11;

/** Picks a random quantity in [1, 20]. */
static int randomQty(std::mt19937& rng) {
    std::uniform_int_distribution<int> d(1, 20);
    return d(rng);
}

/** Picks a random invalid Product_ID from the fixed table. */
static const std::string& randomInvalidId(std::mt19937& rng) {
    std::uniform_int_distribution<int> d(0, INVALID_IDS_SIZE - 1);
    return INVALID_IDS[d(rng)];
}

/**
 * Snapshot: {productId, quantity} pairs in insertion order.
 * Used to compare cart state before and after a rejected operation.
 */
struct ItemSnapshot {
    std::string productId;
    int         quantity;
    bool operator==(const ItemSnapshot& o) const {
        return productId == o.productId && quantity == o.quantity;
    }
};

static std::vector<ItemSnapshot> snapshot(const ShoppingCart& cart) {
    std::vector<ItemSnapshot> snaps;
    for (const CartItem& item : cart.getItems()) {
        snaps.push_back({item.getProductId(), item.getQuantity()});
    }
    return snaps;
}

/**
 * Verifies the order-total invariant: getOrderTotal() must equal the
 * manual sum of all item subtotals (unit_price * quantity).
 * Returns true if they agree within a 1e-9 tolerance.
 */
static bool orderTotalIsConsistent(const ShoppingCart& cart) {
    double manualSum = 0.0;
    for (const CartItem& item : cart.getItems()) {
        manualSum += item.getSubtotal();
    }
    return std::fabs(cart.getOrderTotal() - manualSum) < 1e-9;
}

// ---------------------------------------------------------------------------
// Property 16a: removeItem with a non-existent ID returns false and
//               leaves the cart unchanged.
// ---------------------------------------------------------------------------
TEST_CASE("Property 16a: removeItem with non-existent ID returns false and cart is unchanged") {
    // Feature: bakery-pos-system, Property 16: System survives any invalid input sequence

    constexpr int NUM_ITERATIONS = 150;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> sizeDist(0, CATALOG_SIZE - 1);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        ShoppingCart cart;

        // Populate with 0–4 catalog products
        int numItems = sizeDist(rng);
        std::vector<int> indices = {0, 1, 2, 3, 4, 5, 6, 7};
        std::shuffle(indices.begin(), indices.end(), rng);
        for (int i = 0; i < numItems; ++i) {
            cart.addItem(CATALOG[indices[i]], randomQty(rng));
        }

        const std::string& badId = randomInvalidId(rng);
        std::vector<ItemSnapshot> before = snapshot(cart);

        // --- Precondition: the ID is NOT in the cart ---
        bool notPresent = !cart.containsProduct(badId);
        REQUIRE_MESSAGE(
            notPresent,
            "Test setup error: badId is unexpectedly present in cart."
            " Iteration: " << iter << ", ID: " << badId
        );

        // --- removeItem must return false ---
        bool result = cart.removeItem(badId);
        REQUIRE_MESSAGE(
            result == false,
            "removeItem returned true for a non-existent Product_ID."
            " Iteration: " << iter << ", ID: " << badId
        );

        // --- Cart must be unchanged ---
        std::vector<ItemSnapshot> after = snapshot(cart);
        REQUIRE_MESSAGE(
            after == before,
            "Cart state changed after removeItem with non-existent ID."
            " Iteration: " << iter << ", ID: " << badId
        );
    }
}

// ---------------------------------------------------------------------------
// Property 16b: updateItem with a non-existent ID returns false and
//               leaves the cart unchanged.
// ---------------------------------------------------------------------------
TEST_CASE("Property 16b: updateItem with non-existent ID returns false and cart is unchanged") {
    // Feature: bakery-pos-system, Property 16: System survives any invalid input sequence

    constexpr int NUM_ITERATIONS = 150;
    std::mt19937 rng(77);
    std::uniform_int_distribution<int> sizeDist(0, CATALOG_SIZE - 1);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        ShoppingCart cart;

        // Populate with 0–4 catalog products
        int numItems = sizeDist(rng);
        std::vector<int> indices = {0, 1, 2, 3, 4, 5, 6, 7};
        std::shuffle(indices.begin(), indices.end(), rng);
        for (int i = 0; i < numItems; ++i) {
            cart.addItem(CATALOG[indices[i]], randomQty(rng));
        }

        const std::string& badId = randomInvalidId(rng);
        std::vector<ItemSnapshot> before = snapshot(cart);

        // --- Precondition: the ID is NOT in the cart ---
        bool notPresent = !cart.containsProduct(badId);
        REQUIRE_MESSAGE(
            notPresent,
            "Test setup error: badId is unexpectedly present in cart."
            " Iteration: " << iter << ", ID: " << badId
        );

        // --- updateItem must return false ---
        bool result = cart.updateItem(badId, randomQty(rng));
        REQUIRE_MESSAGE(
            result == false,
            "updateItem returned true for a non-existent Product_ID."
            " Iteration: " << iter << ", ID: " << badId
        );

        // --- Cart must be unchanged ---
        std::vector<ItemSnapshot> after = snapshot(cart);
        REQUIRE_MESSAGE(
            after == before,
            "Cart state changed after updateItem with non-existent ID."
            " Iteration: " << iter << ", ID: " << badId
        );
    }
}

// ---------------------------------------------------------------------------
// Property 16c: Valid addItem still works correctly after a sequence of
//               invalid operations.
// ---------------------------------------------------------------------------
TEST_CASE("Property 16c: Valid addItem succeeds after a sequence of invalid operations") {
    // Feature: bakery-pos-system, Property 16: System survives any invalid input sequence

    constexpr int NUM_ITERATIONS = 100;
    std::mt19937 rng(13);
    std::uniform_int_distribution<int> opCountDist(1, 10);
    std::uniform_int_distribution<int> opTypeDist(0, 1); // 0=invalid remove, 1=invalid update

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        ShoppingCart cart;

        // Fire a random sequence of invalid operations (remove and update with
        // non-existent IDs) to stress-test resilience
        int numInvalidOps = opCountDist(rng);
        for (int op = 0; op < numInvalidOps; ++op) {
            const std::string& badId = randomInvalidId(rng);
            if (opTypeDist(rng) == 0) {
                cart.removeItem(badId);    // must silently return false
            } else {
                cart.updateItem(badId, randomQty(rng)); // must silently return false
            }
        }

        // After all those invalid ops, the cart must still accept a valid addItem
        const Product& p = CATALOG[iter % CATALOG_SIZE];
        int qty = randomQty(rng);
        cart.addItem(p, qty);

        // Verify the item landed correctly
        REQUIRE_MESSAGE(
            cart.containsProduct(p.getId()),
            "containsProduct returned false after addItem following invalid ops."
            " Iteration: " << iter
        );
        REQUIRE_MESSAGE(
            cart.getItems().size() == 1u,
            "Cart should have exactly 1 item after one valid addItem."
            " Iteration: " << iter << ", size: " << cart.getItems().size()
        );
        REQUIRE_MESSAGE(
            cart.getItems()[0].getQuantity() == qty,
            "Quantity mismatch after addItem following invalid ops."
            " Iteration: " << iter
            << ", expected: " << qty
            << ", got: " << cart.getItems()[0].getQuantity()
        );
    }
}

// ---------------------------------------------------------------------------
// Property 16d: getOrderTotal() always equals the sum of item subtotals —
//               the invariant survives any sequence of invalid operations.
// ---------------------------------------------------------------------------
TEST_CASE("Property 16d: Order total invariant holds after mixed valid and invalid operations") {
    // Feature: bakery-pos-system, Property 16: System survives any invalid input sequence

    constexpr int NUM_ITERATIONS = 100;
    std::mt19937 rng(55);
    std::uniform_int_distribution<int> opCountDist(5, 20);
    // Operation types: 0=add valid, 1=remove invalid, 2=update invalid,
    //                  3=remove valid (may succeed or not), 4=update valid (may succeed or not)
    std::uniform_int_distribution<int> opTypeDist(0, 4);
    std::uniform_int_distribution<int> catalogDist(0, CATALOG_SIZE - 1);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        ShoppingCart cart;

        int numOps = opCountDist(rng);
        for (int op = 0; op < numOps; ++op) {
            int type = opTypeDist(rng);
            if (type == 0) {
                // Valid add
                cart.addItem(CATALOG[catalogDist(rng)], randomQty(rng));
            } else if (type == 1) {
                // Invalid remove (non-existent ID) — must leave cart unchanged
                cart.removeItem(randomInvalidId(rng));
            } else if (type == 2) {
                // Invalid update (non-existent ID) — must leave cart unchanged
                cart.updateItem(randomInvalidId(rng), randomQty(rng));
            } else if (type == 3) {
                // Attempt to remove a valid catalog ID (may or may not be in cart)
                cart.removeItem(CATALOG[catalogDist(rng)].getId());
            } else {
                // Attempt to update a valid catalog ID (may or may not be in cart)
                cart.updateItem(CATALOG[catalogDist(rng)].getId(), randomQty(rng));
            }

            // After EVERY operation the total invariant must hold
            REQUIRE_MESSAGE(
                orderTotalIsConsistent(cart),
                "Order total is inconsistent with sum of subtotals."
                " Iteration: " << iter << ", op: " << op << ", op type: " << type
            );
        }
    }
}

// ---------------------------------------------------------------------------
// Property 16e: getItems().size() is always consistent — the count exactly
//               matches the number of distinct products currently in the cart.
// ---------------------------------------------------------------------------
TEST_CASE("Property 16e: Cart item count stays consistent after invalid operations") {
    // Feature: bakery-pos-system, Property 16: System survives any invalid input sequence

    constexpr int NUM_ITERATIONS = 100;
    std::mt19937 rng(99);
    std::uniform_int_distribution<int> sizeDist(1, CATALOG_SIZE);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        ShoppingCart cart;

        // Add a known set of distinct products
        int numToAdd = sizeDist(rng);
        std::vector<int> indices = {0, 1, 2, 3, 4, 5, 6, 7};
        std::shuffle(indices.begin(), indices.end(), rng);

        for (int i = 0; i < numToAdd; ++i) {
            cart.addItem(CATALOG[indices[i]], randomQty(rng));
        }

        std::size_t expectedCount = static_cast<std::size_t>(numToAdd);

        // Fire a random burst of invalid remove and update operations
        for (int invalidOp = 0; invalidOp < 15; ++invalidOp) {
            const std::string& badId = randomInvalidId(rng);
            cart.removeItem(badId);
            cart.updateItem(badId, randomQty(rng));
        }

        // Count must still equal the number of distinct products added
        REQUIRE_MESSAGE(
            cart.getItems().size() == expectedCount,
            "getItems().size() changed after invalid operations."
            " Iteration: " << iter
            << ", expected: " << expectedCount
            << ", got: " << cart.getItems().size()
        );

        // Every product we added must still be present
        for (int i = 0; i < numToAdd; ++i) {
            REQUIRE_MESSAGE(
                cart.containsProduct(CATALOG[indices[i]].getId()),
                "A previously added product is missing after invalid operations."
                " Iteration: " << iter
                << ", Product_ID: " << CATALOG[indices[i]].getId()
            );
        }
    }
}
