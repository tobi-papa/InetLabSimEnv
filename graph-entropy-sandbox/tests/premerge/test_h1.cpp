#include "entropy/premerge/h1.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
using namespace entropy::premerge;

TEST_CASE("H1 of 3-node path = 1.5 bits", "[h1]") {
    EdgeList E = {{0,1},{1,2}};
    NodeSet  V = {0,1,2};
    auto [h1, W] = h1_value(E, V);
    REQUIRE(W == 4);                 // degrees 1,2,1 -> vol 4
    REQUIRE(h1 == Catch::Approx(1.5).epsilon(1e-12));
}
