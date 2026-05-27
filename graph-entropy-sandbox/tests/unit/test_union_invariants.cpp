#include <catch2/catch_test_macros.hpp>
#include "entropy/exp_a/sbm_gen.hpp"
#include <numeric>

using namespace entropy;
using namespace entropy::exp_a;

// Test 7: edges_merge == edges_A + edges_B - edge_overlap_AB
TEST_CASE("Union identity edges_merge = edges_A + edges_B - overlap", "[union][test7]") {
    // G_A: edges 0-1, 0-2, 1-2  (triangle among shared nodes 0,1,2)
    GraphBuilder ba(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
    ba.reserve_nodes(3);
    ba.add_edge(0,1); ba.add_edge(0,2); ba.add_edge(1,2);
    Graph ga = std::move(ba).build("G_A");

    // G_B: same triangle (same edges) => overlap = 3, merge = 3
    GraphBuilder bb(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
    bb.reserve_nodes(3);
    bb.add_edge(0,1); bb.add_edge(0,2); bb.add_edge(1,2);
    Graph gb = std::move(bb).build("G_B");

    MergeResult mr = union_graphs(ga, gb, 3);
    REQUIRE((int64_t)mr.edge_overlap_AB == 3);
    REQUIRE((int64_t)mr.shared_shared_edge_overlap == 3);
    // edges_merge = 3 + 3 - 3 = 3
    REQUIRE((int64_t)mr.g_merge.num_edges() ==
            (int64_t)ga.num_edges() + (int64_t)gb.num_edges() - (int64_t)mr.edge_overlap_AB);
}

TEST_CASE("Union identity with no overlap", "[union][test7b]") {
    // G_A and G_B share node 0 but no edges between them
    // G_A: 0-1, G_B: 0-2 (different edges)
    GraphBuilder ba(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
    ba.reserve_nodes(2);
    ba.add_edge(0,1);
    Graph ga = std::move(ba).build("G_A");

    GraphBuilder bb(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
    bb.reserve_nodes(3);
    bb.add_edge(0,2);
    Graph gb = std::move(bb).build("G_B");

    MergeResult mr = union_graphs(ga, gb, 1);  // n_shared=1 (node 0)
    REQUIRE(mr.edge_overlap_AB == 0);
    REQUIRE(mr.g_merge.num_edges() == ga.num_edges() + gb.num_edges());
}

TEST_CASE("Shared-shared edge overlap is correctly counted", "[union][test7c]") {
    int n_shared = 2;
    // G_A: 0-1 (shared-shared), 1-2 (shared to non-shared)
    GraphBuilder ba(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
    ba.reserve_nodes(3);
    ba.add_edge(0,1); ba.add_edge(1,2);
    Graph ga = std::move(ba).build("G_A");

    // G_B: 0-1 (same shared-shared edge), 0-3 (different)
    GraphBuilder bb(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
    bb.reserve_nodes(4);
    bb.add_edge(0,1); bb.add_edge(0,3);
    Graph gb = std::move(bb).build("G_B");

    MergeResult mr = union_graphs(ga, gb, n_shared);
    REQUIRE(mr.edge_overlap_AB == 1);         // edge 0-1 overlaps
    REQUIRE(mr.shared_shared_edge_overlap == 1);  // it's a shared-shared edge
    REQUIRE((int64_t)mr.g_merge.num_edges() ==
            (int64_t)ga.num_edges() + (int64_t)gb.num_edges() - (int64_t)mr.edge_overlap_AB);
}
