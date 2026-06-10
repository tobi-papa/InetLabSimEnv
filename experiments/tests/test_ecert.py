import ecert_certify_oracle as ec

def test_ecert_gap_nonnegative():
    res = ec.run(seed=1, max_nodes=12)
    assert res["n_instances"] >= 1
    for r in res["rows"]:
        assert r["gap"] >= -1e-9            # heuristic never below exact
        assert r["bias_exact"] >= -1e-9     # standalone never below true oracle
