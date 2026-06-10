#pragma once
#include "entropy/premerge/types.hpp"
#include <map>
#include <utility>
namespace entropy::premerge {
// Degrees keyed by global label. Self-loops counted twice (as in vol).
std::map<Node,long> degrees(const EdgeList& E, const NodeSet& V);
// Returns (H1 in bits, volume W = sum of degrees).
std::pair<double,long> h1_value(const EdgeList& E, const NodeSet& V);
} // namespace entropy::premerge
