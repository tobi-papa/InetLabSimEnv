import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

def grouped_bars_e1(res, out_path):
    fig, ax = plt.subplots(figsize=(5, 4))
    labels = ["dH1", "dS", "H2(G_M)"]
    B  = [res["B"]["dH1"],  res["B"]["dS"],  res["B"]["H_M"]]
    Bp = [res["Bp"]["dH1"], res["Bp"]["dS"], res["Bp"]["H_M"]]
    x = range(len(labels)); width = 0.35
    ax.bar([i - width/2 for i in x], B,  width, label="B (8-cycle)")
    ax.bar([i + width/2 for i in x], Bp, width, label="B' (two 4-cycles)")
    ax.set_xticks(list(x)); ax.set_xticklabels(labels); ax.set_ylabel("bits"); ax.legend()
    ax.set_title("E1: dH1 flat, H2 moves under degree-preserving rewiring")
    fig.tight_layout(); fig.savefig(out_path, dpi=150); plt.close(fig)


def e2_scatter(res, out_path):
    """E2: merge gain vs partner realized community strength (pooled samples),
    with dH1 shown as an invariant flat line."""
    import numpy as np
    fig, ax = plt.subplots(figsize=(5.5, 4))
    x = np.array(res["B_benefit"]); g = np.array(res["gain"])
    ax.scatter(x, g, s=18, alpha=0.6, label="merge gain (per sample)")
    # dH1 is invariant across the sweep -> a single flat reference line
    dH1 = float(np.mean(res["dH1_knob_means"]))
    ax.axhline(dH1, color="C1", ls="--",
               label=f"dH1 = {dH1:.3f} (invariant, Var={res['var_dH1']:.0e})")
    rho = res["spearman_gain_benefit"]
    ax.set_xlabel("partner community strength  B_benefit = H1(G_B) - H2(G_B)  [bits]")
    ax.set_ylabel("bits")
    ax.set_title(f"E2: merge gain tracks partner modularity (rho={rho:.2f}); dH1 blind")
    ax.legend(loc="best", fontsize=8)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)


def e5_bias_scatter(rows, out_path):
    import numpy as np
    s = [r["sigma"] for r in rows]; b = [r["realized_bias"] for r in rows]
    bound = [r["seam_bound"] for r in rows]
    order = np.argsort(s); s = np.array(s)[order]; b = np.array(b)[order]; bound = np.array(bound)[order]
    fig, ax = plt.subplots(figsize=(5, 4))
    ax.scatter(s, b, label="realized bias (lower est.)")
    ax.plot(s, bound, "r--", label="2*sigma*log2(W) bound")
    ax.set_xlabel("sigma = vol(S)/W"); ax.set_ylabel("bits"); ax.legend()
    ax.set_title("E5: standalone bias vs bridge density"); fig.tight_layout()
    fig.savefig(out_path, dpi=150); plt.close(fig)

def e6_curves(rows, out_path):
    import numpy as np
    rows = sorted(rows, key=lambda r: r["sigma"]); s = [r["sigma"] for r in rows]
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(9, 4))
    ax1.plot(s, [r["H_standalone"] for r in rows], "o-", label="standalone")
    ax1.plot(s, [r["H_bridge"] for r in rows], "s-", label="bridge-aware")
    ax1.plot(s, [r["H2_oracle"] for r in rows], "^-", label="oracle (best-found)")
    ax1.set_xlabel("sigma"); ax1.set_ylabel("H^P (bits)"); ax1.legend(); ax1.set_title("E6: three estimators")
    ax2.plot(s, [r["seam_cut_bias"] for r in rows], "d-")
    ax2.set_xlabel("sigma"); ax2.set_ylabel("seam-cut bias (bits)"); ax2.set_title("E6: privacy price")
    fig.tight_layout(); fig.savefig(out_path, dpi=150); plt.close(fig)
