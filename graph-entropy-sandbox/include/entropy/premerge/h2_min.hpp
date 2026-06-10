#pragma once
#include "entropy/premerge/types.hpp"
#include <cstdint>
#include <utility>
namespace entropy::premerge {
// Random-restart greedy agglomerative. Returns (best H^P, partition).
// Upper bound on true H². Deterministic given seed.
std::pair<double,Partition> h2_min_agglo(const EdgeList& E, const NodeSet& V,
                                         int restarts, std::uint64_t seed);
} // namespace entropy::premerge
