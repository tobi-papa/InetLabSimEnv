#pragma once
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace entropy::premerge {
using Node     = std::int64_t;                 // GLOBAL integer label
using EdgePair = std::pair<Node, Node>;         // unordered; stored canonical (min,max)
using EdgeList = std::vector<EdgePair>;
using NodeSet  = std::vector<Node>;
using Partition = std::map<Node, Node>;         // node -> module id

inline EdgePair canon(Node u, Node v) { return (u <= v) ? EdgePair{u, v} : EdgePair{v, u}; }
} // namespace entropy::premerge
