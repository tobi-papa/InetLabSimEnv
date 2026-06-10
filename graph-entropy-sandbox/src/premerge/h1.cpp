#include "entropy/premerge/h1.hpp"
#include "entropy/util/numerics.hpp"
namespace entropy::premerge {
std::map<Node,long> degrees(const EdgeList& E, const NodeSet& V) {
    std::map<Node,long> d;
    for (Node v : V) d[v] = 0;
    for (auto [u,v] : E) { d[u] += 1; d[v] += 1; }
    return d;
}
std::pair<double,long> h1_value(const EdgeList& E, const NodeSet& V) {
    auto d = degrees(E, V);
    long W = 0; for (auto& [n,dn] : d) W += dn;
    double H = 0.0;
    if (W > 0) for (auto& [n,dn] : d) H -= util::xlog2x(static_cast<double>(dn)/W);
    return {H, W};
}
} // namespace entropy::premerge
