#!/usr/bin/env python3
"""Calibration driver for the A8 Simple Monte Carlo ("The Soothsayer") AI
agent's twenty tunable parameters (SimpleMcParams -- see
src/ai_strat/ai_strat_simplemc1.h).

Drives bin/calib_simplemc, which links the game engine directly and prints
one CSV result line per invocation. All the actual game simulation happens
in the compiled C binary; this script only orchestrates subprocess calls and
aggregates/fits/searches the results. Structure and internals (run_match's
**tags-at-the-source fix, the seed-offset convention, --print-defaults-sourced
DEFAULTS) are carried over from aicalibsrc/tactical/calibrate_tactical.py.

DEFAULTS below is read once, at import time, from
`bin/calib_simplemc --print-defaults`, so it can never drift from the
shipped SIMPLEMC_DEFAULTS.

*** This agent's parameters are a fundamentally different shape than A1-A7's,
and `optimize`'s defaults reflect that -- read this before running anything: ***

A1-A7 tune decision-quality WEIGHTS for a fixed-cost algorithm: calibration
finds the one best value for each weight. Most of SimpleMcParams instead
controls how much this agent SEARCHES, and more searching is basically always
at least as strong, just slower -- there is no interior optimum to find for
those fields, only a cost/strength curve. The twenty fields split three ways:

  BUDGET_PARAMS (7): rollout_seed_simulations, rollout_round_simulations,
    limit_stage1/2/3_simulations, limit_max_simulations, limit_total_rollouts.
    Pure compute-budget dials. `optimize` does NOT search these by default --
    doing so would just walk them to their upper bound, which isn't a
    meaningful search. Use `sweep` on these instead, to see how win rate
    trades off against wall-clock cost, and pick a shipped budget that fits
    your time constraints (see the module-level runtime discussion below).

  EFFICIENCY_PARAMS (11): limit_recall_variants, limit_cash_variants,
    limit_max_candidates, prune_zero_win_seed, threshold_confidence_level,
    threshold_stage1/2/3_keep_ratio, limit_stage1/2/3_keep. These trade real
    risk for real speed -- too aggressive a confidence threshold or too small
    a survivor cap can prune away the actual best move on noisy early
    samples; too conservative wastes rollouts on hopeless candidates. This
    IS where a genuine interior optimum can exist, so `optimize` searches
    these by default, at a BUDGET fixed at the shipped defaults.

  Everything else (2): rollout_determinize is a binary research question
    (how much is hidden-information reasoning worth?), not a continuous
    dial -- A/B it directly via `validate --candidate <a JSON file with just
    {"rollout_determinize": 0}>` rather than searching it. rollout_max_turns
    is inert (games never come close to it) and isn't touched at all.

*** Runtime: this agent costs ~100ms/game (measured; A1-A7 cost <1ms/game),
so every default below is sized far smaller than the other agents' drivers'
defaults. Keep exploratory/prelim runs around 30s wall-clock; only the final
shipped-parameter validation run should be allowed to run longer (up to
~20 minutes) -- scale --numsim/--replicates/--maxiter/--popsize up
deliberately for that one run, not as a matter of course. ***

Four subcommands, same shape as every other agent's driver:

  sweep     Univariate diagnostic: hold every parameter at its default
            except one, vary that one, play vs a fixed --opponent (default
            "rand"), both seats, with binomial confidence intervals. This is
            the right tool for BUDGET_PARAMS -- it reports the cost/strength
            curve rather than pretending there's one right answer.

  optimize  Black-box search (scipy.optimize.differential_evolution) over
            EFFICIENCY_PARAMS by default (or an explicit --params subset),
            maximizing win rate against a fixed --opponent (default
            "borealis" -- the rating-50 anchor).

  selfplay  Round-robin among a small set of NAMED candidate parameter sets,
            both seat orders, --ai.a=simplemc vs --ai.b=simplemc. Reports a
            Bradley-Terry fit.

  validate  Compare one candidate parameter set against the shipped
            defaults, vs a chosen opponent, both seats. Also how to A/B
            rollout_determinize (see above).

Candidate parameter sets are given as JSON files (a full or partial
SimpleMcParams dict; missing fields fall back to the compiled defaults) or
the literal string "defaults". `optimize`'s own output file is directly
usable as a `selfplay`/`validate` candidate (it has a "best_params" key,
which these commands know to unwrap).

Examples:
  ./calibrate_simplemc.py sweep --param rollout_round_simulations \\
      --opponent rand --numsim 20 --replicates 2
  ./calibrate_simplemc.py optimize --opponent borealis --numsim 20 \\
      --replicates 1 --maxiter 4 --popsize 6
  ./calibrate_simplemc.py selfplay --candidates defaults results/optimize_borealis.json
  ./calibrate_simplemc.py validate --candidate results/optimize_borealis.json --opponent rand
"""

