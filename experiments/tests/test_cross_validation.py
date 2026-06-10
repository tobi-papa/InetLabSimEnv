"""Cross-validation: C++ kernel vs. Appendix-B pure-Python reference oracle.

Both H1 and H_partition must agree to within 1e-9 on 50 random SBM instances
each. This is the scientific "two independent implementations agree" gate.
"""

import random
import networkx as nx
from premerge_py import kernel as k, reference as ref


def _edges_nodes(G):
    return [(int(u), int(v)) for u, v in G.edges()], [int(n) for n in G.nodes()]


def test_h1_matches_reference():
    rng = random.Random(0)
    for _ in range(50):
        G = nx.stochastic_block_model(
            [8, 8], [[0.6, 0.05], [0.05, 0.6]], seed=rng.randint(0, 10**6)
        )
        E, V = _edges_nodes(G)
        if not E:
            continue
        h_c, W_c = k.h1(E, V)
        h_r, W_r = ref.H1(E, V)
        assert W_c == W_r and abs(h_c - h_r) < 1e-9


def test_hpartition_matches_reference():
    rng = random.Random(1)
    for _ in range(50):
        G = nx.stochastic_block_model(
            [7, 7], [[0.6, 0.05], [0.05, 0.6]], seed=rng.randint(0, 10**6)
        )
        E, V = _edges_nodes(G)
        if not E:
            continue
        part = {n: (0 if n < 7 else 1) for n in V}
        assert abs(k.h_partition(E, V, part) - ref.H_partition(E, V, part)) < 1e-9
