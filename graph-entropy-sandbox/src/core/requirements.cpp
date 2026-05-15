#include "entropy/core/requirements.hpp"

namespace entropy {

RequirementCheck check(const Graph& g, const Requirements& r) {
    if (r.undirected && g.directedness() != GraphDirectedness::Undirected)
        return {false, "algorithm requires an undirected graph"};

    if (!r.unweighted_ok && g.weighting() == GraphWeighting::Unweighted)
        return {false, "algorithm requires a weighted graph"};

    if (!r.weighted_ok && g.weighting() == GraphWeighting::Weighted)
        return {false, "algorithm requires an unweighted graph"};

    if (r.max_nodes && g.num_nodes() > *r.max_nodes)
        return {false, "graph exceeds algorithm's max_nodes limit ("
                        + std::to_string(*r.max_nodes) + ")"};

    // r.simple and r.connected checks require graph traversal — TODO Phase 3.
    return {true, ""};
}

} // namespace entropy
