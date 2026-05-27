#pragma once
#include "entropy/core/graph.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace entropy::exp_a {

// H1 (1D structural entropy, log2 bits). m==0 => 0.0.
double compute_H1(const Graph& g);

// H1 from a degree map (for compact estimator): vol = sum of values.
double H1_from_degree_map(const std::unordered_map<NodeId, double>& deg);

// H_P (2D structural entropy for a fixed partition, log2 bits).
// IMPORTANT: uses full graph degree d_i, NOT internal community degree.
// partition[i] = community index for node i (0-indexed, size == num_nodes).
double compute_HP(const Graph& g, const std::vector<int>& partition);

// modularity Q = sum_j [e_jj/m - (V_j/2m)^2]
// e_jj = edges fully inside j, V_j = sum of full degrees in j.
double modularity(const Graph& g, const std::vector<int>& partition);

struct H2Result {
    double h2_est;
    std::vector<int> best_partition;
    int    best_partition_id;
    std::string method;
    uint64_t seed;
    int    n_candidates;
    int    n_unique_scores;
    double score_best;
    double score_second_best;
    double score_median;
    double score_std;
    double best_minus_second;
    int    n_within_1e6;   // n within 1e-6 of best
    int    n_within_1e4;   // n within 1e-4 of best
    int    n_communities_best;
    bool   fragility_warning;
};

// H2_est(G) = min over candidate partitions of H_P(G).
// seeds_G = max(10, ceil(num_nodes/50)).
// Mandatory: always includes single-block partition => H2_est <= H1.
H2Result compute_H2(const Graph& g, uint64_t base_seed);

} // namespace entropy::exp_a
