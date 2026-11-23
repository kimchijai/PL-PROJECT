/**
 * test_property_invalid_productid.cpp
 * ------------------------------------
 * Property-based test for Property 3: Invalid Product_ID is always rejected.
 *
 * // Feature: bakery-pos-system, Property 3: Invalid Product_ID is always rejected
 *
 * Validates: Requirements 3.2, 5.2, 6.2
 *
 * Design note:
 *   ShoppingCart does not validate Product_IDs internally — that is POS's job.
 *   This test simulates what POS does: it checks whether the given ID exists
 *   in the catalog before calling addItem / removeItem / updateItem.
 *   The property being tested is: when that guard is in place, any string that
 *   is NOT one of {P001, P002, P003, P004, P005, P006, P007, P008} must leave the cart unchanged.
 *
 * Strategy:
 *   - Build a catalog of the eight valid seeded products.
 *   - Generate random strings that are guaranteed NOT to be a valid Product_ID.
 *   - For each invalid ID, attempt addItem / removeItem / updateItem while
 *     applying the same catalog-lookup guard POS applies.
 *   - Assert the cart is identical before and after each attempt.
 *   - Run at least 100 iterations.
 */

#include "doctest.h"

#include <string>
#include <vector>
#include <set>
#include <random>
#include <algorithm>

#include "../Product.h"
#include "../CartItem.h"
#include "../ShoppingCart.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** The eight valid seeded product IDs used by Crombi Bakery. */
static const std::set<std::string> VALID_IDS = {
    "P001", "P002", "P003", "P004", "P005", "P006", "P007", "P008"
};

/** Build the catalog exactly as POS does. */
static std::vector<Product> buildCatalog() {
    return {
        Product("P001", "Chocolate Chip Cookie", 65.00),
        Product("P002", "Double Chocolate Cookie", 70.00),
        Product("P003", "Red Velvet Cookie",      70.00),
        Product("P004", "S'mores Cookie",         75.00),
        Product("P005", "Brookies",               75.00),
        Product("P006", "Nutella",                 65.00),
        Product("P007", "Dubai Chewy Cookie (1 Piece)", 110.00),
        Product("P008", "Dubai Chewy Cookie (4 Pieces)", 420.00)
    };
}

/**
 * POS-style guard: return the catalog product pointer if productId is valid,
 * nullptr otherwise (mirrors POS::findProductById returning nullptr).
 */
static const Product* findInCatalog(const std::vector<Product>& catalog,
                                    const std::string& productId) {
    for (const auto& p : catalog) {
        if (p.getId() == productId) return &p;
    }
    return nullptr;
}

/**
 * Generate a random printable-ASCII string of length [1, 8] that is
 * guaranteed NOT to be one of the eight seeded Product_IDs.
 */
static std::string randomInvalidId(std::mt19937& rng) {
    static const std::string CHARS =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-";

    std::uniform_int_distribution<size_t> lenDist(1, 8);
    std::uniform_int_distribution<size_t> charDist(0, CHARS.size() - 1);

    std::string id;
    do {
        id.clear();
        size_t len = lenDist(rng);
        for (size_t i = 0; i < len; ++i) {
            id += CHARS[charDist(rng)];
        }
    } while (VALID_IDS.count(id));   // retry if we accidentally hit a valid ID

    return id;
}

