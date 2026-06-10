"""E-cert - certify the oracle on small instances (de-risks E5/E6)."""
import argparse
from premerge_py import sigma_sweep as ss, estimators as est, kernel as k, results as R

def run(seed=1, max_nodes=12, restarts=300):
    # small sizes so |V_M| <= max_nodes
    pts = ss.sweep([3, 3], [3, 3], anchor_counts=[1, 2], seed=seed)
    rows = []
    for p in pts:
        if len(p["VM"]) > max_nodes:
            continue
        p_stand = est.partition_standalone(p["EA"], p["VA"], p["EB"], p["VB"], p["S"], restarts, seed)
        h_stand = k.h_partition(p["EM"], p["VM"], p_stand)
        h_heur, _ = est.partition_oracle(p["EM"], p["VM"], method="agglo", restarts=restarts, seed=seed)
        h_exact, _ = k.h2_min(p["EM"], p["VM"], method="exact", max_nodes=max_nodes)
        rows.append({"n_VM": len(p["VM"]), "H2_heuristic": h_heur, "H2_exact": h_exact,
                     "gap": h_heur - h_exact, "bias_heuristic": h_stand - h_heur,
                     "bias_exact": h_stand - h_exact})
    return {"rows": rows, "n_instances": len(rows)}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=1); ap.add_argument("--max_nodes", type=int, default=12)
    ap.add_argument("--out", default="results"); a = ap.parse_args()
    res = run(a.seed, a.max_nodes)
    ok = all(r["gap"] >= -1e-9 for r in res["rows"]) and res["n_instances"] >= 1
    status = "PASS" if ok else "FAIL"
    run_w = R.RunWriter("Ecert", base_seed=a.seed, out_root=a.out)
    for i, r in enumerate(res["rows"]):
        run_w.add_row({"instance_id": str(i), "H2_oracle": r["H2_exact"], "oracle_method": "exact",
                       "realized_bias": r["bias_exact"]})
    run_w.close(config={**vars(a), "status": status,
                        "note": "gap = heuristic - exact >= 0 converts lower-estimate bias to true bias"})
    for r in res["rows"]:
        print(f"  |V_M|={r['n_VM']:2d}  H2_heur={r['H2_heuristic']:.6f}  H2_exact={r['H2_exact']:.6f}  gap={r['gap']:.2e}")
    print(f"E-cert {status}: {res['n_instances']} certified instances, all gap>=0={ok}")

if __name__ == "__main__":
    main()
