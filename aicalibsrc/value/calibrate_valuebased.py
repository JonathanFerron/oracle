#!/usr/bin/env python3
"""Calibration driver for the A1 Value Based AI agent's two tunable
parameters (VB_COST_FLOOR, VB_DEFEND_THRESHOLD).

Drives bin/calib_valuebased, which links the game engine directly and
prints one CSV result line per invocation -- see that file's header and
src/ai_strat/ai_strat_valuebased.c for what the parameters mean. All the
actual game simulation happens in the compiled C binary; this script only
orchestrates subprocess calls and aggregates/fits the results.

Two subcommands:

  sweep     Univariate diagnostic vs Random (value_based_handout.md Sec 11's
            parameter sweeps): hold one parameter at its default and vary
            the other, both seats, with binomial confidence intervals. Use
            this first to sanity-check that a parameter actually moves the
            needle -- a flat curve means it isn't wired in.

  selfplay  Round-robin: every (cost_floor, defend_threshold) combo in a
            grid plays every other combo, both seat orders,
            --ai.a=value vs --ai.b=value. Reports pairwise results plus a
            Bradley-Terry fit (relative strength per combo) across the
            whole grid. More discriminating than the vs-Random sweep once
            vs-Random win rates saturate near a ceiling (A1 beat Random
            ~90% at default parameters -- see doc/changelog.md, 2026-08-21
            -- likely too high to resolve nearby parameter values; a mirror
            match has no such ceiling).

Examples:
  ./calibrate_valuebased.py sweep --param defend_threshold --numsim 2500 --replicates 4
  ./calibrate_valuebased.py selfplay --cost-floors 0.5 1.0 2.0 \\
      --defend-thresholds 0.25 0.5 1.0 2.0 --numsim 2500 --replicates 4
"""

import argparse
import json
import math
import subprocess
import sys
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

import numpy as np
import pandas as pd
from scipy.optimize import minimize

BINARY = Path(__file__).resolve().parent.parent.parent / "bin" / "calib_valuebased"
RESULTS_DIR = Path(__file__).resolve().parent / "results"


def _load_defaults_from_binary():
    if not BINARY.exists():
        # Fall back to the compiled-in values in ai_strat_valuebased.c's
        # VB_COST_FLOOR/VB_DEFEND_THRESHOLD, so --help and argument parsing
        # still work before `make calib_valuebased` has run. Any real
        # command still hits run_match()'s own existence check.
        return {"cost_floor": 1.3, "defend_threshold": 0.8}
    result = subprocess.run([str(BINARY), "--print-defaults"],
                            capture_output=True, text=True, check=True)
    return json.loads(result.stdout)


# Read from the compiled binary (added 2026-08-28) rather than hardcoded
# module constants, which had drifted from the shipped C values (1.0/0.5
# here vs the actual shipped 1.3/0.8) -- see doc/oracle_todo.md's
# Calibration section.
_DEFAULTS = _load_defaults_from_binary()
DEFAULT_COST_FLOOR = _DEFAULTS["cost_floor"]
DEFAULT_DEFEND_THRESHOLD = _DEFAULTS["defend_threshold"]

SWEEP_DEFAULTS = {
    "defend_threshold": [0.0, 0.25, 0.5, 1.0, 2.0],
    "cost_floor": [0.5, 1.0, 2.0, 5.0],
}


def run_match(numsim, seed, agent_a, agent_b,
              cost_floor_a, defend_threshold_a, cost_floor_b, defend_threshold_b, **tags):
    """One call to bin/calib_valuebased. Runs in a worker process.

    **tags are caller-supplied bookkeeping (e.g. _i/_j) merged straight into
    the returned dict, so every result is self-describing -- added
    2026-08-28 (ported from aicalibsrc/balanced/calibrate_balanced.py) to
    fix a result-misattribution bug: selfplay used to track this bookkeeping
    in a separate submission-order list and reattach it after run_many()'s
    ProcessPoolExecutor returned results in *completion* order, silently
    scrambling which result belonged to which config under concurrency (see
    doc/oracle_todo.md's Calibration section)."""
    if not BINARY.exists():
        raise FileNotFoundError(f"{BINARY} not found -- run `make calib_valuebased` first")

    args = [str(BINARY), str(numsim), str(seed), agent_a, agent_b,
            str(cost_floor_a), str(defend_threshold_a),
            str(cost_floor_b), str(defend_threshold_b)]
    result = subprocess.run(args, capture_output=True, text=True, check=True)
    row = result.stdout.strip().split(",")
    return {
        "numsim": int(row[0]), "seed": int(row[1]),
        "agent_a": row[2], "agent_b": row[3],
        "cost_floor_a": float(row[4]), "defend_threshold_a": float(row[5]),
        "cost_floor_b": float(row[6]), "defend_threshold_b": float(row[7]),
        "wins_a": int(row[8]), "wins_b": int(row[9]), "draws": int(row[10]),
        **tags,
    }


