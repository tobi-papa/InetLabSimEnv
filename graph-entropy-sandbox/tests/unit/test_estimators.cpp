#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "entropy/exp_a/estimators.hpp"
#include "entropy/exp_a/entropy2d.hpp"
#include "entropy/exp_a/sbm_gen.hpp"
#include <cmath>
#include <limits>
#include <numeric>

using namespace entropy;
using namespace entropy::exp_a;
using Catch::Approx;

// Test 10: compact estimator uses only vol_B and shared-node degrees from Bob
// (invariant to changes in Bob's topology that preserve those)
TEST_CASE("Compact estimator invariant to Bob topology given same vol_B and shared degrees", "[estimators][test10]") {
    // G_A: triangle 0-1-2
    GraphBuilder ba(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
    ba.reserve_nodes(3);
    ba.add_edge(0,1); ba.add_edge(1,2); ba.add_edge(0,2);
    Graph ga = std::move(ba).build("G_A");

    double H1_A = compute_H1(ga);
    std::vector<NodeId> shared_ids = {0, 1};  // nodes 0,1 are shared

    // Bob topology 1: node 0 has degree 2, node 1 has degree 2, vol_B = 6
    // (0-1, 0-2, 1-3 but 2,3 are B-only)
    GraphBuilder bb1(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
    bb1.reserve_nodes(4);
    bb1.add_edge(0,1); bb1.add_edge(0,2); bb1.add_edge(1,3);  // 3 edges -> vol=6
    Graph gb1 = std::move(bb1).build("G_B1");
    CompactSummary s1 = make_compact_summary(gb1, shared_ids);

    // Build a second summary with the same vol_B and shared degrees directly
    CompactSummary s2;
    s2.vol_B = s1.vol_B;
    s2.degrees_shared = s1.degrees_shared;  // same shared degrees

    // Both summaries are identical => results must be identical
    auto r1 = compact_estimator(ga, s1, shared_ids, H1_A);
    auto r2 = compact_estimator(ga, s2, shared_ids, H1_A);

    REQUIRE(r1.gain_1D_compact == Approx(r2.gain_1D_compact).epsilon(1e-12));
    REQUIRE(r1.H1_merge_compact == Approx(r2.H1_merge_compact).epsilon(1e-12));
}

// Test 11: oracle 1D degree = H1(G_merge) - H1(G_A) exactly
TEST_CASE("Oracle 1D degree gain equals H1(merge) - H1(A)", "[estimators][test11]") {
    GraphBuilder ba(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
    ba.reserve_nodes(3);
    ba.add_edge(0,1); ba.add_edge(1,2); ba.add_edge(0,2);
    Graph ga = std::move(ba).build("G_A");

    GraphBuilder bm(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
    bm.reserve_nodes(5);
    bm.add_edge(0,1); bm.add_edge(1,2); bm.add_edge(0,2);
    bm.add_edge(2,3); bm.add_edge(3,4);
    Graph gm = std::move(bm).build("G_merge");

    double H1_A = compute_H1(ga);
    double gain = oracle_1D_degree_gain(gm, H1_A);

    REQUIRE(gain == Approx(compute_H1(gm) - H1_A).epsilon(1e-12));
}

TEST_CASE("Error metrics computed correctly", "[estimators]") {
    // gain_oracle_est = 1.0, gain_est = 0.8, H2_est_B = 2.0
    auto e = compute_errors(1.0, 0.8, 2.0);
    REQUIRE(e.abs_err == Approx(0.2).epsilon(1e-9));
    REQUIRE(e.rel_err == Approx(0.2).epsilon(1e-9));
    REQUIRE(e.norm_B  == Approx(0.1).epsilon(1e-9));

    // When gain_oracle_est is tiny: rel_err = NaN
    auto e2 = compute_errors(1e-15, 0.5, 2.0);
    REQUIRE(std::isnan(e2.rel_err));
}
