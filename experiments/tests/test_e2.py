import e2_continuous_knob as e2


def test_e2_pass_criteria():
    # Pooled design: dH1 exactly invariant across the knob; merge gain monotone in
    # the partner's realized community strength (B_benefit) over pooled samples.
    res = e2.run(n_points=7, realizations=6, restarts=150, seed=1)
    assert res["var_dH1"] < 1e-6                       # dH1 blind to community structure
    assert abs(res["spearman_gain_benefit"]) >= 0.8    # gain tracks realized modularity
    assert res["n_pooled"] >= 30                        # robust over many samples
