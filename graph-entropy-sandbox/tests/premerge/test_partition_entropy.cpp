#include "entropy/premerge/partition_entropy.hpp"
#include "entropy/premerge/h1.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
using namespace entropy::premerge;

TEST_CASE("H^P 3-node path {0,1}|{2} = 1.2924812 bits", "[hp]") {
    EdgeList E = {{0,1},{1,2}}; NodeSet V = {0,1,2};
    Partition p = {{0,0},{1,0},{2,1}};
    REQUIRE(h_partition(E, V, p) == Catch::Approx(1.2924812).epsilon(1e-6));
}
TEST_CASE("decomposition identity H^P = H1 - H(q) + S", "[hp][identity]") {
    EdgeList E = {{0,1},{1,2}}; NodeSet V = {0,1,2};
    Partition p = {{0,0},{1,0},{2,1}};
    auto d = decompose(E, V, p);
    auto [h1, W] = h1_value(E, V);
    REQUIRE(d.Hq  == Catch::Approx(0.81127812));
    REQUIRE(d.S   == Catch::Approx(0.60375937).epsilon(1e-6));  // corrected: spec had typo 0.60380913
    REQUIRE(h1 - d.Hq + d.S == Catch::Approx(d.HP).epsilon(1e-12));
    REQUIRE(d.HP  == Catch::Approx(1.2924812).epsilon(1e-6));
}