import argparse
import json
import math
import subprocess
import sys
from concurrent.futures import ProcessPoolExecutor, ThreadPoolExecutor, as_completed
from pathlib import Path

import numpy as np
import pandas as pd
from scipy.optimize import differential_evolution, minimize

BINARY = Path(__file__).resolve().parent.parent.parent / "bin" / "calib_simplemc"
RESULTS_DIR = Path(__file__).resolve().parent / "results"

# Declared struct order in SimpleMcParams -- must match parse_params() in
# calib_simplemc.c.
PARAM_NAMES = [
    "limit_recall_variants", "limit_cash_variants", "limit_max_candidates",
    "rollout_seed_simulations", "rollout_round_simulations", "prune_zero_win_seed",
    "threshold_confidence_level", "threshold_stage1_keep_ratio",
    "threshold_stage2_keep_ratio", "threshold_stage3_keep_ratio",
    "limit_stage1_keep", "limit_stage2_keep", "limit_stage3_keep",
    "limit_stage1_simulations", "limit_stage2_simulations", "limit_stage3_simulations",
    "limit_max_simulations", "limit_total_rollouts",
    "rollout_determinize", "rollout_max_turns",
]

_INT_PARAMS = {
    "limit_recall_variants", "limit_cash_variants", "limit_max_candidates",
    "rollout_seed_simulations", "rollout_round_simulations", "prune_zero_win_seed",
    "limit_stage1_keep", "limit_stage2_keep", "limit_stage3_keep",
    "limit_stage1_simulations", "limit_stage2_simulations", "limit_stage3_simulations",
    "limit_max_simulations", "limit_total_rollouts",
    "rollout_determinize", "rollout_max_turns",
}

# See the module docstring's three-way split for why these two groups are
# treated differently by `optimize`.
BUDGET_PARAMS = [
    "rollout_seed_simulations", "rollout_round_simulations",
    "limit_stage1_simulations", "limit_stage2_simulations", "limit_stage3_simulations",
    "limit_max_simulations", "limit_total_rollouts",
]
EFFICIENCY_PARAMS = [
    "limit_recall_variants", "limit_cash_variants", "limit_max_candidates",
    "prune_zero_win_seed", "threshold_confidence_level",
    "threshold_stage1_keep_ratio", "threshold_stage2_keep_ratio", "threshold_stage3_keep_ratio",
    "limit_stage1_keep", "limit_stage2_keep", "limit_stage3_keep",
]
# rollout_determinize (A/B via `validate`, not searched) and rollout_max_turns
# (inert) are the remaining two PARAM_NAMES -- neither appears in BOUNDS/
# SWEEP_DEFAULTS below.


def _load_defaults_from_binary():
    if not BINARY.exists():
        # Fall back to the compiled-in values documented in
        # ai_strat_simplemc1.c's SIMPLEMC_DEFAULTS, so --help and argument
        # parsing still work before `make calib_simplemc` has run.
        return {
            "limit_recall_variants": 2, "limit_cash_variants": 3, "limit_max_candidates": 128,
            "rollout_seed_simulations": 7, "rollout_round_simulations": 25,
            "prune_zero_win_seed": 1, "threshold_confidence_level": 1.96,
            "threshold_stage1_keep_ratio": 0.75, "threshold_stage2_keep_ratio": 0.50,
            "threshold_stage3_keep_ratio": 0.25, "limit_stage1_keep": 30,
            "limit_stage2_keep": 10, "limit_stage3_keep": 4,
            "limit_stage1_simulations": 100, "limit_stage2_simulations": 300,
            "limit_stage3_simulations": 700, "limit_max_simulations": 1500,
            "limit_total_rollouts": 25000, "rollout_determinize": 1,
            "rollout_max_turns": 500,
        }
    result = subprocess.run([str(BINARY), "--print-defaults"],
                            capture_output=True, text=True, check=True)
    return json.loads(result.stdout)


