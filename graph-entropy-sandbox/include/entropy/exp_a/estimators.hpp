#pragma once
#include "entropy/core/graph.hpp"
#include "entropy/exp_a/entropy2d.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace entropy::exp_a {

// Realized external-degree fraction for Bob:
// crossing half-edges in G_B / total_volume_B.
double realized_mu_B(const Graph& g_b, const std::vector<int>& community_b);

struct CompactSummary {
    double vol_B;                           // 2 * |E_B|
    std::unordered_map<NodeId, double> degrees_shared;  // shared node degrees in G_B
};

struct CompactEstimatorResult {
    double H1_merge_compact;
    double gain_1D_compact;
    int    summary_compact_size;   // n_shared + 1
    bool   invalid_vol_unshared;   // true if vol_unshared_B < 0
};

// Compact 1D estimator (spec §9.2).
// g_a: Alice's graph. summary: Bob's compact summary.
// shared_node_ids: global IDs of shared nodes.
// H1_A: already-computed H1 of G_A.
CompactEstimatorResult compact_estimator(
    const Graph& g_a,
    const CompactSummary& summary,
    const std::vector<NodeId>& shared_node_ids,
    double H1_A);

// Build compact summary from G_B and shared node IDs.
CompactSummary make_compact_summary(const Graph& g_b,
                                    const std::vector<NodeId>& shared_node_ids);

// Oracle 1D degree: H1(G_merge) - H1(G_A).
double oracle_1D_degree_gain(const Graph& g_merge, double H1_A);

constexpr double TOL_GAIN = 1e-9;

struct ErrorMetrics {
    double abs_err;
    double rel_err;    // NaN if |gain_oracle| <= TOL_GAIN
    double norm_B;     // NaN if H2_est_B <= TOL_GAIN
};

ErrorMetrics compute_errors(double gain_oracle_est,
                             double gain_est,
                             double H2_est_B);

} // namespace entropy::exp_a
