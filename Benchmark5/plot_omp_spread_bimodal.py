"""
B5 Run 5 — OpenMP Spread Bimodal Histogram
20-trial bandwidth distribution for each thread count (8, 16, 32, 64).
"""

import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np

# ── Raw Run 5 data (GB/s) ────────────────────────────────────────────────────
data = {
    8:  [6.9, 38.5, 38.9, 38.7, 18.2, 18.0, 38.2, 37.9, 37.6, 36.9,
         38.0, 38.3, 37.4, 38.2, 38.2, 34.1, 36.4, 31.4, 27.3, 38.3],
    16: [4.6,  9.0, 15.1, 28.1, 56.7, 72.5, 71.7,  8.6,  1.3,  0.6,
          1.3, 10.0, 12.4, 22.8, 54.4, 70.9, 70.1,  6.0,  0.8,  0.9],
    32: [71.8, 85.2, 107.1, 112.5, 111.6,  8.5,  2.0,  1.2,  1.6,  2.2,
          4.3,  2.9,   5.5,   7.7,  18.9, 20.7, 39.1, 61.6, 85.7, 93.0],
    64: [11.5,  6.4,  6.0,  9.9, 11.8,  7.6,  7.2, 13.7, 20.6, 13.6,
         15.9, 21.3, 18.8, 50.3, 22.6, 44.0, 34.6, 90.9, 49.7, 49.3],
}

# ── Cluster boundaries (manually identified from the report + visual) ────────
# Each entry: (low_thresh, high_thresh, low_label, high_label)
clusters = {
    8:  (25,  None, "Partial load\n(≤25 GB/s)",  "Clean\n(>25 GB/s)"),
    16: (35,  None, "Collapsed\n(≤35 GB/s)",      "Clean\n(>35 GB/s)"),
    32: (40,  None, "Collapsed\n(≤40 GB/s)",      "Clean\n(>40 GB/s)"),
    64: (30,  None, "Low\n(≤30 GB/s)",            "High\n(>30 GB/s)"),
}

# colour palette
COL_LOW  = "#e07b54"   # warm orange  — contaminated / partial
COL_HIGH = "#4c97c9"   # steel blue   — clean / high bandwidth

fig, axes = plt.subplots(2, 2, figsize=(11, 8))
fig.suptitle(
    "B5 Run 5 — OpenMP Spread: Bandwidth Distribution (20 trials)\n"
    "Bimodal behaviour by thread count",
    fontsize=14, fontweight="bold", y=0.98
)

thread_counts = [8, 16, 32, 64]

for ax, T in zip(axes.flat, thread_counts):
    vals = np.array(data[T])
    thresh = clusters[T][0]
    low_vals  = vals[vals <= thresh]
    high_vals = vals[vals >  thresh]

    # Adaptive bin width: ~4–5 bins cover the range nicely for 20 points
    full_range = vals.max() - vals.min()
    bin_width  = max(3, full_range / 10)
    bins = np.arange(0, vals.max() + bin_width * 1.5, bin_width)

    # Plot two overlapping histograms in the same bins
    ax.hist(low_vals,  bins=bins, color=COL_LOW,  alpha=0.85, edgecolor="white",
            linewidth=0.6, label=clusters[T][2], zorder=3)
    ax.hist(high_vals, bins=bins, color=COL_HIGH, alpha=0.85, edgecolor="white",
            linewidth=0.6, label=clusters[T][3], zorder=3)

    # Vertical divider at threshold
    ax.axvline(thresh, color="black", linewidth=1.2, linestyle="--", alpha=0.7,
               zorder=4, label=f"Split = {thresh} GB/s")

    # Annotate cluster sizes
    n_low  = len(low_vals)
    n_high = len(high_vals)
    ax.text(0.03, 0.96, f"Low cluster:  {n_low}/20 trials",
            transform=ax.transAxes, fontsize=8.5, va="top",
            color=COL_LOW, fontweight="bold")
    ax.text(0.03, 0.87, f"High cluster: {n_high}/20 trials",
            transform=ax.transAxes, fontsize=8.5, va="top",
            color=COL_HIGH, fontweight="bold")

    # Mean lines per cluster
    if len(low_vals):
        ax.axvline(low_vals.mean(),  color=COL_LOW,  linewidth=1.5,
                   linestyle=":", alpha=0.9, zorder=5)
    if len(high_vals):
        ax.axvline(high_vals.mean(), color=COL_HIGH, linewidth=1.5,
                   linestyle=":", alpha=0.9, zorder=5)

    # Formatting
    ax.set_title(f"{T} Threads  (all 8 NUMA nodes, {T//8} thread{'s' if T//8>1 else ''}/node)",
                 fontsize=10, pad=6)
    ax.set_xlabel("Bandwidth (GB/s)", fontsize=9)
    ax.set_ylabel("Trial count", fontsize=9)
    ax.set_xlim(left=0)
    ax.set_ylim(bottom=0)
    ax.yaxis.set_major_locator(plt.MaxNLocator(integer=True))
    ax.tick_params(labelsize=8.5)
    ax.grid(axis="y", alpha=0.3, zorder=0)
    ax.spines[["top", "right"]].set_visible(False)
    ax.legend(fontsize=7.5, loc="upper center",
              framealpha=0.7, edgecolor="none")

# ── Shared explanation box ────────────────────────────────────────────────────
explanation = (
    "OMP spread always touches all 8 NUMA nodes regardless of thread count.\n"
    "When co-tenant jobs saturate any node's memory controller, that node's\n"
    "contribution collapses — producing the bimodal distribution above.\n"
    "Dotted vertical lines = per-cluster mean bandwidth."
)
fig.text(0.5, 0.01, explanation, ha="center", va="bottom", fontsize=8.2,
         style="italic", color="#444444",
         bbox=dict(boxstyle="round,pad=0.4", facecolor="#f5f5f5",
                   edgecolor="#cccccc", alpha=0.9))

plt.tight_layout(rect=[0, 0.08, 1, 0.96])

out = "/Users/qwuqwuqwu/Documents/NYU/MultiCore/project/openmp_vs_rust/Benchmark5/omp_spread_bimodal.png"
plt.savefig(out, dpi=160, bbox_inches="tight")
print(f"Saved → {out}")
plt.show()
