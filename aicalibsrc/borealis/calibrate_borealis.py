#!/usr/bin/env python3
"""Calibration driver for the A3 Borealis AI agent's six tunable parameters
(BorealisParams -- see src/ai_strat/ai_strat_borealis.h).

Drives bin/calib_borealis, which links the game engine directly and prints
one CSV result line per invocation. All the actual game simulation happens
in the compiled C binary; this script only orchestrates subprocess calls and
aggregates/fits/searches the results.

Borealis is the Bradley-Terry rating scale's anchor (rating 50 by
definition, see the handout's Sec.1), so it has one property no other
agent's calibration needs to check: luna_value (lambda) must stay a single,
monotone strength dial -- win rate unimodal in lambda (handout Sec.8). Every
other parameter is a behavioural switch, not a strength knob (handout
Sec.3), so `optimize` here checks unimodality after searching, not static
per-parameter bands the way A2's `combo_bonus_threshold`/
`defend_probability_base` bands do -- there is no fixed "acceptable range"
for lambda to stay inside; what matters is the *shape* of win rate as a
function of it.

Four subcommands:

  sweep     Univariate diagnostic: hold every parameter at its default
            except one, vary that one, play vs a fixed --opponent (default
            "rand"), both seats, with binomial confidence intervals.
            `--param luna_value` (the default sweep grid is the handout's
            own Sec.13 headline check: 0.0, 0.25, 0.5, 1.0, 2.0) is the one
            that matters most -- win rate should be unimodal; a flat curve
            means the cost term isn't wired in.

  optimize  Black-box search (scipy.optimize.differential_evolution) over
            some or all six parameters, maximizing win rate against a fixed
            --opponent (default "combo" -- at the handout's defaults,
            Borealis is roughly at parity with A1 Value Based and actually
            *loses* to A2 Combo Threshold, so both have real headroom, and
            combo is the harder of the two). After finding a winner, re-runs
            a lambda sweep with the other five parameters fixed at the found
            values and fits a quadratic (same technique used for A1's
            VB_COST_FLOOR, doc/changelog.md 2026-08-21) to check it's still
            concave-down (unimodal) -- a result that breaks that shape
            should be reviewed and overridden by hand before shipping, not
            accepted just because it measured stronger (same protocol as
            A1's VB_DEFEND_THRESHOLD and A2's aggression_level precedents).

  selfplay  Round-robin among a small set of NAMED candidate parameter sets
            (six parameters make a full grid more feasible than A2's nine,
            but named candidates -- e.g. a handful of `optimize` outputs --
            stay simpler), both seat orders, --ai.a=borealis vs
            --ai.b=borealis. Reports a Bradley-Terry fit.

  validate  Compare one candidate parameter set against the shipped
            defaults, vs a chosen opponent, both seats.

Candidate parameter sets are given as JSON files (a full or partial
BorealisParams dict; missing fields fall back to the compiled defaults) or
the literal string "defaults". `optimize`'s own output file is directly
usable as a `selfplay`/`validate` candidate (it has a "best_params" key,
which these commands know to unwrap).

Examples:
  ./calibrate_borealis.py sweep --param luna_value --opponent rand \\
      --numsim 2000 --replicates 4 --plot
  ./calibrate_borealis.py optimize --opponent combo --numsim 500 \\
      --replicates 2 --maxiter 10 --popsize 10
  ./calibrate_borealis.py selfplay --candidates defaults results/optimize_combo.json
  ./calibrate_borealis.py validate --candidate results/optimize_combo.json --opponent rand
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

BINARY = Path(__file__).resolve().parent.parent.parent / "bin" / "calib_borealis"
RESULTS_DIR = Path(__file__).resolve().parent / "results"

# Declared struct order in BorealisParams -- must match parse_params() in
# calib_borealis.c.
PARAM_NAMES = [
    "luna_value", "tiebreak_epsilon", "hold_lethal_combos",
    "lethal_combo_bonus", "lethal_hold_ceiling", "min_hand_size_target",
]

# The handout's own defaults (Sec.3), ai_strat_borealis.c's
# BOREALIS_DEFAULTS -- not yet a measured optimum; that's what this file's
# `sweep`/`optimize` exist to find.
DEFAULTS = {
    "luna_value": 0.5, "tiebreak_epsilon": 0.5, "hold_lethal_combos": True,
    "lethal_combo_bonus": 16, "lethal_hold_ceiling": 25, "min_hand_size_target": 4,
}

# Default search space for `optimize` and default sweep grids for `sweep`.
# luna_value's upper bound was widened from an initial 3.0 to 6.0 after a
# manual sweep vs `combo` (this file, 2026-08-23) found the real peak much
# higher than the handout's own default guess (0.5) or its Sec.13 sweep
# grid's max (2.0) -- win rate vs `combo` climbs from 36.5% at lambda=0 to a
# peak of ~61-62% around lambda=4.0-4.5 (quadratic fit R^2=0.976, vertex
# 4.098) before declining again at 6.0 (40.9%). 3.0 would have silently
# capped `optimize` short of that peak.
BOUNDS = {
    "luna_value": (0.0, 6.0),
    "tiebreak_epsilon": (0.0, 3.0),
    "hold_lethal_combos": (0, 1),
    "lethal_combo_bonus": (10, 24),
    "lethal_hold_ceiling": (10, 45),
    "min_hand_size_target": (3, 6),
}

SWEEP_DEFAULTS = {
    "luna_value": [0.0, 0.25, 0.5, 1.0, 2.0],  # handout Sec.13's headline check
    "tiebreak_epsilon": [0.0, 0.25, 0.5, 1.0, 2.0],
    "hold_lethal_combos": [0, 1],
    "lethal_combo_bonus": [10, 13, 16, 19, 24],
    "lethal_hold_ceiling": [10, 20, 25, 30, 45],
    "min_hand_size_target": [3, 4, 5, 6],
}

# Extra grid used only by optimize's post-search unimodality re-check
# (Sec.8) -- wider than SWEEP_DEFAULTS["luna_value"] so a found optimum near
# either end of the handout's own sweep still has room to show curvature.
# Matches BOUNDS["luna_value"]'s span (see that dict's comment on why 3.0
# wasn't wide enough).
LAMBDA_UNIMODALITY_GRID = [0.0, 1.0, 2.0, 3.0, 4.0, 4.5, 5.0, 6.0]

# Parameters that must round to int/bool before being handed to the C
# harness -- see parse_params() in calib_borealis.c.
_INT_PARAMS = {"lethal_combo_bonus", "lethal_hold_ceiling", "min_hand_size_target"}


def coerce_param(name, value):
    # bool()/int() casts are load-bearing, not stylistic: DE hands back
    # numpy.float64, and numpy.bool_/numpy.int64 (e.g. from `value >= 0.5`)
    # aren't JSON-serializable -- json.dump() on the optimize output would
    # fail otherwise.
    if name == "hold_lethal_combos":
        return bool(value >= 0.5) if not isinstance(value, bool) else value
    if name in _INT_PARAMS:
        return int(round(value))
    return float(value)


def params_to_args(p):
    return [str(coerce_param(name, p[name])) if name != "hold_lethal_combos"
            else ("1" if coerce_param(name, p[name]) else "0")
            for name in PARAM_NAMES]


def merge_params(overrides):
    p = dict(DEFAULTS)
    p.update(overrides)
    return {name: coerce_param(name, p[name]) for name in PARAM_NAMES}


def load_candidate(spec):
    """spec is either the literal 'defaults' or a path to a JSON file holding
    a full/partial BorealisParams dict, or `optimize`'s own output (which
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
    """One call to bin/calib_borealis. Safe to run in a worker.

    **tags are arbitrary caller bookkeeping (e.g. sweep's `_value`,
    selfplay's `_i`/`_j`) that get merged straight into the returned dict
    instead of being tracked in a separate same-order list -- run_many()'s
    ProcessPoolExecutor + as_completed() returns results in COMPLETION
    order, not submission order, so a caller that stashed metadata in a
    parallel list and reattached it by position after the fact would
    silently pair each match's win/loss counts with the WRONG tag under
    concurrency (confirmed: --workers 1 forces sequential/submission-order
    completion and the corruption vanishes). Tagging each result at the
    source instead of after the fact makes this whole bug class impossible.
    """
    if not BINARY.exists():
        raise FileNotFoundError(f"{BINARY} not found -- run `make calib_borealis` first")

    args = [str(BINARY), str(numsim), str(seed), agent_a, agent_b,
            *params_to_args(params_a), *params_to_args(params_b)]
    result = subprocess.run(args, capture_output=True, text=True, check=True)
    row = result.stdout.strip().split(",")
    # Params are echoed back in the CSV but we already have them as
    # params_a/params_b -- only the trailing win/draw counts matter.
    return {
        "numsim": int(row[0]), "seed": int(row[1]),
        "agent_a": row[2], "agent_b": row[3],
        "wins_a": int(row[-3]), "wins_b": int(row[-2]), "draws": int(row[-1]),
        **tags,
    }


