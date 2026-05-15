#pragma once
#include "graph.hpp"
#include <optional>
#include <string>

namespace entropy {

struct Requirements {
    bool undirected    = false;
    bool simple        = false;
    bool connected     = false;
    bool unweighted_ok = true;
    bool weighted_ok   = true;
    std::optional<NodeId> max_nodes;
};

struct RequirementCheck {
    bool        ok;
    std::string reason;
};

RequirementCheck check(const Graph& g, const Requirements& r);

} // namespace entropy
