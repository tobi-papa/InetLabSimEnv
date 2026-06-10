#include "entropy/premerge/merge.hpp"
#include <set>
#include <algorithm>
namespace entropy::premerge {
Merged merge(const EdgeList& EA, const NodeSet& VA, const EdgeList& EB, const NodeSet& VB) {
    std::set<EdgePair> es;
    for (auto [u,v] : EA) es.insert(canon(u,v));
    for (auto [u,v] : EB) es.insert(canon(u,v));
    std::set<Node> vs(VA.begin(), VA.end()); vs.insert(VB.begin(), VB.end());
    Merged M; M.EM.assign(es.begin(), es.end()); M.VM.assign(vs.begin(), vs.end());
    return M;
}
std::map<Node,long> overlaps(const EdgeList& EA, const EdgeList& EB, const NodeSet& S) {
    std::set<Node> Sset(S.begin(), S.end());
    std::set<EdgePair> a, b;
    for (auto [u,v] : EA) if (Sset.count(u) && Sset.count(v)) a.insert(canon(u,v));
    for (auto [u,v] : EB) if (Sset.count(u) && Sset.count(v)) b.insert(canon(u,v));
    std::map<Node,long> o; for (Node s : S) o[s] = 0;
    for (auto& e : a) if (b.count(e)) { o[e.first] += 1; o[e.second] += 1; }
    return o;
}
} // namespace entropy::premerge