DEFAULTS = _load_defaults_from_binary()

# Search/sweep space -- covers BUDGET_PARAMS + EFFICIENCY_PARAMS (18 of the
# 20 fields; rollout_determinize/rollout_max_turns are deliberately absent,
# see the module docstring).
BOUNDS = {
    "limit_recall_variants": (0, 5),
    "limit_cash_variants": (0, 5),
    "limit_max_candidates": (40, 128),
    "rollout_seed_simulations": (3, 20),
    "rollout_round_simulations": (10, 100),
    "prune_zero_win_seed": (0, 1),
    "threshold_confidence_level": (1.0, 3.0),
    "threshold_stage1_keep_ratio": (0.4, 0.9),
    "threshold_stage2_keep_ratio": (0.2, 0.7),
    "threshold_stage3_keep_ratio": (0.1, 0.5),
    "limit_stage1_keep": (5, 50),
    "limit_stage2_keep": (2, 20),
    "limit_stage3_keep": (1, 8),
    "limit_stage1_simulations": (30, 300),
    "limit_stage2_simulations": (100, 800),
    "limit_stage3_simulations": (300, 2000),
    "limit_max_simulations": (500, 4000),
    "limit_total_rollouts": (2000, 60000),
}

SWEEP_DEFAULTS = {
    "limit_recall_variants": [0, 1, 2, 3, 5],
    "limit_cash_variants": [0, 1, 3, 5],
    "limit_max_candidates": [40, 64, 96, 128],
    "rollout_seed_simulations": [3, 7, 10, 15],
    "rollout_round_simulations": [10, 25, 50, 100],
    "prune_zero_win_seed": [0, 1],
    "threshold_confidence_level": [1.0, 1.5, 1.96, 2.5, 3.0],
    "threshold_stage1_keep_ratio": [0.5, 0.65, 0.75, 0.85],
    "threshold_stage2_keep_ratio": [0.3, 0.4, 0.5, 0.6],
    "threshold_stage3_keep_ratio": [0.15, 0.20, 0.25, 0.35],
    "limit_stage1_keep": [10, 20, 30, 40],
    "limit_stage2_keep": [4, 8, 10, 15],
    "limit_stage3_keep": [2, 3, 4, 6],
    "limit_stage1_simulations": [50, 100, 150, 200],
    "limit_stage2_simulations": [150, 300, 450, 600],
    "limit_stage3_simulations": [400, 700, 1000, 1400],
    "limit_max_simulations": [800, 1500, 2500, 4000],
    "limit_total_rollouts": [5000, 15000, 25000, 40000],
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


def load_candidate(spec):
    """spec is either the literal 'defaults' or a path to a JSON file holding
    a full/partial SimpleMcParams dict, or `optimize`'s own output (which
    nests it under "best_params")."""
    if spec == "defaults":
        return dict(DEFAULTS)
    with open(spec) as f:
        data = json.load(f)
    return merge_params(data.get("best_params", data))


# ---------------------------------------------------------------------------
# Core: one match, many matches, confidence intervals
# ---------------------------------------------------------------------------

def run_match(numsim, seed, agent_a, agent_b, params_a, params_b, **tags):
    """One call to bin/calib_simplemc. Safe to run in a worker.

    **tags are arbitrary caller bookkeeping (e.g. sweep's `_value`,
    selfplay's `_i`/`_j`) merged straight into the returned dict instead of
    being tracked in a separate same-order list -- run_many()'s
    ProcessPoolExecutor + as_completed() returns results in COMPLETION
    order, not submission order, so tagging at the source (rather than
    reattaching by list position afterward) is what keeps this correct
    under real parallelism.
    """
    if not BINARY.exists():
        raise FileNotFoundError(f"{BINARY} not found -- run `make calib_simplemc` first")

    args = [str(BINARY), str(numsim), str(seed), agent_a, agent_b,
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
    """One-shot batch: spins up its own process pool. For repeated small
    batches (as `optimize` needs per generation), use run_many_threaded()
    with a persistent pool instead."""
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


def run_many_threaded(jobs, executor):
    """Like run_many() but uses a caller-owned, already-running executor --
    threads, not processes, since subprocess.run() releases the GIL while
    waiting on the child."""
    futures = [executor.submit(run_match, **job) for job in jobs]
    return pd.DataFrame([f.result() for f in futures])


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


def simplemc_win_rate(df):
    simplemc_in_a = df["agent_a"] == "simplemc"
    wins = int(np.where(simplemc_in_a, df["wins_a"], df["wins_b"]).sum())
    n = int((df["wins_a"] + df["wins_b"] + df["draws"]).sum())
    return wins, n


# ---------------------------------------------------------------------------
# sweep: one param varied, simplemc vs a fixed opponent, both seats
# ---------------------------------------------------------------------------

def build_sweep_jobs(param, values, opponent, numsim, seeds):
    jobs = []
    for v in values:
        p = merge_params({param: v})
        for seed in seeds:
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="simplemc", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS), _value=v, _simplemc_in_a=True))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="simplemc",
                             params_a=dict(DEFAULTS), params_b=p, _value=v, _simplemc_in_a=False))
    return jobs