def run_many(jobs, max_workers=None):
    """jobs: list of kwargs dicts for run_match(). Returns a DataFrame, one row per job."""
    results = []
    with ProcessPoolExecutor(max_workers=max_workers) as pool:
        futures = [pool.submit(run_match, **job) for job in jobs]
        for i, fut in enumerate(as_completed(futures), 1):
            results.append(fut.result())
            print(f"\r  {i}/{len(jobs)} matches done", end="", file=sys.stderr, flush=True)
    print(file=sys.stderr)
    return pd.DataFrame(results)


def wilson_ci(wins, n, z=1.96):
    """95% Wilson score interval for a binomial proportion -- more reliable
    than the normal approximation at the extreme win rates this tool sees
    (e.g. ~90% vs Random)."""
    if n == 0:
        return (float("nan"), float("nan"))
    p = wins / n
    denom = 1 + z**2 / n
    center = (p + z**2 / (2 * n)) / denom
    half = (z * math.sqrt(p * (1 - p) / n + z**2 / (4 * n**2))) / denom
    return (center - half, center + half)


def replicate_seeds(base_seed, replicates):
    return [base_seed + i for i in range(replicates)]


# ---------------------------------------------------------------------------
# sweep: value (one param varied) vs rand (defaults), both seats
# ---------------------------------------------------------------------------

def build_sweep_jobs(param, values, numsim, seeds):
    jobs = []
    for v in values:
        cost_floor = v if param == "cost_floor" else DEFAULT_COST_FLOOR
        defend_threshold = v if param == "defend_threshold" else DEFAULT_DEFEND_THRESHOLD
        for seed in seeds:
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="value", agent_b="rand",
                             cost_floor_a=cost_floor, defend_threshold_a=defend_threshold,
                             cost_floor_b=DEFAULT_COST_FLOOR,
                             defend_threshold_b=DEFAULT_DEFEND_THRESHOLD))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="rand", agent_b="value",
                             cost_floor_a=DEFAULT_COST_FLOOR,
                             defend_threshold_a=DEFAULT_DEFEND_THRESHOLD,
                             cost_floor_b=cost_floor, defend_threshold_b=defend_threshold))
    return jobs


def summarize_sweep(df, param):
    df = df.copy()
    value_in_a = df["agent_a"] == "value"
    value_col = "defend_threshold_a" if param == "defend_threshold" else "cost_floor_a"
    other_col = "defend_threshold_b" if param == "defend_threshold" else "cost_floor_b"
    df["value"] = np.where(value_in_a, df[value_col], df[other_col])
    df["value_wins"] = np.where(value_in_a, df["wins_a"], df["wins_b"])
    df["n"] = df["wins_a"] + df["wins_b"] + df["draws"]

    grouped = df.groupby("value").agg(wins=("value_wins", "sum"), n=("n", "sum")).reset_index()
    grouped["win_rate"] = grouped["wins"] / grouped["n"]
    grouped[["ci_lo", "ci_hi"]] = grouped.apply(
        lambda r: pd.Series(wilson_ci(r["wins"], r["n"])), axis=1)
    return grouped.sort_values("value")


def cmd_sweep(args):
    values = args.values or SWEEP_DEFAULTS[args.param]
    seeds = replicate_seeds(args.base_seed, args.replicates)
    jobs = build_sweep_jobs(args.param, values, args.numsim, seeds)

    print(f"Running {len(jobs)} matches "
         f"({len(values)} values x {args.replicates} replicates x 2 seats)...",
         file=sys.stderr)
    df = run_many(jobs, max_workers=args.workers)

    summary = summarize_sweep(df, args.param)
    print(f"\nSweep of {args.param} (value vs rand, both seats, "
         f"{args.numsim * args.replicates} games/value):")
    print(summary.to_string(index=False, float_format=lambda x: f"{x:.4f}"))

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    out_path = RESULTS_DIR / f"sweep_{args.param}.csv"
    summary.to_csv(out_path, index=False)
    df.to_csv(RESULTS_DIR / f"sweep_{args.param}_raw.csv", index=False)
    print(f"\nSaved: {out_path}")

    if args.plot:
        plot_sweep(summary, args.param, RESULTS_DIR / f"sweep_{args.param}.png")


