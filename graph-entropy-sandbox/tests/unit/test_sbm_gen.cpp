#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "entropy/exp_a/sbm_gen.hpp"
#include "entropy/exp_a/entropy2d.hpp"
#include "entropy/exp_a/estimators.hpp"
#include <numeric>
#include <stdexcept>

using namespace entropy;
using namespace entropy::exp_a;
using Catch::Approx;

// Helper: make node_ids = {0, 1, ..., n-1}
static std::vector<NodeId> iota_ids(int n) {
    std::vector<NodeId> ids(n);
    std::iota(ids.begin(), ids.end(), NodeId{0});
    return ids;
}

TEST_CASE("mu_to_p produces valid probabilities", "[sbm_gen]") {
    // n=300, k=4, avg_degree=10, mu=0.2 => standard params
    SbmParams p{300, 4, 10.0, 0.2, 42};
    auto probs = mu_to_p(p);
    REQUIRE(probs.p_in >= 0.0);
    REQUIRE(probs.p_in <= 1.0);
    REQUIRE(probs.p_out >= 0.0);
    REQUIRE(probs.p_out <= 1.0);
    // p_in = 10*(1-0.2)/(75-1) = 8/74 ~ 0.108
    REQUIRE(probs.p_in == Approx(10.0*0.8/74.0).epsilon(1e-9));
    // p_out = 10*0.2/(300-75) = 2/225 ~ 0.00889
    REQUIRE(probs.p_out == Approx(10.0*0.2/225.0).epsilon(1e-9));
}

TEST_CASE("mu_to_p throws on invalid probability", "[sbm_gen]") {
    // mu=0.0 with very high avg_degree should exceed p_in=1
    SbmParams p{10, 2, 50.0, 0.0, 42};  // p_in = 50*1/(5-1) = 12.5 > 1
    REQUIRE_THROWS_AS(mu_to_p(p), std::invalid_argument);
}

TEST_CASE("generate_sbm assigns correct communities", "[sbm_gen]") {
    SbmParams p{20, 4, 5.0, 0.2, 123};
    auto ids = iota_ids(20);
    PlantedGraph pg = generate_sbm(p, ids);

    // Nodes 0..4 -> community 0, 5..9 -> 1, 10..14 -> 2, 15..19 -> 3
    REQUIRE(pg.community[0] == 0);
    REQUIRE(pg.community[4] == 0);
    REQUIRE(pg.community[5] == 1);
    REQUIRE(pg.community[10] == 2);
    REQUIRE(pg.community[15] == 3);
    REQUIRE(pg.community[19] == 3);
}

// Test 13: disconnected graph rejection path
TEST_CASE("is_connected detects disconnected graphs", "[sbm_gen][test13]") {
    // Build a disconnected graph: two separate edges 0-1 and 2-3
    GraphBuilder b(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
    b.reserve_nodes(4);
    b.add_edge(0,1);
    b.add_edge(2,3);
    Graph g = std::move(b).build("disconnected");
    REQUIRE_FALSE(is_connected(g));

    // Connected path graph
    GraphBuilder b2(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
    b2.reserve_nodes(4);
    b2.add_edge(0,1); b2.add_edge(1,2); b2.add_edge(2,3);
    Graph g2 = std::move(b2).build("path");
    REQUIRE(is_connected(g2));
}

TEST_CASE("realized_mu_B is in [0,1]", "[sbm_gen]") {
    SbmParams p{20, 4, 5.0, 0.2, 456};
    auto ids = iota_ids(20);
    PlantedGraph pg = generate_sbm(p, ids);
    double mu_r = realized_mu_B(pg.g, pg.community);
    REQUIRE(mu_r >= 0.0);
    REQUIRE(mu_r <= 1.0);
}
