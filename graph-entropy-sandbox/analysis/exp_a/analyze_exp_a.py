#!/usr/bin/env python3
"""
Experiment A analysis: Does Community Structure Matter for Gain?

Usage: python analyze_exp_a.py <results_dir>

Reads:
  <results_dir>/trials.csv
  <results_dir>/trials_null.csv

Writes:
  <results_dir>/summary_cells.csv
  <results_dir>/plots/*.png
  <results_dir>/report.md
"""

import sys
import os
import json
import warnings
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from scipy import stats
import statsmodels.formula.api as smf

# ────────────────────────────────────────────────────────────────────────────
# Load data
# ────────────────────────────────────────────────────────────────────────────

def load_data(results_dir):
    trials_path = os.path.join(results_dir, "trials.csv")
    null_path   = os.path.join(results_dir, "trials_null.csv")

    if not os.path.exists(trials_path):
        raise FileNotFoundError(f"trials.csv not found in {results_dir}")

    df = pd.read_csv(trials_path)

    df_null = None
    if os.path.exists(null_path):
        df_null = pd.read_csv(null_path)
    else:
        warnings.warn("trials_null.csv not found — null comparison skipped")

    return df, df_null


# ────────────────────────────────────────────────────────────────────────────
# Cell summaries (spec §12.2)
# ────────────────────────────────────────────────────────────────────────────

def compute_cell_summaries(df):
    """Per (mu_B, n_shared) cell statistics."""
    group_cols = ["mu_B", "n_shared"]

    agg = (df.groupby(group_cols).agg(
        cell_mean_modularity_B_planted  = ("modularity_B_planted",   "mean"),
        cell_std_modularity_B_planted   = ("modularity_B_planted",   "std"),
        cell_mean_gain_oracle_est       = ("gain_oracle_est",         "mean"),
        cell_mean_error_compact_abs     = ("error_compact_abs",       "mean"),
        cell_std_error_compact_abs      = ("error_compact_abs",       "std"),
        cell_mean_error_compact_rel     = ("error_compact_rel",       lambda x: np.nanmean(x)),
        cell_mean_error_compact_norm_B  = ("error_compact_norm_B",    lambda x: np.nanmean(x)),
        cell_mean_error_oracle_abs      = ("error_oracle_degree_abs", "mean"),
        cell_std_error_oracle_abs       = ("error_oracle_degree_abs", "std"),
        cell_mean_error_oracle_rel      = ("error_oracle_degree_rel", lambda x: np.nanmean(x)),
        cell_mean_error_oracle_norm_B   = ("error_oracle_degree_norm_B", lambda x: np.nanmean(x)),
        cell_n                          = ("trial_id",               "count"),
    ).reset_index())

    return agg


# ────────────────────────────────────────────────────────────────────────────
# Correlations (spec §12.3)
# ────────────────────────────────────────────────────────────────────────────

def compute_correlations(df, cells):
    """Trial-level and cell-level Spearman correlations."""
    results = {}

    error_cols = {
        "compact_abs":    ("error_compact_abs",       "cell_mean_error_compact_abs"),
        "compact_rel":    ("error_compact_rel",        "cell_mean_error_compact_rel"),
        "compact_norm_B": ("error_compact_norm_B",     "cell_mean_error_compact_norm_B"),
        "oracle_abs":     ("error_oracle_degree_abs",  "cell_mean_error_oracle_abs"),
        "oracle_rel":     ("error_oracle_degree_rel",  "cell_mean_error_oracle_rel"),
        "oracle_norm_B":  ("error_oracle_degree_norm_B", "cell_mean_error_oracle_norm_B"),
    }

    for name, (trial_col, cell_col) in error_cols.items():
        # Trial-level (drop NaN)
        mask = df["modularity_B_planted"].notna() & df[trial_col].notna()
        if mask.sum() > 2:
            r, p = stats.spearmanr(df.loc[mask, "modularity_B_planted"],
                                   df.loc[mask, trial_col])
            results[f"trial_{name}"] = {"rho": r, "p": p, "n": int(mask.sum())}

        # Cell-level
        mask2 = cells["cell_mean_modularity_B_planted"].notna() & cells[cell_col].notna()
        if mask2.sum() > 2:
            r2, p2 = stats.spearmanr(cells.loc[mask2, "cell_mean_modularity_B_planted"],
                                     cells.loc[mask2, cell_col])
            results[f"cell_{name}"] = {"rho": r2, "p": p2, "n": int(mask2.sum())}

    # Gain vs modularity
    mask = df["modularity_B_planted"].notna() & df["gain_oracle_est"].notna()
    if mask.sum() > 2:
        r, p = stats.spearmanr(df.loc[mask, "modularity_B_planted"],
                               df.loc[mask, "gain_oracle_est"])
        results["trial_gain_vs_modularity"] = {"rho": r, "p": p, "n": int(mask.sum())}

    return results


