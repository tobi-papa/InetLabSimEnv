#pragma once
#include "entropy/premerge/types.hpp"
namespace entropy::premerge {
struct Decomposition { double HP, H1, Hq, S, benefit; long W; };
double h_partition(const EdgeList& E, const NodeSet& V, const Partition& part);
Decomposition decompose(const EdgeList& E, const NodeSet& V, const Partition& part);
} // namespace entropy::premerge