def plot_sweep(summary, param, out_path):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots()
    yerr = [summary["win_rate"] - summary["ci_lo"], summary["ci_hi"] - summary["win_rate"]]
    ax.errorbar(summary["value"], summary["win_rate"], yerr=yerr, marker="o", capsize=4)
    ax.axhline(0.5, color="gray", linestyle="--", linewidth=1)
    ax.set_xlabel(param)
    ax.set_ylabel("win rate vs Random")
    ax.set_title(f"Value Based: {param} sweep (95% Wilson CI)")
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    print(f"Saved: {out_path}")


def vs_random_win_rate(cost_floor, defend_threshold, numsim, seeds, workers):
    """Aggregate win rate for (cost_floor, defend_threshold) vs Random, both
    seats, across `seeds` replicates. Returns (wins, n, rate, ci_lo, ci_hi)."""
    jobs = []
    for seed in seeds:
        jobs.append(dict(numsim=numsim, seed=seed, agent_a="value", agent_b="rand",
                         cost_floor_a=cost_floor, defend_threshold_a=defend_threshold,
                         cost_floor_b=DEFAULT_COST_FLOOR,
                         defend_threshold_b=DEFAULT_DEFEND_THRESHOLD))
        jobs.append(dict(numsim=numsim, seed=seed, agent_a="rand", agent_b="value",
                         cost_floor_a=DEFAULT_COST_FLOOR,
                         defend_threshold_a=DEFAULT_DEFEND_THRESHOLD,
                         cost_floor_b=cost_floor, defend_threshold_b=defend_threshold))
    df = run_many(jobs, max_workers=workers)
    value_in_a = df["agent_a"] == "value"
    wins = int(np.where(value_in_a, df["wins_a"], df["wins_b"]).sum())
    n = int((df["wins_a"] + df["wins_b"] + df["draws"]).sum())
    lo, hi = wilson_ci(wins, n)
    return wins, n, wins / n, lo, hi


def cmd_validate(args):
    """Compare default params vs a given params vs Random, both seats --
    the direct answer to 'how much did this optimization actually improve
    the win rate against Random'."""
    seeds = replicate_seeds(args.base_seed, args.replicates)
    print(f"Validating vs Random ({len(seeds) * 2} matches per config)...", file=sys.stderr)

    d_wins, d_n, d_rate, d_lo, d_hi = vs_random_win_rate(
        DEFAULT_COST_FLOOR, DEFAULT_DEFEND_THRESHOLD, args.numsim, seeds, args.workers)
    c_wins, c_n, c_rate, c_lo, c_hi = vs_random_win_rate(
        args.cost_floor, args.defend_threshold, args.numsim, seeds, args.workers)

    print(f"\nWin rate vs Random ({d_n} games each):")
    print(f"  default (cost_floor={DEFAULT_COST_FLOOR}, "
         f"defend_threshold={DEFAULT_DEFEND_THRESHOLD}): "
         f"{d_rate:.4f} [{d_lo:.4f}, {d_hi:.4f}]")
    print(f"  candidate (cost_floor={args.cost_floor:g}, "
         f"defend_threshold={args.defend_threshold:g}): "
         f"{c_rate:.4f} [{c_lo:.4f}, {c_hi:.4f}]")
    print(f"  delta: {(c_rate - d_rate) * 100:+.2f} percentage points")


# ---------------------------------------------------------------------------
# selfplay: round-robin over a param grid, Bradley-Terry fit
# ---------------------------------------------------------------------------

def build_selfplay_jobs(combos, numsim, seeds, include_mirror):
    jobs = []
    pairs = [(i, j) for i in range(len(combos)) for j in range(len(combos))
             if i < j or (include_mirror and i == j)]
    for i, j in pairs:
        cf_i, dt_i = combos[i]
        cf_j, dt_j = combos[j]
        for seed in seeds:
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="value", agent_b="value",
                             cost_floor_a=cf_i, defend_threshold_a=dt_i,
                             cost_floor_b=cf_j, defend_threshold_b=dt_j,
                             _i=i, _j=j))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="value", agent_b="value",
                             cost_floor_a=cf_j, defend_threshold_a=dt_j,
                             cost_floor_b=cf_i, defend_threshold_b=dt_i,
                             _i=j, _j=i))
    return jobs


