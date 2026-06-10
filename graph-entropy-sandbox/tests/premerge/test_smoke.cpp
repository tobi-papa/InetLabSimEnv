#include <catch2/catch_test_macros.hpp>
TEST_CASE("catch2 is wired", "[smoke]") { REQUIRE(1 + 1 == 2); }

#include "entropy/util/numerics.hpp"
#include <catch2/catch_approx.hpp>
TEST_CASE("bit-based entropy helpers", "[numerics]") {
    // xlog2x(0.5) = 0.5*log2(0.5) = -0.5
    REQUIRE(entropy::util::xlog2x(0.5) == Catch::Approx(-0.5));
    REQUIRE(entropy::util::xlog2x(0.0) == Catch::Approx(0.0));
    REQUIRE(entropy::util::safe_log2(8.0) == Catch::Approx(3.0));
}
