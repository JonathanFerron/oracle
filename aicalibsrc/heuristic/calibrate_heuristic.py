#!/usr/bin/env python3
"""Calibration driver for the A5 Heuristic ("Eps-Gam-Del") AI agent's five
tunable parameters (HeuristicParams -- see src/ai_strat/ai_strat_heuristic.h).

Drives bin/calib_heuristic, which links the game engine directly and prints
one CSV result line per invocation. All the actual game simulation happens
in the compiled C binary; this script only orchestrates subprocess calls and
aggregates/fits/searches the results. Structure and internals (run_match's
**tags-at-the-source fix, summarize_selfplay's direct win/game tracking, the
seed-offset convention, --print-defaults-sourced DEFAULTS) are carried over
verbatim from aicalibsrc/balanced/calibrate_balanced.py -- the newest and
most-corrected of the four earlier drivers (see that file's own comments,
and doc/oracle_todo.md, for the concurrency/drift bugs this pattern avoids).

DEFAULTS below is read once, at import time, from
`bin/calib_heuristic --print-defaults`, so it can never drift from the
shipped HEURISTIC_DEFAULTS.

weight_cash_advantage (delta) is PINNED at its default in every `optimize`
run unless explicitly named in --params: the argmax of a weighted sum of
three terms is invariant to a positive rescaling of all three weights, so
with delta fixed the other two become well-defined ratios against it (see
ai_strat_heuristic.h's header comment). The free-by-default set is therefore
the other four parameters, in both plain and --identity-safe search modes.

Two distinct ways for this agent to stop being itself, tracked by
check_personality_flags():
  - weight_energy_advantage (epsilon) collapsing toward 0 relative to the
    pinned delta -- EnergyAdv, this agent's only lethal-detection signal (the
    HEUR_LETHAL_BONUS term), would barely matter to move choice.
  - weight_taper_exponent growing large -- gamma/delta become effectively 0
    except near full opponent energy, degrading this into a worse Borealis
    rather than a genuine taper.
weight_cards_advantage (gamma) landing near 0 is treated separately, as an
informational finding rather than a personality flag: the stub itself asks
whether epsilon=1, gamma=0 reduces this agent to Balanced Rules' behaviour
(ai_strat_heuristic.h) -- see check_balanced_equivalence().

weight_energy_advantage is this agent's one true strength dial once delta is
pinned (like A3's luna_value, A4's defense_beta), so `optimize` also
re-sweeps it post-search and checks concavity.

Four subcommands, same as aicalibsrc/balanced/:

  sweep     Univariate diagnostic: hold every parameter at its default
            except one, vary that one, play vs a fixed --opponent (default
            "rand"), both seats, with binomial confidence intervals.

  optimize  Black-box search (scipy.optimize.differential_evolution) over
            the four free parameters (delta pinned) by default, maximizing
            win rate against a fixed --opponent (default "borealis" -- the
            rating-50 anchor doc/oracle_todo.md's calibration protocol
            targets for this agent, superseding the design docs' stale
            "calibrate against A4" note now that A4 itself measured below
            the anchor). After finding a winner, re-runs a
            weight_energy_advantage sweep with the other parameters fixed
            at the found values and fits a quadratic to check it's still
            concave-down.

  selfplay  Round-robin among a small set of NAMED candidate parameter sets,
            both seat orders, --ai.a=heuristic vs --ai.b=heuristic. Reports
            a Bradley-Terry fit.

  validate  Compare one candidate parameter set against the shipped
            defaults, vs a chosen opponent, both seats.

Candidate parameter sets are given as JSON files (a full or partial
HeuristicParams dict; missing fields fall back to the compiled defaults) or
the literal string "defaults". `optimize`'s own output file is directly
usable as a `selfplay`/`validate` candidate (it has a "best_params" key,
which these commands know to unwrap).

Examples:
  ./calibrate_heuristic.py sweep --param weight_energy_advantage \\
      --opponent borealis --numsim 2000 --replicates 4 --plot
  ./calibrate_heuristic.py optimize --opponent borealis --numsim 500 \\
      --replicates 2 --maxiter 10 --popsize 10
  ./calibrate_heuristic.py selfplay --candidates defaults results/optimize_borealis.json
  ./calibrate_heuristic.py validate --candidate results/optimize_borealis.json --opponent rand
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

BINARY = Path(__file__).resolve().parent.parent.parent / "bin" / "calib_heuristic"
RESULTS_DIR = Path(__file__).resolve().parent / "results"

# Declared struct order in HeuristicParams -- must match parse_params() in
# calib_heuristic.c.
PARAM_NAMES = [
    "weight_energy_advantage", "weight_cards_advantage",
    "weight_cash_advantage", "weight_taper_exponent", "opp_card_discount",
]

# weight_cash_advantage (delta) is pinned by default -- see module docstring.
FREE_PARAM_NAMES = [n for n in PARAM_NAMES if n != "weight_cash_advantage"]

_INT_PARAMS = set()  # every HeuristicParams field is a float


def _load_defaults_from_binary():
    if not BINARY.exists():
        # Fall back to the compiled-in values documented in
        # ai_strat_heuristic.c's HEURISTIC_DEFAULTS, so --help and argument
        # parsing still work before `make calib_heuristic` has run.
        return {
            "weight_energy_advantage": 1.0, "weight_cards_advantage": 0.15,
            "weight_cash_advantage": 1.0, "weight_taper_exponent": 1.0,
            "opp_card_discount": 1.0,
        }
    result = subprocess.run([str(BINARY), "--print-defaults"],
                            capture_output=True, text=True, check=True)
    return json.loads(result.stdout)


DEFAULTS = _load_defaults_from_binary()

# Search space for `optimize` and default sweep grids for `sweep`.
# weight_energy_advantage is this agent's strength dial (like A3's
# luna_value/A4's defense_beta) so its range is widened well past the
# spec default (1.0) in both directions. weight_cards_advantage is allowed
# down to 0.0 -- the stub's own open question is whether gamma=0 (epsilon=1)
# reduces this agent to Balanced Rules (ai_strat_heuristic.h) -- so 0 must be
# reachable, not just approached -- and its upper bound is widened to 15.0
# after a manual sweep vs `borealis` (this file, calibration run) found the
# real peak far above the spec default (0.15) or an initial 2.0 test bound:
# win rate vs borealis rose from 26.8% at gamma=1 to a peak of ~48.7% around
# gamma=6-8 before collapsing to 19.1% at gamma=12 (clear unimodal shape).
# 2.0 would have silently capped `optimize` well short of that peak.
BOUNDS = {
    "weight_energy_advantage": (0.0, 10.0),
    "weight_cards_advantage": (0.0, 15.0),
    "weight_cash_advantage": (0.0, 3.0),
    "weight_taper_exponent": (0.0, 4.0),
    "opp_card_discount": (0.0, 3.0),
}

# Identity-constrained search space -- used by `optimize --identity-safe`.
# Keeps weight_energy_advantage away from 0 (EnergyAdv, and therefore
# HEUR_LETHAL_BONUS, must still matter) and weight_taper_exponent in [0, 2]
# (past ~3 gamma/delta are effectively zero except near full opponent
# energy -- a worse Borealis, not a taper). weight_cards_advantage keeps its
# full [0, 2] range even here: gamma -> 0 is a legitimate finding (see
# module docstring), not erosion, so it is never excluded the way A4's
# combo_weight is. weight_cash_advantage has no entry -- pinned in every
# optimize mode, not just this one.
BOUNDS_IDENTITY_SAFE = {
    "weight_energy_advantage": (0.2, 5.0),
    "weight_cards_advantage": (0.0, 2.0),
    "weight_taper_exponent": (0.0, 2.0),
    "opp_card_discount": (0.0, 2.0),
}

SWEEP_DEFAULTS = {
    "weight_energy_advantage": [0.0, 0.25, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0],
    "weight_cards_advantage": [0.0, 0.15, 1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0],
    "weight_cash_advantage": [0.0, 0.5, 1.0, 1.5, 2.0, 3.0],
    "weight_taper_exponent": [0.0, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0],
    "opp_card_discount": [0.0, 0.5, 1.0, 1.5, 2.0, 3.0],
}

# Extra grid used only by optimize's post-search personality re-check --
# wider than SWEEP_DEFAULTS["weight_energy_advantage"] so a found optimum
# near either end still has room to show curvature.
EPSILON_UNIMODALITY_GRID = [0.0, 0.25, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0]


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
    a full/partial HeuristicParams dict, or `optimize`'s own output (which
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
    """One call to bin/calib_heuristic. Safe to run in a worker.

    **tags are arbitrary caller bookkeeping (e.g. sweep's `_value`,
    selfplay's `_i`/`_j`) merged straight into the returned dict instead of
    being tracked in a separate same-order list -- run_many()'s
    ProcessPoolExecutor + as_completed() returns results in COMPLETION
    order, not submission order, so tagging at the source (rather than
    reattaching by list position afterward) is what keeps this correct
    under real parallelism.
    """
    if not BINARY.exists():
        raise FileNotFoundError(f"{BINARY} not found -- run `make calib_heuristic` first")

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


def heuristic_win_rate(df):
    heuristic_in_a = df["agent_a"] == "heuristic"
    wins = int(np.where(heuristic_in_a, df["wins_a"], df["wins_b"]).sum())
    n = int((df["wins_a"] + df["wins_b"] + df["draws"]).sum())
    return wins, n


# ---------------------------------------------------------------------------
# sweep: one param varied, heuristic vs a fixed opponent, both seats
# ---------------------------------------------------------------------------

def build_sweep_jobs(param, values, opponent, numsim, seeds):
    jobs = []
    for v in values:
        p = merge_params({param: v})
        for seed in seeds:
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="heuristic", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS), _value=v, _heuristic_in_a=True))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="heuristic",
                             params_a=dict(DEFAULTS), params_b=p, _value=v, _heuristic_in_a=False))
    return jobs


def cmd_sweep(args):
    values = args.values or SWEEP_DEFAULTS[args.param]
    seeds = replicate_seeds(args.base_seed, args.replicates)
    jobs = build_sweep_jobs(args.param, values, args.opponent, args.numsim, seeds)

    print(f"Running {len(jobs)} matches "
         f"({len(values)} values x {args.replicates} replicates x 2 seats, "
         f"vs {args.opponent})...", file=sys.stderr)

    df = run_many(jobs, max_workers=args.workers)
    df["heuristic_wins"] = np.where(df["_heuristic_in_a"], df["wins_a"], df["wins_b"])
    df["n"] = df["wins_a"] + df["wins_b"] + df["draws"]

    grouped = df.groupby("_value").agg(wins=("heuristic_wins", "sum"), n=("n", "sum")).reset_index()
    grouped["win_rate"] = grouped["wins"] / grouped["n"]
    grouped[["ci_lo", "ci_hi"]] = grouped.apply(
        lambda r: pd.Series(wilson_ci(r["wins"], r["n"])), axis=1)
    grouped = grouped.rename(columns={"_value": args.param}).sort_values(args.param)

    print(f"\nSweep of {args.param} (heuristic vs {args.opponent}, both seats, "
         f"{args.numsim * args.replicates * 2} games/value):")
    print(grouped.to_string(index=False, float_format=lambda x: f"{x:.4f}"))

    if args.param == "weight_energy_advantage":
        is_unimodal, implied_optimum, r_squared = check_unimodal_fit(
            grouped[args.param].to_numpy(), grouped["win_rate"].to_numpy())
        print(f"\nweight_energy_advantage shape check (quadratic fit, R^2={r_squared:.3f}): "
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
    ax.set_title(f"Eps-Gam-Del: {param} sweep (95% Wilson CI)")
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
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="heuristic", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="heuristic",
                             params_a=dict(DEFAULTS), params_b=p))
        df = run_many_threaded(jobs, executor)
        wins, n = heuristic_win_rate(df)
        rate = wins / n if n else 0.0
        progress["evals"] += 1
        print(f"\r  eval {progress['evals']}: win_rate={rate:.4f}",
             end="", file=sys.stderr, flush=True)
        return -rate  # differential_evolution minimizes
    return objective


def check_unimodal_fit(x_values, win_rates):
    """Fits a quadratic to (x, win_rate) -- same technique used for A1's
    VB_COST_FLOOR, A3's luna_value, and A4's defense_beta. A concave-down fit
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


def recheck_epsilon_shape(best_params, opponent, numsim, replicates, base_seed, workers):
    """Re-sweeps weight_energy_advantage over EPSILON_UNIMODALITY_GRID with
    every other parameter held at best_params' found values, then fits
    check_unimodal_fit()."""
    seeds = replicate_seeds(base_seed + 200000, replicates)
    rates = []
    print(f"\nRe-checking weight_energy_advantage shape with the other "
         f"{len(PARAM_NAMES) - 1} parameter(s) fixed at the found values...", file=sys.stderr)
    for epsilon in EPSILON_UNIMODALITY_GRID:
        p = dict(best_params)
        p["weight_energy_advantage"] = epsilon
        jobs = []
        for seed in seeds:
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="heuristic", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="heuristic",
                             params_a=dict(DEFAULTS), params_b=p))
        df = run_many(jobs, max_workers=workers, quiet=True)
        wins, n = heuristic_win_rate(df)
        rate = wins / n if n else 0.0
        rates.append(rate)
        print(f"  weight_energy_advantage={epsilon}: win_rate={rate:.4f}", file=sys.stderr)

    return check_unimodal_fit(EPSILON_UNIMODALITY_GRID, rates)