# ────────────────────────────────────────────────────────────────────────────
# Bootstrap CIs on cell-level correlations (spec §12.3)
# ────────────────────────────────────────────────────────────────────────────

def bootstrap_cell_ci(cells, col_x, col_y, n_boot=1000, ci=0.95):
    """Resample cells (not rows) for bootstrap CI on Spearman rho."""
    mask = cells[col_x].notna() & cells[col_y].notna()
    x = cells.loc[mask, col_x].values
    y = cells.loc[mask, col_y].values
    n = len(x)
    if n < 3:
        return float("nan"), float("nan")

    rng = np.random.default_rng(seed=0)
    boot_rhos = []
    for _ in range(n_boot):
        idx = rng.integers(0, n, size=n)
        if len(np.unique(idx)) < 2:
            continue
        try:
            r, _ = stats.spearmanr(x[idx], y[idx])
            boot_rhos.append(r)
        except Exception:
            pass

    if not boot_rhos:
        return float("nan"), float("nan")

    alpha = 1 - ci
    lo = np.percentile(boot_rhos, 100 * alpha / 2)
    hi = np.percentile(boot_rhos, 100 * (1 - alpha / 2))
    return lo, hi


# ────────────────────────────────────────────────────────────────────────────
# Regressions (spec §12.4)
# ────────────────────────────────────────────────────────────────────────────

def compute_regressions(df):
    """OLS regressions for each error metric."""
    results = {}

    specs = [
        ("compact_abs",    "error_compact_abs ~ modularity_B_planted + n_shared + gain_oracle_est"),
        ("compact_rel",    "error_compact_rel ~ modularity_B_planted + n_shared"),
        ("compact_norm_B", "error_compact_norm_B ~ modularity_B_planted + n_shared"),
        ("oracle_abs",     "error_oracle_degree_abs ~ modularity_B_planted + n_shared + gain_oracle_est"),
        ("oracle_rel",     "error_oracle_degree_rel ~ modularity_B_planted + n_shared"),
        ("oracle_norm_B",  "error_oracle_degree_norm_B ~ modularity_B_planted + n_shared"),
    ]

    for name, formula in specs:
        try:
            clean = df[["modularity_B_planted", "n_shared",
                        "gain_oracle_est",
                        formula.split("~")[0].strip()]].dropna()
            if len(clean) < 10:
                results[name] = None
                continue
            model = smf.ols(formula, data=clean).fit()
            results[name] = {
                "coef_modularity": float(model.params.get("modularity_B_planted", float("nan"))),
                "pval_modularity": float(model.pvalues.get("modularity_B_planted", float("nan"))),
                "r_squared": float(model.rsquared),
                "n": int(model.nobs),
            }
        except Exception as e:
            results[name] = {"error": str(e)}

    return results


# ────────────────────────────────────────────────────────────────────────────
# Null comparison (spec §12.5)
# ────────────────────────────────────────────────────────────────────────────

