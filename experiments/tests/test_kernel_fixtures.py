from premerge_py import kernel as k

def test_h1_path():
    h1, W = k.h1([(0,1),(1,2)], [0,1,2])
    assert W == 4 and abs(h1 - 1.5) < 1e-12

def test_hpartition_path():
    hp = k.h_partition([(0,1),(1,2)], [0,1,2], {0:0,1:0,2:1})
    assert abs(hp - 1.2924812) < 1e-6

def test_decompose_identity():
    d = k.decompose([(0,1),(1,2)], [0,1,2], {0:0,1:0,2:1})
    assert abs(d["H1"] - d["Hq"] + d["S"] - d["HP"]) < 1e-12