def cmd_sweep(args):
    values = args.values or SWEEP_DEFAULTS[args.param]
    seeds = replicate_seeds(args.base_seed, args.replicates)
    jobs = build_sweep_jobs(args.param, values, args.opponent, args.numsim, seeds)

    print(f"Running {len(jobs)} matches "
         f"({len(values)} values x {args.replicates} replicates x 2 seats, "
         f"vs {args.opponent}, ~{args.numsim} games/match)...", file=sys.stderr)

    df = run_many(jobs, max_workers=args.workers)
    df["simplemc_wins"] = np.where(df["_simplemc_in_a"], df["wins_a"], df["wins_b"])
    df["n"] = df["wins_a"] + df["wins_b"] + df["draws"]

    grouped = df.groupby("_value").agg(wins=("simplemc_wins", "sum"), n=("n", "sum")).reset_index()
    grouped["win_rate"] = grouped["wins"] / grouped["n"]
    grouped[["ci_lo", "ci_hi"]] = grouped.apply(
        lambda r: pd.Series(wilson_ci(r["wins"], r["n"])), axis=1)
    grouped = grouped.rename(columns={"_value": args.param}).sort_values(args.param)

    print(f"\nSweep of {args.param} (simplemc vs {args.opponent}, both seats, "
         f"{args.numsim * args.replicates * 2} games/value):")
    print(grouped.to_string(index=False, float_format=lambda x: f"{x:.4f}"))
    if args.param in BUDGET_PARAMS:
        print(f"\n({args.param} is a BUDGET_PARAMS field -- read this as a cost/strength "
             f"curve, not a search for one right answer. Pick a value that fits your "
             f"time budget.)")

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    out_path = RESULTS_DIR / f"sweep_{args.param}.csv"
    grouped.to_csv(out_path, index=False)
    print(f"\nSaved: {out_path}")

    if args.plot:
        plot_sweep(grouped, args.param, args.opponent, RESULTS_DIR / f"sweep_{args.param}.png")


def plot_sweep(summary, param, opponent, out_path):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots()
    yerr = [summary["win_rate"] - summary["ci_lo"], summary["ci_hi"] - summary["win_rate"]]
    ax.errorbar(summary[param], summary["win_rate"], yerr=yerr, marker="o", capsize=4)
    ax.axhline(0.5, color="gray", linestyle="--", linewidth=1)
    ax.set_xlabel(param)
    ax.set_ylabel(f"win rate vs {opponent}")
    ax.set_title(f"The Soothsayer: {param} sweep (95% Wilson CI)")
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    print(f"Saved: {out_path}")


# ---------------------------------------------------------------------------
# optimize: differential evolution vs a fixed opponent (EFFICIENCY_PARAMS
# only, by default), search-health re-check
# ---------------------------------------------------------------------------

def vector_to_params(x, free_names, fixed):
    p = dict(fixed)
    for name, val in zip(free_names, x):
        p[name] = val
    return merge_params(p)


