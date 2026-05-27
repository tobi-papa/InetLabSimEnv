#include "entropy/exp_a/estimators.hpp"
#include "entropy/exp_a/entropy2d.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace entropy::exp_a {

// ---------------------------------------------------------------------------
// realized_mu_B (spec §7)
// crossing half-edges in G_B / total_volume_B
// ---------------------------------------------------------------------------
double realized_mu_B(const Graph& g_b, const std::vector<int>& community_b) {
    const uint64_t total_vol = 2 * g_b.num_edges();
    if (total_vol == 0) return 0.0;

    uint64_t crossing_halfedges = 0;
    const NodeId n = g_b.num_nodes();
    for (NodeId u = 0; u < n; ++u) {
        // Skip phantom isolated nodes (community index -1)
        if (community_b[u] < 0) continue;
        for (NodeId v : g_b.neighbors(u)) {
            // Count each undirected edge once (only when v > u)
            if (v <= u) continue;
            // Skip if either endpoint is a phantom isolated node
            if (community_b[v] < 0) continue;
            if (community_b[u] != community_b[v]) {
                crossing_halfedges += 2;  // two half-edges cross
            }
        }
    }

    return static_cast<double>(crossing_halfedges) / static_cast<double>(total_vol);
}

// ---------------------------------------------------------------------------
// make_compact_summary (spec §9.2)
// ---------------------------------------------------------------------------
CompactSummary make_compact_summary(const Graph& g_b,
                                    const std::vector<NodeId>& shared_node_ids) {
    CompactSummary s;
    s.vol_B = 2.0 * static_cast<double>(g_b.num_edges());

    // Include all shared nodes, even those with degree 0, so the estimator
    // can track which nodes are shared.
    for (NodeId global_id : shared_node_ids) {
        double d = static_cast<double>(g_b.neighbors(global_id).size());
        s.degrees_shared[global_id] = d;
    }

    return s;
}

// ---------------------------------------------------------------------------
// compact_estimator (spec §9.2)
// ---------------------------------------------------------------------------
CompactEstimatorResult compact_estimator(
    const Graph& g_a,
    const CompactSummary& summary,
    const std::vector<NodeId>& shared_node_ids,
    double H1_A)
{
    // Build merged degree map starting from G_A degrees
    std::unordered_map<NodeId, double> merged_degrees;
    const NodeId n_a = g_a.num_nodes();
    for (NodeId v = 0; v < n_a; ++v) {
        double d = static_cast<double>(g_a.neighbors(v).size());
        if (d > 0.0) {
            merged_degrees[v] = d;
        }
    }

    // Add Bob's shared-node degrees
    double vol_shared_in_B = 0.0;
    for (NodeId v : shared_node_ids) {
        double bob_d = 0.0;
        auto it = summary.degrees_shared.find(v);
        if (it != summary.degrees_shared.end()) {
            bob_d = it->second;
        }
        merged_degrees[v] += bob_d;  // operator[] default-constructs to 0.0 if absent
        vol_shared_in_B += bob_d;
    }

    // Virtual node for unshared Bob volume
    double vol_unshared_B = summary.vol_B - vol_shared_in_B;
    bool invalid = (vol_unshared_B < -1e-9);
    if (vol_unshared_B < 0.0) {
        vol_unshared_B = 0.0;  // clamp negative due to floating point
    }

    // Use a special key for the virtual node that won't conflict with real NodeIds
    constexpr NodeId VIRTUAL_B_KEY = std::numeric_limits<NodeId>::max();
    if (vol_unshared_B > 0.0) {
        merged_degrees[VIRTUAL_B_KEY] = vol_unshared_B;
    }

    double H1_compact = H1_from_degree_map(merged_degrees);
    double gain = H1_compact - H1_A;
    int summary_size = static_cast<int>(shared_node_ids.size()) + 1;  // n_shared + 1 virtual

    return CompactEstimatorResult{H1_compact, gain, summary_size, invalid};
}

// ---------------------------------------------------------------------------
// oracle_1D_degree_gain (spec §9.3)
// ---------------------------------------------------------------------------
double oracle_1D_degree_gain(const Graph& g_merge, double H1_A) {
    return compute_H1(g_merge) - H1_A;
}

// ---------------------------------------------------------------------------
// compute_errors (spec §10)
// ---------------------------------------------------------------------------
ErrorMetrics compute_errors(double gain_oracle_est,
                             double gain_est,
                             double H2_est_B) {
    ErrorMetrics e;
    e.abs_err = std::abs(gain_oracle_est - gain_est);

    if (std::abs(gain_oracle_est) > TOL_GAIN) {
        e.rel_err = e.abs_err / std::abs(gain_oracle_est);
    } else {
        e.rel_err = std::numeric_limits<double>::quiet_NaN();
    }

    if (H2_est_B > TOL_GAIN) {
        e.norm_B = e.abs_err / H2_est_B;
    } else {
        e.norm_B = std::numeric_limits<double>::quiet_NaN();
    }

    return e;
}

} // namespace entropy::exp_a
