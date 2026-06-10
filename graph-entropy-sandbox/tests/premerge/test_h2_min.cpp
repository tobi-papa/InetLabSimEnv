#include "entropy/premerge/h2_min.hpp"
#include "entropy/premerge/partition_entropy.hpp"
#include "entropy/premerge/h1.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
using namespace entropy::premerge;

TEST_CASE("agglo finds two-triangle split below all-in-one", "[h2]") {
    EdgeList E = {{0,1},{1,2},{0,2},{3,4},{4,5},{3,5},{2,3}}; // two triangles + 1 bridge
    NodeSet V = {0,1,2,3,4,5};
    auto [h2, part] = h2_min_agglo(E, V, /*restarts=*/200, /*seed=*/1);
    Partition all_one; for (Node n : V) all_one[n] = 0;
    REQUIRE(h2 <= h_partition(E, V, all_one) + 1e-9);     // never worse than trivial
    auto [h1, W] = h1_value(E, V);
    REQUIRE(h2 <= h1 + 1e-9);                              // H^P = H1 - benefit <= H1
}
