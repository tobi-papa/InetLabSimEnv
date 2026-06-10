"""E2 - the dichotomy across a continuum of partner community strength.

What E2 demonstrates
--------------------
E1 is a binary witness (one 8-cycle vs two 4-cycles). E2 shows the same
dichotomy holds across a *range* of partner community strength, robustly and not
as a knife-edge:

  * dH1 (degree-only) is EXACTLY invariant across the sweep, by construction.
  * the merge H2-gain MOVES monotonically with the partner's realized community
    structure.

Construction (and why it is built this way)
-------------------------------------------
We need a family of partner graphs G_B that (a) all share the SAME degree
sequence -- so dH1, a function of degrees only, is invariant -- yet (b) span a
range of community structure.

We draw, per realization r, one modular base graph G_base (two-block SBM) and
produce partners by applying nx.double_edge_swap, which preserves every node's
degree EXACTLY. The number of swaps grades from 0 (keep the base's communities)
up to ~2|E| (randomize them). Because every partner in realization r has G_base's
degree sequence, dH1 is identical across the whole sweep within r; and because we
use the SAME set of realizations at every sweep point, the per-knob MEAN of dH1
is constant across the knob -> Var(dH1) = 0 exactly.

Honest construction caveat (measured, not assumed)
--------------------------------------------------
At the small graph sizes that the agglomerative H2 oracle can afford here
(|G_B| ~ 16), the *nominal* swap knob is a poor proxy for realized modularity:
a 16-node SBM's community structure collapses after even a modest number of
swaps, so achieved modularity is near-bimodal in the swap count (verified:
rho(B_benefit, swap_fraction) ~ 0). We therefore do NOT correlate against the
nominal knob. Instead we measure each partner's REALIZED community strength,

    B_benefit = H1(G_B) - H2(G_B)      (higher = stronger communities),

and pool every (realization x swap-fraction) sample into a scatter of
(B_benefit, merge_gain). Pooling populates the B_benefit axis from both the swap
effect and cross-realization variation, giving a robust Spearman correlation over
many points. This is the correct quantity anyway: theory says the merge signal is
driven by the partner's *actual* community structure, not by a nominal parameter.

Pass criterion
--------------
  Var(dH1 per-knob means) < 1e-6     (dH1 blind to community structure)
  |Spearman(merge_gain, B_benefit)| >= 0.8   over the pooled samples
"""
import argparse
import numpy as np
import networkx as nx
from scipy.stats import spearmanr
from premerge_py import gain, generators as gen, results as R, witnesses as w, kernel as k

T_MAX = 0.50          # contrast label range is cosmetic; the real x-axis is B_benefit
P_IN_HI = 0.55        # base SBM intra-block prob (p_in + p_out = 0.60 constant)
P_OUT_HI = 0.05
MAX_SWAPS_MULTIPLIER = 2   # swaps span 0 .. 2|E| across the sweep
_OFFSET = 300


def _build_base_graph(sizes, seed):
    """Draw the modular base SBM once; returns a NetworkX graph (no seam yet)."""
    E, V, _ = gen.gen_sbm(sizes, p_in=P_IN_HI, p_out=P_OUT_HI, seed=seed)
    G = nx.Graph()
    G.add_nodes_from(V)
    G.add_edges_from(E)
    return G


def _partner_at(frac, G_base, seed):
    """Partner via degree-preserving rewiring of G_base.

    frac in [0,1]: 0 keeps the modular base, 1 fully randomizes it (still exact
    same degree sequence). Returns (EB, VB, B_graph) where B_graph is the rewired
    base BEFORE the seam is attached (used to measure B's own community strength).
    """
    n_swaps = int(round(MAX_SWAPS_MULTIPLIER * G_base.number_of_edges() * frac))
    H = G_base.copy()
    if n_swaps > 0:
        nx.double_edge_swap(H, nswap=n_swaps, max_tries=n_swaps * 20, seed=seed)
    nodes = sorted(H.nodes())
    EB = [(int(u) + _OFFSET, int(v) + _OFFSET) for u, v in H.edges()]
    VB = [int(n) + _OFFSET for n in nodes]
    EB += [(VB[0], 0), (VB[-1], 3)]          # seam to G_A anchors 0 and 3
    VB = sorted(set(VB) | set(w.S))
    return EB, VB, H


