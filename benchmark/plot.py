#!/usr/bin/env python3
"""Graphs for nre_benchmark's CSV output.

    python3 benchmark/plot.py results.csv                 summary + timelines for one run
    python3 benchmark/plot.py before.csv after.csv ...    also compares each later run with the first

Writes PNGs to --out (default: <first csv name>_plots/ next to it) and prints the numbers behind them.
Needs matplotlib.
"""
import argparse
import csv
import statistics
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

VARIANTS = ["SceneMode", "drawInstance", "drawModel"]
COLORS = {"SceneMode": "#2a78d6", "drawInstance": "#eb6834", "drawModel": "#1baf7a"}
SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
TEXT_SECONDARY = "#52514e"
GRID = "#e6e5e0"

plt.rcParams.update({
    "figure.facecolor": SURFACE,
    "axes.facecolor": SURFACE,
    "axes.edgecolor": GRID,
    "axes.labelcolor": TEXT_SECONDARY,
    "axes.titlecolor": TEXT,
    "axes.titlesize": 11,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "axes.grid": True,
    "grid.color": GRID,
    "grid.linewidth": 0.8,
    "xtick.color": TEXT_SECONDARY,
    "ytick.color": TEXT_SECONDARY,
    "text.color": TEXT,
    "font.size": 9,
    "legend.frameon": False,
})


def load(path):
    """Returns (metadata, runs) where runs[(scenario, variant)] is a list of per-frame dicts."""
    meta = {}
    lines = []
    with open(path, newline="") as f:
        for line in f:
            if line.startswith("#"):
                for item in line[1:].split():
                    key, _, value = item.partition("=")
                    meta[key] = value
            else:
                lines.append(line)
    runs = defaultdict(list)
    for row in csv.DictReader(lines):
        runs[(row["scenario"], row["variant"])].append({
            "frame": int(row["frame"]),
            "instances": int(row["instances"]),
            "update": float(row["update_ms"]),
            "render": float(row["render_ms"]),
            "total": float(row["total_ms"]),
        })
    return meta, runs


def scenarios_of(runs):
    seen = []
    for scenario, _ in runs:
        if scenario not in seen:
            seen.append(scenario)
    return seen


def median(run, key):
    return statistics.median(frame[key] for frame in run) if run else 0.0


def subtitle(meta):
    return f"{meta.get('build', '?')} build, {meta.get('instances', '?')} objects, {meta.get('frames', '?')} frames per run"


def grouped_hbars(ax, scenarios, values_by_variant, label):
    """One group of horizontal bars per scenario, one bar per variant. Scenarios read top to bottom."""
    height = 0.8 / len(VARIANTS)
    for i, variant in enumerate(VARIANTS):
        ys = [s + (i - (len(VARIANTS) - 1) / 2) * height for s in range(len(scenarios))]
        ax.barh(ys, values_by_variant[variant], height=height, color=COLORS[variant],
                edgecolor=SURFACE, linewidth=1, label=variant)
    ax.set_yticks(range(len(scenarios)), scenarios)
    ax.invert_yaxis()
    ax.set_xlabel(label)
    ax.grid(axis="y", visible=False)
    ax.set_axisbelow(True)


def plot_summary(meta, runs, out):
    scenarios = scenarios_of(runs)
    panels = [("total", "Total frame time"), ("update", "Update: object operations"), ("render", "Render: renderFrame() to endFrame()")]
    fig, axes = plt.subplots(1, len(panels), figsize=(14, 0.45 * len(scenarios) * len(VARIANTS) / 2 + 2), sharey=True)
    for ax, (key, title) in zip(axes, panels):
        values = {v: [median(runs.get((s, v), []), key) for s in scenarios] for v in VARIANTS}
        grouped_hbars(ax, scenarios, values, "median ms per frame")
        ax.set_title(title, loc="left")
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper right", ncol=len(VARIANTS))
    fig.suptitle(f"SceneMode vs FunctionDrawMode  ·  {subtitle(meta)}", x=0.01, ha="left", fontsize=12)
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    path = out / "summary.png"
    fig.savefig(path, dpi=150)
    plt.close(fig)
    return path


def rolling_median(values, window=15):
    half = window // 2
    return [statistics.median(values[max(0, i - half):i + half + 1]) for i in range(len(values))]


