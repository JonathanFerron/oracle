#!/usr/bin/env python3
"""Calibration driver for the A7 Hybrid HBT ("The Grandmaster") AI agent's
thirty-four tunable parameters (HBTParams -- see src/ai_strat/ai_strat_hbt.h).

Drives bin/calib_hbt, which links the game engine directly and prints one
CSV result line per invocation. All the actual game simulation happens in
the compiled C binary; this script only orchestrates subprocess calls and
aggregates/fits/searches the results. Structure and internals (run_match's
**tags-at-the-source fix, summarize_selfplay's direct win/game tracking, the
seed-offset convention, --print-defaults-sourced DEFAULTS) are carried over
verbatim from aicalibsrc/tactical/calibrate_tactical.py, the newest of the
six existing drivers.

DEFAULTS below is read once, at import time, from `bin/calib_hbt
--print-defaults`, so it can never drift from the shipped HBT_DEFAULTS.

This agent is a fixed three-layer synthesis (see ai_strat_hbt.h's header
comment): A4 Balanced Rules' resource targets enter as a soft penalty, A6
Tactical's aggression factor modulates A5 Heuristic's weights, and A5's
enumerate-and-rank shape does the move selection, with A3 Borealis's
lethal-combo hold layered on top. Two fields are PINNED, never searched, for
two different reasons:
  - weight_cash_advantage (delta): scale-invariance redundancy, the same
    reason A5 pins it (ai_strat_heuristic.h) -- the argmax of a weighted sum
    is invariant to a positive rescaling of all three weights.
  - hold_lethal_combos: this agent exists specifically to be combo-aware "to
    a good extent" (the design decision that added A3's hold mechanism to
    this agent's scope at all, ai_strat_hbt.h's header comment) -- it is a
    fixed design choice, not a dial to search away.

Thirty-four parameters is roughly double A6's sixteen (the previous largest
search space) -- `optimize --params <subset>` supports the staged
calibration plan (couplings first, then H's own weights, then a joint pass
only if the staged result falls short) documented in doc/changelog.md.

This agent's identity check (check_personality_flags()) verifies all THREE
layers are still doing work, since about.md's framing is "the personality
IS the fixed three-layer synthesis": T's aggression_factor must still have a
healthy range across a synthetic battery (A6's own test, reused verbatim);
B's penalty weights and target slopes must not have eroded toward 0 (A4's
documented failure mode); H's epsilon/delta ratio and taper must stay
sane (A5's own tests).

Four subcommands, same as every other driver:

  sweep     Univariate diagnostic: hold every parameter at its default
            except one, vary that one, play vs a fixed --opponent (default
            "rand"), both seats, with binomial confidence intervals.

  optimize  Black-box search (scipy.optimize.differential_evolution) over
            some or all free parameters, maximizing win rate against a
            fixed --opponent (default "borealis" -- the rating-50 anchor).

  selfplay  Round-robin among a small set of NAMED candidate parameter sets,
            both seat orders, --ai.a=hbt vs --ai.b=hbt. Reports a
            Bradley-Terry fit.

  validate  Compare one candidate parameter set against the shipped
            defaults, vs a chosen opponent, both seats.

Candidate parameter sets are given as JSON files (a full or partial
HBTParams dict; missing fields fall back to the compiled defaults) or the
literal string "defaults". `optimize`'s own output file is directly usable
as a `selfplay`/`validate` candidate (it has a "best_params" key, which
these commands know to unwrap).

Examples:
  ./calibrate_hbt.py sweep --param penalty_cash_weight --opponent borealis \\
      --numsim 2000 --replicates 4 --plot
  ./calibrate_hbt.py optimize --opponent borealis \\
      --params aggr_energy_gain aggr_resource_fade critical_epsilon_mult \\
               target_aggr_cash_scale target_aggr_cards_scale \\
               penalty_cash_weight penalty_cards_weight defense_stdev_mult \\
      --numsim 500 --replicates 2 --maxiter 15 --popsize 12
  ./calibrate_hbt.py selfplay --candidates defaults results/optimize_borealis.json
  ./calibrate_hbt.py validate --candidate results/optimize_borealis.json --opponent rand
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

BINARY = Path(__file__).resolve().parent.parent.parent / "bin" / "calib_hbt"
RESULTS_DIR = Path(__file__).resolve().parent / "results"

# Declared struct order in HBTParams -- must match parse_params() in
# calib_hbt.c.
PARAM_NAMES = [
    "weight_energy_advantage", "weight_cards_advantage", "weight_cash_advantage",
    "weight_taper_exponent", "opp_card_discount",
    "phase_mid_threshold", "phase_late_threshold", "phase_critical_threshold",
    "aggression_energy_diff_weight", "aggression_opp_late_bonus",
    "aggression_opp_critical_bonus", "aggression_self_late_penalty",
    "aggression_self_critical_penalty", "aggression_hand_power_bonus",
    "aggression_hand_power_penalty", "aggression_cash_surplus_threshold",
    "aggression_cash_surplus_bonus",
    "aggr_energy_gain", "aggr_resource_fade", "critical_epsilon_mult",
    "target_cash_slope", "target_cash_intercept",
    "target_cards_slope", "target_cards_intercept",
    "late_game_aggro", "lethal_horizon",
    "target_aggr_cash_scale", "target_aggr_cards_scale",
    "penalty_cash_weight", "penalty_cards_weight",
    "hold_lethal_combos", "lethal_combo_bonus", "lethal_hold_ceiling",
    "defense_stdev_mult",
]

_INT_PARAMS = {
    "phase_mid_threshold", "phase_late_threshold", "phase_critical_threshold",
    "aggression_cash_surplus_threshold", "lethal_horizon",
    "hold_lethal_combos", "lethal_combo_bonus", "lethal_hold_ceiling",
}

# Pinned -- never free in `optimize` regardless of --params/--identity-safe.
# See module docstring for why each is pinned (different reasons).
PINNED_PARAM_NAMES = {"weight_cash_advantage", "hold_lethal_combos"}
FREE_PARAM_NAMES = [n for n in PARAM_NAMES if n not in PINNED_PARAM_NAMES]


def _load_defaults_from_binary():
    if not BINARY.exists():
        # Fall back to the compiled-in values documented in
        # ai_strat_hbt.c's HBT_DEFAULTS, so --help and argument parsing
        # still work before `make calib_hbt` has run.
        return {
            "weight_energy_advantage": 0.34929208, "weight_cards_advantage": 1.96227051,
            "weight_cash_advantage": 1.0, "weight_taper_exponent": 0.10115113,
            "opp_card_discount": 0.98660043,
            "phase_mid_threshold": 67, "phase_late_threshold": 41,
            "phase_critical_threshold": 18,
            "aggression_energy_diff_weight": 0.0008022129,
            "aggression_opp_late_bonus": 0.1262423,
            "aggression_opp_critical_bonus": 0.2819330,
            "aggression_self_late_penalty": 0.0530097,
            "aggression_self_critical_penalty": 0.1475105,
            "aggression_hand_power_bonus": 0.2479543,
            "aggression_hand_power_penalty": 0.1542592,
            "aggression_cash_surplus_threshold": 10,
            "aggression_cash_surplus_bonus": 0.2301680,
            "aggr_energy_gain": 0.30, "aggr_resource_fade": 0.30,
            "critical_epsilon_mult": 1.50,
            "target_cash_slope": 0.08096868, "target_cash_intercept": -2.72849536,
            "target_cards_slope": 0.03572451, "target_cards_intercept": -0.99130504,
            "late_game_aggro": 2.09102475, "lethal_horizon": 9,
            "target_aggr_cash_scale": 0.40, "target_aggr_cards_scale": 0.30,
            "penalty_cash_weight": 0.50, "penalty_cards_weight": 0.50,
            "hold_lethal_combos": True, "lethal_combo_bonus": 24,
            "lethal_hold_ceiling": 38, "defense_stdev_mult": 1.233,
        }
    result = subprocess.run([str(BINARY), "--print-defaults"],
                            capture_output=True, text=True, check=True)
    return json.loads(result.stdout)


DEFAULTS = _load_defaults_from_binary()

# Search space for `optimize` and default sweep grids for `sweep`. The five
# fields flagged in ai_strat_hbt.h's "commit to the synthesis" design
# decision (penalty_cash_weight, penalty_cards_weight, aggr_energy_gain,
# aggr_resource_fade, and both target_*_slope fields) carry a non-zero floor
# directly in BOUNDS, not only in BOUNDS_IDENTITY_SAFE -- unlike A4/A5, this
# agent does not wait for a free search to erode a mechanism before
# constraining it; the constraint is a starting decision (see the plan's
# "decisions taken" section, doc/changelog.md).
BOUNDS = {
    "weight_energy_advantage": (0.0, 10.0),
    "weight_cards_advantage": (0.0, 15.0),
    "weight_cash_advantage": (0.0, 3.0),  # pinned -- entry kept for completeness
    "weight_taper_exponent": (0.0, 4.0),
    "opp_card_discount": (0.0, 3.0),
    "phase_mid_threshold": (50, 90),
    "phase_late_threshold": (20, 60),
    "phase_critical_threshold": (5, 30),
    "aggression_energy_diff_weight": (0.0, 0.02),
    "aggression_opp_late_bonus": (0.0, 0.5),
    "aggression_opp_critical_bonus": (0.0, 0.6),
    "aggression_self_late_penalty": (0.0, 0.5),
    "aggression_self_critical_penalty": (0.0, 0.6),
    "aggression_hand_power_bonus": (0.0, 0.5),
    "aggression_hand_power_penalty": (0.0, 0.5),
    "aggression_cash_surplus_threshold": (10, 70),
    "aggression_cash_surplus_bonus": (0.0, 0.4),
    "aggr_energy_gain": (0.05, 1.0),
    "aggr_resource_fade": (0.05, 1.0),
    "critical_epsilon_mult": (1.0, 3.0),
    "target_cash_slope": (0.02, 0.35),
    "target_cash_intercept": (-5.0, 5.0),
    "target_cards_slope": (0.01, 0.10),
    "target_cards_intercept": (-2.0, 2.0),
    "late_game_aggro": (1.0, 2.5),
    "lethal_horizon": (0, 40),
    "target_aggr_cash_scale": (0.0, 1.0),
    "target_aggr_cards_scale": (0.0, 1.0),
    "penalty_cash_weight": (0.1, 2.0),
    "penalty_cards_weight": (0.1, 2.0),
    "hold_lethal_combos": (0, 1),  # pinned -- entry kept for completeness
    "lethal_combo_bonus": (10, 40),
    "lethal_hold_ceiling": (15, 50),
    "defense_stdev_mult": (-2.0, 2.0),
}

# Identity-constrained search space -- available as an escape hatch via
# `optimize --identity-safe` if check_personality_flags() ever fires despite
# BOUNDS' own floors above. Tightens the T/B/H core fields the same way
# A4's/A5's/A6's own BOUNDS_IDENTITY_SAFE do.
BOUNDS_IDENTITY_SAFE = {
    "weight_energy_advantage": (0.2, 5.0),
    "weight_cards_advantage": (0.0, 2.0),
    "weight_taper_exponent": (0.0, 2.0),
    "opp_card_discount": (0.0, 2.0),
    "phase_mid_threshold": (60, 85),
    "phase_late_threshold": (25, 55),
    "phase_critical_threshold": (8, 25),
    "aggression_energy_diff_weight": (0.0015, 0.02),
    "aggression_opp_late_bonus": (0.05, 0.5),
    "aggression_opp_critical_bonus": (0.1, 0.6),
    "aggression_self_late_penalty": (0.05, 0.5),
    "aggression_self_critical_penalty": (0.1, 0.6),
    "aggression_hand_power_bonus": (0.05, 0.5),
    "aggression_hand_power_penalty": (0.05, 0.5),
    "aggression_cash_surplus_threshold": (15, 60),
    "aggression_cash_surplus_bonus": (0.05, 0.4),
    "aggr_energy_gain": (0.1, 0.6),
    "aggr_resource_fade": (0.1, 0.6),
    "critical_epsilon_mult": (1.1, 2.0),
    "target_cash_slope": (0.05, 0.25),
    "target_cards_slope": (0.02, 0.08),
    "late_game_aggro": (1.0, 2.5),
    "penalty_cash_weight": (0.2, 1.5),
    "penalty_cards_weight": (0.2, 1.5),
    "lethal_combo_bonus": (15, 35),
    "lethal_hold_ceiling": (20, 45),
    "defense_stdev_mult": (-1.5, 1.5),
}

SWEEP_DEFAULTS = {
    "weight_energy_advantage": [0.0, 0.25, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0],
    "weight_cards_advantage": [0.0, 0.15, 1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0],
    "weight_cash_advantage": [0.0, 0.5, 1.0, 1.5, 2.0, 3.0],
    "weight_taper_exponent": [0.0, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0],
    "opp_card_discount": [0.0, 0.5, 1.0, 1.5, 2.0, 3.0],
    "phase_mid_threshold": [50, 60, 70, 75, 80, 90],
    "phase_late_threshold": [20, 30, 40, 50, 60],
    "phase_critical_threshold": [5, 10, 15, 20, 30],
    "aggression_energy_diff_weight": [0.0, 0.001, 0.003, 0.006, 0.01, 0.02],
    "aggression_opp_late_bonus": [0.0, 0.1, 0.15, 0.25, 0.4],
    "aggression_opp_critical_bonus": [0.0, 0.15, 0.3, 0.45, 0.6],
    "aggression_self_late_penalty": [0.0, 0.1, 0.2, 0.35, 0.5],
    "aggression_self_critical_penalty": [0.0, 0.2, 0.4, 0.5, 0.6],
    "aggression_hand_power_bonus": [0.0, 0.1, 0.2, 0.35, 0.5],
    "aggression_hand_power_penalty": [0.0, 0.1, 0.2, 0.35, 0.5],
    "aggression_cash_surplus_threshold": [10, 25, 40, 55, 70],
    "aggression_cash_surplus_bonus": [0.0, 0.1, 0.15, 0.25, 0.4],
    "aggr_energy_gain": [0.0, 0.1, 0.3, 0.5, 0.75, 1.0],
    "aggr_resource_fade": [0.0, 0.1, 0.3, 0.5, 0.75, 1.0],
    "critical_epsilon_mult": [1.0, 1.25, 1.5, 2.0, 2.5, 3.0],
    "target_cash_slope": [0.0, 0.02, 0.08, 0.15, 0.25, 0.35],
    "target_cash_intercept": [-5.0, -2.5, 0.0, 2.5, 5.0],
    "target_cards_slope": [0.0, 0.01, 0.036, 0.06, 0.10],
    "target_cards_intercept": [-2.0, -1.0, 0.0, 1.0, 2.0],
    "late_game_aggro": [1.0, 1.2, 1.5, 2.0, 2.5],
    "lethal_horizon": [0, 5, 9, 15, 25, 40],
    "target_aggr_cash_scale": [0.0, 0.2, 0.4, 0.6, 0.8, 1.0],
    "target_aggr_cards_scale": [0.0, 0.2, 0.3, 0.5, 0.7, 1.0],
    "penalty_cash_weight": [0.1, 0.25, 0.5, 1.0, 1.5, 2.0],
    "penalty_cards_weight": [0.1, 0.25, 0.5, 1.0, 1.5, 2.0],
    "hold_lethal_combos": [0, 1],
    "lethal_combo_bonus": [10, 16, 24, 32, 40],
    "lethal_hold_ceiling": [15, 25, 38, 45, 50],
    "defense_stdev_mult": [-2.0, -1.233, -0.5, 0.0, 0.5, 1.233, 2.0],
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
    a full/partial HBTParams dict, or `optimize`'s own output (which nests
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
    """One call to bin/calib_hbt. Safe to run in a worker.

    **tags are arbitrary caller bookkeeping (e.g. sweep's `_value`,
    selfplay's `_i`/`_j`) merged straight into the returned dict instead of
    being tracked in a separate same-order list -- run_many()'s
    ProcessPoolExecutor + as_completed() returns results in COMPLETION
    order, not submission order, so tagging at the source (rather than
    reattaching by list position afterward) is what keeps this correct
    under real parallelism.
    """
    if not BINARY.exists():
        raise FileNotFoundError(f"{BINARY} not found -- run `make calib_hbt` first")

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


def hbt_win_rate(df):
    hbt_in_a = df["agent_a"] == "hbt"
    wins = int(np.where(hbt_in_a, df["wins_a"], df["wins_b"]).sum())
    n = int((df["wins_a"] + df["wins_b"] + df["draws"]).sum())
    return wins, n


# ---------------------------------------------------------------------------
# sweep: one param varied, hbt vs a fixed opponent, both seats
# ---------------------------------------------------------------------------

def build_sweep_jobs(param, values, opponent, numsim, seeds):
    jobs = []
    for v in values:
        p = merge_params({param: v})
        for seed in seeds:
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="hbt", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS), _value=v, _hbt_in_a=True))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="hbt",
                             params_a=dict(DEFAULTS), params_b=p, _value=v, _hbt_in_a=False))
    return jobs


def cmd_sweep(args):
    values = args.values or SWEEP_DEFAULTS[args.param]
    seeds = replicate_seeds(args.base_seed, args.replicates)
    jobs = build_sweep_jobs(args.param, values, args.opponent, args.numsim, seeds)

    print(f"Running {len(jobs)} matches "
         f"({len(values)} values x {args.replicates} replicates x 2 seats, "
         f"vs {args.opponent})...", file=sys.stderr)

    df = run_many(jobs, max_workers=args.workers)
    df["hbt_wins"] = np.where(df["_hbt_in_a"], df["wins_a"], df["wins_b"])
    df["n"] = df["wins_a"] + df["wins_b"] + df["draws"]

    grouped = df.groupby("_value").agg(wins=("hbt_wins", "sum"), n=("n", "sum")).reset_index()
    grouped["win_rate"] = grouped["wins"] / grouped["n"]
    grouped[["ci_lo", "ci_hi"]] = grouped.apply(
        lambda r: pd.Series(wilson_ci(r["wins"], r["n"])), axis=1)
    grouped = grouped.rename(columns={"_value": args.param}).sort_values(args.param)

    print(f"\nSweep of {args.param} (hbt vs {args.opponent}, both seats, "
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
    ax.set_title(f"The Grandmaster: {param} sweep (95% Wilson CI)")
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
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="hbt", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="hbt",
                             params_a=dict(DEFAULTS), params_b=p))
        df = run_many_threaded(jobs, executor)
        wins, n = hbt_win_rate(df)
        rate = wins / n if n else 0.0
        progress["evals"] += 1
        print(f"\r  eval {progress['evals']}: win_rate={rate:.4f}",
             end="", file=sys.stderr, flush=True)
        return -rate  # differential_evolution minimizes
    return objective


# Synthetic battery for check_personality_flags()'s T-layer (aggression)
# range test -- identical shape to A6's own AGGRESSION_BATTERY
# (aicalibsrc/tactical/calibrate_tactical.py), spanning the input space
# independent of any real game position. Each tuple is
# (own_energy, opp_energy, my_hand_power, opp_estimated_power, own_cash).
AGGRESSION_BATTERY = [
    (90, 10, 30.0, 10.0, 50),   # way ahead, opponent critical, strong hand, cash surplus
    (10, 90, 10.0, 30.0, 5),    # way behind, self critical, weak hand, no surplus
    (50, 50, 20.0, 20.0, 20),   # even, neutral hand, no surplus
    (10, 10, 20.0, 20.0, 20),   # both critical, neutral hand
    (90, 90, 20.0, 20.0, 20),   # both healthy, neutral hand
    (50, 50, 40.0, 10.0, 20),   # even energy, dominant hand
    (50, 50, 10.0, 40.0, 20),   # even energy, weak hand
]


def game_phase_py(energy, params):
    if energy >= params["phase_mid_threshold"]:
        return "EARLY"
    if energy >= params["phase_late_threshold"]:
        return "MID"
    if energy >= params["phase_critical_threshold"]:
        return "LATE"
    return "CRITICAL"


def aggression_factor_py(own_energy, opp_energy, my_hand_power, opp_estimated_power,
                         own_cash, params):
    """Pure-Python mirror of calculate_aggression_factor() in
    ai_strat_hbt_enum.c (ported verbatim from A6's own formula) -- used only
    to sensitivity-check candidate parameter sets without invoking the C
    binary per synthetic position."""
    my_phase = game_phase_py(own_energy, params)
    opp_phase = game_phase_py(opp_energy, params)

    aggression = 0.5
    aggression += (own_energy - opp_energy) * params["aggression_energy_diff_weight"]

    if opp_phase == "CRITICAL":
        aggression += params["aggression_opp_critical_bonus"]
    elif opp_phase == "LATE":
        aggression += params["aggression_opp_late_bonus"]

    if my_phase == "CRITICAL":
        aggression -= params["aggression_self_critical_penalty"]
    elif my_phase == "LATE":
        aggression -= params["aggression_self_late_penalty"]

    if my_hand_power > opp_estimated_power * 1.5:
        aggression += params["aggression_hand_power_bonus"]
    if my_hand_power < opp_estimated_power * 0.7:
        aggression -= params["aggression_hand_power_penalty"]

    if own_cash > params["aggression_cash_surplus_threshold"]:
        aggression += params["aggression_cash_surplus_bonus"]

    return max(0.0, min(1.0, aggression))


def check_personality_flags(best_params):
    """Returns a list of human-readable warning strings for anything an
    optimizer result should not silently ship with. Never auto-rejects --
    flags for review, same policy as every other agent's precedent.

    Checks all THREE layers are still doing work (about.md's framing: "the
    personality IS the fixed three-layer synthesis"):
      - T alive: aggression_factor range across AGGRESSION_BATTERY, and
        phase-threshold ordering (both ported verbatim from A6's own check).
      - B alive: penalty weights and target slopes haven't eroded toward 0
        (A4's own documented failure mode -- see ai_strat_hbt.h's header
        comment on why this agent uses a penalty instead of A4's filter).
      - H alive: epsilon/delta ratio and taper exponent stay in a sane
        range (A5's own checks)."""
    flags = []

    # --- T alive ---
    values = [aggression_factor_py(*state, best_params) for state in AGGRESSION_BATTERY]
    spread = max(values) - min(values)
    if spread < 0.15:
        flags.append(f"[T] aggression_factor range across the synthetic battery is only "
                     f"{spread:.3f} (< 0.15) -- the Tactical weighting layer has decayed "
                     f"toward a static value; this agent is no longer reading the position "
                     f"the way about.md's synthesis requires.")

    if best_params["phase_critical_threshold"] >= best_params["phase_late_threshold"]:
        flags.append(f"[T] phase_critical_threshold ({best_params['phase_critical_threshold']}) "
                     f">= phase_late_threshold ({best_params['phase_late_threshold']}) -- "
                     f"the phase ordering has inverted.")
    if best_params["phase_late_threshold"] >= best_params["phase_mid_threshold"]:
        flags.append(f"[T] phase_late_threshold ({best_params['phase_late_threshold']}) >= "
                     f"phase_mid_threshold ({best_params['phase_mid_threshold']}) -- phase "
                     f"ordering has inverted.")

    # --- B alive ---
    if best_params["penalty_cash_weight"] <= BOUNDS["penalty_cash_weight"][0] + 1e-6:
        flags.append(f"[B] penalty_cash_weight pinned at its lower bound "
                     f"({best_params['penalty_cash_weight']:.4f}) -- the Balanced Rules "
                     f"resource-shortfall penalty on cash has been optimized away.")
    if best_params["penalty_cards_weight"] <= BOUNDS["penalty_cards_weight"][0] + 1e-6:
        flags.append(f"[B] penalty_cards_weight pinned at its lower bound "
                     f"({best_params['penalty_cards_weight']:.4f}) -- the Balanced Rules "
                     f"resource-shortfall penalty on hand size has been optimized away.")
    if best_params["target_cash_slope"] <= BOUNDS["target_cash_slope"][0] + 1e-6:
        flags.append(f"[B] target_cash_slope pinned at its lower bound "
                     f"({best_params['target_cash_slope']:.4f}) -- A4's documented "
                     f"starvation-avoidance failure mode (target erodes to near-zero, see "
                     f"ai_strat_balanced_rules.c's BALANCED_DEFAULTS comment).")
    if best_params["target_cards_slope"] <= BOUNDS["target_cards_slope"][0] + 1e-6:
        flags.append(f"[B] target_cards_slope pinned at its lower bound "
                     f"({best_params['target_cards_slope']:.4f}) -- same erosion pattern as "
                     f"target_cash_slope above.")

    # --- H alive ---
    epsilon = best_params["weight_energy_advantage"]
    delta = best_params["weight_cash_advantage"]
    if delta > 0 and epsilon / delta < 0.2:
        flags.append(f"[H] weight_energy_advantage/weight_cash_advantage = "
                     f"{epsilon / delta:.3f} (< 0.2) -- energy has become nearly "
                     f"irrelevant relative to cash in the ranking layer.")
    if best_params["weight_taper_exponent"] > 3.0:
        flags.append(f"[H] weight_taper_exponent = {best_params['weight_taper_exponent']:.3f} "
                     f"(> 3.0) -- gamma/delta are effectively zero except near full opponent "
                     f"energy, a degenerate taper rather than a smooth one.")

    return flags


def cmd_optimize(args):
    if args.identity_safe:
        bounds_table = BOUNDS_IDENTITY_SAFE
        free_names = [n for n in (args.params or list(BOUNDS_IDENTITY_SAFE))
                     if n not in PINNED_PARAM_NAMES]
    else:
        bounds_table = BOUNDS
        free_names = [n for n in (args.params or list(FREE_PARAM_NAMES))
                     if n not in PINNED_PARAM_NAMES]
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
        jobs.append(dict(numsim=args.validate_numsim, seed=seed, agent_a="hbt",
                         agent_b=args.opponent, params_a=best_params, params_b=dict(DEFAULTS)))
        jobs.append(dict(numsim=args.validate_numsim, seed=seed, agent_a=args.opponent,
                         agent_b="hbt", params_a=dict(DEFAULTS), params_b=best_params))
    df = run_many(jobs, max_workers=args.workers)
    wins, n = hbt_win_rate(df)
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
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="hbt", agent_b="hbt",
                             params_a=params_map[names[i]], params_b=params_map[names[j]],
                             _i=i, _j=j))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="hbt", agent_b="hbt",
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
            jobs.append(dict(numsim=args.numsim, seed=seed, agent_a="hbt",
                             agent_b=args.opponent, params_a=params, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=args.numsim, seed=seed, agent_a=args.opponent,
                             agent_b="hbt", params_a=dict(DEFAULTS), params_b=params))
        df = run_many(jobs, max_workers=args.workers)
        wins, n = hbt_win_rate(df)
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
                       help="default: search every free (non-pinned) param -- see "
                            "FREE_PARAM_NAMES, or under --identity-safe every key in "
                            "BOUNDS_IDENTITY_SAFE. Pass an explicit subset for the staged "
                            "calibration plan (couplings first, then H's own weights, then "
                            "T's aggression formula only if needed -- doc/changelog.md)")
    p_opt.add_argument("--identity-safe", action="store_true",
                       help="search BOUNDS_IDENTITY_SAFE instead of BOUNDS -- an escape "
                            "hatch if check_personality_flags() fires despite BOUNDS' own "
                            "floors (see module docstring)")
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
