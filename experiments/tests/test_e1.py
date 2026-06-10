from premerge_py import witnesses as w, kernel as k
import e1_dichotomy as e1

def test_versions_share_M1_M2_M3_differ_M4():
    EB1, VB1, pB1 = w.partner_v1(); EB2, VB2, pB2 = w.partner_v2()
    m1 = k.build_message(EB1, VB1, w.S, pB1); m2 = k.build_message(EB2, VB2, w.S, pB2)
    assert m1["M1"] == m2["M1"]
    assert sorted(m1["M2"]) == sorted(m2["M2"])
    assert m1["M3"] == m2["M3"]
    assert m1["M4"] != m2["M4"]

def test_e1_pass_criteria():
    res = e1.run(restarts=400, seed=1)
    assert abs(res["B"]["dH1"] - res["Bp"]["dH1"]) < 1e-9
    assert abs(res["B"]["H_M"] - res["Bp"]["H_M"]) > 0.05
