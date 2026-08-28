#!/usr/bin/env python3
"""Calibration driver for the A2 Combo Threshold AI agent's nine tunable
parameters (ComboThresholdParams -- see src/ai_strat/ai_strat_combo_threshold.h).

Drives bin/calib_combo_threshold, which links the game engine directly and
prints one CSV result line per invocation. All the actual game simulation
happens in the compiled C binary; this script only orchestrates subprocess
calls and aggregates/fits/searches the results.

Four subcommands:

  sweep     Univariate diagnostic: hold every parameter at its default
            except one, vary that one, play vs a fixed --opponent (default
            "rand"), both seats, with binomial confidence intervals. Use
            this first to sanity-check that a parameter actually moves the
            needle before spending time on `optimize`.

  optimize  Black-box search (scipy.optimize.differential_evolution) over
            some or all of the nine parameters, maximizing win rate against
            a fixed --opponent (default "value" -- vs-Random saturates near
            a ceiling almost regardless of parameters, per Phase 2
            measurement, so it can't discriminate candidates the way
            vs-value can). Reports the winner, re-validates it with more
            games, and flags any parameter that drifted outside its
            handout-specified personality band (defend_probability_base,
            defend_damage_threshold) or otherwise looks like a character
            change rather than a strength gain (combo_bonus_threshold <= 5,
            combo_weight ~ 0) -- inspect these by hand before shipping,
            the same way A1's VB_DEFEND_THRESHOLD optimum was overridden to
            protect its character (doc/changelog.md, 2026-08-21).

  selfplay  Round-robin among a small set of NAMED candidate parameter sets
            (not a full 9-dimensional grid -- combinatorially infeasible),
            both seat orders, --ai.a=combo vs --ai.b=combo. Reports a
            Bradley-Terry fit (relative strength per candidate). Useful to
            compare a handful of `optimize` outputs, or the defaults against
            one or two hand-picked alternatives.

  validate  Compare one candidate parameter set against the shipped defaults,
            vs a chosen opponent, both seats -- the direct "did this help"
            answer.

Candidate parameter sets are given as JSON files (a full or partial
ComboThresholdParams dict; missing fields fall back to the compiled
defaults) or the literal string "defaults". `optimize`'s own output file is
directly usable as a `selfplay`/`validate` candidate (it has a "best_params"
key, which these commands know to unwrap).

Examples:
  ./calibrate_combo_threshold.py sweep --param aggression_level --numsim 2000 --replicates 4
  ./calibrate_combo_threshold.py optimize --opponent value --numsim 500 \\
      --replicates 2 --maxiter 10 --popsize 10
  ./calibrate_combo_threshold.py selfplay --candidates defaults results/optimize_value.json
  ./calibrate_combo_threshold.py validate --candidate results/optimize_value.json --opponent rand
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

BINARY = Path(__file__).resolve().parent.parent.parent / "bin" / "calib_combo_threshold"
RESULTS_DIR = Path(__file__).resolve().parent / "results"

# Declared struct order in ComboThresholdParams -- must match parse_params()
# in calib_combo_threshold.c.
PARAM_NAMES = [
    "aggression_level", "combo_bonus_threshold", "combo3_bonus_threshold",
    "combo_weight", "min_play_score_floor", "defend_probability_base",
    "defend_damage_threshold", "min_hand_size_target", "save_big_combos_for_lethal",
]

def _load_defaults_from_binary():
    if not BINARY.exists():
        # Fall back to the compiled-in values in ai_strat_combo_threshold.c's
        # CT_DEFAULTS, so --help and argument parsing still work before
        # `make calib_combo_threshold` has run. Any real command still hits
        # run_match()'s own existence check.
        return {
            "aggression_level": 1.3, "combo_bonus_threshold": 11,
            "combo3_bonus_threshold": 16, "combo_weight": 2.3626,
            "min_play_score_floor": 2.1184, "defend_probability_base": 0.4085,
            "defend_damage_threshold": 8, "min_hand_size_target": 5,
            "save_big_combos_for_lethal": True,
        }
    result = subprocess.run([str(BINARY), "--print-defaults"],
                            capture_output=True, text=True, check=True)
    return json.loads(result.stdout)


# Read from the compiled binary (added 2026-08-28) rather than a hardcoded
# dict, which had drifted from the shipped CT_DEFAULTS on 7 of 9 fields (most
# notably combo_weight: 1.0 here vs the actual shipped 2.3626) -- see
# doc/oracle_todo.md's Calibration section.
DEFAULTS = _load_defaults_from_binary()

# Handout §6.3 ranges -- default search space for `optimize` and default
# sweep grids for `sweep`.
BOUNDS = {
    "aggression_level": (1.0, 3.0),
    "combo_bonus_threshold": (7, 12),
    "combo3_bonus_threshold": (14, 24),
    "combo_weight": (0.0, 3.0),
    "min_play_score_floor": (0.0, 10.0),
    "defend_probability_base": (0.40, 0.70),
    "defend_damage_threshold": (6, 10),
    "min_hand_size_target": (3, 5),
    "save_big_combos_for_lethal": (0, 1),
}

SWEEP_DEFAULTS = {
    "aggression_level": [1.0, 1.5, 2.0, 2.5, 3.0],
    "combo_bonus_threshold": [7, 8, 9, 10, 11, 12],
    "combo3_bonus_threshold": [14, 16, 18, 20, 24],
    "combo_weight": [0.0, 0.5, 1.0, 1.5, 2.0, 3.0],
    "min_play_score_floor": [0.0, 2.0, 4.0, 6.0, 8.0],
    "defend_probability_base": [0.40, 0.50, 0.55, 0.60, 0.70],
    "defend_damage_threshold": [6, 7, 8, 9, 10],
    "min_hand_size_target": [3, 4, 5],
    "save_big_combos_for_lethal": [0, 1],
}

# Parameters that must round to int / bool before being handed to the C
# harness -- see parse_params() in calib_combo_threshold.c.
_INT_PARAMS = {"combo_bonus_threshold", "combo3_bonus_threshold",
              "defend_damage_threshold", "min_hand_size_target"}

# Character-preservation bands (calibration protocol, see this file's module
# docstring and ai_strat_combo_threshold.c's header comment): an optimizer
# result outside these should be reviewed and overridden by hand, not shipped
# as-is, the same way VB_DEFEND_THRESHOLD's self-play optimum was overridden
# for A1 (doc/changelog.md, 2026-08-21).
PERSONALITY_BANDS = {
    "defend_probability_base": (0.40, 0.70),
    "defend_damage_threshold": (6, 10),
}


def coerce_param(name, value):
    # bool()/int() casts are load-bearing, not stylistic: DE hands back
    # numpy.float64, and numpy.bool_/numpy.int64 (e.g. from `value >= 0.5`)
    # aren't JSON-serializable -- json.dump() on the optimize output would
    # fail otherwise.
    if name == "save_big_combos_for_lethal":
        return bool(value >= 0.5) if not isinstance(value, bool) else value
    if name in _INT_PARAMS:
        return int(round(value))
    return float(value)


def params_to_args(p):
    return [str(coerce_param(name, p[name])) if name != "save_big_combos_for_lethal"
            else ("1" if coerce_param(name, p[name]) else "0")
            for name in PARAM_NAMES]


def merge_params(overrides):
    p = dict(DEFAULTS)
    p.update(overrides)
    return {name: coerce_param(name, p[name]) for name in PARAM_NAMES}


def load_candidate(spec):
    """spec is either the literal 'defaults' or a path to a JSON file holding
    a full/partial ComboThresholdParams dict, or `optimize`'s own output
    (which nests it under "best_params")."""
    if spec == "defaults":
        return dict(DEFAULTS)
    with open(spec) as f:
        data = json.load(f)
    return merge_params(data.get("best_params", data))


# ---------------------------------------------------------------------------
# Core: one match, many matches, confidence intervals
# ---------------------------------------------------------------------------

def run_match(numsim, seed, agent_a, agent_b, params_a, params_b, **tags):
    """One call to bin/calib_combo_threshold. Safe to run in a worker.

    **tags are caller-supplied bookkeeping (e.g. _i/_j, _value/_combo_in_a)
    merged straight into the returned dict, so every result is
    self-describing -- added 2026-08-28 (ported from aicalibsrc/balanced/
    calibrate_balanced.py) to fix a result-misattribution bug: sweep/selfplay
    used to track this bookkeeping in a separate submission-order list and
    reattach it after run_many()'s ProcessPoolExecutor returned results in
    *completion* order, silently scrambling which result belonged to which
    config under concurrency (see doc/oracle_todo.md's Calibration section;
    repro was a luna_value-style sweep going jagged/non-monotone with
    multiple workers, smooth with --workers 1)."""
    if not BINARY.exists():
        raise FileNotFoundError(f"{BINARY} not found -- run `make calib_combo_threshold` first")

    args = [str(BINARY), str(numsim), str(seed), agent_a, agent_b,
            *params_to_args(params_a), *params_to_args(params_b)]
    result = subprocess.run(args, capture_output=True, text=True, check=True)
    row = result.stdout.strip().split(",")
    # Params are echoed back in the CSV (fields 4..21) but we already have
    # them as params_a/params_b -- only the trailing win/draw counts matter.
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
    waiting on the child. Meant for `optimize`, which calls this hundreds of
    times with a handful of jobs each; a fresh process pool per call would
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


def combo_win_rate(df):
    combo_in_a = df["agent_a"] == "combo"
    wins = int(np.where(combo_in_a, df["wins_a"], df["wins_b"]).sum())
    n = int((df["wins_a"] + df["wins_b"] + df["draws"]).sum())
    return wins, n


# ---------------------------------------------------------------------------
# sweep: one param varied, combo vs a fixed opponent, both seats
# ---------------------------------------------------------------------------

def build_sweep_jobs(param, values, opponent, numsim, seeds):
    jobs = []
    for v in values:
        p = merge_params({param: v})
        for seed in seeds:
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="combo", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS), _value=v, _combo_in_a=True))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="combo",
                             params_a=dict(DEFAULTS), params_b=p, _value=v, _combo_in_a=False))
    return jobs


def cmd_sweep(args):
    values = args.values or SWEEP_DEFAULTS[args.param]
    seeds = replicate_seeds(args.base_seed, args.replicates)
    jobs = build_sweep_jobs(args.param, values, args.opponent, args.numsim, seeds)

    print(f"Running {len(jobs)} matches "
         f"({len(values)} values x {args.replicates} replicates x 2 seats, "
         f"vs {args.opponent})...", file=sys.stderr)

    df = run_many(jobs, max_workers=args.workers)
    df["combo_wins"] = np.where(df["_combo_in_a"], df["wins_a"], df["wins_b"])
    df["n"] = df["wins_a"] + df["wins_b"] + df["draws"]

    grouped = df.groupby("_value").agg(wins=("combo_wins", "sum"), n=("n", "sum")).reset_index()
    grouped["win_rate"] = grouped["wins"] / grouped["n"]
    grouped[["ci_lo", "ci_hi"]] = grouped.apply(
        lambda r: pd.Series(wilson_ci(r["wins"], r["n"])), axis=1)
    grouped = grouped.rename(columns={"_value": args.param}).sort_values(args.param)

    print(f"\nSweep of {args.param} (combo vs {args.opponent}, both seats, "
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
    ax.set_title(f"Combo Threshold: {param} sweep (95% Wilson CI)")
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    print(f"Saved: {out_path}")


# ---------------------------------------------------------------------------
# optimize: differential evolution vs a fixed opponent, personality check
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
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="combo", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="combo",
                             params_a=dict(DEFAULTS), params_b=p))
        df = run_many_threaded(jobs, executor)
        wins, n = combo_win_rate(df)
        rate = wins / n if n else 0.0
        progress["evals"] += 1
        print(f"\r  eval {progress['evals']}: win_rate={rate:.4f}",
             end="", file=sys.stderr, flush=True)
        return -rate  # differential_evolution minimizes
    return objective


def personality_report(params):
    """Returns (flagged: bool, lines: list[str])."""
    flagged = False
    lines = []
    for name, (lo, hi) in PERSONALITY_BANDS.items():
        v = params[name]
        ok = lo <= v <= hi
        flagged = flagged or not ok
        lines.append(f"  {name} = {v}: {'OK' if ok else 'OUT OF BAND -- review'} "
                    f"(band [{lo}, {hi}])")
    if params["combo_bonus_threshold"] <= 5:
        flagged = True
        lines.append(f"  combo_bonus_threshold = {params['combo_bonus_threshold']}: "
                    f"<=5 means even color-pair combos qualify -- character drift, review")
    if params["combo_weight"] < 0.1:
        flagged = True
        lines.append(f"  combo_weight = {params['combo_weight']:.3f}: near zero -- "
                    f"agent has stopped chasing combos, character drift, review")
    return flagged, lines


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

    flagged, lines = personality_report(best_params)
    print("\nPersonality band check:")
    print("\n".join(lines))
    if flagged:
        print("\n*** At least one parameter drifted outside its personality band. Per the "
             "calibration protocol, inspect and override by hand before shipping -- do not "
             "ship the raw optimizer output (see A1's VB_DEFEND_THRESHOLD precedent, "
             "doc/changelog.md 2026-08-21). ***")

    print(f"\nRe-validating winner with more games "
         f"({args.validate_replicates * 2} matches x {args.validate_numsim} games)...",
         file=sys.stderr)
    val_seeds = replicate_seeds(args.base_seed + 100000, args.validate_replicates)
    jobs = []
    for seed in val_seeds:
        jobs.append(dict(numsim=args.validate_numsim, seed=seed, agent_a="combo",
                         agent_b=args.opponent, params_a=best_params, params_b=dict(DEFAULTS)))
        jobs.append(dict(numsim=args.validate_numsim, seed=seed, agent_a=args.opponent,
                         agent_b="combo", params_a=dict(DEFAULTS), params_b=best_params))
    df = run_many(jobs, max_workers=args.workers)
    wins, n = combo_win_rate(df)
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
            "personality_flagged": flagged,
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
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="combo", agent_b="combo",
                             params_a=params_map[names[i]], params_b=params_map[names[j]],
                             _i=i, _j=j))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="combo", agent_b="combo",
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
    # rederived from the wins/games matrices via a transpose trick. wins[i,j]
    # only ever holds "i's wins when seated as A against j as B", so
    # wins.T[i,:].sum() would sum the *opponents'* A-seat wins in games where
    # i was B, not i's own wins. Every match report carries both wins_a and
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
            jobs.append(dict(numsim=args.numsim, seed=seed, agent_a="combo",
                             agent_b=args.opponent, params_a=params, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=args.numsim, seed=seed, agent_a=args.opponent,
                             agent_b="combo", params_a=dict(DEFAULTS), params_b=params))
        df = run_many(jobs, max_workers=args.workers)
        wins, n = combo_win_rate(df)
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

    flagged, lines = personality_report(candidate)
    print("\nPersonality band check:")
    print("\n".join(lines))
    if flagged:
        print("\n*** Candidate has at least one parameter outside its personality band. ***")


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
                       help="default: search all nine")
    p_opt.add_argument("--opponent", default="value")
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
