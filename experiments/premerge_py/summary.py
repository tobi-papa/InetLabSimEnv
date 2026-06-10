import csv, os

def write_summary(rows, out_path):
    # rows: list of dicts with keys experiment, claim, metric, threshold, observed, status
    cols = ["experiment","claim","metric","threshold","observed","status"]
    os.makedirs(os.path.dirname(out_path), exist_ok=True) if os.path.dirname(out_path) else None
    with open(out_path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols); w.writeheader()
        for r in rows: w.writerow({c: r.get(c, "") for c in cols})
    return out_path
