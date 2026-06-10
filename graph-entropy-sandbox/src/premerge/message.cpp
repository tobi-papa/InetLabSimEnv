#include "entropy/premerge/message.hpp"
#include "entropy/premerge/h1.hpp"
#include "entropy/util/numerics.hpp"
#include <set>
#include <algorithm>
namespace entropy::premerge {

Message build_message(const EdgeList& EB, const NodeSet& VB, const NodeSet& S, const Partition& partB) {
    std::set<Node> Sset(S.begin(), S.end());
    auto dB = degrees(EB, VB);
    Message m;
    for (Node i : S) m.M1[i] = dB.count(i) ? dB[i] : 0;                 // M1
    for (auto [u,v] : EB) if (Sset.count(u) && Sset.count(v)) m.M2.push_back(canon(u,v)); // M2 = E_B[S]
    for (auto& [n,dn] : dB) if (!Sset.count(n)) m.M3[dn] += 1;          // M3 over private nodes
    // M4 per B-module over PRIVATE nodes only (anchors carved out, Phase-2)
    for (auto& [n,dn] : dB) if (!Sset.count(n)) m.M4[partB.at(n)].first += dn;   // private volume
    for (auto [u,v] : EB) {
        if (Sset.count(u) || Sset.count(v)) continue;                  // skip anchor-incident
        if (partB.at(u) == partB.at(v)) m.M4[partB.at(u)].second += 1; // private internal edge
    }
    return m;
}

double reconstruct(const EdgeList& EA, const NodeSet& VA, const NodeSet& S,
                   const Partition& partA, const Message& msg) {
    std::set<Node> Sset(S.begin(), S.end());
    auto dA = degrees(EA, VA);
    // overlaps: edges among S present in both E_A and msg.M2
    std::set<EdgePair> EA_S, M2set;
    for (auto [u,v] : EA) if (Sset.count(u) && Sset.count(v)) EA_S.insert(canon(u,v));
    for (auto& e : msg.M2) M2set.insert(canon(e.first,e.second));
    std::map<Node,long> o; for (Node s : S) o[s]=0;
    for (auto& e : EA_S) if (M2set.count(e)) { o[e.first]+=1; o[e.second]+=1; }
    // merged degrees A knows
    std::map<Node,long> dM = dA;
    for (Node i : S) dM[i] = dA[i] + msg.M1.at(i) - o[i];
    // W = A-side merged volume + B-private volume (from M3)
    long W = 0; for (auto& [n,dn] : dM) W += dn;
    for (auto& [k,nk] : msg.M3) W += k * nk;
    // module volumes & internal edges. A-modules from partA; B-modules from M4.
    std::map<Node,long> Vj, eint;
    for (auto& [n,dn] : dM) Vj[partA.at(n)] += dn;          // A-side (anchors on A-side)
    for (auto [u,v] : EA) if (partA.at(u)==partA.at(v)) eint[partA.at(u)] += 1; // A internal
    // add B-internal edges among anchors that are NOT already A-internal (from M2)
    for (auto& e : M2set) if (partA.at(e.first)==partA.at(e.second) && !EA_S.count(e)) eint[partA.at(e.first)] += 1;
    long modbase = 1; for (auto& [j,vj] : Vj) modbase = std::max<long>(modbase, j+1);
    for (auto& [bmod, vp] : msg.M4) { Vj[modbase+bmod] = vp.first; eint[modbase+bmod] = vp.second; }
    // H1(GM): A-side d log d + B-private (from M3); H1 = -(1/W) sum d log2 d + log2 W
    double sum = 0.0;
    for (auto& [n,dn] : dM) if (dn>0) sum += dn * util::safe_log2(static_cast<double>(dn));
    for (auto& [k,nk] : msg.M3) if (k>0) sum += static_cast<double>(k) * util::safe_log2((double)k) * nk;
    double H1M = -(sum / W) + util::safe_log2((double)W);
    // H(q), cuts g_j = V_j - 2 e_j^int, S-term
    double Hq = 0.0, Sterm = 0.0;
    for (auto& [j,vj] : Vj) if (vj>0) {
        double q = static_cast<double>(vj)/W;
        long g = vj - 2*eint[j];
        Hq -= util::xlog2x(q);
        Sterm -= (static_cast<double>(g)/W) * util::safe_log2(q);
    }
    return H1M - Hq + Sterm;
}
} // namespace entropy::premerge
