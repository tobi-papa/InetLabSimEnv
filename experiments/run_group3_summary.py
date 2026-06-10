"""Run E5, E6, E-cert and emit the Group-3 PASS/FAIL summary table."""
import e5_bias_vs_sigma as e5, e6_standalone_vs_bridge as e6, ecert_certify_oracle as ec
from premerge_py import summary

def main():
    r5 = e5.run(); r6 = e6.run(); rc = ec.run()
    e5_ok = all(r["within_bound"] for r in r5["rows"])
    e6_ok = all(r["H_standalone"] >= r["H_bridge"] - 1e-9 and r["H_bridge"] >= r["H2_oracle"] - 1e-9 for r in r6["rows"])
    ecert_ok = all(r["gap"] >= -1e-9 for r in rc["rows"])
    rows = [
        {"experiment": "E5", "claim": "bias <= 2*sigma*log2(W) + 1.06", "metric": "within_bound (all)",
         "threshold": "all True", "observed": e5_ok, "status": "PASS" if e5_ok else "FAIL"},
        {"experiment": "E6", "claim": "H_stand >= H_bridge >= oracle", "metric": "ordering (all)",
         "threshold": "all True", "observed": e6_ok, "status": "PASS" if e6_ok else "FAIL"},
        {"experiment": "E-cert", "claim": "gap = heuristic - exact >= 0", "metric": "min gap",
         "threshold": ">= 0", "observed": min(r["gap"] for r in rc["rows"]),
         "status": "PASS" if ecert_ok else "FAIL"},
    ]
    out = summary.write_summary(rows, "results/group3_summary.csv")
    print("wrote", out)
    for r in rows: print(f"  {r['experiment']:7s} {r['status']}  ({r['claim']})")

if __name__ == "__main__":
    main()
