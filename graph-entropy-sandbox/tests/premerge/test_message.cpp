#include "entropy/premerge/message.hpp"
#include "entropy/premerge/merge.hpp"
#include "entropy/premerge/partition_entropy.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <map>
#include <set>
#include <vector>
using namespace entropy::premerge;

// Two 5-cliques + bridge on each side; B remapped to share 3 anchors with A.
static void build_S3(EdgeList& EA, NodeSet& VA, EdgeList& EB, NodeSet& VB,
                     NodeSet& S, Partition& partA, Partition& partB) {
    auto clique = [](std::vector<Node> ns, EdgeList& out){
        for (size_t i=0;i<ns.size();++i) for (size_t j=i+1;j<ns.size();++j) out.push_back(canon(ns[i],ns[j]));
    };
    std::vector<Node> A1={0,1,2,3,4}, A2={5,6,7,8,9};
    clique(A1,EA); clique(A2,EA); EA.push_back(canon(4,5));
    VA={0,1,2,3,4,5,6,7,8,9};
    // B cliques on 1000.. and 2000.., remap (1000->4),(1001->3),(2000->5) => S={3,4,5}
    std::vector<Node> B1={1000,1001,1002,1003,1004}, B2={2000,2001,2002,2003,2004};
    EdgeList rawB; clique(B1,rawB); clique(B2,rawB); rawB.push_back(canon(1004,2000));
    std::map<Node,Node> remap={{1000,4},{1001,3},{2000,5}};
    std::set<Node> vb;
    for (auto [u,v]:rawB){ Node ru=remap.count(u)?remap[u]:u, rv=remap.count(v)?remap[v]:v; if(ru!=rv){EB.push_back(canon(ru,rv)); vb.insert(ru); vb.insert(rv);} }
    VB.assign(vb.begin(), vb.end());
    S={3,4,5};
    for (Node n : A1) partA[n]=0;
    for (Node n : A2) partA[n]=1;
    // B private modules: B1-> 100, B2-> 101 (anchors excluded from partB private)
    for (Node n : VB) if (n>=1000) partB[n] = (n<2000)?100:101;
}

TEST_CASE("M1-M4 reconstruction == direct H^P (|S|=3)", "[message]") {
    EdgeList EA,EB; NodeSet VA,VB,S; Partition partA,partB; build_S3(EA,VA,EB,VB,S,partA,partB);
    auto msg = build_message(EB, VB, S, partB);
    double recon = reconstruct(EA, VA, S, partA, msg);
    // ground truth: direct H^P on the merged standalone partition
    auto M = merge(EA,VA,EB,VB);
    Partition pM = partA; for (auto& [n,m]: partB) pM[n] = m;   // A-side + B-private
    for (Node s : S) pM[s] = partA.at(s);                       // anchors folded to A-side
    double direct = h_partition(M.EM, M.VM, pM);
    REQUIRE(recon == Catch::Approx(direct).epsilon(1e-9));
}