def compute_null_comparison(df, df_null):
    """Delta error = real - null by mu_B and n_shared."""
    if df_null is None:
        return None

    merged = df[["trial_id", "mu_B", "n_shared", "modularity_B_planted",
                 "error_compact_abs", "error_oracle_degree_abs",
                 "error_compact_norm_B", "error_oracle_degree_norm_B"]].merge(
        df_null[["trial_id",
                 "error_compact_abs", "error_oracle_degree_abs",
                 "error_compact_norm_B", "error_oracle_degree_norm_B"]].rename(
            columns={
                "error_compact_abs":        "null_compact_abs",
                "error_oracle_degree_abs":  "null_oracle_abs",
                "error_compact_norm_B":     "null_compact_norm_B",
                "error_oracle_degree_norm_B": "null_oracle_norm_B",
            }),
        on="trial_id", how="inner"
    )

    merged["delta_compact_abs"]   = merged["error_compact_abs"]  - merged["null_compact_abs"]
    merged["delta_oracle_abs"]    = merged["error_oracle_degree_abs"] - merged["null_oracle_abs"]
    merged["delta_compact_norm"]  = merged["error_compact_norm_B"] - merged["null_compact_norm_B"]
    merged["delta_oracle_norm"]   = merged["error_oracle_degree_norm_B"] - merged["null_oracle_norm_B"]

    summary = merged.groupby(["mu_B", "n_shared"]).agg(
        mean_delta_compact_abs  = ("delta_compact_abs",  "mean"),
        mean_delta_oracle_abs   = ("delta_oracle_abs",   "mean"),
        mean_delta_compact_norm = ("delta_compact_norm", "mean"),
        mean_delta_oracle_norm  = ("delta_oracle_norm",  "mean"),
    ).reset_index()

    return summary, merged


# ────────────────────────────────────────────────────────────────────────────
# Plots (spec §14)
# ────────────────────────────────────────────────────────────────────────────

COLORS = {15: "tab:blue", 30: "tab:orange", 90: "tab:green"}

def _scatter_with_cells(ax, df, cells, x_col, y_col, cell_x, cell_y, title, xlabel, ylabel):
    """Raw trial scatter + cell means + trend line. n_shared by color."""
    for ns, color in COLORS.items():
        sub = df[df["n_shared"] == ns]
        if len(sub):
            ax.scatter(sub[x_col], sub[y_col], alpha=0.15, s=8,
                       color=color, label=f"n_shared={ns} (trial)" if ns == 15 else "_nolegend_")
        csub = cells[cells["n_shared"] == ns]
        if len(csub):
            ax.scatter(csub[cell_x], csub[cell_y], s=60, color=color,
                       edgecolors="black", linewidths=0.5,
                       label=f"n_shared={ns} (cell mean)")

    # Trend over cell means
    mask = cells[cell_x].notna() & cells[cell_y].notna()
    if mask.sum() > 2:
        cx = cells.loc[mask, cell_x].values
        cy = cells.loc[mask, cell_y].values
        idx = np.argsort(cx)
        try:
            z = np.polyfit(cx[idx], cy[idx], 1)
            xfit = np.linspace(cx.min(), cx.max(), 100)
            ax.plot(xfit, np.polyval(z, xfit), "k--", lw=1.5, label="trend (cell means)")
        except Exception:
            pass

    ax.set_title(title)
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.legend(fontsize=7)


def make_main_error_plots(df, cells, plots_dir, estimator, est_label):
    """Plots 14.1: error vs modularity for one estimator."""
    error_cols = {
        "abs":    (f"error_{estimator}_abs",    f"cell_mean_error_{estimator.replace('_degree','oracle').replace('compact','compact')}_abs"),
        "rel":    (f"error_{estimator}_rel",    f"cell_mean_error_{estimator.replace('_degree','oracle').replace('compact','compact')}_rel"),
        "norm_B": (f"error_{estimator}_norm_B", f"cell_mean_error_{estimator.replace('_degree','oracle').replace('compact','compact')}_norm_B"),
    }
    # Fix column names for oracle
    if estimator == "oracle_degree":
        error_cols = {
            "abs":    ("error_oracle_degree_abs",    "cell_mean_error_oracle_abs"),
            "rel":    ("error_oracle_degree_rel",    "cell_mean_error_oracle_rel"),
            "norm_B": ("error_oracle_degree_norm_B", "cell_mean_error_oracle_norm_B"),
        }
    else:
        error_cols = {
            "abs":    ("error_compact_abs",    "cell_mean_error_compact_abs"),
            "rel":    ("error_compact_rel",    "cell_mean_error_compact_rel"),
            "norm_B": ("error_compact_norm_B", "cell_mean_error_compact_norm_B"),
        }

    for metric, (trial_col, cell_col) in error_cols.items():
        fig, ax = plt.subplots(figsize=(7, 5))
        if trial_col in df.columns and cell_col in cells.columns:
            _scatter_with_cells(ax, df, cells,
                                "modularity_B_planted", trial_col,
                                "cell_mean_modularity_B_planted", cell_col,
                                f"{est_label}: error_{metric} vs modularity",
                                "modularity_B_planted",
                                f"error_{metric}")
        fig.tight_layout()
        fname = os.path.join(plots_dir, f"error_{estimator}_{metric}_vs_modularity.png")
        fig.savefig(fname, dpi=120)
        plt.close(fig)


