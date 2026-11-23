/**
 * test_property_cartitem_update.cpp
 * ----------------------------------
 * Property-based test for quantity update correctness.
 *
 * Property 10: Quantity update correctness
 * For any CartItem and any new positive integer N, after setQuantity(N):
 *   - getQuantity() == N
 *   - getSubtotal() == getUnitPrice() * N
 *
 * // Feature: bakery-pos-system, Property 10: Quantity update correctness
 *
 * Validates: Requirements 6.5
 *
 * Framework: doctest (header-only), random testing loop (>=100 iterations)
 *
 * Compile:
 *   g++ -std=c++17 -I. -Itests tests/test_property_cartitem_update.cpp Product.cpp CartItem.cpp -o tests/test_property_cartitem_update
 */

#include "doctest.h"
#include "Product.h"
#include "CartItem.h"

#include <vector>
#include <string>
#include <random>
#include <cmath>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** The eight seeded catalog products and their configured prices. */
static std::vector<Product> buildCatalog() {
    return {
        Product("P001", "Chocolate Chip Cookie",  65.00),
        Product("P002", "Double Chocolate Cookie", 70.00),
        Product("P003", "Red Velvet Cookie",       70.00),
        Product("P004", "S'mores Cookie",          75.00),
        Product("P005", "Brookies",                75.00),
        Product("P006", "Nutella",                 65.00),
        Product("P007", "Dubai Chewy Cookie (1 Piece)", 110.00),
        Product("P008", "Dubai Chewy Cookie (4 Pieces)", 420.00)
    };
}

// ---------------------------------------------------------------------------
// Property 10: Quantity update correctness
// ---------------------------------------------------------------------------

TEST_CASE("Property 10: Quantity update correctness — setQuantity(N) yields quantity==N and subtotal==unitPrice*N") {
    // Feature: bakery-pos-system, Property 10: Quantity update correctness

    // -----------------------------------------------------------------------
    // Run 200 iterations (well above the minimum of 100) with randomly
    // chosen products, initial quantities, and new quantities so the
    // property is verified across a wide cross-section of the input space.
    // -----------------------------------------------------------------------
    constexpr int NUM_ITERATIONS = 200;

    std::mt19937 rng(99); // Fixed seed — reproducible failures

    const std::vector<Product> catalog = buildCatalog();

    std::uniform_int_distribution<int> idxDist(0, static_cast<int>(catalog.size()) - 1);
    std::uniform_int_distribution<int> initQtyDist(1, 50);  // initial quantity > 0
    std::uniform_int_distribution<int> newQtyDist(1, 100);  // new quantity N > 0

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        // Pick a random product and an arbitrary initial quantity
        const Product& product  = catalog[idxDist(rng)];
        const int      initQty  = initQtyDist(rng);
        const int      N        = newQtyDist(rng);

        CartItem item(product, initQty);

        // ---- precondition sanity check: item was constructed correctly ----
        REQUIRE(item.getQuantity() == initQty);

        // ---- apply the mutation under test --------------------------------
        item.setQuantity(N);

        // ---- Property 10a: quantity must equal N --------------------------
        CHECK_MESSAGE(
            item.getQuantity() == N,
            "setQuantity(" << N << ") left quantity=" << item.getQuantity()
            << " (iteration " << iter << ", product=" << product.getId() << ")"
        );

        // ---- Property 10b: subtotal must equal unitPrice * N --------------
        const double expectedSubtotal = item.getUnitPrice() * N;
        CHECK_MESSAGE(
            item.getSubtotal() == doctest::Approx(expectedSubtotal),
            "getSubtotal()=" << item.getSubtotal()
            << " != unitPrice(" << item.getUnitPrice() << ") * N(" << N << ")="
            << expectedSubtotal
            << " (iteration " << iter << ", product=" << product.getId() << ")"
        );
    }
}

TEST_CASE("Property 10 (boundary): setQuantity(1) yields minimum valid subtotal") {
    // Feature: bakery-pos-system, Property 10: Quantity update correctness

    // Verifies the lower boundary (N=1) for every catalog product.
    constexpr int NUM_ITERATIONS = 100;

    std::mt19937 rng(7);

    const std::vector<Product> catalog = buildCatalog();
    std::uniform_int_distribution<int> idxDist(0, static_cast<int>(catalog.size()) - 1);
    std::uniform_int_distribution<int> initQtyDist(2, 50); // start > 1 so setQuantity(1) is a real change

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        const Product& product = catalog[idxDist(rng)];
        const int      initQty = initQtyDist(rng);

        CartItem item(product, initQty);
        item.setQuantity(1);

        CHECK_MESSAGE(
            item.getQuantity() == 1,
            "After setQuantity(1), quantity=" << item.getQuantity()
            << " (iteration " << iter << ")"
        );

        CHECK_MESSAGE(
            item.getSubtotal() == doctest::Approx(item.getUnitPrice()),
            "After setQuantity(1), subtotal=" << item.getSubtotal()
            << " should equal unitPrice=" << item.getUnitPrice()
            << " (iteration " << iter << ")"
        );
    }
}
