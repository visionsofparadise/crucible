// Placeholder Catch2 v3 test — verifies the test infrastructure is wired
// up. Real Core-level tests will replace this in a later phase.

#include <catch2/catch_test_macros.hpp>

TEST_CASE ("test infrastructure works", "[infra]")
{
    REQUIRE (1 + 1 == 2);
}
