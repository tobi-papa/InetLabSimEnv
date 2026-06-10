#pragma once
#include "entropy/premerge/types.hpp"
#include <cstdint>
#include <utility>
namespace entropy::premerge {
// Random-restart greedy agglomerative. Returns (best H^P, partition).
// Upper bound on true H². Deterministic given seed.
std::pair<double,Partition> h2_min_agglo(const EdgeList& E, const NodeSet& V,
                                         int restarts, std::uint64_t seed);

// Certified minimum over ALL partitions (set partitions, Bell-number cost).
// Throws std::runtime_error if |V| > max_nodes. Use only for small instances.
std::pair<double,Partition> h2_min_exact(const EdgeList& E, const NodeSet& V, int max_nodes);
} // namespace entropy::premerge
