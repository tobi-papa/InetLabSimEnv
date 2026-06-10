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