def plot_timelines(meta, runs, out):
    scenarios = scenarios_of(runs)
    cols = 3
    rows = (len(scenarios) + cols - 1) // cols
    fig, axes = plt.subplots(rows, cols, figsize=(14, 3 * rows), squeeze=False)
    for ax, scenario in zip(axes.flat, scenarios):
        # Where the drawn count changes (grow/shrink), plot against it; otherwise against the frame.
        counts = {f["instances"] for v in VARIANTS for f in runs.get((scenario, v), [])}
        by_count = len(counts) > 2
        for variant in VARIANTS:
            run = runs.get((scenario, variant), [])
            if not run:
                continue
            xs = [f["instances"] if by_count else f["frame"] for f in run]
            ax.plot(xs, rolling_median([f["total"] for f in run]), color=COLORS[variant], linewidth=2, label=variant)
        ax.set_title(scenario, loc="left")
        ax.set_xlabel("objects drawn" if by_count else "frame")
        ax.set_ylabel("total ms (rolling median)")
        ax.set_ylim(bottom=0)
    for ax in list(axes.flat)[len(scenarios):]:
        ax.set_visible(False)
    handles, labels = axes.flat[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper right", ncol=len(VARIANTS))
    fig.suptitle(f"Frame time over each run  ·  {subtitle(meta)}", x=0.01, ha="left", fontsize=12)
    fig.tight_layout(rect=(0, 0, 1, 0.96))
    path = out / "timelines.png"
    fig.savefig(path, dpi=150)
    plt.close(fig)
    return path


def plot_comparison(base_name, base_runs, other_name, other_runs, out):
    scenarios = [s for s in scenarios_of(base_runs) if s in scenarios_of(other_runs)]
    values = {}
    for variant in VARIANTS:
        values[variant] = []
        for s in scenarios:
            before = median(base_runs.get((s, variant), []), "total")
            after = median(other_runs.get((s, variant), []), "total")
            values[variant].append((after / before - 1) * 100 if before else 0.0)
    fig, ax = plt.subplots(figsize=(9, 0.45 * len(scenarios) * len(VARIANTS) / 2 + 2))
    grouped_hbars(ax, scenarios, values, f"% change in median total frame time (negative = {other_name} is faster)")
    ax.axvline(0, color=TEXT_SECONDARY, linewidth=1)
    ax.legend(loc="lower right")
    ax.set_title(f"{other_name} vs {base_name}", loc="left")
    fig.tight_layout()
    path = out / f"compare_{other_name}_vs_{base_name}.png"
    fig.savefig(path, dpi=150)
    plt.close(fig)
    return path, scenarios, values


def print_table(name, runs):
    print(f"\n{name}: median ms per frame (update / render / total)")
    print(f"  {'scenario':<12}" + "".join(f"{v:>26}" for v in VARIANTS))
    for s in scenarios_of(runs):
        cells = []
        for v in VARIANTS:
            run = runs.get((s, v), [])
            cells.append(f"{median(run, 'update'):6.3f} /{median(run, 'render'):6.3f} /{median(run, 'total'):6.3f}")
        print(f"  {s:<12}" + "".join(f"{c:>26}" for c in cells))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("csv", nargs="+", type=Path, help="nre_benchmark CSV files; later ones are compared with the first")
    parser.add_argument("--out", type=Path, help="where to write the PNGs")
    args = parser.parse_args()

    base = args.csv[0]
    out = args.out or base.with_name(base.stem + "_plots")
    out.mkdir(parents=True, exist_ok=True)

    loaded = [(path.stem, *load(path)) for path in args.csv]
    written = []
    for name, meta, runs in loaded:
        print_table(name, runs)
        target = out if len(loaded) == 1 else out / name
        target.mkdir(parents=True, exist_ok=True)
        written += [plot_summary(meta, runs, target), plot_timelines(meta, runs, target)]

    base_name, _, base_runs = loaded[0]
    for name, _, runs in loaded[1:]:
        path, scenarios, values = plot_comparison(base_name, base_runs, name, runs, out)
        written.append(path)
        print(f"\n{name} vs {base_name}: % change in median total frame time (negative = faster)")
        print(f"  {'scenario':<12}" + "".join(f"{v:>14}" for v in VARIANTS))
        for i, s in enumerate(scenarios):
            print(f"  {s:<12}" + "".join(f"{values[v][i]:>+13.1f}%" for v in VARIANTS))

    print("\nWrote:")
    for path in written:
        print(f"  {path}")


if __name__ == "__main__":
    main()
