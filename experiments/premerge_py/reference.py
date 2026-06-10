"""Appendix-B reference oracle — pure-Python, independent of the C++ kernel.

This module is a TEST ORACLE ONLY. It transcribes the structural-entropy
primitives verbatim from Appendix B of the research document (in bits).
Do NOT use for production computation; use premerge_py.kernel instead.
"""

from math import log2


def degrees(edges, nodes):
    d = {v: 0 for v in nodes}
    for (u, v) in edges:
        d[u] += 1
        d[v] += 1
    return d


def H1(edges, nodes):
    d = degrees(edges, nodes)
    W = sum(d.values())
    return (-sum((di / W) * log2(di / W) for di in d.values() if di > 0), W)


def H_partition(edges, nodes, part):
    d = degrees(edges, nodes)
    W = sum(d.values())
    mods = {}
    for v in nodes:
        mods.setdefault(part[v], []).append(v)
    Vj = {j: sum(d[v] for v in mem) for j, mem in mods.items()}
    g = {j: 0 for j in mods}
    eint = {j: 0 for j in mods}
    for (u, v) in edges:
        if part[u] == part[v]:
            eint[part[u]] += 1
        else:
            g[part[u]] += 1
            g[part[v]] += 1
    term1 = -sum(
        (d[v] / W) * log2(d[v] / Vj[j])
        for j, mem in mods.items()
        for v in mem
        if d[v] > 0 and Vj[j] > 0
    )
    term2 = -sum(
        (g[j] / W) * log2(Vj[j] / W) for j in mods if Vj[j] > 0
    )
    return term1 + term2