def run_many(jobs, max_workers=None, quiet=False):
    """jobs: list of kwargs dicts for run_match(). One-shot batch: spins up
    its own process pool. For repeated small batches (as `optimize` needs
    per generation), use run_many_threaded() with a persistent pool instead
    -- recreating a ProcessPoolExecutor per call is too slow for that."""
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
    waiting on the child. Meant for `optimize`, which calls this many times
    with a handful of jobs each; a fresh process pool per call would
    dominate runtime with startup overhead."""
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


def borealis_win_rate(df):
    borealis_in_a = df["agent_a"] == "borealis"
    wins = int(np.where(borealis_in_a, df["wins_a"], df["wins_b"]).sum())
    n = int((df["wins_a"] + df["wins_b"] + df["draws"]).sum())
    return wins, n


# ---------------------------------------------------------------------------
# sweep: one param varied, borealis vs a fixed opponent, both seats
# ---------------------------------------------------------------------------

def build_sweep_jobs(param, values, opponent, numsim, seeds):
    jobs = []
    for v in values:
        p = merge_params({param: v})
        for seed in seeds:
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="borealis", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS), _value=v, _borealis_in_a=True))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="borealis",
                             params_a=dict(DEFAULTS), params_b=p, _value=v, _borealis_in_a=False))
    return jobs


def cmd_sweep(args):
    values = args.values or SWEEP_DEFAULTS[args.param]
    seeds = replicate_seeds(args.base_seed, args.replicates)
    jobs = build_sweep_jobs(args.param, values, args.opponent, args.numsim, seeds)

    print(f"Running {len(jobs)} matches "
         f"({len(values)} values x {args.replicates} replicates x 2 seats, "
         f"vs {args.opponent})...", file=sys.stderr)

    # _value/_borealis_in_a ride through run_match() as **tags (see its
    # docstring) and come back attached to the right row regardless of
    # completion order -- no separate same-order list to reattach by hand.
    df = run_many(jobs, max_workers=args.workers)
    df["borealis_wins"] = np.where(df["_borealis_in_a"], df["wins_a"], df["wins_b"])
    df["n"] = df["wins_a"] + df["wins_b"] + df["draws"]

    grouped = df.groupby("_value").agg(wins=("borealis_wins", "sum"), n=("n", "sum")).reset_index()
    grouped["win_rate"] = grouped["wins"] / grouped["n"]
    grouped[["ci_lo", "ci_hi"]] = grouped.apply(
        lambda r: pd.Series(wilson_ci(r["wins"], r["n"])), axis=1)
    grouped = grouped.rename(columns={"_value": args.param}).sort_values(args.param)

    print(f"\nSweep of {args.param} (borealis vs {args.opponent}, both seats, "
         f"{args.numsim * args.replicates * 2} games/value):")
    print(grouped.to_string(index=False, float_format=lambda x: f"{x:.4f}"))

    if args.param == "luna_value":
        is_unimodal, implied_optimum, r_squared = check_unimodal_fit(
            grouped[args.param].to_numpy(), grouped["win_rate"].to_numpy())
        print(f"\nUnimodality check (quadratic fit, R^2={r_squared:.3f}): "
             f"{'unimodal' if is_unimodal else 'NOT unimodal -- flat/convex, review'}")
        print(f"Implied optimum (fit vertex): {implied_optimum:.3f}")

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
    ax.set_title(f"Borealis: {param} sweep (95% Wilson CI)")
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    print(f"Saved: {out_path}")


# ---------------------------------------------------------------------------
# optimize: differential evolution vs a fixed opponent, unimodality re-check
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
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="borealis", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="borealis",
                             params_a=dict(DEFAULTS), params_b=p))
        df = run_many_threaded(jobs, executor)
        wins, n = borealis_win_rate(df)
        rate = wins / n if n else 0.0
        progress["evals"] += 1
        print(f"\r  eval {progress['evals']}: win_rate={rate:.4f}",
             end="", file=sys.stderr, flush=True)
        return -rate  # differential_evolution minimizes
    return objective


def check_unimodal_fit(lambda_values, win_rates):
    """Fits a quadratic to (lambda, win_rate) -- same technique used for A1's
    VB_COST_FLOOR calibration (doc/changelog.md, 2026-08-21). A concave-down
    fit (leading coefficient < 0) means win rate rises then falls, i.e. has
    a single interior maximum -- the property that makes Borealis usable as
    a calibratable benchmark: detune it in either direction from that peak
    and it stays coherent (handout Sec.8). Returns
    (is_unimodal, implied_optimum_lambda, r_squared)."""
    lambda_values = np.asarray(lambda_values, dtype=float)
    win_rates = np.asarray(win_rates, dtype=float)
    a, b, c = np.polyfit(lambda_values, win_rates, 2)
    fitted = np.polyval((a, b, c), lambda_values)
    ss_res = np.sum((win_rates - fitted) ** 2)
    ss_tot = np.sum((win_rates - np.mean(win_rates)) ** 2)
    r_squared = float(1 - ss_res / ss_tot) if ss_tot > 0 else float("nan")
    # bool()/float() casts are load-bearing, not stylistic -- see
    # coerce_param()'s comment: numpy.bool_/numpy.float64 (from polyfit's
    # numpy inputs) aren't JSON-serializable, and cmd_optimize() dumps this
    # return value straight into its results JSON.
    is_unimodal = bool(a < 0)
    implied_optimum = float(-b / (2 * a)) if a != 0 else float("nan")
    return is_unimodal, implied_optimum, r_squared


def recheck_lambda_unimodality(best_params, opponent, numsim, replicates, base_seed, workers):
    """Re-sweeps luna_value over LAMBDA_UNIMODALITY_GRID with every other
    parameter held at best_params' found values, then fits check_unimodal_fit()
    -- Sec.8's property re-verified at the optimizer's chosen point, not just
    at the handout's untuned defaults."""
    seeds = replicate_seeds(base_seed + 200000, replicates)
    rates = []
    print(f"\nRe-checking lambda unimodality with the other {len(PARAM_NAMES) - 1} "
         f"parameter(s) fixed at the found values...", file=sys.stderr)
    for lam in LAMBDA_UNIMODALITY_GRID:
        p = dict(best_params)
        p["luna_value"] = lam
        jobs = []
        for seed in seeds:
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="borealis", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="borealis",
                             params_a=dict(DEFAULTS), params_b=p))
        df = run_many(jobs, max_workers=workers, quiet=True)
        wins, n = borealis_win_rate(df)
        rate = wins / n if n else 0.0
        rates.append(rate)
        print(f"  lambda={lam}: win_rate={rate:.4f}", file=sys.stderr)

    return check_unimodal_fit(LAMBDA_UNIMODALITY_GRID, rates)