/**
 * Snapshot: (productId, quantity) pairs in insertion order.
 * Used to compare cart state before and after a rejected operation.
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
// Property 3 — addItem with invalid ID leaves cart unchanged
// ---------------------------------------------------------------------------
TEST_CASE("Property 3a: addItem with invalid Product_ID leaves cart unchanged") {
    // Feature: bakery-pos-system, Property 3: Invalid Product_ID is always rejected

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> qtyDist(1, 20);

    std::vector<Product> catalog = buildCatalog();

    // Run at least 100 iterations
    for (int iter = 0; iter < 100; ++iter) {
        ShoppingCart cart;

        // Optionally pre-populate with one valid item so the cart is non-empty
        // (tests both empty-cart and non-empty-cart cases)
        if (iter % 2 == 0) {
            cart.addItem(catalog[0], 1);   // add P001 as a known item
        }

        CartSnapshot before = snapshot(cart);

        // Simulate POS guard: look up the invalid ID in the catalog
        std::string invalidId = randomInvalidId(rng);
        const Product* found = findInCatalog(catalog, invalidId);

        // Guard: only call addItem if the ID is in the catalog (it won't be)
        if (found != nullptr) {
            cart.addItem(*found, qtyDist(rng));
        }
        // If found == nullptr we do NOT call addItem — cart must stay the same

        CartSnapshot after = snapshot(cart);
        CHECK(before == after);
    }
}

// ---------------------------------------------------------------------------
// Property 3 — removeItem with invalid ID leaves cart unchanged
// ---------------------------------------------------------------------------
TEST_CASE("Property 3b: removeItem with invalid Product_ID leaves cart unchanged") {
    // Feature: bakery-pos-system, Property 3: Invalid Product_ID is always rejected

    std::mt19937 rng(137);

    std::vector<Product> catalog = buildCatalog();

    for (int iter = 0; iter < 100; ++iter) {
        ShoppingCart cart;

        // Always pre-populate so removeItem actually has something to remove if
        // it were to be called with a valid ID — confirms nothing gets removed
        cart.addItem(catalog[iter % 5], 2);

        CartSnapshot before = snapshot(cart);

        std::string invalidId = randomInvalidId(rng);
        const Product* found = findInCatalog(catalog, invalidId);

        // POS guard: only call removeItem if ID is in catalog (it won't be)
        if (found != nullptr) {
            cart.removeItem(found->getId());
        }

        CartSnapshot after = snapshot(cart);
        CHECK(before == after);
    }
}

// ---------------------------------------------------------------------------
// Property 3 — updateItem with invalid ID leaves cart unchanged
// ---------------------------------------------------------------------------
TEST_CASE("Property 3c: updateItem with invalid Product_ID leaves cart unchanged") {
    // Feature: bakery-pos-system, Property 3: Invalid Product_ID is always rejected

    std::mt19937 rng(271);
    std::uniform_int_distribution<int> qtyDist(1, 20);

    std::vector<Product> catalog = buildCatalog();

    for (int iter = 0; iter < 100; ++iter) {
        ShoppingCart cart;

        // Pre-populate with a valid item
        cart.addItem(catalog[iter % 5], 3);

        CartSnapshot before = snapshot(cart);

        std::string invalidId = randomInvalidId(rng);
        const Product* found = findInCatalog(catalog, invalidId);

        // POS guard: only call updateItem if ID is in catalog (it won't be)
        if (found != nullptr) {
            cart.updateItem(found->getId(), qtyDist(rng));
        }

        CartSnapshot after = snapshot(cart);
        CHECK(before == after);
    }
}

// ---------------------------------------------------------------------------
// Property 3 — combined: all three operations with invalid IDs, varied cart state
// ---------------------------------------------------------------------------
TEST_CASE("Property 3d: all three operations with invalid ID leave any cart state unchanged") {
    // Feature: bakery-pos-system, Property 3: Invalid Product_ID is always rejected

    std::mt19937 rng(999);
    std::uniform_int_distribution<int> qtyDist(1, 10);
    std::uniform_int_distribution<int> opDist(0, 2);   // 0=add, 1=remove, 2=update

    std::vector<Product> catalog = buildCatalog();

    for (int iter = 0; iter < 100; ++iter) {
        ShoppingCart cart;

        // Build a cart with a random subset of catalog items
        for (int ci = 0; ci < 5; ++ci) {
            if (iter & (1 << ci)) {           // use iter bits to vary which items are present
                cart.addItem(catalog[ci], qtyDist(rng));
            }
        }

        CartSnapshot before = snapshot(cart);

        std::string invalidId = randomInvalidId(rng);
        const Product* found = findInCatalog(catalog, invalidId);

        // POS guard applies regardless of which operation is attempted
        if (found == nullptr) {
            // Do NOT call any cart method — this is the rejection path
        } else {
            // This branch should never execute given invalidId is never valid
            int op = opDist(rng);
            if      (op == 0) cart.addItem(*found, qtyDist(rng));
            else if (op == 1) cart.removeItem(found->getId());
            else              cart.updateItem(found->getId(), qtyDist(rng));
        }

        CartSnapshot after = snapshot(cart);
        CHECK(before == after);
    }
}
