#include "entropy/premerge/h2_min.hpp"
#include "entropy/premerge/partition_entropy.hpp"
#include <random>
#include <set>
#include <map>
#include <vector>
#include <algorithm>
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
} // namespace entropy::premerge
