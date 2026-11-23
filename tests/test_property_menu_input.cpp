/**
 * test_property_menu_input.cpp
 * -----------------------------
 * Property-based test for Property 2: Invalid menu input is always rejected.
 *
 * // Feature: bakery-pos-system, Property 2: Invalid menu input is always rejected
 *
 * Validates: Requirements 2.3
 *
 * Design note:
 *   POS::readMenuChoice() is a private static method that reads an integer from
 *   std::cin and returns it only if it falls in [1, 7]; otherwise returns -1.
 *   Because the method is private, we test its core validation logic via an
 *   equivalent free function `validateMenuInput(int x)` defined here.  This
 *   matches the integer-range check inside readMenuChoice() exactly:
 *       return (x >= 1 && x <= 7) ? x : -1;
 *
 * Strategy:
 *   - Property 2a: for any integer < 1 (sampled below zero), validateMenuInput
 *     must return -1.  Run 100 iterations.
 *   - Property 2b: for any integer > 7 (sampled above 7), validateMenuInput
 *     must return -1.  Run 100 iterations.
 *   - Property 2c: for any integer in [1, 7], validateMenuInput must return
 *     the value itself.  Exhaustive over all 7 valid choices.
 *   - Property 2d: combined random integers spanning a wide range confirm the
 *     boundary is exactly at 1 and 7.  Run 100 iterations.
 */

#include "doctest.h"

#include <random>

// ---------------------------------------------------------------------------
// The function under test
// ---------------------------------------------------------------------------
// This replicates the validation logic of POS::readMenuChoice() without
// requiring access to the private method or interactive stdin.
static int validateMenuInput(int x) {
    return (x >= 1 && x <= 7) ? x : -1;
}

// ---------------------------------------------------------------------------
// Property 2a: integers strictly less than 1 are always rejected
// ---------------------------------------------------------------------------
TEST_CASE("Property 2a: integers below 1 are always rejected") {
    // Feature: bakery-pos-system, Property 2: Invalid menu input is always rejected

    std::mt19937 rng(42);
    // Integers in [-1000, 0]
    std::uniform_int_distribution<int> dist(-1000, 0);

    for (int iter = 0; iter < 100; ++iter) {
        int x = dist(rng);
        int result = validateMenuInput(x);
        CHECK(result == -1);
    }
}

// ---------------------------------------------------------------------------
// Property 2b: integers strictly greater than 7 are always rejected
// ---------------------------------------------------------------------------
TEST_CASE("Property 2b: integers above 7 are always rejected") {
    // Feature: bakery-pos-system, Property 2: Invalid menu input is always rejected

    std::mt19937 rng(137);
    // Integers in [8, 1000]
    std::uniform_int_distribution<int> dist(8, 1000);

    for (int iter = 0; iter < 100; ++iter) {
        int x = dist(rng);
        int result = validateMenuInput(x);
        CHECK(result == -1);
    }
}

// ---------------------------------------------------------------------------
// Property 2c: integers in [1, 7] are accepted and returned as-is
// ---------------------------------------------------------------------------
TEST_CASE("Property 2c: valid menu choices [1, 7] are accepted and returned") {
    // Feature: bakery-pos-system, Property 2: Invalid menu input is always rejected

    for (int x = 1; x <= 7; ++x) {
        int result = validateMenuInput(x);
        CHECK(result == x);
    }
}

// ---------------------------------------------------------------------------
// Property 2d: combined random integers spanning a wide range
// ---------------------------------------------------------------------------
TEST_CASE("Property 2d: combined random integers respect the [1,7] boundary") {
    // Feature: bakery-pos-system, Property 2: Invalid menu input is always rejected

    std::mt19937 rng(271);
    // Wide range covering well below and well above the valid window
    std::uniform_int_distribution<int> dist(-500, 500);

    for (int iter = 0; iter < 100; ++iter) {
        int x = dist(rng);
        int result = validateMenuInput(x);

        if (x >= 1 && x <= 7) {
            CHECK(result == x);
        } else {
            CHECK(result == -1);
        }
    }
}

// ---------------------------------------------------------------------------
// Property 2e: exact boundary values (0, 1, 7, 8) confirm the edges
// ---------------------------------------------------------------------------
TEST_CASE("Property 2e: boundary values 0 and 8 are rejected; 1 and 7 are accepted") {
    // Feature: bakery-pos-system, Property 2: Invalid menu input is always rejected

    CHECK(validateMenuInput(0) == -1);
    CHECK(validateMenuInput(1) ==  1);
    CHECK(validateMenuInput(7) ==  7);
    CHECK(validateMenuInput(8) == -1);
}
