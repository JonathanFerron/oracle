#!/usr/bin/env python3
"""Calibration driver for the A4 Balanced Rules ("Bean Counter") AI agent's
ten tunable parameters (BalancedRulesParams -- see
src/ai_strat/ai_strat_balanced_rules.h).

Drives bin/calib_balanced, which links the game engine directly and prints
one CSV result line per invocation. All the actual game simulation happens
in the compiled C binary; this script only orchestrates subprocess calls and
aggregates/fits/searches the results. Structure and internals (run_match's
**tags-at-the-source fix, summarize_selfplay's direct win/game tracking, the
seed-offset convention) are carried over verbatim from
aicalibsrc/borealis/calibrate_borealis.py -- see that file's own comments for
the concurrency bug this pattern avoids (doc/oracle_todo.md).

Unlike the three earlier drivers, DEFAULTS below is never hand-copied: it is
read once, at import time, from `bin/calib_balanced --print-defaults`, so it
can never drift from the shipped BALANCED_DEFAULTS the way
aicalibsrc/value/'s and aicalibsrc/combo/'s and aicalibsrc/borealis/'s copies
have (doc/oracle_todo.md tracks that as an open item for those three).

Balanced Rules has two distinct ways to stop being itself, not one:
  - Slope degeneracy: if target_cash_slope/target_cards_slope collapse
    toward 0, the resource targets stop responding to opponent energy and
    the agent becomes "spend everything, always" -- a different, dumber
    agent that might still measure well.
  - combo_weight drift: this agent is combo-blind by design (it ships at
    0.0); an optimizer pushing it up has rediscovered a worse version of
    A2/A3, not improved Bean Counter.
defense_beta is this agent's one true strength dial (like A3's luna_value),
so `optimize` also re-sweeps it post-search and checks concavity, the same
technique used for A1's VB_COST_FLOOR and A3's luna_value
(doc/changelog.md).

Four subcommands, same as aicalibsrc/borealis/:

  sweep     Univariate diagnostic: hold every parameter at its default
            except one, vary that one, play vs a fixed --opponent (default
            "rand"), both seats, with binomial confidence intervals.
            `--param defense_beta` first is the most informative single
            check -- a flat curve means the variance term isn't wired in.

  optimize  Black-box search (scipy.optimize.differential_evolution) over
            some or all ten parameters, maximizing win rate against a fixed
            --opponent (default "borealis" -- Balanced Rules' design-intent
            rating (62) *is* its win rate against the anchor, so that is the
            natural target; if defaults measure as a floor effect there
            instead of a two-sided contest, --opponent combo is the
            fallback, the same way A1/A2/A3 each picked the opponent with
            real headroom). After finding a winner, re-runs a defense_beta
            sweep with the other nine parameters fixed at the found values
            and fits a quadratic to check it's still concave-down.

  selfplay  Round-robin among a small set of NAMED candidate parameter sets
            (ten parameters make a full grid infeasible; named candidates --
            e.g. a handful of `optimize` outputs -- stay simpler), both seat
            orders, --ai.a=balanced vs --ai.b=balanced. Reports a
            Bradley-Terry fit.

  validate  Compare one candidate parameter set against the shipped
            defaults, vs a chosen opponent, both seats.

Candidate parameter sets are given as JSON files (a full or partial
BalancedRulesParams dict; missing fields fall back to the compiled defaults)
or the literal string "defaults". `optimize`'s own output file is directly
usable as a `selfplay`/`validate` candidate (it has a "best_params" key,
which these commands know to unwrap).

Examples:
  ./calibrate_balanced.py sweep --param defense_beta --opponent borealis \\
      --numsim 2000 --replicates 4 --plot
  ./calibrate_balanced.py optimize --opponent borealis --numsim 500 \\
      --replicates 2 --maxiter 10 --popsize 10
  ./calibrate_balanced.py selfplay --candidates defaults results/optimize_borealis.json
  ./calibrate_balanced.py validate --candidate results/optimize_borealis.json --opponent rand
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

BINARY = Path(__file__).resolve().parent.parent.parent / "bin" / "calib_balanced"
RESULTS_DIR = Path(__file__).resolve().parent / "results"

# Declared struct order in BalancedRulesParams -- must match parse_params()
# in calib_balanced.c.
PARAM_NAMES = [
    "target_cash_slope", "target_cash_intercept",
    "target_cards_slope", "target_cards_intercept",
    "defense_beta", "late_game_aggro", "combo_weight",
    "lethal_horizon", "draw2_hand_threshold", "draw3_hand_threshold",
]

_INT_PARAMS = {"lethal_horizon", "draw2_hand_threshold", "draw3_hand_threshold"}


def _load_defaults_from_binary():
    if not BINARY.exists():
        # Fall back to the compiled-in values documented in
        # ai_strat_balanced_rules.c's BALANCED_DEFAULTS, so --help and
        # argument parsing still work before `make calib_balanced` has run.
        # Any real command still hits run_match()'s own existence check.
        return {
            "target_cash_slope": 30.0 / 91.0, "target_cash_intercept": 0.0,
            "target_cards_slope": 5.0 / 91.0, "target_cards_intercept": 0.0,
            "defense_beta": 1.0, "late_game_aggro": 1.2, "combo_weight": 0.0,
            "lethal_horizon": 25, "draw2_hand_threshold": 7, "draw3_hand_threshold": 6,
        }
    result = subprocess.run([str(BINARY), "--print-defaults"],
                            capture_output=True, text=True, check=True)
    return json.loads(result.stdout)


DEFAULTS = _load_defaults_from_binary()

# Search space for `optimize` and default sweep grids for `sweep`. Widened
# relative to the design docs' own numbers (see ai_strat_balanced_rules.h):
# target_cash_slope's default (INITIAL_CASH_DEFAULT/91 ~= 0.33) already puts
# the cash surplus at ~0 when the opponent is at full energy (an early-game
# bootstrap trap confirmed by hand -- vs `borealis` at these defaults the
# agent wins ~6% of games), so the bounds must reach comfortably below the
# default to let the optimizer actually find slack.
BOUNDS = {
    "target_cash_slope": (0.0, 0.60),
    "target_cash_intercept": (-5.0, 15.0),
    "target_cards_slope": (0.0, 0.15),
    "target_cards_intercept": (-2.0, 6.0),
    "defense_beta": (0.0, 2.5),
    "late_game_aggro": (1.0, 2.5),
    "combo_weight": (0.0, 3.0),
    "lethal_horizon": (0, 40),
    "draw2_hand_threshold": (4, 9),
    "draw3_hand_threshold": (3, 8),
}

# Identity-constrained search space -- used by `optimize --identity-safe`.
# Two free `optimize` runs (2026-08-24, see doc/changelog.md) independently
# drove target_cash_slope/target_cards_slope toward 0 and defense_beta past
# 2.0 ("rarely defend"), each measuring stronger but eroding exactly the
# traits the module docstring's personality checks exist to protect --
# matching A2's rejected aggression_level=2.21 precedent. Rather than
# hand-pick a compromise from limited sweep data, this space keeps the
# search inside the region check_personality_flags() considers non-degenerate
# (mirrors those thresholds) and finds the best achievable result there.
BOUNDS_IDENTITY_SAFE = {
    "target_cash_slope": (0.08, 0.35),
    "target_cash_intercept": (-3.0, 8.0),
    "target_cards_slope": (0.02, 0.10),
    "target_cards_intercept": (-1.0, 3.0),
    "defense_beta": (0.25, 2.0),
    "late_game_aggro": (1.0, 2.5),
    "lethal_horizon": (0, 40),
    "draw2_hand_threshold": (4, 9),
    "draw3_hand_threshold": (3, 8),
}

SWEEP_DEFAULTS = {
    "target_cash_slope": [0.0, 0.05, 0.10, 0.20, 0.3297, 0.45, 0.60],
    "target_cash_intercept": [-5.0, -2.0, 0.0, 5.0, 10.0],
    "target_cards_slope": [0.0, 0.02, 0.0549, 0.08, 0.12],
    "target_cards_intercept": [-2.0, 0.0, 2.0, 4.0],
    "defense_beta": [0.0, 0.5, 1.0, 1.5, 2.0, 2.5],
    "late_game_aggro": [1.0, 1.2, 1.5, 2.0, 2.5],
    "combo_weight": [0.0, 0.5, 1.0, 2.0, 3.0],
    "lethal_horizon": [0, 10, 25, 35, 40],
    "draw2_hand_threshold": [4, 5, 6, 7, 8, 9],
    "draw3_hand_threshold": [3, 4, 5, 6, 7, 8],
}

# Extra grid used only by optimize's post-search personality re-check --
# wider than SWEEP_DEFAULTS["defense_beta"] so a found optimum near either
# end still has room to show curvature. defense_beta is this agent's one
# true strength dial (see this file's module docstring).
BETA_UNIMODALITY_GRID = [0.0, 0.25, 0.5, 1.0, 1.5, 2.0, 2.5]


def coerce_param(name, value):
    # int() cast is load-bearing, not stylistic: DE hands back numpy.float64,
    # and numpy.int64 isn't JSON-serializable -- json.dump() on the optimize
    # output would fail otherwise (same note as calibrate_borealis.py's
    # coerce_param()).
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
    a full/partial BalancedRulesParams dict, or `optimize`'s own output
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
    """One call to bin/calib_balanced. Safe to run in a worker.

    **tags are arbitrary caller bookkeeping (e.g. sweep's `_value`,
    selfplay's `_i`/`_j`) merged straight into the returned dict instead of
    being tracked in a separate same-order list -- run_many()'s
    ProcessPoolExecutor + as_completed() returns results in COMPLETION
    order, not submission order, so tagging at the source (rather than
    reattaching by list position afterward) is what keeps this correct
    under real parallelism; see aicalibsrc/borealis/calibrate_borealis.py's
    identical note and doc/oracle_todo.md's tracked bug in the two earlier
    drivers that don't do this.
    """
    if not BINARY.exists():
        raise FileNotFoundError(f"{BINARY} not found -- run `make calib_balanced` first")

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


def balanced_win_rate(df):
    balanced_in_a = df["agent_a"] == "balanced"
    wins = int(np.where(balanced_in_a, df["wins_a"], df["wins_b"]).sum())
    n = int((df["wins_a"] + df["wins_b"] + df["draws"]).sum())
    return wins, n


# ---------------------------------------------------------------------------
# sweep: one param varied, balanced vs a fixed opponent, both seats
# ---------------------------------------------------------------------------

def build_sweep_jobs(param, values, opponent, numsim, seeds):
    jobs = []
    for v in values:
        p = merge_params({param: v})
        for seed in seeds:
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="balanced", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS), _value=v, _balanced_in_a=True))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="balanced",
                             params_a=dict(DEFAULTS), params_b=p, _value=v, _balanced_in_a=False))
    return jobs


def cmd_sweep(args):
    values = args.values or SWEEP_DEFAULTS[args.param]
    seeds = replicate_seeds(args.base_seed, args.replicates)
    jobs = build_sweep_jobs(args.param, values, args.opponent, args.numsim, seeds)

    print(f"Running {len(jobs)} matches "
         f"({len(values)} values x {args.replicates} replicates x 2 seats, "
         f"vs {args.opponent})...", file=sys.stderr)

    df = run_many(jobs, max_workers=args.workers)
    df["balanced_wins"] = np.where(df["_balanced_in_a"], df["wins_a"], df["wins_b"])
    df["n"] = df["wins_a"] + df["wins_b"] + df["draws"]

    grouped = df.groupby("_value").agg(wins=("balanced_wins", "sum"), n=("n", "sum")).reset_index()
    grouped["win_rate"] = grouped["wins"] / grouped["n"]
    grouped[["ci_lo", "ci_hi"]] = grouped.apply(
        lambda r: pd.Series(wilson_ci(r["wins"], r["n"])), axis=1)
    grouped = grouped.rename(columns={"_value": args.param}).sort_values(args.param)

    print(f"\nSweep of {args.param} (balanced vs {args.opponent}, both seats, "
         f"{args.numsim * args.replicates * 2} games/value):")
    print(grouped.to_string(index=False, float_format=lambda x: f"{x:.4f}"))

    if args.param == "defense_beta":
        is_unimodal, implied_optimum, r_squared = check_unimodal_fit(
            grouped[args.param].to_numpy(), grouped["win_rate"].to_numpy())
        print(f"\ndefense_beta shape check (quadratic fit, R^2={r_squared:.3f}): "
             f"{'concave (single peak)' if is_unimodal else 'flat/convex -- review'}")
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
    ax.set_title(f"Balanced Rules: {param} sweep (95% Wilson CI)")
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
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="balanced", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="balanced",
                             params_a=dict(DEFAULTS), params_b=p))
        df = run_many_threaded(jobs, executor)
        wins, n = balanced_win_rate(df)
        rate = wins / n if n else 0.0
        progress["evals"] += 1
        print(f"\r  eval {progress['evals']}: win_rate={rate:.4f}",
             end="", file=sys.stderr, flush=True)
        return -rate  # differential_evolution minimizes
    return objective


def check_unimodal_fit(x_values, win_rates):
    """Fits a quadratic to (x, win_rate) -- same technique used for A1's
    VB_COST_FLOOR and A3's luna_value (doc/changelog.md). A concave-down fit
    (leading coefficient < 0) means win rate rises then falls, i.e. a single
    interior maximum. Returns (is_unimodal, implied_optimum, r_squared)."""
    x_values = np.asarray(x_values, dtype=float)
    win_rates = np.asarray(win_rates, dtype=float)
    a, b, c = np.polyfit(x_values, win_rates, 2)
    fitted = np.polyval((a, b, c), x_values)
    ss_res = np.sum((win_rates - fitted) ** 2)
    ss_tot = np.sum((win_rates - np.mean(win_rates)) ** 2)
    r_squared = float(1 - ss_res / ss_tot) if ss_tot > 0 else float("nan")
    is_unimodal = bool(a < 0)
    implied_optimum = float(-b / (2 * a)) if a != 0 else float("nan")
    return is_unimodal, implied_optimum, r_squared


def recheck_beta_shape(best_params, opponent, numsim, replicates, base_seed, workers):
    """Re-sweeps defense_beta over BETA_UNIMODALITY_GRID with every other
    parameter held at best_params' found values, then fits check_unimodal_fit()."""
    seeds = replicate_seeds(base_seed + 200000, replicates)
    rates = []
    print(f"\nRe-checking defense_beta shape with the other {len(PARAM_NAMES) - 1} "
         f"parameter(s) fixed at the found values...", file=sys.stderr)
    for beta in BETA_UNIMODALITY_GRID:
        p = dict(best_params)
        p["defense_beta"] = beta
        jobs = []
        for seed in seeds:
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="balanced", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="balanced",
                             params_a=dict(DEFAULTS), params_b=p))
        df = run_many(jobs, max_workers=workers, quiet=True)
        wins, n = balanced_win_rate(df)
        rate = wins / n if n else 0.0
        rates.append(rate)
        print(f"  defense_beta={beta}: win_rate={rate:.4f}", file=sys.stderr)

    return check_unimodal_fit(BETA_UNIMODALITY_GRID, rates)


