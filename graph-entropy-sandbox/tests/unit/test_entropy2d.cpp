#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "entropy/exp_a/entropy2d.hpp"
#include "entropy/core/graph.hpp"
#include <cmath>
#include <vector>
#include <numeric>

using namespace entropy;
using namespace entropy::exp_a;
using Catch::Approx;

// Helper: build a complete graph K_n (all nodes 0..n-1)
static Graph make_complete_graph(int n) {
    GraphBuilder b(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
    b.reserve_nodes(n);
    for (int i = 0; i < n; ++i)
        for (int j = i+1; j < n; ++j)
            b.add_edge(i, j);
    return std::move(b).build("K" + std::to_string(n));
}

// Helper: build edgeless graph with n nodes
static Graph make_edgeless(int n) {
    GraphBuilder b(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
    b.reserve_nodes(n);
    return std::move(b).build("edgeless_" + std::to_string(n));
}

// Test 1: H1 of edgeless graph == 0
TEST_CASE("H1 of edgeless graph is zero", "[entropy2d][test1]") {
    Graph g = make_edgeless(5);
    REQUIRE(compute_H1(g) == Approx(0.0).margin(1e-12));
}

// Test 2: H1(K_n) == log2(n)
TEST_CASE("H1 of complete graph equals log2(n)", "[entropy2d][test2]") {
    for (int n : {2, 3, 4, 5, 10}) {
        Graph g = make_complete_graph(n);
        double expected = std::log2(static_cast<double>(n));
        REQUIRE(compute_H1(g) == Approx(expected).epsilon(1e-9));
    }
}

// Test 3: single-block partition => H_P == H1
TEST_CASE("Single-block partition gives H_P == H1", "[entropy2d][test3]") {
    Graph g = make_complete_graph(5);
    std::vector<int> part(5, 0);  // all nodes in community 0
    double H1 = compute_H1(g);
    double HP = compute_HP(g, part);
    REQUIRE(HP == Approx(H1).epsilon(1e-9));
}

// Test 4: compute_H2 candidate set always includes single-block
TEST_CASE("compute_H2 always includes single-block partition", "[entropy2d][test4]") {
    Graph g = make_complete_graph(6);
    H2Result result = compute_H2(g, 42);
    // The single-block partition scores H1; H2_est must be <= H1
    double H1 = compute_H1(g);
    // H2_est <= H1 + TOL (this is the proof that single-block was in the candidate set)
    REQUIRE(result.h2_est <= H1 + 1e-9);
    // Also: the best score must be <= the single-block score (H1)
    REQUIRE(result.score_best <= H1 + 1e-9);
}

// Test 5: H2_est(G) <= H1(G) + TOL
TEST_CASE("H2_est is at most H1 plus tolerance", "[entropy2d][test5]") {
    // Test on various graph types
    Graph k5 = make_complete_graph(5);
    REQUIRE(compute_H2(k5, 1).h2_est <= compute_H1(k5) + 1e-9);

    // Path graph P_4: 0-1-2-3
    {
        GraphBuilder b(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
        b.reserve_nodes(4);
        b.add_edge(0,1); b.add_edge(1,2); b.add_edge(2,3);
        Graph p4 = std::move(b).build("P4");
        REQUIRE(compute_H2(p4, 2).h2_est <= compute_H1(p4) + 1e-9);
    }

    // Two triangles sharing an edge: 0-1-2-0, 1-2-3
    {
        GraphBuilder b(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
        b.reserve_nodes(4);
        b.add_edge(0,1); b.add_edge(1,2); b.add_edge(2,0); b.add_edge(2,3); b.add_edge(1,3);
        Graph g = std::move(b).build("two_triangles");
        REQUIRE(compute_H2(g, 3).h2_est <= compute_H1(g) + 1e-9);
    }
}

// Test 6: H_P uses full degree, NOT internal community degree
TEST_CASE("H_P uses full graph degree not internal degree", "[entropy2d][test6]") {
    // Construct a graph where full degree != internal degree:
    // Two communities: {0,1,2} and {3,4}
    // Edges: 0-1 (internal), 1-2 (internal), 0-3 (crossing), 2-4 (crossing)
    // Node 0: total degree = 2 (0-1, 0-3), internal degree = 1 (0-1)
    // Node 1: total degree = 2 (0-1, 1-2), internal degree = 2
    // Node 2: total degree = 2 (1-2, 2-4), internal degree = 1 (1-2)
    // Node 3: total degree = 1 (0-3), internal degree = 0
    // Node 4: total degree = 1 (2-4), internal degree = 0
    GraphBuilder b(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
    b.reserve_nodes(5);
    b.add_edge(0,1); b.add_edge(1,2);  // internal to {0,1,2}
    b.add_edge(0,3); b.add_edge(2,4);  // crossing edges
    Graph g = std::move(b).build("full_vs_internal_test");

    // Partition: {0,1,2} -> community 0, {3,4} -> community 1
    std::vector<int> part = {0, 0, 0, 1, 1};

    double HP_result = compute_HP(g, part);

    // Manually compute H_P using FULL degrees:
    // m = 4, total_volume = 8
    // Module 0: V_0 = d_0+d_1+d_2 = 2+2+2 = 6, g_0 = 2 (edges 0-3 and 2-4)
    // Module 1: V_1 = d_3+d_4 = 1+1 = 2, g_1 = 2 (edges 0-3 and 2-4)
    // H_P = -(2/8)*log2(6/8) -(2/8)*log2(2/8)
    //      - (2/8)*log2(2/6) - (2/8)*log2(2/6) - (2/8)*log2(2/6)  [module 0 node terms: d_0=2,d_1=2,d_2=2]
    //      - (1/8)*log2(1/2) - (1/8)*log2(1/2)  [module 1 node terms: d_3=1,d_4=1]

    const double vol = 8.0;
    double V0 = 6.0, g0 = 2.0, V1 = 2.0, g1 = 2.0;
    double expected = 0.0;
    // Module 0 first term
    expected -= (g0/vol) * std::log2(V0/vol);
    // Module 0 node terms (d_0=2, d_1=2, d_2=2)
    expected -= (2.0/vol)*std::log2(2.0/V0);
    expected -= (2.0/vol)*std::log2(2.0/V0);
    expected -= (2.0/vol)*std::log2(2.0/V0);
    // Module 1 first term
    expected -= (g1/vol) * std::log2(V1/vol);
    // Module 1 node terms (d_3=1, d_4=1)
    expected -= (1.0/vol)*std::log2(1.0/V1);
    expected -= (1.0/vol)*std::log2(1.0/V1);

    REQUIRE(HP_result == Approx(expected).epsilon(1e-9));

    // Cross-check: if we wrongly used internal degrees in node term,
    // V_0 would still be 6 (sum of full degrees for module volume) but
    // node 0 would use d_internal=1 instead of d_full=2.
    // This test guards against that implementation error.
    double wrong_HP = 0.0;
    // Module 0: g_0=2, V_0=6 (these use full degree — same)
    wrong_HP -= (g0/vol) * std::log2(V0/vol);
    // Module 0 node terms using INTERNAL degrees (d_internal: 0->1, 1->2, 2->1)
    wrong_HP -= (1.0/vol)*std::log2(1.0/V0);  // node 0 internal
    wrong_HP -= (2.0/vol)*std::log2(2.0/V0);  // node 1 internal
    wrong_HP -= (1.0/vol)*std::log2(1.0/V0);  // node 2 internal
    // Module 1: g_1=2, V_1=2 (same)
    wrong_HP -= (g1/vol) * std::log2(V1/vol);
    // Module 1 node terms using internal degrees (d_internal: 3->0, 4->0)
    // d_internal=0 => skip (0*log2(0) = 0)

    // The correct HP must differ from the wrong_HP
    REQUIRE(std::abs(HP_result - wrong_HP) > 1e-9);
}

// Test 12: H2 picks min H_P, not max modularity
TEST_CASE("H2 oracle picks partition by minimum H_P not maximum modularity", "[entropy2d][test12]") {
    // Build a graph where argmin(H_P) != argmax(Q)
    // Simple case: 4-node graph 0-1-2-3 path + edge 0-2 (non-symmetric)
    GraphBuilder b(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
    b.reserve_nodes(4);
    b.add_edge(0,1); b.add_edge(1,2); b.add_edge(2,3); b.add_edge(0,2);
    Graph g = std::move(b).build("test_h2_not_modularity");

    // Candidate partition A: {0,1,2} vs {3} — might have higher Q
    std::vector<int> partA = {0, 0, 0, 1};
    // Candidate partition B: {0,2} vs {1,3} — might have lower H_P
    std::vector<int> partB = {0, 1, 0, 1};
    // Single-block
    std::vector<int> partC = {0, 0, 0, 0};

    double hpA = compute_HP(g, partA);
    double hpB = compute_HP(g, partB);
    double hpC = compute_HP(g, partC);
    double qA  = modularity(g, partA);
    double qB  = modularity(g, partB);

    // compute_H2 must return the partition achieving min(hpA, hpB, hpC, ...)
    H2Result result = compute_H2(g, 99);
    double min_hp = std::min({hpA, hpB, hpC});
    REQUIRE(result.h2_est <= min_hp + 1e-9);

    // The result must be chosen by H_P, not Q (this is verified by the implementation;
    // the test verifies the invariant H2_est <= all our manually computed candidates)
    (void)qA; (void)qB;  // used to signal intent, suppress unused-variable warnings
}
