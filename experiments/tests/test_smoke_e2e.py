"""End-to-end smoke test: generate → merge → estimate → write results.

Construction:
  G_A: two SBM blocks of 6, nodes 0..11
  G_B: two SBM blocks of 6, nodes 100..111
  Anchor remap: B nodes 100..104 are remapped to A nodes 0..4, so the two
  graphs share 5 anchors. Edges in G_B that become self-loops after remap
  are dropped. VB is rebuilt as the union of anchors and remapped B-private
  nodes actually present in EB.
"""

from premerge_py import kernel as k, estimators as est, generators as gen, results as R


def _build_merge():
    EA, VA, _ = gen.gen_sbm([6, 6], 0.7, 0.05, seed=1)
    EB_raw, _VB_raw, _ = gen.gen_sbm([6, 6], 0.7, 0.05, seed=2)

    # B originally lives on 100..111; remap 100->0, 101->1, ..., 104->4
    # (i.e. the first 5 nodes of each graph become shared anchors)
    remap = {100 + i: i for i in range(5)}

    def remap_node(n):
        return remap.get(n + 100, n + 100)  # raw gen gives 0-based; shift to 100-range first

    # gen_sbm gives nodes 0..11; shift to 100-range, then apply anchor remap
    EB = []
    for u, v in EB_raw:
        u2 = remap_node(u)
        v2 = remap_node(v)
        if u2 != v2:  # drop self-loops created by remap
            EB.append((u2, v2))

    # VB = anchors ∪ B-private nodes that appear in EB
    anchors = set(remap.values())  # {0,1,2,3,4}
    nodes_in_eb = {u for u, v in EB} | {v for u, v in EB}
    VB = sorted(anchors | nodes_in_eb)

    S = sorted(set(VA) & set(VB))
    assert len(S) >= 1, f"No shared anchors! VA={VA[:5]}, VB={VB[:5]}"
    return EA, VA, EB, VB, S


def test_end_to_end(tmp_path):
    EA, VA, EB, VB, S = _build_merge()

    EM, VM = k.merge(EA, VA, EB, VB)

    p_stand = est.partition_standalone(EA, VA, EB, VB, S)
    h_stand = k.h_partition(EM, VM, p_stand)

    h_oracle, _ = est.partition_oracle(EM, VM, method="agglo", restarts=100, seed=1)

    assert h_stand >= h_oracle - 1e-9, (
        f"Ordering violated: h_stand={h_stand:.6f} < h_oracle={h_oracle:.6f}"
    )

    run = R.RunWriter("E_smoke", base_seed=1, out_root=str(tmp_path))
    run.add_row({
        "instance_id": "0",
        "H_standalone": h_stand,
        "H2_oracle": h_oracle,
        "n_VA": len(VA),
        "n_VB": len(VB),
        "n_S": len(S),
    })
    run.close(config={"smoke": True})
