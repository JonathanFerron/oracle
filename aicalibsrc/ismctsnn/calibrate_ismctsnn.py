#!/usr/bin/env python3
"""Calibration driver for A11 IS-MCTS+NN ("AlphaOracle Prime")'s
nn_value_trust dial -- see src/ai_strat/ai_strat_ismcts1.h and
doc/ai_agents.md's A11 section's "Confirmed
plan" step 3 (two-gate measurement).

Drives bin/calib_ismctsnn, which links the game engine directly and prints
one CSV result line per invocation. All the actual game simulation happens
in the compiled C binary; this script only orchestrates subprocess calls
and aggregates/fits the results. Structure ported directly from
aicalibsrc/carto/calibrate_a13.py (wilson_ci, bradley_terry_fit, the
sweep/validate/selfplay subcommand shape, run_match's tags-at-the-source
fix) -- about.md names A13's calibration record as the direct precedent for
how this exact kind of trust-dial gets measured (A9's reply_trust, A13's
hplus_trust both had a "monotonic decline as trust rises" failure signature
to watch for).

DEFAULTS is read once, at import time, from `bin/calib_ismctsnn
--print-defaults` (ISMCTS_DEFAULTS with nn_value_trust=1.0 -- see
ai_strat_ismctsnn.h), so it can never drift from the shipped C constants.

Unlike A13's driver, there is no `optimize` (differential-evolution)
subcommand here: nn_value_trust is the only real free dial (PARAM_NAMES
carries the full 20-field ISMCTSParams shape because the C harness needs it
positionally, but FREE_PARAM_NAMES is just this one field) -- for a single
continuous dial, `sweep` already finds the best point on the response curve
directly; a black-box search over one scalar would just be `sweep` with
extra steps.

A weights file (aicalibsrc/ismctsnn/export_weights.py's output) is REQUIRED
for every subcommand via --weights, even for an ismcts-only sanity check --
the C harness always loads it (calib_ismctsnn.c's own doc comment).

Two gates (about.md): `validate` (candidate trust vs ismcts, both seats,
Wilson CI lower bound > 50%) is Gate 2, the real bar -- and its baseline
side (trust=0.0 vs ismcts) is a live re-check of the superset guarantee on
every run, the same "Stage 0" sanity role A13's own `validate --candidate
defaults` played. `selfplay` here is narrower than A13's: round-robin among
named TRUST CANDIDATES only (useful for picking the best point with more
statistical power), not a reproduction of the whole-roster Bradley-Terry
rating -- Gate 1 ("roster rating, context only") means running the
EXISTING --stda.rating full-roster benchmark once a trust value is chosen,
not duplicating that here.

Subcommands:
  sweep     Univariate diagnostic: nn_value_trust varied, ismctsnn vs a
            fixed --opponent (default "ismcts" -- A10 itself, the real
            gate), both seats, with Wilson confidence intervals. Watch for
            A9/A13's monotonic-decline signature.
  validate  Candidate trust value vs the shipped default (nn_value_trust=0,
            i.e. pure A10) AND vs --opponent (default "ismcts"), both
            seats -- this is Gate 2.
  selfplay  Round-robin among a small set of named trust candidates,
            ismctsnn vs ismctsnn. Reports a Bradley-Terry fit among them.

Examples:
  ./calibrate_ismctsnn.py sweep --weights checkpoints/pilot_c_weights.bin \\
      --numsim 500 --replicates 4 --plot
  ./calibrate_ismctsnn.py validate --weights checkpoints/pilot_c_weights.bin \\
      --trust 1.0 --numsim 2000 --replicates 4
  ./calibrate_ismctsnn.py selfplay --weights checkpoints/pilot_c_weights.bin \\
      --trusts 0.0 0.25 0.5 0.75 1.0 --numsim 1000 --replicates 2
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

BINARY = Path(__file__).resolve().parent.parent.parent / "bin" / "calib_ismctsnn"
RESULTS_DIR = Path(__file__).resolve().parent / "results"

# Declared struct order in ISMCTSParams (ai_strat_ismcts1.h) -- must match
# parse_params()/print_params_csv() in calib_ismctsnn.c.
PARAM_NAMES = [
    "limit_iterations", "limit_playout_steps", "limit_max_nodes",
    "limit_recall_variants", "limit_cash_variants", "limit_max_candidates",
    "search_exploration_constant", "search_use_availability", "search_expand_threshold",
    "threshold_widening_k", "threshold_widening_alpha", "prior_use_heuristic",
    "rollout_max_turns", "rollout_cutoff_depth",
    "weight_energy_advantage", "weight_cash_advantage", "weight_hand_advantage",
    "limit_flat_iterations", "limit_flat_candidates",
    "nn_value_trust",
]

_INT_PARAMS = {
    "limit_iterations", "limit_playout_steps", "limit_max_nodes",
    "limit_recall_variants", "limit_cash_variants", "limit_max_candidates",
    "search_use_availability",  # bool -> "0"/"1", matching calib_ismctsnn.c's strtol(...) != 0
    "search_expand_threshold", "prior_use_heuristic",
    "rollout_max_turns", "rollout_cutoff_depth",
    "limit_flat_iterations", "limit_flat_candidates",
}

# Everything except nn_value_trust is pinned at A10's own shipped values --
# this agent changes leaf evaluation, not the tree/determinization/compute-
# budget machinery (about.md's "Deliberately out of scope").
FREE_PARAM_NAMES = ["nn_value_trust"]


def _load_defaults_from_binary():
    if not BINARY.exists():
        # Fallback so --help works before `make calib_ismctsnn` has run --
        # see aicalibsrc/hbt/README.md's pitfall #2 on why this rots if not
        # kept in sync; ISMCTS_DEFAULTS (ai_strat_ismcts1.h) is the source
        # of truth, this is just enough to not crash argument parsing.
        return {
            "limit_iterations": 4000, "limit_playout_steps": 200, "limit_max_nodes": 200000,
            "limit_recall_variants": 2, "limit_cash_variants": 3, "limit_max_candidates": 128,
            "search_exploration_constant": 1.41421356, "search_use_availability": True,
            "search_expand_threshold": 3, "threshold_widening_k": 2.0,
            "threshold_widening_alpha": 0.5, "prior_use_heuristic": False,
            "rollout_max_turns": 500, "rollout_cutoff_depth": 0,
            "weight_energy_advantage": 0.0, "weight_cash_advantage": 0.0,
            "weight_hand_advantage": 0.0, "limit_flat_iterations": 2000,
            "limit_flat_candidates": 36, "nn_value_trust": 1.0,
        }
    result = subprocess.run([str(BINARY), "--print-defaults"],
                            capture_output=True, text=True, check=True)
    return json.loads(result.stdout)


DEFAULTS = _load_defaults_from_binary()

SWEEP_DEFAULTS = {
    "nn_value_trust": [0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0],
}


def coerce_param(name, value):
    if name in _INT_PARAMS:
        return int(round(value))
    return float(value)


def params_to_args(p):
    return [str(coerce_param(name, p[name])) for name in PARAM_NAMES]


def merge_params(overrides):
    p = dict(DEFAULTS)
    p.update(overrides)
    return {name: coerce_param(name, p[name]) for name in PARAM_NAMES}


def trust_params(trust):
    return merge_params({"nn_value_trust": trust})


# ---------------------------------------------------------------------------
# Core: one match, many matches, confidence intervals
# ---------------------------------------------------------------------------

def run_match(weights, numsim, seed, agent_a, agent_b, params_a, params_b, **tags):
    """One call to bin/calib_ismctsnn. Safe to run in a worker.

    **tags are arbitrary caller bookkeeping merged into the returned dict
    at the source -- run_many()'s ProcessPoolExecutor + as_completed()
    returns results in COMPLETION order, not submission order (see
    aicalibsrc/hbt/README.md's pitfall #1)."""
    if not BINARY.exists():
        raise FileNotFoundError(f"{BINARY} not found -- run `make calib_ismctsnn` first")

    args = [str(BINARY), str(weights), str(numsim), str(seed), agent_a, agent_b,
            *params_to_args(params_a), *params_to_args(params_b)]
    result = subprocess.run(args, capture_output=True, text=True, check=True)
    row = result.stdout.strip().split(",")
    return {
        "numsim": int(row[0]), "seed": int(row[1]),
        "agent_a": row[2], "agent_b": row[3],
        "wins_a": int(row[-3]), "wins_b": int(row[-2]), "draws": int(row[-1]),
        **tags,
    }


def run_many(jobs, max_workers=None, quiet=False):
    results = []
    with ProcessPoolExecutor(max_workers=max_workers) as pool:
        futures = [pool.submit(run_match, **job) for job in jobs]
        for i, fut in enumerate(as_completed(futures), 1):
            results.append(fut.result())
            if not quiet:
                print(f"\r  {i}/{len(jobs)} matches done", end="", file=sys.stderr, flush=True)
    if not quiet:
        print(file=sys.stderr)
    return pd.DataFrame(results)


def wilson_ci(wins, n, z=1.96):
    """95% Wilson score interval for a binomial proportion."""
    if n == 0:
        return (float("nan"), float("nan"))
    p = wins / n
    denom = 1 + z**2 / n
    center = (p + z**2 / (2 * n)) / denom
    half = (z * math.sqrt(p * (1 - p) / n + z**2 / (4 * n**2))) / denom
    return (center - half, center + half)


def replicate_seeds(base_seed, replicates):
    return [base_seed + i for i in range(replicates)]


def ismctsnn_win_rate(df):
    in_a = df["agent_a"] == "ismctsnn"
    wins = int(np.where(in_a, df["wins_a"], df["wins_b"]).sum())
    n = int((df["wins_a"] + df["wins_b"] + df["draws"]).sum())
    return wins, n


# ---------------------------------------------------------------------------
# sweep: nn_value_trust varied, ismctsnn vs a fixed opponent, both seats
# ---------------------------------------------------------------------------

def build_sweep_jobs(values, opponent, weights, numsim, seeds):
    jobs = []
    for v in values:
        p = trust_params(v)
        for seed in seeds:
            jobs.append(dict(weights=weights, numsim=numsim, seed=seed, agent_a="ismctsnn",
                             agent_b=opponent, params_a=p, params_b=dict(DEFAULTS),
                             _value=v, _nn_in_a=True))
            jobs.append(dict(weights=weights, numsim=numsim, seed=seed, agent_a=opponent,
                             agent_b="ismctsnn", params_a=dict(DEFAULTS), params_b=p,
                             _value=v, _nn_in_a=False))
    return jobs


def cmd_sweep(args):
    values = args.values if args.values is not None else SWEEP_DEFAULTS["nn_value_trust"]
    seeds = replicate_seeds(args.base_seed, args.replicates)
    jobs = build_sweep_jobs(values, args.opponent, args.weights, args.numsim, seeds)

    print(f"Running {len(jobs)} matches "
         f"({len(values)} values x {args.replicates} replicates x 2 seats, "
         f"vs {args.opponent})...", file=sys.stderr)

    df = run_many(jobs, max_workers=args.workers)
    df["nn_wins"] = np.where(df["_nn_in_a"], df["wins_a"], df["wins_b"])
    df["n"] = df["wins_a"] + df["wins_b"] + df["draws"]

    grouped = df.groupby("_value").agg(wins=("nn_wins", "sum"), n=("n", "sum")).reset_index()
    grouped["win_rate"] = grouped["wins"] / grouped["n"]
    grouped[["ci_lo", "ci_hi"]] = grouped.apply(
        lambda r: pd.Series(wilson_ci(r["wins"], r["n"])), axis=1)
    grouped = grouped.rename(columns={"_value": "nn_value_trust"}).sort_values("nn_value_trust")

    print(f"\nSweep of nn_value_trust (ismctsnn vs {args.opponent}, both seats, "
         f"{args.numsim * args.replicates * 2} games/value):")
    print(grouped.to_string(index=False, float_format=lambda x: f"{x:.4f}"))

    is_monotonic_decline = grouped["win_rate"].is_monotonic_decreasing
    if is_monotonic_decline:
        print("\nWARNING: win rate declines MONOTONICALLY as trust rises -- this is the "
             "A9 reply_trust / A13 hplus_trust failure signature (the mechanism is "
             "actively misleading, not just unhelpful). See about.md's null-result policy.")

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    out_path = RESULTS_DIR / "sweep_nn_value_trust.csv"
    grouped.to_csv(out_path, index=False)
    print(f"\nSaved: {out_path}")

    if args.plot:
        plot_sweep(grouped, args.opponent, RESULTS_DIR / "sweep_nn_value_trust.png")


def plot_sweep(summary, opponent, out_path):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots()
    yerr = [summary["win_rate"] - summary["ci_lo"], summary["ci_hi"] - summary["win_rate"]]
    ax.errorbar(summary["nn_value_trust"], summary["win_rate"], yerr=yerr, marker="o", capsize=4)
    ax.axhline(0.5, color="gray", linestyle="--", linewidth=1)
    ax.set_xlabel("nn_value_trust")
    ax.set_ylabel(f"win rate vs {opponent}")
    ax.set_title("AlphaOracle Prime: nn_value_trust sweep (95% Wilson CI)")
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    print(f"Saved: {out_path}")


# ---------------------------------------------------------------------------
# validate: candidate trust vs the shipped baseline (trust=0), vs --opponent
# ---------------------------------------------------------------------------

def cmd_validate(args):
    candidate = trust_params(args.trust)
    baseline = trust_params(0.0)  # pure A10 -- the superset-guarantee re-check
    seeds = replicate_seeds(args.base_seed, args.replicates)

    def vs_opponent(params):
        jobs = []
        for seed in seeds:
            jobs.append(dict(weights=args.weights, numsim=args.numsim, seed=seed,
                             agent_a="ismctsnn", agent_b=args.opponent,
                             params_a=params, params_b=dict(DEFAULTS)))
            jobs.append(dict(weights=args.weights, numsim=args.numsim, seed=seed,
                             agent_a=args.opponent, agent_b="ismctsnn",
                             params_a=dict(DEFAULTS), params_b=params))
        df = run_many(jobs, max_workers=args.workers)
        wins, n = ismctsnn_win_rate(df)
        lo, hi = wilson_ci(wins, n)
        return wins, n, (wins / n if n else 0.0), lo, hi

    print(f"Validating vs {args.opponent} ({len(seeds) * 2} matches per config)...",
         file=sys.stderr)
    b_wins, b_n, b_rate, b_lo, b_hi = vs_opponent(baseline)
    c_wins, c_n, c_rate, c_lo, c_hi = vs_opponent(candidate)

    print(f"\nWin rate vs {args.opponent} ({b_n} games each):")
    print(f"  trust=0.0 (baseline): {b_rate:.4f} [{b_lo:.4f}, {b_hi:.4f}]  "
         f"(should read ~0.50 -- superset-guarantee sanity check)")
    print(f"  trust={args.trust}: {c_rate:.4f} [{c_lo:.4f}, {c_hi:.4f}]")
    print(f"  delta: {(c_rate - b_rate) * 100:+.2f} percentage points")
    print(f"\nGate 2 (about.md): {'PASS' if c_lo > 0.5 else 'FAIL'} "
         f"-- Wilson CI lower bound {c_lo:.4f} {'>' if c_lo > 0.5 else '<='} 0.50")


# ---------------------------------------------------------------------------
# selfplay: round-robin among named trust candidates, BT fit
# ---------------------------------------------------------------------------

def bradley_terry_fit(n, wins_matrix, games_matrix):
    """MLE fit: P(i beats j) = 1/(1+exp(-(r_i-r_j))). r[0] anchored to 0."""
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


def build_selfplay_jobs(names, params_map, weights, numsim, seeds):
    jobs = []
    pairs = [(i, j) for i in range(len(names)) for j in range(len(names)) if i < j]
    for i, j in pairs:
        for seed in seeds:
            jobs.append(dict(weights=weights, numsim=numsim, seed=seed,
                             agent_a="ismctsnn", agent_b="ismctsnn",
                             params_a=params_map[names[i]], params_b=params_map[names[j]],
                             _i=i, _j=j))
            jobs.append(dict(weights=weights, numsim=numsim, seed=seed,
                             agent_a="ismctsnn", agent_b="ismctsnn",
                             params_a=params_map[names[j]], params_b=params_map[names[i]],
                             _i=j, _j=i))
    return jobs


def summarize_selfplay(df, names):
    n = len(names)
    wins = np.zeros((n, n))
    games = np.zeros((n, n))
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

    strengths = bradley_terry_fit(n, wins, games)
    summary = pd.DataFrame({
        "candidate": names,
        "bt_strength": strengths,
        "overall_win_rate": total_wins / total_games,
        "games_played": total_games.astype(int),
    })
    return summary.sort_values("bt_strength", ascending=False)


def cmd_selfplay(args):
    if len(args.trusts) < 2:
        print("Need at least 2 --trusts values", file=sys.stderr)
        sys.exit(1)

    names = [f"trust={t}" for t in args.trusts]
    params_map = {name: trust_params(t) for name, t in zip(names, args.trusts)}

    seeds = replicate_seeds(args.base_seed, args.replicates)
    jobs = build_selfplay_jobs(names, params_map, args.weights, args.numsim, seeds)

    n_pairs = len(jobs) // (2 * args.replicates)
    print(f"Running {len(jobs)} matches "
         f"({n_pairs} pairs x {args.replicates} replicates x 2 seats) "
         f"over {len(names)} trust candidates...", file=sys.stderr)

    df = run_many(jobs, max_workers=args.workers)

    summary = summarize_selfplay(df, names)
    print("\nSelf-play round-robin among trust candidates (Bradley-Terry fit, higher=stronger):")
    print(summary.to_string(index=False, float_format=lambda x: f"{x:.4f}"))
    print(f"\nBest candidate: {summary.iloc[0]['candidate']}")

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    out_path = RESULTS_DIR / "selfplay_trust.csv"
    summary.to_csv(out_path, index=False)
    print(f"\nSaved: {out_path}")


# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    p_sweep = sub.add_parser("sweep", help="univariate nn_value_trust sweep vs a fixed opponent")
    p_sweep.add_argument("--weights", required=True)
    p_sweep.add_argument("--values", type=float, nargs="+",
                         help="default: [0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0]")
    p_sweep.add_argument("--opponent", default="ismcts")
    p_sweep.add_argument("--numsim", type=int, default=2000)
    p_sweep.add_argument("--replicates", type=int, default=4)
    p_sweep.add_argument("--base-seed", type=int, default=1337)
    p_sweep.add_argument("--workers", type=int, default=None)
    p_sweep.add_argument("--plot", action="store_true")
    p_sweep.set_defaults(func=cmd_sweep)

    p_val = sub.add_parser("validate", help="one trust value vs trust=0.0, both vs --opponent")
    p_val.add_argument("--weights", required=True)
    p_val.add_argument("--trust", type=float, required=True)
    p_val.add_argument("--opponent", default="ismcts")
    p_val.add_argument("--numsim", type=int, default=2000)
    p_val.add_argument("--replicates", type=int, default=4)
    p_val.add_argument("--base-seed", type=int, default=1337)
    p_val.add_argument("--workers", type=int, default=None)
    p_val.set_defaults(func=cmd_validate)

    p_self = sub.add_parser("selfplay", help="round-robin among named trust candidates, BT fit")
    p_self.add_argument("--weights", required=True)
    p_self.add_argument("--trusts", type=float, nargs="+", required=True)
    p_self.add_argument("--numsim", type=int, default=2000)
    p_self.add_argument("--replicates", type=int, default=4)
    p_self.add_argument("--base-seed", type=int, default=1337)
    p_self.add_argument("--workers", type=int, default=None)
    p_self.set_defaults(func=cmd_selfplay)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
