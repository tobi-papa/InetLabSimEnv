"""E6 - standalone vs bridge-aware vs oracle (privacy price in bits)."""
import argparse
from premerge_py import sigma_sweep as ss, estimators as est, kernel as k, results as R

def run(anchor_counts=(1, 2, 3, 4), seed=1, restarts=300, sizes_A=(6, 6), sizes_B=(6, 6)):
    pts = ss.sweep(list(sizes_A), list(sizes_B), list(anchor_counts), seed)
    rows = []
    for p in pts:
        p_stand = est.partition_standalone(p["EA"], p["VA"], p["EB"], p["VB"], p["S"], restarts, seed)
        p_bridge = est.partition_bridge(p["EA"], p["VA"], p["EB"], p["VB"], p["S"], restarts, seed)
        h_stand = k.h_partition(p["EM"], p["VM"], p_stand)
        h_bridge = k.h_partition(p["EM"], p["VM"], p_bridge)
        h_agglo, _ = est.partition_oracle(p["EM"], p["VM"], method="agglo", restarts=restarts * 2, seed=seed)
        h_oracle = min(h_agglo, h_stand, h_bridge)     # tightest feasible upper bound
        rows.append({"sigma": p["sigma"], "W": p["W"], "n_S": len(p["S"]),
                     "H_standalone": h_stand, "H_bridge": h_bridge, "H2_oracle": h_oracle,
                     "bias_standalone": h_stand - h_oracle, "bias_bridge": h_bridge - h_oracle,
                     "seam_cut_bias": h_stand - h_bridge})
    return {"rows": rows}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--anchors", type=int, nargs="+", default=[1, 2, 3, 4])
    ap.add_argument("--seed", type=int, default=1); ap.add_argument("--restarts", type=int, default=300)
    ap.add_argument("--out", default="results"); a = ap.parse_args()
    res = run(tuple(a.anchors), a.seed, a.restarts)
    ordering_ok = all(r["H_standalone"] >= r["H_bridge"] - 1e-9 and r["H_bridge"] >= r["H2_oracle"] - 1e-9
                      for r in res["rows"])
    status = "PASS" if ordering_ok else "FAIL"
    run_w = R.RunWriter("E6", base_seed=a.seed, out_root=a.out)
    for i, r in enumerate(res["rows"]):
        run_w.add_row({"instance_id": str(i), "n_S": r["n_S"], "W": r["W"], "sigma": r["sigma"],
                       "H_standalone": r["H_standalone"], "H_bridge": r["H_bridge"], "H2_oracle": r["H2_oracle"],
                       "realized_bias": r["bias_standalone"]})
    run_w.close(config={**vars(a), "ordering_ok": ordering_ok, "status": status,
                        "oracle": "min over feasible partitions"})
    import os
    from plots import e6_curves
    e6_curves(res["rows"], os.path.join(run_w.run_dir, "e6_curves.png"))
    print(f"E6 {status}: ordering H_stand>=H_bridge>=oracle on all={ordering_ok}")

if __name__ == "__main__":
    main()
