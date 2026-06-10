#pragma once
#include "entropy/premerge/types.hpp"
#include <map>
namespace entropy::premerge {
struct Merged { EdgeList EM; NodeSet VM; };
Merged merge(const EdgeList& EA, const NodeSet& VA, const EdgeList& EB, const NodeSet& VB);
// o_i for i in S: number of edges {i,j}, j in S, present in BOTH E_A and E_B.
std::map<Node,long> overlaps(const EdgeList& EA, const EdgeList& EB, const NodeSet& S);
} // namespace entropy::premerge
