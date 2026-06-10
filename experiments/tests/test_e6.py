import e6_standalone_vs_bridge as e6

def test_e6_ordering_holds():
    res = e6.run(anchor_counts=[1,2,3,4], seed=1, restarts=300)
    for r in res["rows"]:
        assert r["H_standalone"] >= r["H_bridge"] - 1e-9
        assert r["H_bridge"]     >= r["H2_oracle"] - 1e-9