def make_objective(free_names, fixed, opponent, numsim, seeds, executor, progress):
    def objective(x):
        p = vector_to_params(x, free_names, fixed)
        jobs = []
        for seed in seeds:
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="simplemc", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="simplemc",
                             params_a=dict(DEFAULTS), params_b=p))
        df = run_many_threaded(jobs, executor)
        wins, n = simplemc_win_rate(df)
        rate = wins / n if n else 0.0
        progress["evals"] += 1
        print(f"\r  eval {progress['evals']}: win_rate={rate:.4f}",
             end="", file=sys.stderr, flush=True)
        return -rate  # differential_evolution minimizes
    return objective


def check_personality_flags(best_params):
    """Returns a list of human-readable warning strings for anything an
    optimizer result should not silently ship with. Never auto-rejects --
    flags for review, same policy as every other agent's driver.

    Unlike A1-A7 (whose flags catch a decision-quality mechanism collapsing
    toward a static/degenerate value), this agent has no strategic weights to
    collapse -- what can go wrong here is the SEARCH becoming unsound: pruning
    so aggressively that it risks discarding the true best move on noisy
    early samples, or the stage-cap ordering inverting (which would truncate
    survivors *upward* partway through a search instead of narrowing it)."""
    flags = []

    z = best_params["threshold_confidence_level"]
    if z < 1.0:
        flags.append(f"threshold_confidence_level ({z:.3f}) is below 1.0 -- pruning this "
                     f"aggressive risks discarding the true best move on noisy early "
                     f"samples (a ~68% or narrower confidence band, not the ~95%+ a normal "
                     f"approximation to the binomial should use here).")

    r1 = best_params["threshold_stage1_keep_ratio"]
    r2 = best_params["threshold_stage2_keep_ratio"]
    r3 = best_params["threshold_stage3_keep_ratio"]
    if not (r1 >= r2 >= r3):
        flags.append(f"threshold_stage1/2/3_keep_ratio ({r1:.2f}, {r2:.2f}, {r3:.2f}) are not "
                     f"monotonically decreasing -- a later stage cap would stop narrowing (or "
                     f"would re-widen) the survivor set instead of progressively pruning it.")

    k1 = best_params["limit_stage1_keep"]
    k2 = best_params["limit_stage2_keep"]
    k3 = best_params["limit_stage3_keep"]
    if not (k1 >= k2 >= k3):
        flags.append(f"limit_stage1/2/3_keep ({k1}, {k2}, {k3}) are not monotonically "
                     f"decreasing -- same issue as the ratios above, at the hard-cap level.")

    return flags


def cmd_optimize(args):
    free_names = args.params or list(EFFICIENCY_PARAMS)
    fixed = {n: DEFAULTS[n] for n in PARAM_NAMES if n not in free_names}
    bounds = [BOUNDS[n] for n in free_names]
    seeds = replicate_seeds(args.base_seed, args.replicates)

    print(f"Optimizing {len(free_names)} param(s) vs {args.opponent}: {free_names}",
         file=sys.stderr)
    print(f"({len(seeds) * 2} matches x {args.numsim} games per evaluation)",
         file=sys.stderr)

    progress = {"evals": 0}
    with ThreadPoolExecutor(max_workers=args.workers or 8) as executor:
        objective = make_objective(free_names, fixed, args.opponent, args.numsim,
                                   seeds, executor, progress)
        result = differential_evolution(objective, bounds, maxiter=args.maxiter,
                                        popsize=args.popsize, seed=args.opt_seed,
                                        tol=0.01, mutation=(0.5, 1.0),
                                        recombination=0.7, polish=False)
    print(file=sys.stderr)

    best_params = vector_to_params(result.x, free_names, fixed)
    print("\nBest parameters found:")
    for name in PARAM_NAMES:
        marker = "" if name in free_names else "  (fixed at default)"
        print(f"  {name} = {best_params[name]}{marker}")
    print(f"  win rate vs {args.opponent} (in-search estimate, {args.numsim} games/match): "
         f"{-result.fun:.4f}")

    personality_flags = check_personality_flags(best_params)
    if personality_flags:
        print("\n*** Search-health flags (review before shipping, do not auto-ship past these): ***")
        for flag in personality_flags:
            print(f"  - {flag}")

    print(f"\nRe-validating winner with more games "
         f"({args.validate_replicates * 2} matches x {args.validate_numsim} games)...",
         file=sys.stderr)
    val_seeds = replicate_seeds(args.base_seed + 100000, args.validate_replicates)
    jobs = []
    for seed in val_seeds:
        jobs.append(dict(numsim=args.validate_numsim, seed=seed, agent_a="simplemc",
                         agent_b=args.opponent, params_a=best_params, params_b=dict(DEFAULTS)))
        jobs.append(dict(numsim=args.validate_numsim, seed=seed, agent_a=args.opponent,
                         agent_b="simplemc", params_a=dict(DEFAULTS), params_b=best_params))
    df = run_many(jobs, max_workers=args.workers)
    wins, n = simplemc_win_rate(df)
    lo, hi = wilson_ci(wins, n)
    rate = wins / n if n else 0.0
    print(f"\nValidated win rate vs {args.opponent}: {rate:.4f} [{lo:.4f}, {hi:.4f}] "
         f"over {n} games")

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    out_path = RESULTS_DIR / f"optimize_{args.opponent}.json"
    with open(out_path, "w") as f:
        json.dump({
            "best_params": best_params, "free_params": free_names,
            "opponent": args.opponent, "in_search_win_rate": -result.fun,
            "validated_win_rate": rate, "validated_ci": [lo, hi], "n_games": n,
            "personality_flags": personality_flags,
        }, f, indent=2)
    print(f"Saved: {out_path}")


