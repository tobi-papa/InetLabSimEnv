#include "entropy/premerge/partition_entropy.hpp"
#include "entropy/premerge/h1.hpp"
#include "entropy/util/numerics.hpp"
#include <map>
namespace entropy::premerge {

static void module_stats(const EdgeList& E, const NodeSet& V, const Partition& part,
                         std::map<Node,long>& deg, std::map<Node,long>& Vj,
                         std::map<Node,long>& gj, std::map<Node,long>& eint, long& W) {
    deg = degrees(E, V);
    W = 0; for (auto& [n,dn] : deg) W += dn;
    for (auto& [n,dn] : deg) Vj[part.at(n)] += dn;
    for (auto [u,v] : E) {
        Node pu = part.at(u), pv = part.at(v);
        if (pu == pv) eint[pu] += 1;
        else { gj[pu] += 1; gj[pv] += 1; }
    }
}

double h_partition(const EdgeList& E, const NodeSet& V, const Partition& part) {
    std::map<Node,long> deg, Vj, gj, eint; long W;
    module_stats(E, V, part, deg, Vj, gj, eint, W);
    double term1 = 0.0; // -sum_v (d_v/W) log2(d_v/Vj)
    for (auto& [n,dn] : deg) {
        Node j = part.at(n);
        if (dn > 0 && Vj[j] > 0)
            term1 -= (static_cast<double>(dn)/W) * util::safe_log2(static_cast<double>(dn)/Vj[j]);
    }
    double term2 = 0.0; // -sum_j (g_j/W) log2(Vj/W)
    for (auto& [j,vj] : Vj)
        if (vj > 0) term2 -= (static_cast<double>(gj[j])/W) * util::safe_log2(static_cast<double>(vj)/W);
    return term1 + term2;
}

Decomposition decompose(const EdgeList& E, const NodeSet& V, const Partition& part) {
    std::map<Node,long> deg, Vj, gj, eint; long W;
    module_stats(E, V, part, deg, Vj, gj, eint, W);
    auto [H1, W2] = h1_value(E, V); (void)W2;
    double Hq = 0.0, S = 0.0;
    for (auto& [j,vj] : Vj) if (vj > 0) {
        double q = static_cast<double>(vj)/W;
        Hq -= util::xlog2x(q);
        S  -= (static_cast<double>(gj[j])/W) * util::safe_log2(q);
    }
    Decomposition d; d.W = W; d.H1 = H1; d.Hq = Hq; d.S = S;
    d.HP = H1 - Hq + S; d.benefit = Hq - S;
    return d;
}
} // namespace entropy::premerge