def _b_benefit(H, restarts, seed):
    """B's realized community strength = H1(B) - H2(B) (bits). Higher = more modular."""
    Eb = [(int(u), int(v)) for u, v in H.edges()]
    Vb = [int(n) for n in H.nodes()]
    h1, _ = k.h1(Eb, Vb)
    h2, _ = k.h2_min(Eb, Vb, method="agglo", restarts=restarts, seed=seed)
    return h1 - h2


def run(n_points=9, realizations=8, restarts=200, seed=1, sizes=(8, 8)):
    fracs = np.linspace(0.0, 1.0, n_points)
    bases = [_build_base_graph(list(sizes), seed=seed + r * 1000) for r in range(realizations)]
    pooled_x, pooled_gain = [], []     # (B_benefit, merge_gain) per sample
    dH1_knob_means = []                # per-knob mean of dH1 (for the invariance headline)
    for f in fracs:
        dH1_col = []
        for r in range(realizations):
            G = bases[r]
            tseed = seed + 100 * r + int(round(f * 1000))
            EB, VB, H = _partner_at(f, G, seed=tseed)
            bben = _b_benefit(H, restarts=max(80, restarts // 2), seed=7)
            g = gain.gain_decomposition(w.GA_EDGES, w.GA_NODES, EB, VB, w.S, "oracle", restarts, seed + r)
            pooled_x.append(bben)
            pooled_gain.append(g["gain"])
            dH1_col.append(g["dH1"])
        dH1_knob_means.append(float(np.mean(dH1_col)))
    rho = spearmanr(pooled_x, pooled_gain).correlation
    return {
        "B_benefit": [float(x) for x in pooled_x],
        "gain": [float(x) for x in pooled_gain],
        "dH1_knob_means": dH1_knob_means,
        "var_dH1": float(np.var(dH1_knob_means)),
        "spearman_gain_benefit": float(rho),
        "n_pooled": len(pooled_x),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n_points", type=int, default=9)
    ap.add_argument("--realizations", type=int, default=8)
    ap.add_argument("--restarts", type=int, default=200)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--out", default="results")
    a = ap.parse_args()
    res = run(a.n_points, a.realizations, a.restarts, a.seed)
    ok = res["var_dH1"] < 1e-6 and abs(res["spearman_gain_benefit"]) >= 0.8
    status = "PASS" if ok else "FAIL"
    run_w = R.RunWriter("E2", base_seed=a.seed, out_root=a.out)
    for i in range(res["n_pooled"]):
        run_w.add_row({"instance_id": str(i), "DeltaH1": "", "gain_truth": res["gain"][i],
                       "params": f"B_benefit={res['B_benefit'][i]:.4f}"})
    run_w.close(config={**vars(a), "var_dH1": res["var_dH1"],
                        "spearman_gain_benefit": res["spearman_gain_benefit"],
                        "n_pooled": res["n_pooled"], "status": status,
                        "x_axis": "B_benefit = H1(G_B) - H2(G_B) (realized community strength)",
                        "note": "nominal swap knob is near-bimodal at this scale; x-axis is realized modularity"})
    import os
    from plots import e2_scatter
    e2_scatter(res, os.path.join(run_w.run_dir, "e2_scatter.png"))
    print(f"E2 {status}: Var(dH1)={res['var_dH1']:.2e} (<1e-6), "
          f"rho(gain,B_benefit)={res['spearman_gain_benefit']:.3f} over {res['n_pooled']} pts (|rho|>=0.8)")


if __name__ == "__main__":
    main()
