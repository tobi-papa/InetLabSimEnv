from premerge_py import kernel as k, estimators as est

def _decomp_at(E, V, part):
    return k.decompose(E, V, part)  # dict HP,H1,Hq,S,benefit,W

def gain_decomposition(EA, VA, EB, VB, S, partition="oracle", restarts=300, seed=0):
    """Gain terms for the merge vs G_A's own H2 optimum (notes 4.1)."""
    EM, VM = k.merge(EA, VA, EB, VB)
    h2_GA, pA_star = k.h2_min(EA, VA, method="agglo", restarts=restarts, seed=seed)
    dA = _decomp_at(EA, VA, pA_star)
    if partition == "oracle":
        h_M, pM = k.h2_min(EM, VM, method="agglo", restarts=restarts, seed=seed)
    elif partition == "standalone":
        pM = est.partition_standalone(EA, VA, EB, VB, S, restarts, seed)
    elif partition == "bridge":
        pM = est.partition_bridge(EA, VA, EB, VB, S, restarts, seed)
    else:
        raise ValueError(partition)
    dM = _decomp_at(EM, VM, pM)
    return {
        "gain": dM["HP"] - dA["HP"],
        "dH1": dM["H1"] - dA["H1"],
        "dHq": dM["Hq"] - dA["Hq"],
        "dS":  dM["S"]  - dA["S"],
        "H2_GA": dA["HP"], "H_M": dM["HP"], "W": dM["W"],
    }