# ---------------------------------------------------------------------------
# selfplay: round-robin among named candidates, Bradley-Terry fit
# ---------------------------------------------------------------------------

def build_selfplay_jobs(names, params_map, numsim, seeds, include_mirror):
    jobs = []
    pairs = [(i, j) for i in range(len(names)) for j in range(len(names))
             if i < j or (include_mirror and i == j)]
    for i, j in pairs:
        for seed in seeds:
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="simplemc", agent_b="simplemc",
                             params_a=params_map[names[i]], params_b=params_map[names[j]],
                             _i=i, _j=j))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="simplemc", agent_b="simplemc",
                             params_a=params_map[names[j]], params_b=params_map[names[i]],
                             _i=j, _j=i))
    return jobs


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
    if len(args.candidates) < 2:
        print("Need at least 2 --candidates (e.g. defaults plus one JSON file)",
             file=sys.stderr)
        sys.exit(1)

    names = [Path(c).stem if c != "defaults" else "defaults" for c in args.candidates]
    if len(set(names)) != len(names):
        print("Candidate names (filename stems) must be unique", file=sys.stderr)
        sys.exit(1)
    params_map = {name: load_candidate(spec) for name, spec in zip(names, args.candidates)}

    seeds = replicate_seeds(args.base_seed, args.replicates)
    jobs = build_selfplay_jobs(names, params_map, args.numsim, seeds, args.include_mirror)

    n_pairs = len(jobs) // (2 * args.replicates)
    print(f"Running {len(jobs)} matches "
         f"({n_pairs} pairs x {args.replicates} replicates x 2 seats) "
         f"over {len(names)} candidates...", file=sys.stderr)

    df = run_many(jobs, max_workers=args.workers)

    summary = summarize_selfplay(df, names)
    print("\nSelf-play round-robin (Bradley-Terry fit, higher = stronger):")
    print(summary.to_string(index=False, float_format=lambda x: f"{x:.4f}"))
    print(f"\nBest candidate: {summary.iloc[0]['candidate']}")

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    out_path = RESULTS_DIR / "selfplay_named.csv"
    summary.to_csv(out_path, index=False)
    print(f"\nSaved: {out_path}")


# ---------------------------------------------------------------------------
# validate: one candidate vs the shipped defaults, vs a chosen opponent.
# Also the way to A/B rollout_determinize -- pass a candidate JSON of just
# {"rollout_determinize": 0} to measure what determinization is worth.
# ---------------------------------------------------------------------------