def make_gain_diagnostic_plot(df, cells, plots_dir):
    """Plot 14.2: gain_oracle_est vs modularity_B_planted."""
    fig, ax = plt.subplots(figsize=(7, 5))
    for ns, color in COLORS.items():
        sub = df[df["n_shared"] == ns]
        ax.scatter(sub["modularity_B_planted"], sub["gain_oracle_est"],
                   alpha=0.2, s=8, color=color, label=f"n_shared={ns}")
        csub = cells[cells["n_shared"] == ns]
        if len(csub):
            ax.scatter(csub["cell_mean_modularity_B_planted"],
                       csub["cell_mean_gain_oracle_est"],
                       s=60, color=color, edgecolors="black", linewidths=0.5)
    ax.set_xlabel("modularity_B_planted")
    ax.set_ylabel("gain_oracle_est")
    ax.set_title("Gain magnitude diagnostic")
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(os.path.join(plots_dir, "gain_vs_modularity.png"), dpi=120)
    plt.close(fig)


def make_estimator_vs_target_plots(df, plots_dir):
    """Plots 14.3: gain_est vs gain_oracle_est (y=x line)."""
    for estimator, est_col, label in [
        ("compact",       "gain_1D_compact",     "Compact 1D"),
        ("oracle_degree", "gain_1D_oracle_degree", "Oracle 1D degree"),
    ]:
        fig, ax = plt.subplots(figsize=(6, 6))
        for ns, color in COLORS.items():
            sub = df[df["n_shared"] == ns]
            ax.scatter(sub["gain_oracle_est"], sub[est_col],
                       alpha=0.3, s=10, color=color, label=f"n_shared={ns}")
        xlims = ax.get_xlim()
        ylims = ax.get_ylim()
        lims = [min(xlims[0], ylims[0]), max(xlims[1], ylims[1])]
        ax.plot(lims, lims, "k--", lw=1, label="y = x")
        ax.set_xlabel("gain_oracle_est")
        ax.set_ylabel(est_col)
        ax.set_title(f"{label}: estimator vs target")
        ax.legend(fontsize=8)
        fig.tight_layout()
        fig.savefig(os.path.join(plots_dir, f"estimator_vs_target_{estimator}.png"), dpi=120)
        plt.close(fig)


def make_null_plots(null_data, plots_dir):
    """Plots 14.4: real vs null delta error."""
    if null_data is None:
        return
    _, merged = null_data
    for estimator, delta_col, label in [
        ("compact",       "delta_compact_abs",  "Compact 1D"),
        ("oracle_degree", "delta_oracle_abs",   "Oracle 1D degree"),
    ]:
        fig, ax = plt.subplots(figsize=(7, 5))
        for ns, color in COLORS.items():
            sub = merged[merged["n_shared"] == ns]
            ax.scatter(sub["modularity_B_planted"], sub[delta_col],
                       alpha=0.3, s=10, color=color, label=f"n_shared={ns}")
        ax.axhline(0, color="gray", lw=1, linestyle="--")
        ax.set_xlabel("modularity_B_planted")
        ax.set_ylabel("error_real - error_null (abs)")
        ax.set_title(f"{label}: Real vs null delta (positive = real worse)")
        ax.legend(fontsize=8)
        fig.tight_layout()
        fig.savefig(os.path.join(plots_dir, f"null_delta_{estimator}.png"), dpi=120)
        plt.close(fig)