def check_personality_flags(best_params):
    """Returns a list of human-readable warning strings for anything an
    optimizer result should not silently ship with (module docstring). Never
    auto-rejects -- flags for review, same policy as A1-A4's precedents."""
    flags = []
    delta = best_params["weight_cash_advantage"]
    epsilon_ratio = best_params["weight_energy_advantage"] / delta if delta else float("inf")
    if epsilon_ratio < 0.2:
        flags.append(f"weight_energy_advantage/weight_cash_advantage = {epsilon_ratio:.3f} "
                     f"< 0.2 -- epsilon is collapsing relative to the pinned delta, so "
                     f"EnergyAdv (and HEUR_LETHAL_BONUS, this agent's only lethal-detection "
                     f"signal) barely influences move choice.")
    if best_params["weight_taper_exponent"] > 3.0:
        flags.append(f"weight_taper_exponent={best_params['weight_taper_exponent']:.3f} > 3.0 "
                     f"-- gamma/delta are effectively zero except near full opponent energy, "
                     f"degrading this into a worse Borealis rather than a genuine taper.")
    if best_params["weight_cards_advantage"] > 1.0:
        flags.append(f"weight_cards_advantage={best_params['weight_cards_advantage']:.3f} is "
                     f"more than 6x the spec default (0.15) -- verify this still reads as "
                     f"light card-counting and not a different, card-hoarding mechanism.")
    return flags


