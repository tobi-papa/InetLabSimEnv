#include "entropy/exp_a/sbm_gen.hpp"
#include "entropy/util/rng.hpp"
#include <algorithm>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace entropy::exp_a {

// ---------------------------------------------------------------------------
// mu_to_p: convert SBM parameters to edge probabilities
//
// p_in  = avg * (1 - mu) / (n/k - 1)
// p_out = avg * mu       / (n - n/k)
//
// Integer division for n/k (block size).
// Throws std::invalid_argument if either probability is outside [0, 1].
// ---------------------------------------------------------------------------
SbmProbabilities mu_to_p(const SbmParams& p) {
    const int block_size = p.n / p.k;  // integer division

    if (block_size <= 1) {
        throw std::invalid_argument(
            "mu_to_p: block_size = n/k must be > 1 (n=" +
            std::to_string(p.n) + ", k=" + std::to_string(p.k) + ")");
    }
    if (p.n <= block_size) {
        throw std::invalid_argument(
            "mu_to_p: n - n/k must be > 0 (n=" +
            std::to_string(p.n) + ", k=" + std::to_string(p.k) + ")");
    }

    const double p_in  = p.avg_degree * (1.0 - p.mu) / static_cast<double>(block_size - 1);
    const double p_out = p.avg_degree * p.mu           / static_cast<double>(p.n - block_size);

    if (p_in < 0.0 || p_in > 1.0) {
        throw std::invalid_argument(
            "mu_to_p: p_in=" + std::to_string(p_in) + " not in [0,1]");
    }
    if (p_out < 0.0 || p_out > 1.0) {
        throw std::invalid_argument(
            "mu_to_p: p_out=" + std::to_string(p_out) + " not in [0,1]");
    }

    return {p_in, p_out};
}

// ---------------------------------------------------------------------------
// generate_sbm: generate a stochastic block model graph in the global ID space
//
// node_ids[i] = global NodeId for local node i (i = 0..p.n-1).
// Communities: local node i belongs to community i / (p.n / p.k).
// ---------------------------------------------------------------------------
PlantedGraph generate_sbm(const SbmParams& p, const std::vector<NodeId>& node_ids) {
    if (static_cast<int>(node_ids.size()) != p.n) {
        throw std::invalid_argument(
            "generate_sbm: node_ids.size() != p.n");
    }

    const SbmProbabilities probs = mu_to_p(p);
    const int block_size = p.n / p.k;

    // Assign each local node to a community
    std::vector<int> community(static_cast<std::size_t>(p.n));
    for (int i = 0; i < p.n; ++i) {
        community[static_cast<std::size_t>(i)] = i / block_size;
    }

    // Seed the RNG
    std::mt19937_64 rng(util::splitmix64(p.seed));
    std::uniform_real_distribution<double> uni(0.0, 1.0);

    GraphBuilder builder(GraphDirectedness::Undirected, GraphWeighting::Unweighted);

    // Add edges
    for (int i = 0; i < p.n; ++i) {
        for (int j = i + 1; j < p.n; ++j) {
            const double pij =
                (community[static_cast<std::size_t>(i)] ==
                 community[static_cast<std::size_t>(j)])
                ? probs.p_in
                : probs.p_out;
            if (uni(rng) < pij) {
                builder.add_edge(node_ids[static_cast<std::size_t>(i)],
                                 node_ids[static_cast<std::size_t>(j)]);
            }
        }
    }

    // Reserve nodes to ensure isolated nodes in the global ID space are included
    const NodeId max_id = *std::max_element(node_ids.begin(), node_ids.end());
    builder.reserve_nodes(max_id + 1);

    // Build community vector indexed by global NodeId (size = max_id + 1)
    std::vector<int> global_community(static_cast<std::size_t>(max_id + 1), -1);
    for (int i = 0; i < p.n; ++i) {
        global_community[static_cast<std::size_t>(node_ids[static_cast<std::size_t>(i)])] =
            community[static_cast<std::size_t>(i)];
    }

    // Build label string
    std::ostringstream label_ss;
    label_ss << "SBM_n" << p.n << "_k" << p.k
             << "_mu" << p.mu << "_seed" << p.seed;

    Graph g = std::move(builder).build(label_ss.str());
    return PlantedGraph{std::move(g), std::move(global_community)};
}

