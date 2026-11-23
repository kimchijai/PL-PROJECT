/**
 * test_property_cartitem_add.cpp
 * ------------------------------
 * Property-based test for new CartItem construction correctness.
 *
 * Property 5: New item addition correctness
 * For any valid product and any positive quantity, the constructed
 * CartItem must:
 *   (a) store the correct quantity (getQuantity() == supplied quantity)
 *   (b) compute the correct subtotal (getSubtotal() == getUnitPrice() * getQuantity())
 *
 * // Feature: bakery-pos-system, Property 5: New item addition correctness
 *
 * Validates: Requirements 3.5
 *
 * Framework: doctest (header-only), random testing loop (>=100 iterations)
 *
 * Compile:
 *   g++ -std=c++17 -I. -Itests tests/test_property_cartitem_add.cpp Product.cpp CartItem.cpp -o tests/test_property_cartitem_add
 */

#include "doctest.h"
#include "Product.h"
#include "CartItem.h"

#include <string>
#include <random>
#include <sstream>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Generates a random product id of the form "P###".
 */
static std::string randomProductId(std::mt19937& rng) {
    std::uniform_int_distribution<int> numDist(1, 999);
    std::ostringstream oss;
    oss << "P" << numDist(rng);
    return oss.str();
}

/**
 * Generates a random non-empty product name from a small fixed set.
 * Using a fixed set keeps the generator simple while still providing
 * variation in the product data.
 */
static std::string randomProductName(std::mt19937& rng) {
    static const std::string names[] = {
        "Chocolate Chip Cookie",
        "Double Chocolate Cookie",
        "Red Velvet Cookie",
        "S'mores Cookie",
        "Brookies"
    };
    std::uniform_int_distribution<int> idx(0, 4);
    return names[idx(rng)];
}

/**
 * Generates a random positive price in the range [1.00, 500.00].
 * Increments of 0.25 to keep values representable as exact sums.
 */
static double randomPrice(std::mt19937& rng) {
    // 1 to 2000 quarter-peso units → PHP 0.25 .. PHP 500.00
    std::uniform_int_distribution<int> quarters(4, 2000);
    return quarters(rng) * 0.25;
}

/**
 * Generates a random positive quantity in the range [1, 100].
 */
static int randomPositiveQty(std::mt19937& rng) {
    std::uniform_int_distribution<int> qtyDist(1, 100);
    return qtyDist(rng);
}

// ---------------------------------------------------------------------------
// Property test
// ---------------------------------------------------------------------------

TEST_CASE("Property 5: New item addition correctness — quantity and subtotal are correct after construction") {
    // Feature: bakery-pos-system, Property 5: New item addition correctness

    // ----------------------------------------------------------------
    // Run 200 iterations with randomly generated products and quantities.
    // ----------------------------------------------------------------
    constexpr int NUM_ITERATIONS = 200;

    std::mt19937 rng(7); // Fixed seed for reproducibility

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        // Generate a random, valid product
        const std::string id    = randomProductId(rng);
        const std::string name  = randomProductName(rng);
        const double      price = randomPrice(rng);
        Product product(id, name, price);

        // Generate a random positive quantity
        const int qty = randomPositiveQty(rng);

        // Construct the CartItem under test
        CartItem item(product, qty);

        // --- (a) Quantity correctness ---
        // getQuantity() must equal the quantity passed to the constructor.
        REQUIRE_MESSAGE(
            item.getQuantity() == qty,
            "Quantity mismatch on iteration " << iter
            << ": expected " << qty
            << ", got " << item.getQuantity()
        );

        // --- (b) Subtotal correctness ---
        // getSubtotal() must equal getUnitPrice() * getQuantity().
        const double expectedSubtotal = item.getUnitPrice() * item.getQuantity();
        REQUIRE_MESSAGE(
            item.getSubtotal() == doctest::Approx(expectedSubtotal),
            "Subtotal mismatch on iteration " << iter
            << ": expected " << expectedSubtotal
            << ", got " << item.getSubtotal()
        );

        // --- Consistency: unit price must equal the product's price ---
        // This confirms the CartItem copied the price correctly.
        REQUIRE_MESSAGE(
            item.getUnitPrice() == doctest::Approx(price),
            "Unit price mismatch on iteration " << iter
            << ": expected " << price
            << ", got " << item.getUnitPrice()
        );
    }
}

TEST_CASE("Property 5 (boundary): Minimum quantity of 1 produces correct subtotal") {
    // Feature: bakery-pos-system, Property 5: New item addition correctness
    // Edge case: quantity == 1 is the smallest valid value.
    // subtotal must equal the unit price exactly.

    constexpr int NUM_ITERATIONS = 100;
    std::mt19937 rng(13);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        const double price = randomPrice(rng);
        Product product(randomProductId(rng), randomProductName(rng), price);

        CartItem item(product, 1);

        REQUIRE(item.getQuantity() == 1);
        REQUIRE(item.getSubtotal() == doctest::Approx(item.getUnitPrice() * 1));
        REQUIRE(item.getSubtotal() == doctest::Approx(price));
    }
}

TEST_CASE("Property 5 (catalog products): All eight catalog products produce correct CartItems") {
    // Feature: bakery-pos-system, Property 5: New item addition correctness
    // Verifies the property against the actual bakery catalog products
    // across varied quantities.

    const Product catalog[] = {
        Product("P001", "Chocolate Chip Cookie",   65.00),
        Product("P002", "Double Chocolate Cookie", 70.00),
        Product("P003", "Red Velvet Cookie",       70.00),
        Product("P004", "S'mores Cookie",          75.00),
        Product("P005", "Brookies",               75.00),
        Product("P006", "Nutella",                65.00),
        Product("P007", "Dubai Chewy Cookie (1 Piece)", 110.00),
        Product("P008", "Dubai Chewy Cookie (4 Pieces)", 420.00)
    };

    std::mt19937 rng(99);
    std::uniform_int_distribution<int> qtyDist(1, 50);

    // Run 20 random quantities per catalog product (160 checks total)
    for (const auto& product : catalog) {
        for (int rep = 0; rep < 20; ++rep) {
            const int qty = qtyDist(rng);
            CartItem item(product, qty);

            CHECK(item.getQuantity() == qty);
            CHECK(item.getSubtotal() == doctest::Approx(item.getUnitPrice() * item.getQuantity()));
            CHECK(item.getUnitPrice() == doctest::Approx(product.getPrice()));
        }
    }
}
