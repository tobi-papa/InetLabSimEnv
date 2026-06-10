#pragma once
#include "entropy/premerge/types.hpp"
#include <map>
namespace entropy::premerge {
struct BiasBounds { double sigma, seam_bound, renorm_cap; };
// sigma = vol_{G_M}(S)/W ; seam_bound = 2*sigma*log2(W) ; renorm_cap = 2*log2(e)/e ~= 1.06.
BiasBounds bias_bounds(const NodeSet& S, const std::map<Node,long>& dM, long W);
} // namespace entropy::premerge