// ---------------------------------------------------------------------------
// is_connected: BFS connectivity check over non-isolated nodes
//
// Returns true if all nodes with at least one edge are reachable from the
// first such node. Empty graphs and all-isolated graphs return true.
// ---------------------------------------------------------------------------
bool is_connected(const Graph& g) {
    const NodeId n = g.num_nodes();
    if (n == 0) return true;

    // Find first non-isolated node
    NodeId start = n;  // sentinel: invalid
    for (NodeId v = 0; v < n; ++v) {
        if (!g.neighbors(v).empty()) { start = v; break; }
    }
    if (start == n) return true;  // all nodes isolated — trivially connected

    std::vector<bool> visited(static_cast<std::size_t>(n), false);
    std::queue<NodeId> q;
    q.push(start);
    visited[static_cast<std::size_t>(start)] = true;

    while (!q.empty()) {
        const NodeId u = q.front(); q.pop();
        for (const NodeId v : g.neighbors(u)) {
            if (!visited[static_cast<std::size_t>(v)]) {
                visited[static_cast<std::size_t>(v)] = true;
                q.push(v);
            }
        }
    }

    // All non-isolated nodes must be reachable
    for (NodeId v = 0; v < n; ++v) {
        if (!g.neighbors(v).empty() && !visited[static_cast<std::size_t>(v)]) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// union_graphs: merge G_A and G_B (no multi-edges, no self-loops)
//
// edge_overlap_AB       = edges present in both G_A and G_B
// shared_shared_edge_overlap = overlap edges where BOTH endpoints are shared nodes
//                              (global id < n_shared)
// ---------------------------------------------------------------------------
MergeResult union_graphs(const Graph& g_a, const Graph& g_b, int n_shared) {
    // Collect canonical (u < v) edges from G_A
    std::set<std::pair<NodeId, NodeId>> edge_set_a;
    {
        const NodeId na = g_a.num_nodes();
        for (NodeId u = 0; u < na; ++u) {
            for (const NodeId v : g_a.neighbors(u)) {
                if (v > u) edge_set_a.emplace(u, v);
            }
        }
    }

    // Collect canonical edges from G_B
    std::set<std::pair<NodeId, NodeId>> edge_set_b;
    {
        const NodeId nb = g_b.num_nodes();
        for (NodeId u = 0; u < nb; ++u) {
            for (const NodeId v : g_b.neighbors(u)) {
                if (v > u) edge_set_b.emplace(u, v);
            }
        }
    }

    // Count overlapping edges
    uint64_t edge_overlap_AB = 0;
    uint64_t shared_shared_overlap = 0;
    for (const auto& e : edge_set_a) {
        if (edge_set_b.count(e)) {
            ++edge_overlap_AB;
            if (e.first < static_cast<NodeId>(n_shared) &&
                e.second < static_cast<NodeId>(n_shared)) {
                ++shared_shared_overlap;
            }
        }
    }

    // Build merged graph: union of edge sets (no duplicates)
    const NodeId max_node_count =
        std::max(g_a.num_nodes(), g_b.num_nodes());

    GraphBuilder builder(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
    builder.reserve_nodes(max_node_count);

    for (const auto& [u, v] : edge_set_a) builder.add_edge(u, v);
    for (const auto& e : edge_set_b) {
        if (!edge_set_a.count(e)) builder.add_edge(e.first, e.second);
    }

    Graph g_merge = std::move(builder).build("G_merge");
    return MergeResult{std::move(g_merge), edge_overlap_AB, shared_shared_overlap};
}

// ---------------------------------------------------------------------------
// rewire_degree_preserving: double-edge swap null model
//
// Attempts >= 10 * |E_B| swaps. Returns a graph that is simple and, whenever
// possible, connected. If the result of the swaps is disconnected, the
// function attempts additional swaps up to 100 * |E_B| before giving up and
// returning the last known state (the caller is responsible for checking
// connectivity if strictness is required).
// ---------------------------------------------------------------------------
Graph rewire_degree_preserving(const Graph& g_b, uint64_t seed, int& successful_swaps) {
    const int n = static_cast<int>(g_b.num_nodes());

    // Build canonical edge list and adjacency set
    std::vector<std::pair<NodeId, NodeId>> edges;
    for (NodeId u = 0; u < static_cast<NodeId>(n); ++u) {
        for (const NodeId v : g_b.neighbors(u)) {
            if (v > u) edges.emplace_back(u, v);
        }
    }

    if (edges.empty()) {
        // No edges — return a copy of g_b
        successful_swaps = 0;
        GraphBuilder builder(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
        builder.reserve_nodes(static_cast<NodeId>(n));
        return std::move(builder).build("G_B_null");
    }

    // Use a set for O(log E) membership tests
    std::set<std::pair<NodeId, NodeId>> edge_set(edges.begin(), edges.end());

    std::mt19937_64 rng(util::splitmix64(seed));
    std::uniform_int_distribution<std::size_t> edge_dist(0, edges.size() - 1);

    const int target_attempts = 10 * static_cast<int>(edges.size());
    successful_swaps = 0;

    auto do_swaps = [&](int n_attempts) {
        if (edges.size() < 2) return;  // need at least 2 edges for a swap
        for (int attempt = 0; attempt < n_attempts; ++attempt) {
            const std::size_t i1 = edge_dist(rng);
            std::size_t i2 = edge_dist(rng);
            while (i2 == i1) i2 = edge_dist(rng);

            const auto [a, b] = edges[i1];
            const auto [c, d] = edges[i2];

            // All four endpoints must be distinct
            if (a == c || a == d || b == c || b == d) continue;

            // Try swap variant 1: (a,b),(c,d) -> (a,c),(b,d)
            const auto e1 = std::make_pair(std::min(a, c), std::max(a, c));
            const auto e2 = std::make_pair(std::min(b, d), std::max(b, d));

            // No self-loops (guaranteed since all 4 endpoints distinct) and
            // neither new edge may already exist
            if (!edge_set.count(e1) && !edge_set.count(e2)) {
                // Remove old edges from set and edge list
                edge_set.erase({std::min(a, b), std::max(a, b)});
                edge_set.erase({std::min(c, d), std::max(c, d)});
                edge_set.insert(e1);
                edge_set.insert(e2);
                edges[i1] = e1;
                edges[i2] = e2;
                ++successful_swaps;
                continue;
            }

            // Try swap variant 2: (a,b),(c,d) -> (a,d),(b,c)
            const auto e3 = std::make_pair(std::min(a, d), std::max(a, d));
            const auto e4 = std::make_pair(std::min(b, c), std::max(b, c));

            if (!edge_set.count(e3) && !edge_set.count(e4)) {
                edge_set.erase({std::min(a, b), std::max(a, b)});
                edge_set.erase({std::min(c, d), std::max(c, d)});
                edge_set.insert(e3);
                edge_set.insert(e4);
                edges[i1] = e3;
                edges[i2] = e4;
                ++successful_swaps;
            }
        }
    };

    do_swaps(target_attempts);

    // Build a temporary graph to check connectivity
    auto build_from_edge_set = [&](const std::string& label) -> Graph {
        GraphBuilder builder(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
        builder.reserve_nodes(static_cast<NodeId>(n));
        for (const auto& [u, v] : edge_set) builder.add_edge(u, v);
        return std::move(builder).build(label);
    };

    Graph result = build_from_edge_set("G_B_null");

    // If disconnected, attempt additional swaps (up to 90 * |E| more)
    if (!is_connected(result)) {
        const int extra_attempts = 90 * static_cast<int>(edges.size());
        do_swaps(extra_attempts);
        result = build_from_edge_set("G_B_null");
    }

    return result;
}

} // namespace entropy::exp_a