def check_personality_flags(best_params):
    """Returns a list of human-readable warning strings for anything an
    optimizer result should not silently ship with (module docstring). Never
    auto-rejects -- flags for review, same policy as A1/A2/A3's precedents
    (doc/changelog.md's rejected aggression_level=2.21 for A2)."""
    flags = []
    cash_span = best_params["target_cash_slope"] * 91.0
    cards_span = best_params["target_cards_slope"] * 91.0
    if cash_span < 7.0:
        flags.append(f"target_cash_slope implies only {cash_span:.1f} lunas of range "
                     f"across opp energy 8-99 (< 7, a quarter of the {round(DEFAULTS['target_cash_slope']*91)}-"
                     f"luna default span) -- targets barely respond to opponent energy.")
    if cards_span < 1.4:
        flags.append(f"target_cards_slope implies only {cards_span:.2f} cards of range "
                     f"across opp energy 8-99 (< 1.4) -- targets barely respond to opponent energy.")
    if best_params["combo_weight"] > 0.5:
        flags.append(f"combo_weight={best_params['combo_weight']:.3f} > 0.5 -- this agent "
                     f"is meant to be combo-blind on selection; shipping non-zero is a "
                     f"deliberate human call, not an automatic optimizer win.")
    if not (0.25 <= best_params["defense_beta"] <= 2.0):
        flags.append(f"defense_beta={best_params['defense_beta']:.3f} outside [0.25, 2.0] "
                     f"-- 0 means 'match expected attack exactly', large means 'never defend'.")
    return flags


