#include "entropy/premerge/h2_min.hpp"
#include "entropy/premerge/partition_entropy.hpp"
#include <random>
#include <set>
#include <map>
#include <vector>
#include <algorithm>
#include <stdexcept>
namespace entropy::premerge {

// adjacency at module granularity: which module pairs are connected by >=1 edge
static std::set<std::pair<Node,Node>> module_adjacency(const EdgeList& E, const Partition& part) {
    std::set<std::pair<Node,Node>> adj;
    for (auto [u,v] : E) {
        Node a = part.at(u), b = part.at(v);
        if (a != b) adj.insert({std::min(a,b), std::max(a,b)});
    }
    return adj;
}

std::pair<double,Partition> h2_min_agglo(const EdgeList& E, const NodeSet& V,
                                         int restarts, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    double best = 1e300; Partition best_part;
    for (int r = 0; r < restarts; ++r) {
        Partition part; for (Node n : V) part[n] = n;     // singletons
        double cur = h_partition(E, V, part);
        bool improved = true;
        while (improved) {
            improved = false;
            auto adj = module_adjacency(E, part);
            std::vector<std::pair<Node,Node>> pairs(adj.begin(), adj.end());
            std::shuffle(pairs.begin(), pairs.end(), rng);  // random ordering per restart
            double bestDelta = 0.0; std::pair<Node,Node> bestPair{0,0}; bool found = false;
            for (auto [a,b] : pairs) {
                Partition cand = part;
                for (auto& [n,m] : cand) if (m == b) m = a;  // merge b into a
                double h = h_partition(E, V, cand);
                if (h < cur - bestDelta - 1e-12) { bestDelta = cur - h; bestPair = {a,b}; found = true; }
            }
            if (found) {
                for (auto& [n,m] : part) if (m == bestPair.second) m = bestPair.first;
                cur -= bestDelta; improved = true;
            }
        }
        if (cur < best) { best = cur; best_part = part; }
    }
    return {best, best_part};
}

std::pair<double,Partition> h2_min_exact(const EdgeList& E, const NodeSet& V, int max_nodes) {
    if (static_cast<int>(V.size()) > max_nodes)
        throw std::runtime_error("h2_min_exact: instance exceeds max_nodes");
    int n = static_cast<int>(V.size());
    std::vector<int> a(n, 0), b(n, 0);     // restricted growth string + running max
    double best = 1e300; Partition best_part;
    auto eval = [&]() {
        Partition part; for (int i = 0; i < n; ++i) part[V[i]] = a[i];
        double h = h_partition(E, V, part);
        if (h < best) { best = h; best_part = part; }
    };
    // iterate all restricted growth strings (each = one set partition)
    while (true) {
        eval();
        int i = n - 1;
        while (i > 0 && a[i] == b[i-1] + 1) { a[i] = 0; --i; }
        if (i == 0) break;
        a[i] += 1;
        int bi = std::max(b[i-1], a[i]);
        for (int j = i; j < n; ++j) b[j] = (j==i)? bi : b[i];
        for (int j = i+1; j < n; ++j) b[j] = std::max(b[j-1], a[j]);
    }
    return {best, best_part};
}

} // namespace entropy::premerge
