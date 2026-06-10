#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "entropy/premerge/h1.hpp"
#include "entropy/premerge/partition_entropy.hpp"
#include "entropy/premerge/merge.hpp"
#include "entropy/premerge/h2_min.hpp"
#include "entropy/premerge/message.hpp"
#include "entropy/premerge/bias.hpp"
namespace py = pybind11; using namespace entropy::premerge;
using namespace pybind11::literals;
PYBIND11_MODULE(premerge_kernel, m) {
    m.def("h1", &h1_value);                         // (E,V) -> (H1, W)
    m.def("h_partition", &h_partition);             // (E,V,part) -> float
    m.def("decompose", [](const EdgeList& E, const NodeSet& V, const Partition& p){
        auto d = decompose(E,V,p);
        return py::dict("HP"_a=d.HP,"H1"_a=d.H1,"Hq"_a=d.Hq,"S"_a=d.S,"benefit"_a=d.benefit,"W"_a=d.W);
    });
    m.def("merge", [](const EdgeList& EA, const NodeSet& VA, const EdgeList& EB, const NodeSet& VB){
        auto M = merge(EA,VA,EB,VB); return py::make_tuple(M.EM, M.VM); });
    m.def("overlaps", &overlaps);
    m.def("h2_min", [](const EdgeList& E, const NodeSet& V, const std::string& method, int restarts, std::uint64_t seed, int max_nodes){
        if (method=="exact") { auto [h,p]=h2_min_exact(E,V,max_nodes); return py::make_tuple(h,p); }
        auto [h,p]=h2_min_agglo(E,V,restarts,seed); return py::make_tuple(h,p);
    }, py::arg("E"), py::arg("V"), py::arg("method")="agglo", py::arg("restarts")=200, py::arg("seed")=0, py::arg("max_nodes")=12);
    m.def("build_message", [](const EdgeList& EB, const NodeSet& VB, const NodeSet& S, const Partition& pB){
        auto msg = build_message(EB,VB,S,pB);
        return py::dict("M1"_a=msg.M1,"M2"_a=msg.M2,"M3"_a=msg.M3,"M4"_a=msg.M4);
    });
    m.def("reconstruct", [](const EdgeList& EA, const NodeSet& VA, const NodeSet& S, const Partition& pA, const py::dict& d){
        Message msg;
        msg.M1 = d["M1"].cast<std::map<Node,long>>();
        msg.M2 = d["M2"].cast<EdgeList>();
        msg.M3 = d["M3"].cast<std::map<long,long>>();
        msg.M4 = d["M4"].cast<std::map<Node,std::pair<long,long>>>();
        return reconstruct(EA,VA,S,pA,msg);
    });
    m.def("bias_bounds", [](const NodeSet& S, const std::map<Node,long>& dM, long W){
        auto b = bias_bounds(S,dM,W); return py::make_tuple(b.sigma,b.seam_bound,b.renorm_cap); });
}
