from premerge_py import sigma_sweep as ss

def test_sigma_increases_with_anchors():
    pts = ss.sweep(sizes_A=[6,6], sizes_B=[6,6], anchor_counts=[1,2,3,4], seed=1)
    sigmas = [p["sigma"] for p in pts]
    assert sigmas == sorted(sigmas)
    assert all(0.0 <= s <= 1.0 for s in sigmas)

import e5_bias_vs_sigma as e5

def test_e5_bound_never_violated():
    res = e5.run(anchor_counts=[1,2,3,4,5], seed=1, restarts=300)
    assert all(r["within_bound"] for r in res["rows"])
