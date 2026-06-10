import json, os
from premerge_py import results as R

SCHEMA = R.SCHEMA_COLUMNS

def test_schema_and_manifest(tmp_path):
    run = R.RunWriter("E_test", base_seed=42, out_root=str(tmp_path))
    run.add_row({c: 0 for c in SCHEMA})
    run.close(config={"k": "v"})
    files = os.listdir(run.run_dir)
    assert any(f.endswith(".csv") for f in files) and "manifest.json" in files
    man = json.load(open(os.path.join(run.run_dir, "manifest.json")))
    assert man["base_seed"] == 42 and "git_commit" in man

def test_derived_seed_deterministic():
    assert R.derive_seed(42, "inst-1") == R.derive_seed(42, "inst-1")
    assert R.derive_seed(42, "inst-1") != R.derive_seed(42, "inst-2")
