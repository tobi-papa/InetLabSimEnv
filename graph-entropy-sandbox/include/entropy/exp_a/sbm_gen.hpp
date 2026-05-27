#pragma once
#include "entropy/core/graph.hpp"
#include <cstdint>
#include <vector>

namespace entropy::exp_a {

struct SbmParams {
    int    n;           // number of nodes in this graph
    int    k;           // number of equal-size communities
    double avg_degree;  // target expected average degree
    double mu;          // external-degree fraction
    uint64_t seed;
};

struct SbmProbabilities {
    double p_in;
    double p_out;
};

// mu_to_p: p_in = avg*(1-mu)/(n/k-1), p_out = avg*mu/(n-n/k).
// Throws std::invalid_argument if p_in or p_out not in [0,1].
SbmProbabilities mu_to_p(const SbmParams& p);

struct PlantedGraph {
    Graph g;
    std::vector<int> community;  // community[i] = community for node i
};

// Generate SBM with global node ID space.
// node_ids: the global IDs assigned to nodes 0..n-1 in this graph.
// node_ids.size() must equal p.n.
PlantedGraph generate_sbm(const SbmParams& p, const std::vector<NodeId>& node_ids);

// BFS connectivity check.
bool is_connected(const Graph& g);

struct MergeResult {
    Graph     g_merge;
    uint64_t  edge_overlap_AB;
    uint64_t  shared_shared_edge_overlap;
};

// Union of G_A and G_B. No self-loops, no multi-edges.
// n_shared: first n_shared global IDs are shared nodes.
// edges_merge = edges_A + edges_B - edge_overlap_AB.
MergeResult union_graphs(const Graph& g_a, const Graph& g_b, int n_shared);

// Degree-preserving rewiring via double-edge swaps.
// Attempts >= 10*|E_B| swaps. Returns graph simple+connected.
// successful_swaps is set to number of accepted swaps.
Graph rewire_degree_preserving(const Graph& g_b, uint64_t seed, int& successful_swaps);

} // namespace entropy::exp_a