def cmd_validate(args):
    candidate = load_candidate(args.candidate)
    seeds = replicate_seeds(args.base_seed, args.replicates)

    def vs_opponent(params):
        jobs = []
        for seed in seeds:
            jobs.append(dict(numsim=args.numsim, seed=seed, agent_a="simplemc",
                             agent_b=args.opponent, params_a=params, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=args.numsim, seed=seed, agent_a=args.opponent,
                             agent_b="simplemc", params_a=dict(DEFAULTS), params_b=params))
        df = run_many(jobs, max_workers=args.workers)
        wins, n = simplemc_win_rate(df)
        lo, hi = wilson_ci(wins, n)
        return wins, n, (wins / n if n else 0.0), lo, hi

    print(f"Validating vs {args.opponent} ({len(seeds) * 2} matches per config)...",
         file=sys.stderr)
    d_wins, d_n, d_rate, d_lo, d_hi = vs_opponent(dict(DEFAULTS))
    c_wins, c_n, c_rate, c_lo, c_hi = vs_opponent(candidate)

    print(f"\nWin rate vs {args.opponent} ({d_n} games each):")
    print(f"  defaults:  {d_rate:.4f} [{d_lo:.4f}, {d_hi:.4f}]")
    print(f"  candidate: {c_rate:.4f} [{c_lo:.4f}, {c_hi:.4f}]")
    print(f"  delta: {(c_rate - d_rate) * 100:+.2f} percentage points")


# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    p_sweep = sub.add_parser("sweep", help="univariate sweep vs a fixed opponent")
    p_sweep.add_argument("--param", choices=PARAM_NAMES, required=True)
    p_sweep.add_argument("--values", type=float, nargs="+",
                         help="default: this file's suggested sweep for the param")
    p_sweep.add_argument("--opponent", default="rand")
    p_sweep.add_argument("--numsim", type=int, default=20,
                         help="small by default -- this agent costs ~100ms/game")
    p_sweep.add_argument("--replicates", type=int, default=2)
    p_sweep.add_argument("--base-seed", type=int, default=1337)
    p_sweep.add_argument("--workers", type=int, default=None)
    p_sweep.add_argument("--plot", action="store_true")
    p_sweep.set_defaults(func=cmd_sweep)

    p_opt = sub.add_parser("optimize", help="differential-evolution search vs a fixed opponent")
    p_opt.add_argument("--params", choices=PARAM_NAMES, nargs="+",
                       help="default: search EFFICIENCY_PARAMS only (see module docstring) "
                            "-- pass explicit names (e.g. from BUDGET_PARAMS) to override")
    p_opt.add_argument("--opponent", default="borealis")
    p_opt.add_argument("--numsim", type=int, default=20,
                       help="games per match during search (small by default -- this runs "
                            "many times and costs ~100ms/game; increase for the final "
                            "validation instead)")
    p_opt.add_argument("--replicates", type=int, default=1,
                       help="seeds per evaluation (x2 seats = matches per evaluation)")
    p_opt.add_argument("--base-seed", type=int, default=1337)
    p_opt.add_argument("--opt-seed", type=int, default=42, help="DE's own RNG seed")
    p_opt.add_argument("--maxiter", type=int, default=4)
    p_opt.add_argument("--popsize", type=int, default=6)
    p_opt.add_argument("--workers", type=int, default=8,
                       help="thread pool size for per-evaluation matches")
    p_opt.add_argument("--validate-numsim", type=int, default=50)
    p_opt.add_argument("--validate-replicates", type=int, default=2)
    p_opt.set_defaults(func=cmd_optimize)

    p_self = sub.add_parser("selfplay", help="round-robin among named candidates, BT fit")
    p_self.add_argument("--candidates", nargs="+", required=True,
                        help="'defaults' and/or paths to JSON param files "
                             "(optimize's own output works directly)")
    p_self.add_argument("--numsim", type=int, default=20)
    p_self.add_argument("--replicates", type=int, default=2)
    p_self.add_argument("--base-seed", type=int, default=1337)
    p_self.add_argument("--workers", type=int, default=None)
    p_self.add_argument("--include-mirror", action="store_true",
                        help="also play each candidate against an identical copy of "
                             "itself (seat/mulligan noise floor, not needed for the BT fit)")
    p_self.set_defaults(func=cmd_selfplay)

    p_val = sub.add_parser("validate", help="one candidate vs the shipped defaults")
    p_val.add_argument("--candidate", required=True,
                       help="'defaults' or a path to a JSON param file -- pass "
                            "{\"rollout_determinize\": 0} to A/B determinization")
    p_val.add_argument("--opponent", default="rand")
    p_val.add_argument("--numsim", type=int, default=20)
    p_val.add_argument("--replicates", type=int, default=2)
    p_val.add_argument("--base-seed", type=int, default=1337)
    p_val.add_argument("--workers", type=int, default=None)
    p_val.set_defaults(func=cmd_validate)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
