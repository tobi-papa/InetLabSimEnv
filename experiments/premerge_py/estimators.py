from premerge_py import kernel as k


def _h2_part(E, V, restarts=200, seed=0):
    _, part = k.h2_min(E, V, method="agglo", restarts=restarts, seed=seed)
    return part


def partition_standalone(EA, VA, EB, VB, S, restarts=200, seed=0):
    Sset = set(S)
    pA = _h2_part(EA, VA, restarts, seed)
    pB = _h2_part(EB, VB, restarts, seed)
    part = {}
    for v in VA: part[v] = ("A", pA[v])                 # anchors land on A-side
    for v in VB:
        if v not in Sset: part[v] = ("B", pB[v])        # only B-private
    # relabel to ints for the kernel
    labels = {lab: i for i, lab in enumerate(sorted(set(part.values()), key=str))}
    return {n: labels[lab] for n, lab in part.items()}


def _neighbors_in(EM, S):
    Sset = set(S); nbr = set(S)
    for u, v in EM:
        if u in Sset: nbr.add(v)
        if v in Sset: nbr.add(u)
    return nbr


def partition_bridge(EA, VA, EB, VB, S, restarts=200, seed=0):
    EM, VM = k.merge(EA, VA, EB, VB)
    part = partition_standalone(EA, VA, EB, VB, S, restarts, seed)
    movable = _neighbors_in(EM, S)
    modules = sorted(set(part.values()))
    cur = k.h_partition(EM, VM, part)
    improved = True
    while improved:
        improved = False
        for node in movable:
            best_m, best_h = part[node], cur
            for m in modules:
                if m == part[node]: continue
                trial = dict(part); trial[node] = m
                h = k.h_partition(EM, VM, trial)
                if h < best_h - 1e-12: best_h, best_m = h, m
            if best_m != part[node]:
                part[node] = best_m; cur = best_h; improved = True
    return part


def partition_oracle(EM, VM, method="agglo", restarts=400, seed=0, max_nodes=12):
    h, part = k.h2_min(EM, VM, method=method, restarts=restarts, seed=seed, max_nodes=max_nodes)
    return h, part
