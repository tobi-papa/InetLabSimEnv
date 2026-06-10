#pragma once
#include "entropy/premerge/types.hpp"
#include <map>
#include <vector>
namespace entropy::premerge {
struct Message {
    std::map<Node,long>                M1;   // anchor B-degrees
    EdgeList                           M2;   // E_B[S]
    std::map<long,long>                M3;   // private B-degree histogram value->count
    std::map<Node,std::pair<long,long>> M4;  // module -> (private volume, private internal edges)
};
// Phase-2 adjustment (carve anchors out of their B-module) is applied inside build_message.
Message build_message(const EdgeList& EB, const NodeSet& VB, const NodeSet& S, const Partition& partB);
// A reconstructs H^{P_M}(G_M) from G_A + message.
double reconstruct(const EdgeList& EA, const NodeSet& VA, const NodeSet& S,
                   const Partition& partA, const Message& msg);
} // namespace entropy::premerge
