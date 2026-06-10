#include "entropy/premerge/bias.hpp"
#include <algorithm>
#include <cmath>
namespace entropy::premerge {
BiasBounds bias_bounds(const NodeSet& S, const std::map<Node,long>& dM, long W) {
    long volS = 0;
    for (Node s : S) {
        auto it = dM.find(s);
        if (it != dM.end()) volS += it->second;
    }
    double sigma = (W > 0) ? static_cast<double>(volS) / W : 0.0;
    BiasBounds b;
    b.sigma = sigma;
    b.seam_bound = 2.0 * sigma * std::log2(static_cast<double>(std::max<long>(W, 2)));
    b.renorm_cap = 2.0 * std::log2(std::exp(1.0)) / std::exp(1.0); // ~= 1.0615
    return b;
}
} // namespace entropy::premerge