def bradley_terry_fit(combos, wins_matrix, games_matrix):
    """MLE fit of a Bradley-Terry model: P(i beats j) = 1/(1+exp(-(r_i-r_j))).
    r[0] is anchored to 0 for identifiability; the rest are free. Returns the
    full r array (length len(combos))."""
    n = len(combos)

    def neg_log_likelihood(free_r):
        r = np.concatenate(([0.0], free_r))
        ll = 0.0
        for i in range(n):
            for j in range(n):
                if games_matrix[i, j] == 0:
                    continue
                p = 1.0 / (1.0 + np.exp(-(r[i] - r[j])))
                p = min(max(p, 1e-9), 1 - 1e-9)
                ll += wins_matrix[i, j] * np.log(p)
        return -ll

    x0 = np.zeros(n - 1)
    result = minimize(neg_log_likelihood, x0, method="BFGS")
    return np.concatenate(([0.0], result.x))


def summarize_selfplay(df, combos):
    n = len(combos)
    wins = np.zeros((n, n))
    games = np.zeros((n, n))
    # Per-candidate totals, tracked directly from wins_a/wins_b -- NOT
    # rederived from the wins/games matrices via a transpose trick (fixed
    # 2026-08-28, ported from aicalibsrc/combo/calibrate_combo_threshold.py).
    # wins[i,j] only ever holds "i's wins when seated as A against j as B",
    # so wins.T[i,:].sum() sums the *opponents'* A-seat wins in games where i
    # was B, not i's own wins -- every match report carries both wins_a and
    # wins_b, so just credit each side directly instead.
    total_wins = np.zeros(n)
    total_games = np.zeros(n)
    for _, row in df.iterrows():
        i, j = int(row["_i"]), int(row["_j"])
        n_games = row["wins_a"] + row["wins_b"] + row["draws"]
        wins[i, j] += row["wins_a"]
        games[i, j] += n_games
        total_wins[i] += row["wins_a"]
        total_wins[j] += row["wins_b"]
        total_games[i] += n_games
        total_games[j] += n_games

    strength = bradley_terry_fit(combos, wins, games)

    summary = pd.DataFrame({
        "cost_floor": [c[0] for c in combos],
        "defend_threshold": [c[1] for c in combos],
        "bt_strength": strength,
        "overall_win_rate": np.divide(total_wins, total_games,
                                      out=np.zeros(n), where=total_games > 0),
        "games_played": total_games.astype(int),
    })
    return summary.sort_values("bt_strength", ascending=False)


def cmd_selfplay(args):
    combos = [(cf, dt) for cf in args.cost_floors for dt in args.defend_thresholds]
    seeds = replicate_seeds(args.base_seed, args.replicates)
    jobs = build_selfplay_jobs(combos, args.numsim, seeds, args.include_mirror)

    n_pairs = len(jobs) // (2 * args.replicates)
    print(f"Running {len(jobs)} matches "
         f"({n_pairs} pairs x {args.replicates} replicates x 2 seats) "
         f"over {len(combos)} parameter combos...", file=sys.stderr)

    df = run_many(jobs, max_workers=args.workers)

    summary = summarize_selfplay(df, combos)
    print("\nSelf-play round-robin (Bradley-Terry fit, higher = stronger, "
         "combo with the most negative/first strength is the anchor at 0):")
    print(summary.to_string(index=False, float_format=lambda x: f"{x:.4f}"))
    print(f"\nBest combo: cost_floor={summary.iloc[0]['cost_floor']}, "
         f"defend_threshold={summary.iloc[0]['defend_threshold']}")

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    out_path = RESULTS_DIR / "selfplay_grid.csv"
    summary.to_csv(out_path, index=False)
    df.to_csv(RESULTS_DIR / "selfplay_grid_raw.csv", index=False)
    print(f"\nSaved: {out_path}")

    if args.plot:
        plot_selfplay(summary, RESULTS_DIR / "selfplay_grid.png")

    if not args.skip_validate:
        best_cf = float(summary.iloc[0]["cost_floor"])
        best_dt = float(summary.iloc[0]["defend_threshold"])
        print(f"\nValidating the self-play winner against Random "
             f"(vs the compiled defaults)...", file=sys.stderr)
        val_seeds = replicate_seeds(args.base_seed + 100000, args.replicates)
        d_wins, d_n, d_rate, d_lo, d_hi = vs_random_win_rate(
            DEFAULT_COST_FLOOR, DEFAULT_DEFEND_THRESHOLD, args.numsim, val_seeds, args.workers)
        c_wins, c_n, c_rate, c_lo, c_hi = vs_random_win_rate(
            best_cf, best_dt, args.numsim, val_seeds, args.workers)
        print(f"\nWin rate vs Random ({d_n} games each):")
        print(f"  default   (cost_floor={DEFAULT_COST_FLOOR}, "
             f"defend_threshold={DEFAULT_DEFEND_THRESHOLD}): "
             f"{d_rate:.4f} [{d_lo:.4f}, {d_hi:.4f}]")
        print(f"  self-play winner (cost_floor={best_cf:g}, "
             f"defend_threshold={best_dt:g}): "
             f"{c_rate:.4f} [{c_lo:.4f}, {c_hi:.4f}]")
        print(f"  delta: {(c_rate - d_rate) * 100:+.2f} percentage points")


