import networkx as nx

def gen_sbm(sizes, p_in, p_out, seed):
    L = len(sizes)
    P = [[p_in if i == j else p_out for j in range(L)] for i in range(L)]
    G = nx.stochastic_block_model(sizes, P, seed=seed)
    planted = {}; base = 0
    for blk, sz in enumerate(sizes):
        for n in range(base, base + sz): planted[n] = blk
        base += sz
    return [(int(u), int(v)) for u, v in G.edges()], [int(n) for n in G.nodes()], planted

def gen_lfr(n, mu, seed, tau1=3, tau2=1.5, average_degree=8, min_community=10):
    G = nx.LFR_benchmark_graph(n, tau1, tau2, mu, average_degree=average_degree,
                               min_community=min_community, seed=seed)
    G.remove_edges_from(nx.selfloop_edges(G))
    planted = {}
    for n_ in G.nodes():
        comm = frozenset(G.nodes[n_]["community"])
        planted[int(n_)] = hash(comm) & 0xffffffff
    return [(int(u), int(v)) for u, v in G.edges()], [int(x) for x in G.nodes()], planted

def degree_preserving_rewire(G, n_swaps, seed):
    H = G.copy()
    nx.double_edge_swap(H, nswap=n_swaps, max_tries=n_swaps * 20, seed=seed)
    return H
