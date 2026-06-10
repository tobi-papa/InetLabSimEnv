from premerge_py import estimators as est, kernel as k


def _two_triangle_merge():
    # G_A: triangle {0,1,2}; G_B: triangle {2,3,4} sharing anchor 2
    EA, VA = [(0,1),(1,2),(0,2)], [0,1,2]
    EB, VB = [(2,3),(3,4),(2,4)], [2,3,4]
    S = [2]
    return EA, VA, EB, VB, S


def test_partition_ordering():
    EA, VA, EB, VB, S = _two_triangle_merge()
    EM, VM = k.merge(EA, VA, EB, VB)
    p_stand = est.partition_standalone(EA, VA, EB, VB, S)
    p_bridge = est.partition_bridge(EA, VA, EB, VB, S)
    h_stand = k.h_partition(EM, VM, p_stand)
    h_bridge = k.h_partition(EM, VM, p_bridge)
    h_oracle, _ = k.h2_min(EM, VM, method="exact", max_nodes=12)
    assert h_stand >= h_bridge - 1e-9 >= 0
    assert h_bridge >= h_oracle - 1e-9