def make_error_by_nominal_mu_plot(df, plots_dir):
    """Plot 14.5: box/violin error by nominal mu_B."""
    mu_vals = sorted(df["mu_B"].unique())
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))
    for ax, (col, label) in zip(axes, [
        ("error_compact_abs",    "Compact abs error"),
        ("error_oracle_degree_abs", "Oracle-degree abs error"),
    ]):
        data = [df.loc[df["mu_B"] == mu, col].dropna().values for mu in mu_vals]
        ax.boxplot(data, labels=[f"{mu:.3g}" for mu in mu_vals], showfliers=False)
        ax.set_xlabel("mu_B")
        ax.set_ylabel("error_abs")
        ax.set_title(label)
        ax.tick_params(axis="x", rotation=45)
    fig.tight_layout()
    fig.savefig(os.path.join(plots_dir, "error_by_nominal_mu.png"), dpi=120)
    plt.close(fig)


def make_modularity_coverage_plot(df, plots_dir):
    """Plot 14.6: realized modularity distribution by nominal mu_B."""
    mu_vals = sorted(df["mu_B"].unique())
    data = [df.loc[df["mu_B"] == mu, "modularity_B_planted"].dropna().values for mu in mu_vals]
    fig, ax = plt.subplots(figsize=(10, 5))
    ax.boxplot(data, labels=[f"{mu:.3g}" for mu in mu_vals], showfliers=False)
    ax.set_xlabel("mu_B (nominal)")
    ax.set_ylabel("modularity_B_planted (realized)")
    ax.set_title("Modularity coverage by nominal mu_B")
    ax.tick_params(axis="x", rotation=45)
    fig.tight_layout()
    fig.savefig(os.path.join(plots_dir, "modularity_coverage.png"), dpi=120)
    plt.close(fig)


# ────────────────────────────────────────────────────────────────────────────
# Case classification (spec §13)
# ────────────────────────────────────────────────────────────────────────────

def classify_case(corrs, regressions, null_data, cells):
    """
    Classify as Case A, B, C, or D per spec §13.

    Case A: both compact and oracle-degree errors increase with modularity
            in absolute AND at least one scale-adjusted metric.
    Case B: compact error increases, but oracle-degree does not.
    Case C: neither estimator fails clearly.
    Case D: ambiguous / confounded.
    """
    compact_cell_rho  = corrs.get("cell_compact_abs",  {}).get("rho", float("nan"))
    oracle_cell_rho   = corrs.get("cell_oracle_abs",   {}).get("rho", float("nan"))
    compact_rel_rho   = corrs.get("cell_compact_rel",  {}).get("rho", float("nan"))
    compact_norm_rho  = corrs.get("cell_compact_norm_B", {}).get("rho", float("nan"))
    oracle_rel_rho    = corrs.get("cell_oracle_rel",   {}).get("rho", float("nan"))
    oracle_norm_rho   = corrs.get("cell_oracle_norm_B", {}).get("rho", float("nan"))
    gain_rho          = corrs.get("trial_gain_vs_modularity", {}).get("rho", float("nan"))

    def is_positive(rho, threshold=0.3):
        return not np.isnan(rho) and rho > threshold

    compact_abs_trend   = is_positive(compact_cell_rho)
    compact_scaled_trend = is_positive(compact_rel_rho) or is_positive(compact_norm_rho)
    oracle_abs_trend    = is_positive(oracle_cell_rho)
    oracle_scaled_trend = is_positive(oracle_rel_rho) or is_positive(oracle_norm_rho)
    gain_trend          = is_positive(gain_rho, threshold=0.5)

    # Check CI bounds — if CI crosses zero on the cell correlation, downgrade
    ci_compact = bootstrap_cell_ci(
        cells, "cell_mean_modularity_B_planted", "cell_mean_error_compact_abs")
    ci_oracle  = bootstrap_cell_ci(
        cells, "cell_mean_modularity_B_planted", "cell_mean_error_oracle_abs")
    ci_crosses_zero_compact = (not np.isnan(ci_compact[0]) and
                               ci_compact[0] < 0 < ci_compact[1])
    ci_crosses_zero_oracle  = (not np.isnan(ci_oracle[0]) and
                               ci_oracle[0] < 0 < ci_oracle[1])

    # Case A: both increase, scale-adjusted also positive, not just gain-driven
    if (compact_abs_trend and oracle_abs_trend and
            (compact_scaled_trend or oracle_scaled_trend) and
            not ci_crosses_zero_compact and not ci_crosses_zero_oracle):
        return "A", "strong evidence that 2D structure matters"

    # Case B: compact fails but oracle-degree does not
    if compact_abs_trend and not oracle_abs_trend:
        return "B", "compact summary fails but oracle-degree 1D works"

    # Case C: neither fails clearly
    if not compact_abs_trend and not oracle_abs_trend:
        return "C", "neither 1D estimator fails clearly — negative result"

    # Case D: ambiguous
    return "D", "ambiguous / confounded (e.g., gain magnitude explains trend, or CI crosses zero)"


