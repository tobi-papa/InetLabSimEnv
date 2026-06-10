import csv, json, os, subprocess, hashlib, datetime

SCHEMA_COLUMNS = [
    "experiment_id","instance_id","dataset","params",
    "n_VA","n_VB","n_S","W","sigma",
    "H1_GA","H1_GB","H1_GM","H2_GA","H2_GB",
    "H_standalone","H_bridge","H2_oracle","oracle_method","oracle_restarts",
    "DeltaH1","DeltaHq","DeltaS",
    "gain_standalone","gain_bridge","gain_truth",
    "realized_bias","seam_bound","renorm_cap","within_bound",
    "message_bits","n_EB_S","n_hist_bins","n_B_modules",
    "seed","timestamp","code_version",
]

def _splitmix64(x):
    x = (x + 0x9E3779B97F4A7C15) & 0xFFFFFFFFFFFFFFFF
    x = ((x ^ (x >> 30)) * 0xBF58476D1CE4E5B9) & 0xFFFFFFFFFFFFFFFF
    x = ((x ^ (x >> 27)) * 0x94D049BB133111EB) & 0xFFFFFFFFFFFFFFFF
    return x ^ (x >> 31)

def derive_seed(base_seed, instance_id):
    h = int.from_bytes(hashlib.sha256(str(instance_id).encode()).digest()[:8], "little")
    return _splitmix64(base_seed ^ h)

def _git(args, default="unknown"):
    try: return subprocess.check_output(["git"] + args, text=True).strip()
    except Exception: return default

class RunWriter:
    def __init__(self, experiment_id, base_seed, out_root="results"):
        self.experiment_id = experiment_id; self.base_seed = base_seed
        ts = datetime.datetime.utcnow().strftime("%Y-%m-%dT%H-%M-%SZ")
        self.run_dir = os.path.join(out_root, experiment_id, ts)
        os.makedirs(self.run_dir, exist_ok=True)
        self.rows = []; self.timestamp = ts
        self.code_version = _git(["rev-parse", "HEAD"])
    def add_row(self, row):
        full = {c: row.get(c, "") for c in SCHEMA_COLUMNS}
        full["experiment_id"] = self.experiment_id
        full["timestamp"] = self.timestamp; full["code_version"] = self.code_version
        self.rows.append(full)
    def close(self, config):
        csv_path = os.path.join(self.run_dir, "results.csv")
        with open(csv_path, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=SCHEMA_COLUMNS); w.writeheader(); w.writerows(self.rows)
        cfg = json.dumps(config, sort_keys=True)
        manifest = {
            "experiment_id": self.experiment_id, "base_seed": self.base_seed,
            "git_commit": self.code_version,
            "git_dirty": _git(["status", "--porcelain"]) != "",
            "config": config, "config_sha256": hashlib.sha256(cfg.encode()).hexdigest(),
            "n_rows": len(self.rows), "timestamp": self.timestamp,
        }
        json.dump(manifest, open(os.path.join(self.run_dir, "manifest.json"), "w"), indent=2)
        return self.run_dir
