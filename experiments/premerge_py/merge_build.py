"""Build a synthetic two-graph merge with a known anchor set S (aligned ids)."""
from premerge_py import generators as gen, kernel as k

def build_sbm_merge(sizes_A, sizes_B, n_anchors, seed, p_in=0.6, p_out=0.05):
    EA, VA, pA_planted = gen.gen_sbm(sizes_A, p_in, p_out, seed=seed)
    EB0, VB0, _ = gen.gen_sbm(sizes_B, p_in, p_out, seed=seed + 1)
    shift = max(VA) + 1
    VB = [n + shift for n in VB0]; EB = [(u + shift, v + shift) for u, v in EB0]
    a_targets = sorted(VA)[:n_anchors]
    b_sources = sorted(VB)[:n_anchors]
    remap = dict(zip(b_sources, a_targets))
    VB = sorted({remap.get(n, n) for n in VB})
    EB = [(remap.get(u, u), remap.get(v, v)) for u, v in EB]
    EB = [(u, v) for u, v in EB if u != v]
    S = sorted(set(VA) & set(VB))
    _, pA = k.h2_min(EA, VA, method="agglo", restarts=200, seed=seed)
    _, pB = k.h2_min(EB, VB, method="agglo", restarts=200, seed=seed + 2)
    return {"EA": EA, "VA": VA, "EB": EB, "VB": VB, "S": S, "pA": pA, "pB": pB}