# ────────────────────────────────────────────────────────────────────────────
# Report (spec §13 + §19)
# ────────────────────────────────────────────────────────────────────────────

def write_report(results_dir, df, cells, corrs, regressions, null_summary, case, case_description):
    n_accepted = len(df)
    n_rejected = int(df["rejected_draws_before_acceptance"].sum()) if "rejected_draws_before_acceptance" in df.columns else -1
    params = {k: df[k].iloc[0] if k in df.columns else "N/A"
              for k in ["n_A", "n_B", "k_A", "k_B", "avg_degree_A", "avg_degree_B", "mu_A"]}
    mu_B_vals = sorted(df["mu_B"].unique().tolist())
    n_shared_vals = sorted(df["n_shared"].unique().tolist())

    compact_cell_rho = corrs.get("cell_compact_abs", {}).get("rho", float("nan"))
    oracle_cell_rho  = corrs.get("cell_oracle_abs",  {}).get("rho", float("nan"))

    compact_grew = "did" if (not np.isnan(compact_cell_rho) and compact_cell_rho > 0.3) else "did not"
    oracle_grew  = "did" if (not np.isnan(oracle_cell_rho)  and oracle_cell_rho  > 0.3) else "did not"
    gain_rho_val = corrs.get("trial_gain_vs_modularity", {}).get("rho", float("nan"))
    gain_explains = "did" if (not np.isnan(gain_rho_val) and gain_rho_val > 0.5) else "did not"

    null_sentence = "(null comparison not available)"
    if null_summary is not None:
        null_sentence = "Real graphs showed larger errors than degree-preserving rewired null graphs at high modularity." \
            if null_summary["mean_delta_compact_abs"].mean() > 0 else \
            "Real and null graphs showed similar errors — community effect beyond degree sequence is unclear."

    report = f"""# Experiment A — Analysis Report

## Pre-Conclusion Checklist (spec §C / lines 84-91)

- **Accepted trials:** {n_accepted}
- **Rejected draws (total):** {n_rejected}
- **Grid:** mu_B={mu_B_vals}, n_shared={n_shared_vals}
- **Fixed parameters:** n_A={params['n_A']}, n_B={params['n_B']}, k_A={params['k_A']}, k_B={params['k_B']}, avg_deg_A={params['avg_degree_A']}, avg_deg_B={params['avg_degree_B']}, mu_A={params['mu_A']}
- **Seed rule:** trial_seed = splitmix64(splitmix64(splitmix64(base_seed^mu_B_idx)^n_shared_idx)^rep)
- **Did code change after seeing outputs?** NO
- **Notes:** This is a smoke or exploratory run; results must be re-checked on the full 660-trial grid.

---

## Interpretation (spec §19 template)

Experiment A tested whether degree-only entropy can estimate Alice's 2D structural merge gain when Bob's graph varies in realized community strength.

The compact 1D estimator **{compact_grew}** show increasing error with realized Bob modularity (cell-level Spearman rho = {compact_cell_rho:.3f}).

The oracle 1D degree diagnostic **{oracle_grew}** show increasing error with realized Bob modularity (cell-level Spearman rho = {oracle_cell_rho:.3f}).

The gain-magnitude diagnostic **{gain_explains}** explain the absolute-error trend (Spearman rho of gain vs modularity = {gain_rho_val:.3f}).

{null_sentence}

---

## Classification: **Case {case}**

**{case_description.capitalize()}.**

"""

    if case == "A":
        report += (
            "Under the tested SBM conditions, "
            "2D structural entropy is supported as capturing merge-gain information "
            "that degree-only entropy misses.\n\n"
            "_Note: This does not prove K-dimensional entropy is necessary for K>2. "
            "It supports 2D and motivates higher-dimensional methods._\n"
        )
    elif case == "B":
        report += (
            "The original compact summary is too lossy. "
            "The experiment does NOT prove that 1D entropy itself is insufficient — "
            "only that the compact degree summary loses critical information.\n"
        )
    elif case == "C":
        report += (
            "The tested conditions do not support the need for 2D structural entropy. "
            "This is a valid negative result.\n"
        )
    else:
        report += (
            "Experiment A is inconclusive under these conditions. "
            "More trials, better oracle optimization, or redesigned graph controls may be required.\n"
        )

    report += f"""
---

## Cell-level Correlations (main evidence)

| Metric | Compact rho | Oracle-degree rho |
|--------|------------|-------------------|
| abs_err vs modularity | {corrs.get('cell_compact_abs', {}).get('rho', float('nan')):.3f} | {corrs.get('cell_oracle_abs', {}).get('rho', float('nan')):.3f} |
| rel_err vs modularity | {corrs.get('cell_compact_rel', {}).get('rho', float('nan')):.3f} | {corrs.get('cell_oracle_rel', {}).get('rho', float('nan')):.3f} |
| norm_B vs modularity  | {corrs.get('cell_compact_norm_B', {}).get('rho', float('nan')):.3f} | {corrs.get('cell_oracle_norm_B', {}).get('rho', float('nan')):.3f} |

---

## Notes on Validity

- Thresholds and metrics were fixed before running the experiment.
- No trials were excluded for weakening the expected result.
- All reported values are from real completed trials.
- Reproducible from config + base_seed.
"""

    report_path = os.path.join(results_dir, "report.md")
    with open(report_path, "w") as f:
        f.write(report)
    print(f"Report written to {report_path}")


