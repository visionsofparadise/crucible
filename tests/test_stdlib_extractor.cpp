// Phase 3.2 verification: StdlibExtractor materialises the bundled
// Faust standard library into the user data directory and reports the
// destination, with `stdfaust.lib` present after extraction.

#include <catch2/catch_test_macros.hpp>

#include "StdlibExtractor.h"

TEST_CASE ("StdlibExtractor materialises stdfaust.lib", "[stdlib]")
{
    const auto dir = crucible::StdlibExtractor::ensureExtracted();
    REQUIRE (dir.isDirectory());

    const auto stdfaust = dir.getChildFile ("stdfaust.lib");
    REQUIRE (stdfaust.existsAsFile());
    REQUIRE (stdfaust.getSize() > 0);

    // Calling twice is idempotent — the second call should not error
    // or change the directory contents.
    const auto dir2 = crucible::StdlibExtractor::ensureExtracted();
    REQUIRE (dir2 == dir);
    REQUIRE (stdfaust.existsAsFile());
}
