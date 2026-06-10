"""E5 - bias vs bridge density sigma."""
import argparse
from scipy.stats import spearmanr
from premerge_py import sigma_sweep as ss, estimators as est, kernel as k, results as R

def run(anchor_counts=(1, 2, 3, 4, 5), seed=1, restarts=300, sizes_A=(6, 6), sizes_B=(6, 6)):
    pts = ss.sweep(list(sizes_A), list(sizes_B), list(anchor_counts), seed)
    rows = []
    for p in pts:
        p_stand = est.partition_standalone(p["EA"], p["VA"], p["EB"], p["VB"], p["S"], restarts, seed)
        h_stand = k.h_partition(p["EM"], p["VM"], p_stand)
        h_agglo, _ = est.partition_oracle(p["EM"], p["VM"], method="agglo", restarts=restarts * 2, seed=seed)
        h_oracle = min(h_agglo, h_stand)           # tightest feasible upper bound on true H^2
        bias = h_stand - h_oracle
        within = bias <= p["seam_bound"] + p["renorm_cap"] + 1e-9
        rows.append({"sigma": p["sigma"], "W": p["W"], "volS": p["volS"], "n_S": len(p["S"]),
                     "H_standalone": h_stand, "H2_oracle": h_oracle, "realized_bias": bias,
                     "seam_bound": p["seam_bound"], "renorm_cap": p["renorm_cap"], "within_bound": within})
    rho = spearmanr([r["sigma"] for r in rows], [r["realized_bias"] for r in rows]).correlation
    return {"rows": rows, "spearman_bias_sigma": float(rho) if rho == rho else 0.0}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--anchors", type=int, nargs="+", default=[1, 2, 3, 4, 5])
    ap.add_argument("--seed", type=int, default=1); ap.add_argument("--restarts", type=int, default=300)
    ap.add_argument("--out", default="results"); a = ap.parse_args()
    res = run(tuple(a.anchors), a.seed, a.restarts)
    ok = all(r["within_bound"] for r in res["rows"])
    status = "PASS" if ok else "FAIL"
    run_w = R.RunWriter("E5", base_seed=a.seed, out_root=a.out)
    for i, r in enumerate(res["rows"]):
        run_w.add_row({"instance_id": str(i), "n_S": r["n_S"], "W": r["W"], "sigma": r["sigma"],
                       "H_standalone": r["H_standalone"], "H2_oracle": r["H2_oracle"], "oracle_method": "min_feasible",
                       "realized_bias": r["realized_bias"], "seam_bound": r["seam_bound"],
                       "renorm_cap": r["renorm_cap"], "within_bound": r["within_bound"]})
    run_w.close(config={**vars(a), "spearman_bias_sigma": res["spearman_bias_sigma"], "status": status,
                        "oracle": "min over feasible partitions (heuristic upper bound on true H2; bias is a lower estimate)"})
    import os
    from plots import e5_bias_scatter
    e5_bias_scatter(res["rows"], os.path.join(run_w.run_dir, "e5_bias.png"))
    print(f"E5 {status}: within_bound on all={ok}, rho(bias,sigma)={res['spearman_bias_sigma']:.3f}")

if __name__ == "__main__":
    main()