def cmd_optimize(args):
    free_names = args.params or list(PARAM_NAMES)
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

    is_unimodal, implied_optimum, r_squared = recheck_lambda_unimodality(
        best_params, args.opponent, args.numsim, args.replicates, args.base_seed, args.workers)
    print(f"\nLambda unimodality check (quadratic fit, R^2={r_squared:.3f}):")
    print(f"  {'unimodal (concave down)' if is_unimodal else 'NOT unimodal -- flat or convex fit'}")
    print(f"  implied optimum lambda (fit vertex): {implied_optimum:.3f}")
    flagged = not is_unimodal
    if flagged:
        print("\n*** Lambda does not fit a single-peaked curve at these other-parameter "
             "values. Per handout Sec.8, this breaks the property that makes Borealis "
             "usable as a Bradley-Terry anchor -- do not ship this combination as-is; "
             "either fix the other parameters by hand and re-search, or widen/shift the "
             "grid before concluding it truly isn't unimodal. ***")

    print(f"\nRe-validating winner with more games "
         f"({args.validate_replicates * 2} matches x {args.validate_numsim} games)...",
         file=sys.stderr)
    val_seeds = replicate_seeds(args.base_seed + 100000, args.validate_replicates)
    jobs = []
    for seed in val_seeds:
        jobs.append(dict(numsim=args.validate_numsim, seed=seed, agent_a="borealis",
                         agent_b=args.opponent, params_a=best_params, params_b=dict(DEFAULTS)))
        jobs.append(dict(numsim=args.validate_numsim, seed=seed, agent_a=args.opponent,
                         agent_b="borealis", params_a=dict(DEFAULTS), params_b=best_params))
    df = run_many(jobs, max_workers=args.workers)
    wins, n = borealis_win_rate(df)
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
            "lambda_unimodal": is_unimodal, "lambda_implied_optimum": implied_optimum,
            "lambda_fit_r_squared": r_squared,
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
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="borealis", agent_b="borealis",
                             params_a=params_map[names[i]], params_b=params_map[names[j]],
                             _i=i, _j=j))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="borealis", agent_b="borealis",
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
    # Per-candidate totals, tracked directly from wins_a/wins_b -- NOT
    # rederived from the wins/games matrices via a transpose trick (see A2's
    # calibrate_combo_threshold.py for the bug that pattern caused).
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

    strength = bradley_terry_fit(n, wins, games)
    summary = pd.DataFrame({
        "candidate": names,
        "bt_strength": strength,
        "overall_win_rate": np.divide(total_wins, total_games,
                                      out=np.zeros(n), where=total_games > 0),
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

    # _i/_j ride through run_match() as **tags -- see its docstring.
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
# validate: one candidate vs the shipped defaults, vs a chosen opponent
# ---------------------------------------------------------------------------

def cmd_validate(args):
    candidate = load_candidate(args.candidate)
    seeds = replicate_seeds(args.base_seed, args.replicates)

    def vs_opponent(params):
        jobs = []
        for seed in seeds:
            jobs.append(dict(numsim=args.numsim, seed=seed, agent_a="borealis",
                             agent_b=args.opponent, params_a=params, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=args.numsim, seed=seed, agent_a=args.opponent,
                             agent_b="borealis", params_a=dict(DEFAULTS), params_b=params))
        df = run_many(jobs, max_workers=args.workers)
        wins, n = borealis_win_rate(df)
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
    p_sweep.add_argument("--numsim", type=int, default=2000)
    p_sweep.add_argument("--replicates", type=int, default=4)
    p_sweep.add_argument("--base-seed", type=int, default=1337)
    p_sweep.add_argument("--workers", type=int, default=None)
    p_sweep.add_argument("--plot", action="store_true")
    p_sweep.set_defaults(func=cmd_sweep)

    p_opt = sub.add_parser("optimize", help="differential-evolution search vs a fixed opponent")
    p_opt.add_argument("--params", choices=PARAM_NAMES, nargs="+",
                       help="default: search all six")
    p_opt.add_argument("--opponent", default="combo")
    p_opt.add_argument("--numsim", type=int, default=500,
                       help="games per match during search (keep modest -- this runs "
                            "many times); increase for the final validation instead")
    p_opt.add_argument("--replicates", type=int, default=2,
                       help="seeds per evaluation (x2 seats = matches per evaluation)")
    p_opt.add_argument("--base-seed", type=int, default=1337)
    p_opt.add_argument("--opt-seed", type=int, default=42, help="DE's own RNG seed")
    p_opt.add_argument("--maxiter", type=int, default=10)
    p_opt.add_argument("--popsize", type=int, default=10)
    p_opt.add_argument("--workers", type=int, default=8,
                       help="thread pool size for per-evaluation matches")
    p_opt.add_argument("--validate-numsim", type=int, default=5000)
    p_opt.add_argument("--validate-replicates", type=int, default=4)
    p_opt.set_defaults(func=cmd_optimize)

    p_self = sub.add_parser("selfplay", help="round-robin among named candidates, BT fit")
    p_self.add_argument("--candidates", nargs="+", required=True,
                        help="'defaults' and/or paths to JSON param files "
                             "(optimize's own output works directly)")
    p_self.add_argument("--numsim", type=int, default=2000)
    p_self.add_argument("--replicates", type=int, default=4)
    p_self.add_argument("--base-seed", type=int, default=1337)
    p_self.add_argument("--workers", type=int, default=None)
    p_self.add_argument("--include-mirror", action="store_true",
                        help="also play each candidate against an identical copy of "
                             "itself (seat/mulligan noise floor, not needed for the BT fit)")
    p_self.set_defaults(func=cmd_selfplay)

    p_val = sub.add_parser("validate", help="one candidate vs the shipped defaults")
    p_val.add_argument("--candidate", required=True,
                       help="'defaults' or a path to a JSON param file")
    p_val.add_argument("--opponent", default="rand")
    p_val.add_argument("--numsim", type=int, default=2000)
    p_val.add_argument("--replicates", type=int, default=4)
    p_val.add_argument("--base-seed", type=int, default=1337)
    p_val.add_argument("--workers", type=int, default=None)
    p_val.set_defaults(func=cmd_validate)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
