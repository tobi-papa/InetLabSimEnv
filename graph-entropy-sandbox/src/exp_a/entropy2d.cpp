#include "entropy/exp_a/entropy2d.hpp"
#include "entropy/util/numerics.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>

// splitmix64 for seed expansion (mirrors rng.hpp, avoids including the full header)
#include "entropy/util/rng.hpp"

namespace entropy::exp_a {

// ---------------------------------------------------------------------------
// H1: 1-D structural entropy (log2 bits)
// H1 = -sum_i (d_i / 2m) * log2(d_i / 2m)
// ---------------------------------------------------------------------------
double compute_H1(const Graph& g) {
    const EdgeId m = g.num_edges();
    if (m == 0) return 0.0;

    const double total_volume = 2.0 * static_cast<double>(m);
    const NodeId n = g.num_nodes();
    double H = 0.0;

    for (NodeId v = 0; v < n; ++v) {
        const std::size_t d = g.neighbors(v).size();
        if (d == 0) continue;
        const double p = static_cast<double>(d) / total_volume;
        H -= util::xlog2x(p);  // xlog2x(p) = p*log2(p) <= 0, so -xlog2x(p) >= 0
    }
    return H;
}

// ---------------------------------------------------------------------------
// H1 from a degree map (used by compact estimators)
// ---------------------------------------------------------------------------
double H1_from_degree_map(const std::unordered_map<NodeId, double>& deg) {
    double vol = 0.0;
    for (const auto& [node, d] : deg) {
        if (d > 0.0) vol += d;
    }
    if (vol == 0.0) return 0.0;

    double H = 0.0;
    for (const auto& [node, d] : deg) {
        if (d <= 0.0) continue;
        const double p = d / vol;
        H -= util::xlog2x(p);
    }
    return H;
}

// ---------------------------------------------------------------------------
// H_P: 2-D structural entropy for a fixed partition (log2 bits)
//
// H_P = -sum_j (g_j/2m)*log2(V_j/2m)
//       -sum_j sum_{i in X_j} (d_i/2m)*log2(d_i/V_j)
//
// where:
//   V_j = sum of FULL degrees d_i for nodes i in X_j
//   g_j = number of edges with exactly one endpoint in X_j  (cut edges of X_j)
//
// IMPORTANT: d_i is always the full graph degree, never the internal degree.
// ---------------------------------------------------------------------------
double compute_HP(const Graph& g, const std::vector<int>& partition) {
    const EdgeId m = g.num_edges();
    if (m == 0) return 0.0;

    const double total_volume = 2.0 * static_cast<double>(m);
    const NodeId n = g.num_nodes();

    // Determine number of communities
    int n_communities = 0;
    for (NodeId v = 0; v < n; ++v) {
        const int c = partition[static_cast<std::size_t>(v)];
        if (c + 1 > n_communities) n_communities = c + 1;
    }
    if (n_communities == 0) return 0.0;

    // V_j: module volume (sum of full degrees)
    std::vector<double> V_j(static_cast<std::size_t>(n_communities), 0.0);
    // g_j: cut edges (edges with exactly one endpoint in X_j)
    std::vector<double> g_j(static_cast<std::size_t>(n_communities), 0.0);

    for (NodeId v = 0; v < n; ++v) {
        const int c = partition[static_cast<std::size_t>(v)];
        V_j[static_cast<std::size_t>(c)] += static_cast<double>(g.neighbors(v).size());
    }

    // Count cut edges: for each undirected edge (u,v) with u < v,
    // if partition[u] != partition[v], both modules get +1 cut edge.
    for (NodeId u = 0; u < n; ++u) {
        for (NodeId v : g.neighbors(u)) {
            if (v <= u) continue;  // count each undirected edge once
            if (partition[static_cast<std::size_t>(u)] != partition[static_cast<std::size_t>(v)]) {
                g_j[static_cast<std::size_t>(partition[static_cast<std::size_t>(u)])] += 1.0;
                g_j[static_cast<std::size_t>(partition[static_cast<std::size_t>(v)])] += 1.0;
            }
        }
    }

    // Build per-community node lists
    std::vector<std::vector<NodeId>> nodes_in_community(static_cast<std::size_t>(n_communities));
    for (NodeId v = 0; v < n; ++v) {
        nodes_in_community[static_cast<std::size_t>(partition[static_cast<std::size_t>(v)])].push_back(v);
    }

    double H = 0.0;
    for (int j = 0; j < n_communities; ++j) {
        const double Vj = V_j[static_cast<std::size_t>(j)];
        if (Vj == 0.0) continue;

        const double gj = g_j[static_cast<std::size_t>(j)];
        if (gj > 0.0) {
            // First term: -(g_j/2m) * log2(V_j/2m)
            H -= (gj / total_volume) * util::log2_safe(Vj / total_volume);
        }

        // Node terms: -sum_{i in X_j} (d_i/2m) * log2(d_i/V_j)
        for (NodeId i : nodes_in_community[static_cast<std::size_t>(j)]) {
            const double d_i = static_cast<double>(g.neighbors(i).size());
            if (d_i == 0.0) continue;
            H -= (d_i / total_volume) * util::log2_safe(d_i / Vj);
        }
    }
    return H;
}

// ---------------------------------------------------------------------------
// Modularity Q = sum_j [e_jj/m - (V_j/2m)^2]
// ---------------------------------------------------------------------------
double modularity(const Graph& g, const std::vector<int>& partition) {
    const EdgeId m = g.num_edges();
    if (m == 0) return 0.0;

    const double total_volume = 2.0 * static_cast<double>(m);
    const NodeId n = g.num_nodes();

    int n_communities = 0;
    for (NodeId v = 0; v < n; ++v) {
        const int c = partition[static_cast<std::size_t>(v)];
        if (c + 1 > n_communities) n_communities = c + 1;
    }
    if (n_communities == 0) return 0.0;

    std::vector<double> V_j(static_cast<std::size_t>(n_communities), 0.0);
    std::vector<double> e_jj(static_cast<std::size_t>(n_communities), 0.0);

    for (NodeId u = 0; u < n; ++u) {
        V_j[static_cast<std::size_t>(partition[static_cast<std::size_t>(u)])] +=
            static_cast<double>(g.neighbors(u).size());
    }

    for (NodeId u = 0; u < n; ++u) {
        for (NodeId v : g.neighbors(u)) {
            if (v <= u) continue;  // count each undirected edge once
            if (partition[static_cast<std::size_t>(u)] == partition[static_cast<std::size_t>(v)]) {
                e_jj[static_cast<std::size_t>(partition[static_cast<std::size_t>(u)])] += 1.0;
            }
        }
    }

    double Q = 0.0;
    for (int j = 0; j < n_communities; ++j) {
        const double vj_frac = V_j[static_cast<std::size_t>(j)] / total_volume;
        Q += e_jj[static_cast<std::size_t>(j)] / static_cast<double>(m)
             - vj_frac * vj_frac;
    }
    return Q;
}

// ---------------------------------------------------------------------------
// H2_est: minimum H_P over candidate partitions
// ---------------------------------------------------------------------------
H2Result compute_H2(const Graph& g, uint64_t base_seed) {
    const int n = static_cast<int>(g.num_nodes());

    // seeds_G = max(10, ceil(n / 50))
    const int seeds_G = std::max(10, static_cast<int>(std::ceil(static_cast<double>(n) / 50.0)));

    // Candidate list: (score, partition)
    std::vector<std::pair<double, std::vector<int>>> candidates;
    candidates.reserve(static_cast<std::size_t>(seeds_G + 2));

    // -----------------------------------------------------------------------
    // Candidate 1: single-block partition (all nodes in community 0)
    // By construction this gives H_P == H1, so H2_est <= H1.
    // -----------------------------------------------------------------------
    {
        std::vector<int> part(static_cast<std::size_t>(n), 0);
        const double score = compute_HP(g, part);
        candidates.push_back({score, std::move(part)});
    }

    // -----------------------------------------------------------------------
    // Candidate 2: all-singletons (diagnostic)
    // -----------------------------------------------------------------------
    {
        std::vector<int> part(static_cast<std::size_t>(n));
        std::iota(part.begin(), part.end(), 0);
        const double score = compute_HP(g, part);
        candidates.push_back({score, std::move(part)});
    }

    // -----------------------------------------------------------------------
    // Candidates 3+: multi-seed local search
    // -----------------------------------------------------------------------
    uint64_t seed = base_seed;
    for (int s = 0; s < seeds_G; ++s) {
        seed = util::splitmix64(seed);
        std::mt19937_64 rng(seed);

        // Random initial partition with k_init communities
        const int k_init = std::max(2, std::min(n / 5, 20));
        std::uniform_int_distribution<int> comm_dist(0, k_init - 1);
        std::vector<int> part(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) part[static_cast<std::size_t>(i)] = comm_dist(rng);

        // Local search: try moving each node to its neighbors' communities
        bool improved = true;
        int max_iter = 50;
        while (improved && max_iter-- > 0) {
            improved = false;
            double current_score = compute_HP(g, part);

            for (int v = 0; v < n; ++v) {
                const int old_comm = part[static_cast<std::size_t>(v)];

                // Collect set of neighbor communities (plus current)
                std::set<int> neighbor_comms;
                neighbor_comms.insert(old_comm);
                for (NodeId u : g.neighbors(static_cast<NodeId>(v))) {
                    neighbor_comms.insert(part[static_cast<std::size_t>(u)]);
                }

                int best_comm = old_comm;
                double best_score = current_score;
                for (const int c : neighbor_comms) {
                    if (c == old_comm) continue;
                    part[static_cast<std::size_t>(v)] = c;
                    const double score = compute_HP(g, part);
                    if (score < best_score - 1e-12) {
                        best_score = score;
                        best_comm = c;
                    }
                }
                part[static_cast<std::size_t>(v)] = best_comm;
                if (best_comm != old_comm) {
                    improved = true;
                    current_score = best_score;
                }
            }
        }

        // Normalize (compress) partition labels to 0..k-1
        std::unordered_map<int, int> relabel;
        int next_label = 0;
        for (int& c : part) {
            auto it = relabel.find(c);
            if (it == relabel.end()) {
                relabel[c] = next_label;
                c = next_label++;
            } else {
                c = it->second;
            }
        }

        const double final_score = compute_HP(g, part);
        candidates.push_back({final_score, std::move(part)});
    }

    // -----------------------------------------------------------------------
    // Find best (minimum H_P)
    // -----------------------------------------------------------------------
    int best_idx = 0;
    for (int i = 1; i < static_cast<int>(candidates.size()); ++i) {
        if (candidates[static_cast<std::size_t>(i)].first <
            candidates[static_cast<std::size_t>(best_idx)].first) {
            best_idx = i;
        }
    }

    // -----------------------------------------------------------------------
    // Compute diagnostics
    // -----------------------------------------------------------------------
    std::vector<double> scores;
    scores.reserve(candidates.size());
    for (const auto& [sc, _] : candidates) scores.push_back(sc);

    std::vector<double> sorted_scores = scores;
    std::sort(sorted_scores.begin(), sorted_scores.end());

    const double best_score = candidates[static_cast<std::size_t>(best_idx)].first;
    const double second_best = (sorted_scores.size() > 1) ? sorted_scores[1] : best_score;

    // Count unique scores (within 1e-9 tolerance after sorting)
    int n_unique = 1;
    for (std::size_t i = 1; i < sorted_scores.size(); ++i) {
        if (sorted_scores[i] - sorted_scores[i - 1] > 1e-9) ++n_unique;
    }

    const double median = sorted_scores[sorted_scores.size() / 2];
    const double mean = std::accumulate(sorted_scores.begin(), sorted_scores.end(), 0.0)
                        / static_cast<double>(sorted_scores.size());
    double variance = 0.0;
    for (const double sc : sorted_scores) variance += (sc - mean) * (sc - mean);
    const double std_dev = std::sqrt(variance / static_cast<double>(sorted_scores.size()));

    int n_within_1e6 = 0;
    int n_within_1e4 = 0;
    for (const double sc : scores) {
        if (sc - best_score <= 1e-6) ++n_within_1e6;
        if (sc - best_score <= 1e-4) ++n_within_1e4;
    }

    const auto& best_part = candidates[static_cast<std::size_t>(best_idx)].second;
    const int n_communities_best = best_part.empty()
        ? 0
        : *std::max_element(best_part.begin(), best_part.end()) + 1;

    // Fragility warning: very few candidates agree with the best, and spread is high
    const bool fragility =
        (static_cast<int>(candidates.size()) < 5) && (std_dev > 0.1) && (n_within_1e6 == 1);

    H2Result result;
    result.h2_est              = best_score;
    result.best_partition      = best_part;
    result.best_partition_id   = best_idx;
    result.method              = "greedy_local_search";
    result.seed                = base_seed;
    result.n_candidates        = static_cast<int>(candidates.size());
    result.n_unique_scores     = n_unique;
    result.score_best          = best_score;
    result.score_second_best   = second_best;
    result.score_median        = median;
    result.score_std           = std_dev;
    result.best_minus_second   = second_best - best_score;
    result.n_within_1e6        = n_within_1e6;
    result.n_within_1e4        = n_within_1e4;
    result.n_communities_best  = n_communities_best;
    result.fragility_warning   = fragility;
    return result;
}

} // namespace entropy::exp_a
