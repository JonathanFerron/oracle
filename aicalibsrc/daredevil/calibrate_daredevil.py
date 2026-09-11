#!/usr/bin/env python3
"""Calibration driver for A15 Risk Threshold ("The Daredevil")'s 5 free
parameters plus 1 ablation switch (A15Params -- see src/ai_strat/ai_strat_a15.h).

Drives bin/calib_daredevil, which links the game engine directly and prints
one CSV result line per invocation. All the actual game simulation happens
in the compiled C binary; this script only orchestrates subprocess calls
and aggregates/fits/searches the results. Structure and internals (the
subcommand shape, run_match's tags-at-the-source fix, the seed-offset
convention, --print-defaults-sourced DEFAULTS) are carried over from
aicalibsrc/hbt2ply/calibrate_hbt2ply.py, this project's simplest precedent
(the smallest free-parameter count before this agent).

DEFAULTS below is read once, at import time, from `bin/calib_daredevil
--print-defaults`, so it can never drift from the shipped compiled values.

Unlike an inherited-parameter agent (A9/A13/A14), NONE of this agent's 6
fields are pinned: the whole design (ideas/A15 .../about.md, folded into
doc/ai_agents.md's A15 section) is a from-scratch rule transcription with
exactly 5 free calibration targets (R4's defense_loss_threshold, R8's
endgame_q1-q4) plus one structural ablation switch (endgame_enabled) --
there is no inherited "identity" to protect and so no --identity-safe mode,
matching the hbt2ply/carto/puct precedent for a from-scratch or
narrow-scope agent's driver.

A15's own defining trait (see ai_strat_a15.h) is that it is a
TRANSCRIPTION of Jonathan's real-table play, not a design aimed at a rating
target -- so unlike every prior agent, there is no "personality" for
check_personality_flags() to protect against erosion. The one thing worth
flagging here is different: whether R8 (the speculative, approximate half
of this agent -- see ai_strat_a15_prob.c's normal-approximation comment)
is actually contributing, or whether endgame_enabled=False (the ablation)
matches or beats endgame_enabled=True at the same defense_loss_threshold.

Four subcommands, same as every other driver:

  sweep     Univariate diagnostic: hold every parameter at its default
            except one, vary that one, play vs a fixed --opponent (default
            "rand"), both seats, with binomial confidence intervals.

  optimize  Black-box search (scipy.optimize.differential_evolution) over
            some or all free parameters, maximizing win rate against a
            fixed --opponent (default "borealis" -- the rating-50 anchor).

  selfplay  Round-robin among a small set of NAMED candidate parameter sets,
            both seat orders, --ai.a=daredevil vs --ai.b=daredevil. Reports
            a Bradley-Terry fit.

  validate  Compare one candidate parameter set against the shipped
            defaults, vs a chosen opponent, both seats.

Candidate parameter sets are given as JSON files (a full or partial
A15Params dict; missing fields fall back to the compiled defaults) or the
literal string "defaults". `optimize`'s own output file is directly usable
as a `selfplay`/`validate` candidate (it has a "best_params" key, which
these commands know to unwrap).

Examples:
  ./calibrate_daredevil.py sweep --param defense_loss_threshold --opponent borealis \\
      --numsim 2000 --replicates 4 --plot
  ./calibrate_daredevil.py optimize --opponent borealis \\
      --numsim 500 --replicates 2 --maxiter 15 --popsize 12
  ./calibrate_daredevil.py selfplay --candidates defaults results/optimize_borealis.json
  ./calibrate_daredevil.py validate --candidate results/optimize_borealis.json --opponent hbt
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

BINARY = Path(__file__).resolve().parent.parent.parent / "bin" / "calib_daredevil"
RESULTS_DIR = Path(__file__).resolve().parent / "results"

# Declared struct order in A15Params (ai_strat_a15.h) -- must match
# parse_params() in calib_daredevil.c.
PARAM_NAMES = [
    "defense_loss_threshold",
    "endgame_q1", "endgame_q2", "endgame_q3", "endgame_q4",
    "endgame_enabled",
]

_INT_PARAMS = {"endgame_enabled"}  # printed/parsed as 0/1, not a float

# No pinned fields -- see module docstring. All 6 are FREE_PARAM_NAMES.
PINNED_PARAM_NAMES = set()
FREE_PARAM_NAMES = [n for n in PARAM_NAMES if n not in PINNED_PARAM_NAMES]

# R4's threshold plus R8's 4 horizon thresholds are real behaviour dials.
# endgame_enabled is a boolean ablation switch, not a continuous dial --
# `optimize` (differential_evolution needs a continuous search space) never
# searches it by default; use two separate `validate`/`selfplay` runs (one
# per value) to answer "does R8 help at all" instead.
OPTIMIZE_PARAM_NAMES = ["defense_loss_threshold", "endgame_q1", "endgame_q2",
                        "endgame_q3", "endgame_q4"]


def _load_defaults_from_binary():
    if not BINARY.exists():
        # Fall back to the compiled-in values (ai_strat_a15.c's
        # A15_DEFAULTS) so --help and argument parsing still work before
        # `make calib_daredevil` has run.
        return {
            "defense_loss_threshold": 0.30,
            "endgame_q1": 0.5, "endgame_q2": 0.5, "endgame_q3": 0.5, "endgame_q4": 0.5,
            "endgame_enabled": True,
        }
    result = subprocess.run([str(BINARY), "--print-defaults"],
                            capture_output=True, text=True, check=True)
    return json.loads(result.stdout)


DEFAULTS = _load_defaults_from_binary()

# Search space for `optimize` and default sweep grids for `sweep`.
BOUNDS = {
    "defense_loss_threshold": (0.0, 1.0),
    "endgame_q1": (0.0, 1.0),
    "endgame_q2": (0.0, 1.0),
    "endgame_q3": (0.0, 1.0),
    "endgame_q4": (0.0, 1.0),
}

SWEEP_DEFAULTS = {
    # The design doc's own suggested starting point for p is ~0.30; sweep
    # around it plus the extremes (never defend / always defend once any
    # real chance of death exists).
    "defense_loss_threshold": [0.1, 0.2, 0.3, 0.5, 0.7, 0.9],
    "endgame_q1": [0.0, 0.25, 0.5, 0.75, 1.0],
    "endgame_q2": [0.0, 0.25, 0.5, 0.75, 1.0],
    "endgame_q3": [0.0, 0.25, 0.5, 0.75, 1.0],
    "endgame_q4": [0.0, 0.25, 0.5, 0.75, 1.0],
    # Ablation, not a real sweep -- see build_sweep_jobs()'s bool handling.
    "endgame_enabled": [0, 1],
}


def coerce_param(name, value):
    if name == "endgame_enabled":
        return bool(int(round(float(value))))
    if name in _INT_PARAMS:
        return int(round(value))
    return float(value)


def params_to_args(p):
    out = []
    for name in PARAM_NAMES:
        v = p[name]
        out.append(str(int(bool(v))) if name == "endgame_enabled" else str(coerce_param(name, v)))
    return out


def merge_params(overrides):
    p = dict(DEFAULTS)
    p.update(overrides)
    return {name: coerce_param(name, p[name]) for name in PARAM_NAMES}


def load_candidate(spec):
    """spec is either the literal 'defaults' or a path to a JSON file holding
    a full/partial A15Params dict, or `optimize`'s own output (which nests
    it under "best_params")."""
    if spec == "defaults":
        return dict(DEFAULTS)
    with open(spec) as f:
        data = json.load(f)
    return merge_params(data.get("best_params", data))


# ---------------------------------------------------------------------------
# Core: one match, many matches, confidence intervals
# ---------------------------------------------------------------------------

def run_match(numsim, seed, agent_a, agent_b, params_a, params_b, **tags):
    """One call to bin/calib_daredevil. Safe to run in a worker.

    **tags are arbitrary caller bookkeeping (e.g. sweep's `_value`,
    selfplay's `_i`/`_j`) merged straight into the returned dict instead of
    being tracked in a separate same-order list -- run_many()'s
    ProcessPoolExecutor + as_completed() returns results in COMPLETION
    order, not submission order, so tagging at the source (rather than
    reattaching by list position afterward) is what keeps this correct
    under real parallelism.
    """
    if not BINARY.exists():
        raise FileNotFoundError(f"{BINARY} not found -- run `make calib_daredevil` first")

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


def daredevil_win_rate(df):
    in_a = df["agent_a"] == "daredevil"
    wins = int(np.where(in_a, df["wins_a"], df["wins_b"]).sum())
    n = int((df["wins_a"] + df["wins_b"] + df["draws"]).sum())
    return wins, n


# ---------------------------------------------------------------------------
# sweep: one param varied, daredevil vs a fixed opponent, both seats
# ---------------------------------------------------------------------------

def build_sweep_jobs(param, values, opponent, numsim, seeds):
    jobs = []
    for v in values:
        p = merge_params({param: v})
        for seed in seeds:
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="daredevil", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS), _value=v, _daredevil_in_a=True))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="daredevil",
                             params_a=dict(DEFAULTS), params_b=p, _value=v, _daredevil_in_a=False))
    return jobs


def cmd_sweep(args):
    values = args.values or SWEEP_DEFAULTS[args.param]
    seeds = replicate_seeds(args.base_seed, args.replicates)
    jobs = build_sweep_jobs(args.param, values, args.opponent, args.numsim, seeds)

    print(f"Running {len(jobs)} matches "
         f"({len(values)} values x {args.replicates} replicates x 2 seats, "
         f"vs {args.opponent})...", file=sys.stderr)

    df = run_many(jobs, max_workers=args.workers)
    df["daredevil_wins"] = np.where(df["_daredevil_in_a"], df["wins_a"], df["wins_b"])
    df["n"] = df["wins_a"] + df["wins_b"] + df["draws"]

    grouped = df.groupby("_value").agg(wins=("daredevil_wins", "sum"), n=("n", "sum")).reset_index()
    grouped["win_rate"] = grouped["wins"] / grouped["n"]
    grouped[["ci_lo", "ci_hi"]] = grouped.apply(
        lambda r: pd.Series(wilson_ci(r["wins"], r["n"])), axis=1)
    grouped = grouped.rename(columns={"_value": args.param}).sort_values(args.param)

    print(f"\nSweep of {args.param} (daredevil vs {args.opponent}, both seats, "
         f"{args.numsim * args.replicates * 2} games/value):")
    print(grouped.to_string(index=False, float_format=lambda x: f"{x:.4f}"))

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
    ax.set_title(f"The Daredevil: {param} sweep (95% Wilson CI)")
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    print(f"Saved: {out_path}")


# ---------------------------------------------------------------------------
# optimize: differential evolution vs a fixed opponent, personality re-check
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
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="daredevil", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="daredevil",
                             params_a=dict(DEFAULTS), params_b=p))
        df = run_many_threaded(jobs, executor)
        wins, n = daredevil_win_rate(df)
        rate = wins / n if n else 0.0
        progress["evals"] += 1
        print(f"\r  eval {progress['evals']}: win_rate={rate:.4f}",
             end="", file=sys.stderr, flush=True)
        return -rate  # differential_evolution minimizes
    return objective


def check_personality_flags(best_params):
    """Returns a list of human-readable warning strings for anything an
    optimizer result should not silently ship with. Never auto-rejects --
    flags for review, same policy as every other agent's precedent.

    Unlike every prior agent, A15 has no designed "personality" to protect
    (ai_strat_a15.h: it's a transcription, not a design). What's worth
    flagging instead is whether the speculative, approximate half of this
    agent (R8, ai_strat_a15_prob.c's normal-approximation P(finish within N))
    survived calibration at all, or whether it's been pushed to a corner
    that's functionally equivalent to disabling it -- the same class of
    check A9's reply_trust/A13's hplus_trust got, generalised to 4 dials.

    NOTE (2026-09-10, post-redesign): endgame_q1 is NOT expected to land near
    the other three -- it's checked LAST (find_triggering_horizon(),
    ai_strat_a15_endgame.c) as a loose "don't rule out pursuing a win" catch-
    all, while q2/q3/q4 are genuine confidence bars. The shipped defaults
    themselves are q1=0.01, q2=0.78, q3=0.75, q4=0.69 -- do not read a low q1
    next to high q2-q4 as a personality flag on its own; that asymmetry is
    the calibrated, measured-load-bearing shape of this mechanism, not a
    warning sign."""
    flags = []

    q_names = ["endgame_q1", "endgame_q2", "endgame_q3", "endgame_q4"]
    if all(best_params[n] >= 0.95 for n in q_names):
        flags.append("All four endgame_q1-4 optimized to >=0.95 -- R8 would almost never "
                     "fire in a real game; functionally equivalent to endgame_enabled=False. "
                     "Compare against a direct endgame_enabled=False validate() run.")

    if best_params["defense_loss_threshold"] >= 0.95:
        flags.append(f"defense_loss_threshold optimized to "
                     f"{best_params['defense_loss_threshold']:.4f} (>=0.95) -- R4 would "
                     f"almost never defend at all, a large personality shift from the "
                     f"design doc's own ~30% starting estimate. Worth a manual read before "
                     f"shipping even if it measures stronger.")

    return flags


def cmd_optimize(args):
    free_names = [n for n in (args.params or list(OPTIMIZE_PARAM_NAMES))
                 if n not in PINNED_PARAM_NAMES]
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
        print("\n*** Personality flags (review before shipping, do not auto-ship past these): ***")
        for flag in personality_flags:
            print(f"  - {flag}")

    print(f"\nRe-validating winner with more games "
         f"({args.validate_replicates * 2} matches x {args.validate_numsim} games)...",
         file=sys.stderr)
    val_seeds = replicate_seeds(args.base_seed + 100000, args.validate_replicates)
    jobs = []
    for seed in val_seeds:
        jobs.append(dict(numsim=args.validate_numsim, seed=seed, agent_a="daredevil",
                         agent_b=args.opponent, params_a=best_params, params_b=dict(DEFAULTS)))
        jobs.append(dict(numsim=args.validate_numsim, seed=seed, agent_a=args.opponent,
                         agent_b="daredevil", params_a=dict(DEFAULTS), params_b=best_params))
    df = run_many(jobs, max_workers=args.workers)
    wins, n = daredevil_win_rate(df)
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
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="daredevil", agent_b="daredevil",
                             params_a=params_map[names[i]], params_b=params_map[names[j]],
                             _i=i, _j=j))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="daredevil", agent_b="daredevil",
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
# validate: one candidate vs the shipped defaults, vs a chosen opponent
# ---------------------------------------------------------------------------

def cmd_validate(args):
    candidate = load_candidate(args.candidate)
    seeds = replicate_seeds(args.base_seed, args.replicates)

    def vs_opponent(params):
        jobs = []
        for seed in seeds:
            jobs.append(dict(numsim=args.numsim, seed=seed, agent_a="daredevil",
                             agent_b=args.opponent, params_a=params, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=args.numsim, seed=seed, agent_a=args.opponent,
                             agent_b="daredevil", params_a=dict(DEFAULTS), params_b=params))
        df = run_many(jobs, max_workers=args.workers)
        wins, n = daredevil_win_rate(df)
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
    p_opt.add_argument("--params", choices=OPTIMIZE_PARAM_NAMES, nargs="+",
                       help="default: all 5 continuous dials (OPTIMIZE_PARAM_NAMES) -- "
                            "endgame_enabled is a boolean ablation switch, not searched "
                            "here; use two validate()/selfplay() runs instead")
    p_opt.add_argument("--opponent", default="borealis")
    p_opt.add_argument("--numsim", type=int, default=500,
                       help="games per match during search (keep modest -- this runs "
                            "many times); increase for the final validation instead")
    p_opt.add_argument("--replicates", type=int, default=2,
                       help="seeds per evaluation (x2 seats = matches per evaluation)")
    p_opt.add_argument("--base-seed", type=int, default=1337)
    p_opt.add_argument("--opt-seed", type=int, default=42, help="DE's own RNG seed")
    p_opt.add_argument("--maxiter", type=int, default=15)
    p_opt.add_argument("--popsize", type=int, default=12)
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