def plot_selfplay(summary, out_path):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    pivot = summary.pivot(index="defend_threshold", columns="cost_floor", values="bt_strength")
    fig, ax = plt.subplots()
    im = ax.imshow(pivot.values, aspect="auto", origin="lower", cmap="viridis")
    ax.set_xticks(range(len(pivot.columns)), labels=[f"{c:g}" for c in pivot.columns])
    ax.set_yticks(range(len(pivot.index)), labels=[f"{r:g}" for r in pivot.index])
    ax.set_xlabel("cost_floor")
    ax.set_ylabel("defend_threshold")
    ax.set_title("Bradley-Terry strength (self-play round-robin)")
    fig.colorbar(im, ax=ax, label="strength")
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    print(f"Saved: {out_path}")


# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    p_sweep = sub.add_parser("sweep", help="univariate sweep vs Random")
    p_sweep.add_argument("--param", choices=["cost_floor", "defend_threshold"], required=True)
    p_sweep.add_argument("--values", type=float, nargs="+",
                         help="default: the handout's suggested sweep for this param")
    p_sweep.add_argument("--numsim", type=int, default=2500)
    p_sweep.add_argument("--replicates", type=int, default=4)
    p_sweep.add_argument("--base-seed", type=int, default=1337)
    p_sweep.add_argument("--workers", type=int, default=None)
    p_sweep.add_argument("--plot", action="store_true")
    p_sweep.set_defaults(func=cmd_sweep)

    p_self = sub.add_parser("selfplay", help="round-robin over a param grid, Bradley-Terry fit")
    p_self.add_argument("--cost-floors", type=float, nargs="+", default=[0.5, 1.0, 2.0])
    p_self.add_argument("--defend-thresholds", type=float, nargs="+",
                        default=[0.25, 0.5, 1.0, 2.0])
    p_self.add_argument("--numsim", type=int, default=2500)
    p_self.add_argument("--replicates", type=int, default=4)
    p_self.add_argument("--base-seed", type=int, default=1337)
    p_self.add_argument("--workers", type=int, default=None)
    p_self.add_argument("--include-mirror", action="store_true",
                        help="also play each combo against an identical copy of itself "
                             "(measures seat/mulligan noise floor, not needed for the BT fit)")
    p_self.add_argument("--plot", action="store_true")
    p_self.add_argument("--skip-validate", action="store_true",
                        help="skip the automatic best-vs-default-vs-Random comparison at the end")
    p_self.set_defaults(func=cmd_selfplay)

    p_val = sub.add_parser("validate",
                           help="compare a candidate param set vs Random against the defaults")
    p_val.add_argument("--cost-floor", type=float, required=True)
    p_val.add_argument("--defend-threshold", type=float, required=True)
    p_val.add_argument("--numsim", type=int, default=2500)
    p_val.add_argument("--replicates", type=int, default=4)
    p_val.add_argument("--base-seed", type=int, default=1337)
    p_val.add_argument("--workers", type=int, default=None)
    p_val.set_defaults(func=cmd_validate)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