def check_balanced_equivalence(best_params):
    """Informational, not a rejection flag: the stub itself asks whether
    epsilon=1, gamma=0 reduces this agent to Balanced Rules' behaviour
    (ai_strat_heuristic.h's header comment). A search landing near
    gamma=0 with delta pinned near 1 is answering that open design
    question, not eroding this agent's character -- report it plainly."""
    gamma = best_params["weight_cards_advantage"]
    epsilon = best_params["weight_energy_advantage"]
    delta = best_params["weight_cash_advantage"]
    if gamma < 0.02 and delta > 0 and abs(epsilon / delta - 1.0) < 0.3:
        return (f"gamma={gamma:.4f} with epsilon/delta={epsilon / delta:.3f} (~1.0) -- this "
               f"lands close to the stub's own hypothesized 'epsilon=1, gamma=0' equivalence "
               f"to Balanced Rules. Worth recording as a finding, not shipping as if it were "
               f"erosion.")
    return None


def cmd_optimize(args):
    bounds_table = BOUNDS
    if args.identity_safe:
        # weight_cash_advantage has no entry in BOUNDS_IDENTITY_SAFE -- it
        # stays pinned in this mode regardless of --params, same as every
        # other optimize mode (module docstring): delta being fixed is what
        # makes epsilon/gamma well-defined ratios, not a personality
        # constraint to negotiate with a search.
        free_names = [n for n in (args.params or list(BOUNDS_IDENTITY_SAFE))
                     if n != "weight_cash_advantage"]
        bounds_table = BOUNDS_IDENTITY_SAFE
    else:
        # An explicit --params list is honored verbatim (including delta, if
        # the user really names it); with no --params, delta stays pinned by
        # defaulting to FREE_PARAM_NAMES instead of the full PARAM_NAMES.
        free_names = list(args.params) if args.params else list(FREE_PARAM_NAMES)
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
        marker = "" if name in free_names else "  (fixed/pinned at default)"
        print(f"  {name} = {best_params[name]}{marker}")
    print(f"  win rate vs {args.opponent} (in-search estimate, {args.numsim} games/match): "
         f"{-result.fun:.4f}")

    is_unimodal, implied_optimum, r_squared = recheck_epsilon_shape(
        best_params, args.opponent, args.numsim, args.replicates, args.base_seed, args.workers)
    print(f"\nweight_energy_advantage shape check (quadratic fit, R^2={r_squared:.3f}):")
    print(f"  {'concave (single peak)' if is_unimodal else 'NOT concave -- flat or convex fit'}")
    print(f"  implied optimum weight_energy_advantage (fit vertex): {implied_optimum:.3f}")

    personality_flags = check_personality_flags(best_params)
    if not is_unimodal:
        personality_flags.append("weight_energy_advantage is not single-peaked at the found "
                                 "point (see the shape check above) -- review before shipping.")

    if personality_flags:
        print("\n*** Personality flags (review before shipping, do not auto-ship past these): ***")
        for flag in personality_flags:
            print(f"  - {flag}")

    equivalence_note = check_balanced_equivalence(best_params)
    if equivalence_note:
        print(f"\n*** Design-question finding (not a flag): {equivalence_note} ***")

    print(f"\nRe-validating winner with more games "
         f"({args.validate_replicates * 2} matches x {args.validate_numsim} games)...",
         file=sys.stderr)
    val_seeds = replicate_seeds(args.base_seed + 100000, args.validate_replicates)
    jobs = []
    for seed in val_seeds:
        jobs.append(dict(numsim=args.validate_numsim, seed=seed, agent_a="heuristic",
                         agent_b=args.opponent, params_a=best_params, params_b=dict(DEFAULTS)))
        jobs.append(dict(numsim=args.validate_numsim, seed=seed, agent_a=args.opponent,
                         agent_b="heuristic", params_a=dict(DEFAULTS), params_b=best_params))
    df = run_many(jobs, max_workers=args.workers)
    wins, n = heuristic_win_rate(df)
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
            "epsilon_unimodal": is_unimodal, "epsilon_implied_optimum": implied_optimum,
            "epsilon_fit_r_squared": r_squared, "personality_flags": personality_flags,
            "balanced_equivalence_note": equivalence_note,
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
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="heuristic", agent_b="heuristic",
                             params_a=params_map[names[i]], params_b=params_map[names[j]],
                             _i=i, _j=j))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="heuristic", agent_b="heuristic",
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
            jobs.append(dict(numsim=args.numsim, seed=seed, agent_a="heuristic",
                             agent_b=args.opponent, params_a=params, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=args.numsim, seed=seed, agent_a=args.opponent,
                             agent_b="heuristic", params_a=dict(DEFAULTS), params_b=params))
        df = run_many(jobs, max_workers=args.workers)
        wins, n = heuristic_win_rate(df)
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
                       help="default: search the four free params (weight_cash_advantage "
                            "stays pinned unless explicitly named here)")
    p_opt.add_argument("--identity-safe", action="store_true",
                       help="search BOUNDS_IDENTITY_SAFE instead of BOUNDS -- keeps "
                            "weight_energy_advantage away from 0 and weight_taper_exponent "
                            "in [0, 2] rather than letting the search erode this agent's "
                            "designed character (see module docstring)")
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
