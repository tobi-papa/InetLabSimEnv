#include <catch2/catch_test_macros.hpp>
#include "entropy/exp_a/sbm_gen.hpp"
#include "entropy/exp_a/entropy2d.hpp"
#include <numeric>

using namespace entropy;
using namespace entropy::exp_a;

static std::vector<NodeId> iota_ids(int n) {
    std::vector<NodeId> ids(n); std::iota(ids.begin(), ids.end(), NodeId{0}); return ids;
}

// Test 8: same seed => identical graphs (content_hash)
TEST_CASE("Same seed produces identical graphs", "[reproducibility][test8]") {
    SbmParams p{30, 3, 8.0, 0.15, 12345};
    auto ids = iota_ids(30);
    PlantedGraph pg1 = generate_sbm(p, ids);
    PlantedGraph pg2 = generate_sbm(p, ids);
    REQUIRE(pg1.g.content_hash() == pg2.g.content_hash());
    REQUIRE(pg1.g.num_edges() == pg2.g.num_edges());
}

// Test 9: different seeds => distinct logged seeds
TEST_CASE("Different trial seeds produce different graphs", "[reproducibility][test9]") {
    auto ids = iota_ids(30);

    SbmParams p1{30, 3, 8.0, 0.15, 11111};
    SbmParams p2{30, 3, 8.0, 0.15, 22222};
    PlantedGraph pg1 = generate_sbm(p1, ids);
    PlantedGraph pg2 = generate_sbm(p2, ids);

    // Different seeds should (with overwhelmingly high probability) produce different graphs
    // This may rarely fail for pathological seeds but is acceptable as a correctness test
    REQUIRE(pg1.g.content_hash() != pg2.g.content_hash());

    // Also verify that H2 results with different base seeds record different seeds
    H2Result r1 = compute_H2(pg1.g, 100);
    H2Result r2 = compute_H2(pg1.g, 200);
    REQUIRE(r1.seed != r2.seed);
}