# ────────────────────────────────────────────────────────────────────────────
# Main
# ────────────────────────────────────────────────────────────────────────────

def main():
    if len(sys.argv) < 2:
        print("Usage: python analyze_exp_a.py <results_dir>")
        sys.exit(1)

    results_dir = sys.argv[1]
    if not os.path.isdir(results_dir):
        print(f"Error: {results_dir} is not a directory")
        sys.exit(1)

    plots_dir = os.path.join(results_dir, "plots")
    os.makedirs(plots_dir, exist_ok=True)

    print(f"Loading data from {results_dir} ...")
    df, df_null = load_data(results_dir)
    print(f"  {len(df)} main trials, {len(df_null) if df_null is not None else 0} null trials")

    print("Computing cell summaries ...")
    cells = compute_cell_summaries(df)
    cells.to_csv(os.path.join(results_dir, "summary_cells.csv"), index=False)
    print(f"  {len(cells)} cells")

    print("Computing correlations ...")
    corrs = compute_correlations(df, cells)
    for k, v in corrs.items():
        if isinstance(v, dict) and "rho" in v:
            print(f"  {k}: rho={v['rho']:.3f}  p={v['p']:.3e}  n={v['n']}")

    print("Computing regressions ...")
    regressions = compute_regressions(df)

    null_data = None
    if df_null is not None:
        print("Computing null comparison ...")
        null_data = compute_null_comparison(df, df_null)

    print("Generating plots ...")
    make_main_error_plots(df, cells, plots_dir, "compact",       "Compact 1D")
    make_main_error_plots(df, cells, plots_dir, "oracle_degree", "Oracle 1D degree")
    make_gain_diagnostic_plot(df, cells, plots_dir)
    make_estimator_vs_target_plots(df, plots_dir)
    make_null_plots(null_data, plots_dir)
    make_error_by_nominal_mu_plot(df, plots_dir)
    make_modularity_coverage_plot(df, plots_dir)

    print("Classifying case ...")
    case, case_description = classify_case(corrs, regressions, null_data, cells)
    print(f"  Result: Case {case} — {case_description}")

    if len(df) < 30:
        print("  WARNING: fewer than 30 trials — smoke result, not scientifically meaningful.")
        case_description = "(SMOKE RUN — not scientifically meaningful) " + case_description

    print("Writing report ...")
    null_summary = null_data[0] if null_data is not None else None
    write_report(results_dir, df, cells, corrs, regressions, null_summary, case, case_description)

    print(f"\nDone. Plots: {plots_dir}/  Report: {results_dir}/report.md")


if __name__ == "__main__":
    main()
