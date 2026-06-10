"""E3 - exact reconstruction from the M1-M4 message."""
import argparse
from premerge_py import merge_build as mb, kernel as k, results as R

def _build_PM(inst):
    Sset = set(inst["S"])
    pM = {}
    for n in inst["VA"]: pM[n] = ("A", inst["pA"][n])
    for n in inst["VB"]:
        if n not in Sset: pM[n] = ("B", inst["pB"][n])
    for s in inst["S"]: pM[s] = ("A", inst["pA"][s])
    labels = {lab: i for i, lab in enumerate(sorted(set(pM.values()), key=str))}
    return {n: labels[lab] for n, lab in pM.items()}

def run(n_instances=5, seed=1, sizes_A=(8, 8), sizes_B=(8, 8)):
    rows = []; max_err = 0.0
    for i in range(n_instances):
        inst = mb.build_sbm_merge(list(sizes_A), list(sizes_B), n_anchors=2 + i, seed=seed + i)
        msg = k.build_message(inst["EB"], inst["VB"], inst["S"], inst["pB"])
        recon = k.reconstruct(inst["EA"], inst["VA"], inst["S"], inst["pA"], msg)
        EM, VM = k.merge(inst["EA"], inst["VA"], inst["EB"], inst["VB"])
        pM = _build_PM(inst)
        direct = k.h_partition(EM, VM, pM)
        deg = {}
        for u, v in EM: deg[u] = deg.get(u, 0) + 1; deg[v] = deg.get(v, 0) + 1
        volume_ok = sum(deg.values()) == 2 * len(EM)
        err = abs(recon - direct); max_err = max(max_err, err)
        rows.append({"instance": i, "n_S": len(inst["S"]), "recon": recon, "direct": direct,
                     "abs_error": err, "volume_ok": volume_ok,
                     "n_EB_S": len(msg["M2"]), "n_hist_bins": len(msg["M3"]), "n_B_modules": len(msg["M4"])})
    return {"rows": rows, "max_abs_error": max_err}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n_instances", type=int, default=5); ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--out", default="results"); a = ap.parse_args()
    res = run(a.n_instances, a.seed)
    status = "PASS" if res["max_abs_error"] < 1e-9 else "FAIL"
    run_w = R.RunWriter("E3", base_seed=a.seed, out_root=a.out)
    for r in res["rows"]:
        run_w.add_row({"instance_id": str(r["instance"]), "n_S": r["n_S"],
                       "realized_bias": r["abs_error"], "within_bound": r["volume_ok"],
                       "n_EB_S": r["n_EB_S"], "n_hist_bins": r["n_hist_bins"], "n_B_modules": r["n_B_modules"]})
    run_w.close(config={**vars(a), "max_abs_error": res["max_abs_error"], "status": status})
    print(f"E3 {status}: max|recon-direct|={res['max_abs_error']:.2e} (<1e-9)")

if __name__ == "__main__":
    main()
