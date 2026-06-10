"""Build a family of merges with rising bridge density sigma = vol_{G_M}(S)/W."""
from premerge_py import merge_build as mb, kernel as k

def _merged_degrees(EM, VM):
    d = {n: 0 for n in VM}
    for u, v in EM: d[u] += 1; d[v] += 1
    return d

def sweep(sizes_A, sizes_B, anchor_counts, seed, p_in=0.6, p_out=0.05):
    points = []
    for j, na in enumerate(anchor_counts):
        inst = mb.build_sbm_merge(sizes_A, sizes_B, n_anchors=na, seed=seed + j, p_in=p_in, p_out=p_out)
        EM, VM = k.merge(inst["EA"], inst["VA"], inst["EB"], inst["VB"])
        dM = _merged_degrees(EM, VM)
        W = sum(dM.values())
        volS = sum(dM[s] for s in inst["S"])
        sigma, seam, renorm = k.bias_bounds(inst["S"], dM, W)
        points.append({**inst, "EM": EM, "VM": VM, "dM": dM, "W": W, "volS": volS,
                       "sigma": sigma, "seam_bound": seam, "renorm_cap": renorm, "n_anchors": na})
    return points
