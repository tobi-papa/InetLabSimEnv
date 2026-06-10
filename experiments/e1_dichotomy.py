"""E1 - degree-preserving partner rewiring (binary witness)."""
import argparse, os
from premerge_py import witnesses as w, gain, results as R

def run(restarts=400, seed=1):
    EB1, VB1, _ = w.partner_v1(); EB2, VB2, _ = w.partner_v2()
    gB  = gain.gain_decomposition(w.GA_EDGES, w.GA_NODES, EB1, VB1, w.S, "oracle", restarts, seed)
    gBp = gain.gain_decomposition(w.GA_EDGES, w.GA_NODES, EB2, VB2, w.S, "oracle", restarts, seed)
    return {"B": gB, "Bp": gBp}

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--restarts", type=int, default=400)
    ap.add_argument("--seed", type=int, default=1); ap.add_argument("--out", default="results")
    a = ap.parse_args()
    res = run(a.restarts, a.seed)
    invH1 = abs(res["B"]["dH1"] - res["Bp"]["dH1"])
    sens  = abs(res["B"]["H_M"] - res["Bp"]["H_M"])
    status = "PASS" if (invH1 < 1e-9 and sens > 0.05) else "FAIL"
    run_w = R.RunWriter("E1", base_seed=a.seed, out_root=a.out)
    for tag, g in (("B", res["B"]), ("Bp", res["Bp"])):
        run_w.add_row({"instance_id": tag, "DeltaH1": g["dH1"], "DeltaHq": g["dHq"],
                       "DeltaS": g["dS"], "H2_oracle": g["H_M"], "gain_truth": g["gain"]})
    run_w.close(config={"restarts": a.restarts, "seed": a.seed,
                        "invariance_gap_H1": invH1, "sensitivity_gap_H2": sens, "status": status})
    from plots import grouped_bars_e1
    grouped_bars_e1(res, os.path.join(run_w.run_dir, "e1_bars.png"))
    print(f"E1 {status}: |dDH1|={invH1:.2e} (<1e-9), |dH2|={sens:.4f} bits (>0.05)")

if __name__ == "__main__":
    main()
