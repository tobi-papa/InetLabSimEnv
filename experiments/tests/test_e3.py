from premerge_py import merge_build as mb, kernel as k
import e3_reconstruction as e3

def test_builder_shares_anchors():
    inst = mb.build_sbm_merge(sizes_A=[8,8], sizes_B=[8,8], n_anchors=3, seed=1)
    assert len(inst["S"]) == 3
    assert set(inst["S"]).issubset(set(inst["VA"]) & set(inst["VB"]))

def test_e3_reconstruction_exact():
    res = e3.run(n_instances=5, seed=1)
    assert res["max_abs_error"] < 1e-9
    assert all(r["volume_ok"] for r in res["rows"])
