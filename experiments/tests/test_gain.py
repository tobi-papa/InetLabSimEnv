from premerge_py import gain, kernel as k

def test_gain_terms_sum_to_total():
    EA, VA = [(0,1),(1,2),(0,2)], [0,1,2]
    EB, VB = [(2,3),(3,4),(2,4)], [2,3,4]
    S = [2]
    g = gain.gain_decomposition(EA, VA, EB, VB, S, partition="oracle")
    assert abs(g["gain"] - (g["dH1"] - g["dHq"] + g["dS"])) < 1e-9
