"""E4 - irredundancy witnesses (M1-M4 is tight)."""
import argparse
from premerge_py import kernel as k, results as R, witness_e4 as W

def _hp_merged(ga, EB, partB):
    VB = sorted({n for e in EB for n in e} | set(ga["S"]))
    EM, VM = k.merge(ga["E"], ga["V"], EB, VB)
    pM = {}
    for n in ga["V"]: pM[n] = ("A", ga["part"][n])
    Sset = set(ga["S"])
    for n in VB:
        if n not in Sset: pM[n] = ("B", partB[n])
    labels = {lab: i for i, lab in enumerate(sorted(set(pM.values()), key=str))}
    return k.h_partition(EM, VM, {n: labels[lab] for n, lab in pM.items()})

def run():
    rows = []
    for (target, ga_key, EB1, EB2, partB1, partB2, exp1, exp2) in W.WITNESSES:
        ga = W.GA6 if ga_key == "GA6" else W.GA
        h1 = _hp_merged(ga, EB1, partB1); h2 = _hp_merged(ga, EB2, partB2)
        rows.append({"target": target, "HP_v1": h1, "HP_v2": h2,
                     "differ": abs(h1 - h2) > 1e-6, "exp_v1": exp1, "exp_v2": exp2,
                     "match": abs(h1 - exp1) < 1e-5 and abs(h2 - exp2) < 1e-5})
    return rows

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--out", default="results"); a = ap.parse_args()
    rows = run()
    status = "PASS" if all(r["differ"] and r["match"] for r in rows) else "FAIL"
    run_w = R.RunWriter("E4", base_seed=0, out_root=a.out)
    for r in rows:
        run_w.add_row({"instance_id": r["target"], "H_standalone": r["HP_v1"], "H_bridge": r["HP_v2"]})
    run_w.close(config={"status": status, "rows": rows})
    for r in rows:
        print(f"  {r['target']:7s} {r['HP_v1']:.6f} vs {r['HP_v2']:.6f}  differ={r['differ']} match={r['match']}")
    print(f"E4 {status}")

if __name__ == "__main__":
    main()