def cmd_optimize(args):
    bounds_table = BOUNDS
    if args.identity_safe:
        # combo_weight has no entry in BOUNDS_IDENTITY_SAFE -- it stays
        # blind (fixed at its 0.0 default) in this mode regardless of
        # --params, since "combo-blind" isn't a band to search within, it's
        # this agent's scope boundary (about.md).
        free_names = [n for n in (args.params or list(BOUNDS_IDENTITY_SAFE))
                     if n != "combo_weight"]
        bounds_table = BOUNDS_IDENTITY_SAFE
    else:
        free_names = args.params or list(PARAM_NAMES)
    fixed = {n: DEFAULTS[n] for n in PARAM_NAMES if n not in free_names}
    bounds = [bounds_table[n] for n in free_names]
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

    is_unimodal, implied_optimum, r_squared = recheck_beta_shape(
        best_params, args.opponent, args.numsim, args.replicates, args.base_seed, args.workers)
    print(f"\ndefense_beta shape check (quadratic fit, R^2={r_squared:.3f}):")
    print(f"  {'concave (single peak)' if is_unimodal else 'NOT concave -- flat or convex fit'}")
    print(f"  implied optimum defense_beta (fit vertex): {implied_optimum:.3f}")

    personality_flags = check_personality_flags(best_params)
    if not is_unimodal:
        personality_flags.append("defense_beta is not single-peaked at the found point "
                                 "(see the shape check above) -- review before shipping.")

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
        jobs.append(dict(numsim=args.validate_numsim, seed=seed, agent_a="balanced",
                         agent_b=args.opponent, params_a=best_params, params_b=dict(DEFAULTS)))
        jobs.append(dict(numsim=args.validate_numsim, seed=seed, agent_a=args.opponent,
                         agent_b="balanced", params_a=dict(DEFAULTS), params_b=best_params))
    df = run_many(jobs, max_workers=args.workers)
    wins, n = balanced_win_rate(df)
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
            "beta_unimodal": is_unimodal, "beta_implied_optimum": implied_optimum,
            "beta_fit_r_squared": r_squared, "personality_flags": personality_flags,
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
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="balanced", agent_b="balanced",
                             params_a=params_map[names[i]], params_b=params_map[names[j]],
                             _i=i, _j=j))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="balanced", agent_b="balanced",
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
    # rederived from the wins/games matrices via a transpose trick (see A1's
    # calibrate_valuebased.py for the bug that pattern caused).
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
            jobs.append(dict(numsim=args.numsim, seed=seed, agent_a="balanced",
                             agent_b=args.opponent, params_a=params, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=args.numsim, seed=seed, agent_a=args.opponent,
                             agent_b="balanced", params_a=dict(DEFAULTS), params_b=params))
        df = run_many(jobs, max_workers=args.workers)
        wins, n = balanced_win_rate(df)
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
                       help="default: search all ten (all nine free ones under "
                            "--identity-safe, which always keeps combo_weight blind)")
    p_opt.add_argument("--identity-safe", action="store_true",
                       help="search BOUNDS_IDENTITY_SAFE instead of BOUNDS -- keeps "
                            "target_cash_slope/target_cards_slope non-degenerate and "
                            "defense_beta in [0.25, 2.0] rather than letting the search "
                            "erode this agent's designed character (see module docstring "
                            "and doc/changelog.md's 2026-08-24 entry)")
    p_opt.add_argument("--opponent", default="borealis")
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
