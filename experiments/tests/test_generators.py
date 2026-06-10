import networkx as nx
from premerge_py import generators as gen

def test_rewire_preserves_degrees():
    G = nx.stochastic_block_model([10, 10], [[0.6, 0.05], [0.05, 0.6]], seed=3)
    before = sorted(d for _, d in G.degree())
    H = gen.degree_preserving_rewire(G, n_swaps=200, seed=7)
    after = sorted(d for _, d in H.degree())
    assert before == after

def test_sbm_returns_planted_partition():
    E, V, planted = gen.gen_sbm([10, 10], p_in=0.6, p_out=0.05, seed=1)
    assert set(planted.values()) == {0, 1} and len(V) == 20
