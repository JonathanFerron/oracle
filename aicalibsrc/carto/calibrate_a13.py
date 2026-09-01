#!/usr/bin/env python3
"""Calibration driver for A13 Cartographer's ten new parameters (A13Params
-- see src/ai_strat/ai_strat_a13.h).

Drives bin/calib_a13, which links the game engine directly and prints one
CSV result line per invocation. All the actual game simulation happens in
the compiled C binary; this script only orchestrates subprocess calls and
aggregates/fits/searches the results. Structure and internals (the
subcommand shape, run_match's tags-at-the-source fix, the seed-offset
convention, --print-defaults-sourced DEFAULTS) are carried over from
aicalibsrc/hbt2ply/calibrate_hbt2ply.py, itself carried from
aicalibsrc/hbt/calibrate_hbt.py.

DEFAULTS below is read once, at import time, from `bin/calib_a13
--print-defaults`, so it can never drift from the shipped compiled values.

Like A9 HBT 2-Ply's driver, this one's PARAM_NAMES carries the full 44-field
A13Params shape (the 34 HBTParams fields inherited from A7, plus this
agent's own 10) because the C harness needs all 44 positionally -- but the
34 inherited fields are HARD PINNED (PINNED_PARAM_NAMES), not just
defaulted: A13 re-derives none of A7's own tuning (ideas/A13 .../about.md),
so `--param`/`--params` only ever accept this agent's own 10 fields
(FREE_PARAM_NAMES). There is no `--identity-safe` escape hatch here, for the
same reason A9's driver has none: there is nothing of A7's own tuning to
erode. The ONE exception, at Stage 4 only, is `defense_stdev_mult` -- see
STAGE_4_EXTRA below and ai_strat_a13.h's struct comment on why that one
inherited field is re-fittable (Layer R turns it from THE value into a
BASELINE of a now state-dependent quantity).

Of the 10 free fields, `race_use_belief_opp` is a structural boolean choice,
not a continuous dial -- differential_evolution can't search it directly, so
it is swept (both values, `sweep --param race_use_belief_opp`), never part
of an `optimize` run (CONTINUOUS_PARAM_NAMES excludes it; passing it to
`optimize --params` is rejected by argparse `choices`).

Per the staged calibration plan (ideas/A13 .../about.md, Step 5 of the
approved implementation plan), `optimize` requires --params explicitly (no
default free set) so a run can't accidentally skip the staging discipline
that burned A7's own stage 2 (freeing everything at once found no gain and
let weight_cards_advantage drift for nothing):

  Stage 1 (race, belief-independent): race_scale, race_stdev_ahead,
      race_stdev_behind[, race_eps_gain] -- vs `borealis`. Sweep first.
  Stage 2 (draw valuation, Layer R frozen at stage 1's result):
      belief_draw_weight, belief_reshuffle_trust -- vs `borealis`.
  Stage 3 (the risky half -- run sweep --param belief_opp_block_trust and
      hplus_trust FIRST, exactly A9's reply_trust protocol, watching for a
      MONOTONIC decline vs both `borealis` and `hbt` before ever running
      `optimize` on this stage): belief_opp_block_trust, hplus_trust,
      hplus_block_combo.
  Stage 4 (optional joint re-fit): the 10 new dials plus defense_stdev_mult
      ONLY -- never the other 33 base fields.

Opponent rule (A9's hardest lesson): `optimize` against `borealis` (every
other agent's convention, and the rating-50 anchor), but VALIDATE head-to-
head against `hbt` at every stage -- `hbt` is this agent's actual ship gate
(A13 must beat A7, not just clear a Borealis-relative bar; a Borealis-
relative win says nothing about the pairwise matchup, as A7 vs A5 and A9 vs
A7 both demonstrated). Never optimize directly against `hbt`.

check_personality_flags() mirrors about.md's risk ranking: all Layer-R dials
collapsed to 0 ("race arithmetic optimized into irrelevance"), all belief/
hplus dials at 0 ("the belief layer optimized into irrelevance"),
race_stdev_ahead converging to race_stdev_behind ("state-dependence
collapsed to A7's constant"), race_scale pinned at its ceiling ("saturated
to a constant"), and a sign check against ai_strat_a13.h's documented
convention (race_stdev_ahead > 0, race_stdev_behind < 0 -- flagged as
informational if violated, not auto-rejected, since a real calibration
finding overturning the documented intuition is possible, same as A5's
gamma drift turning out legitimate).

Four subcommands, same as every other driver:

  sweep     Univariate diagnostic: hold every parameter at its default
            except one, vary that one, play vs a fixed --opponent (default
            "rand"), both seats, with binomial confidence intervals.

  optimize  Black-box search (scipy.optimize.differential_evolution) over
            an EXPLICITLY REQUIRED --params subset, maximizing win rate
            against a fixed --opponent (default "borealis").

  selfplay  Round-robin among a small set of NAMED candidate parameter sets,
            both seat orders, --ai.a=carto vs --ai.b=carto. Reports a
            Bradley-Terry fit.

  validate  Compare one candidate parameter set against the shipped
            defaults, vs a chosen opponent, both seats.

Candidate parameter sets are given as JSON files (a full or partial
A13Params dict; missing fields fall back to the compiled defaults) or the
literal string "defaults". `optimize`'s own output file is directly usable
as a `selfplay`/`validate` candidate (it has a "best_params" key, which
these commands know to unwrap).

Examples:
  ./calibrate_a13.py sweep --param race_scale --opponent borealis \\
      --numsim 2000 --replicates 4 --plot
  ./calibrate_a13.py optimize --opponent borealis \\
      --params race_scale race_stdev_ahead race_stdev_behind \\
      --numsim 500 --replicates 2 --maxiter 15 --popsize 12
  ./calibrate_a13.py selfplay --candidates defaults results/optimize_borealis.json
  ./calibrate_a13.py validate --candidate results/optimize_borealis.json --opponent hbt
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

BINARY = Path(__file__).resolve().parent.parent.parent / "bin" / "calib_a13"
RESULTS_DIR = Path(__file__).resolve().parent / "results"

# Declared struct order in A13Params: the 34 HBTParams fields (base), then
# this agent's own 10 -- must match parse_params() in calib_a13.c.
BASE_PARAM_NAMES = [
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
NEW_PARAM_NAMES = [
    "race_scale", "race_stdev_ahead", "race_stdev_behind", "race_eps_gain",
    "race_use_belief_opp",
    "belief_draw_weight", "belief_reshuffle_trust",
    "belief_opp_block_trust", "hplus_trust", "hplus_block_combo",
]
PARAM_NAMES = BASE_PARAM_NAMES + NEW_PARAM_NAMES

_INT_PARAMS = {
    "phase_mid_threshold", "phase_late_threshold", "phase_critical_threshold",
    "aggression_cash_surplus_threshold", "lethal_horizon",
    "hold_lethal_combos", "lethal_combo_bonus", "lethal_hold_ceiling",
    "race_use_belief_opp",  # bool -> "0"/"1", matching calib_a13.c's strtol(...) != 0
}

# Hard pin: A13 re-derives none of A7's 34 fields except defense_stdev_mult
# (the STAGE_4_EXTRA exception below) -- Layer R turns that one field from
# THE value into a BASELINE of a now state-dependent quantity, ai_strat_a13.h's
# struct comment has the full reasoning. Not an "identity-safe" soft
# constraint the way other agents' PINNED_PARAM_NAMES entries are -- there is
# no escape hatch for the other 33 fields here. defense_stdev_mult is only
# ever actually searched when a stage explicitly names it in --params (which
# `optimize` always requires anyway, see module docstring), never by default.
PINNED_PARAM_NAMES = set(BASE_PARAM_NAMES) - {"defense_stdev_mult"}
FREE_PARAM_NAMES = [n for n in PARAM_NAMES if n not in PINNED_PARAM_NAMES]

# race_use_belief_opp is a structural bool, not a continuous dial -- excluded
# from `optimize`'s searchable set (differential_evolution needs bounds, and
# a bool has none here); sweep both values instead.
CONTINUOUS_PARAM_NAMES = [n for n in FREE_PARAM_NAMES if n != "race_use_belief_opp"]

# Named stage groupings, for reference/documentation only -- `optimize`
# still requires --params explicitly (see module docstring for why).
STAGE_1_PARAMS = ["race_scale", "race_stdev_ahead", "race_stdev_behind", "race_eps_gain"]
STAGE_2_PARAMS = ["belief_draw_weight", "belief_reshuffle_trust"]
STAGE_3_PARAMS = ["belief_opp_block_trust", "hplus_trust", "hplus_block_combo"]
STAGE_4_EXTRA = ["defense_stdev_mult"]  # the one base field ever freed, stage 4 only


def _load_defaults_from_binary():
    if not BINARY.exists():
        # Fall back to the compiled-in values (ai_strat_a13.c's
        # a13_get_default_params(), and A7's own HBT_DEFAULTS for the 34
        # base fields) so --help and argument parsing still work before
        # `make calib_a13` has run. Labelled here because it WILL rot the
        # moment either DEFAULTS changes and this fallback isn't updated --
        # see aicalibsrc/hbt/README.md's pitfall #2 on exactly this failure
        # mode (a 9.2x-drifted luna_value once shipped this way).
        return {
            "weight_energy_advantage": 0.30140110, "weight_cards_advantage": 1.94846331,
            "weight_cash_advantage": 1.0, "weight_taper_exponent": 0.08984036,
            "opp_card_discount": 1.34949438,
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
            "aggr_energy_gain": 0.11170919, "aggr_resource_fade": 0.10308390,
            "critical_epsilon_mult": 1.91039995,
            "target_cash_slope": 0.08096868, "target_cash_intercept": -2.72849536,
            "target_cards_slope": 0.03572451, "target_cards_intercept": -0.99130504,
            "late_game_aggro": 2.09102475, "lethal_horizon": 9,
            "target_aggr_cash_scale": 0.09638681, "target_aggr_cards_scale": 0.20156727,
            "penalty_cash_weight": 0.85077871, "penalty_cards_weight": 0.30673896,
            "hold_lethal_combos": True, "lethal_combo_bonus": 24,
            "lethal_hold_ceiling": 38, "defense_stdev_mult": 0.21564917,
            "race_scale": 0.0, "race_stdev_ahead": 0.0, "race_stdev_behind": 0.0,
            "race_eps_gain": 0.0, "race_use_belief_opp": False,
            "belief_draw_weight": 0.0, "belief_reshuffle_trust": 0.0,
            "belief_opp_block_trust": 0.0, "hplus_trust": 0.0, "hplus_block_combo": 0.0,
        }
    result = subprocess.run([str(BINARY), "--print-defaults"],
                            capture_output=True, text=True, check=True)
    return json.loads(result.stdout)


DEFAULTS = _load_defaults_from_binary()

# Search space for `optimize` (continuous dials only) and default sweep
# grids for `sweep` (every free dial, including the structural bool).
BOUNDS = {
    "race_scale": (0.0, 12.0),
    "race_stdev_ahead": (-2.0, 2.0),
    "race_stdev_behind": (-2.0, 2.0),
    "race_eps_gain": (-1.0, 1.0),
    "belief_draw_weight": (-2.0, 2.0),
    "belief_reshuffle_trust": (0.0, 1.0),
    "belief_opp_block_trust": (0.0, 1.0),
    "hplus_trust": (0.0, 1.0),
    "hplus_block_combo": (0.0, 10.0),
    # Stage 4 only (STAGE_4_EXTRA) -- same [-2,2] range A7's own calibration
    # used for this field (aicalibsrc/hbt/README.md's defense_stdev_mult sweep).
    "defense_stdev_mult": (-2.0, 2.0),
}

SWEEP_DEFAULTS = {
    "race_scale": [0.0, 1.0, 2.0, 4.0, 8.0, 12.0],
    "race_stdev_ahead": [-1.0, -0.5, 0.0, 0.5, 1.0],
    "race_stdev_behind": [-1.0, -0.5, 0.0, 0.5, 1.0],
    "race_eps_gain": [-1.0, -0.5, 0.0, 0.5, 1.0],
    "race_use_belief_opp": [0, 1],
    "belief_draw_weight": [-2.0, -1.0, -0.5, 0.0, 0.5, 1.0, 2.0],
    "belief_reshuffle_trust": [0.0, 0.25, 0.5, 0.75, 1.0],
    "belief_opp_block_trust": [0.0, 0.25, 0.5, 0.75, 1.0],
    "hplus_trust": [0.0, 0.25, 0.5, 0.75, 1.0],
    "hplus_block_combo": [0.0, 1.0, 3.0, 5.0, 10.0],
    "defense_stdev_mult": [-2.0, -1.0, -0.5, 0.0, 0.215649, 0.5, 1.0, 2.0],
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
    a full/partial A13Params dict, or `optimize`'s own output (which nests
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
    """One call to bin/calib_a13. Safe to run in a worker.

    **tags are arbitrary caller bookkeeping (e.g. sweep's `_value`,
    selfplay's `_i`/`_j`) merged straight into the returned dict instead of
    being tracked in a separate same-order list -- run_many()'s
    ProcessPoolExecutor + as_completed() returns results in COMPLETION
    order, not submission order, so tagging at the source (rather than
    reattaching by list position afterward) is what keeps this correct
    under real parallelism (aicalibsrc/hbt/README.md's pitfall #1).
    """
    if not BINARY.exists():
        raise FileNotFoundError(f"{BINARY} not found -- run `make calib_a13` first")

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


def a13_win_rate(df):
    in_a = df["agent_a"] == "carto"
    wins = int(np.where(in_a, df["wins_a"], df["wins_b"]).sum())
    n = int((df["wins_a"] + df["wins_b"] + df["draws"]).sum())
    return wins, n


# ---------------------------------------------------------------------------
# sweep: one param varied, carto vs a fixed opponent, both seats
# ---------------------------------------------------------------------------

def build_sweep_jobs(param, values, opponent, numsim, seeds, fixed=None):
    """`fixed` overrides other params away from DEFAULTS for the whole sweep
    -- needed whenever the swept param only has an observable effect in
    combination with another non-default value. Layer R is the motivating
    case: race_stdev_ahead/race_stdev_behind are multiplied by a term that
    is always 0 when race_scale <= 0 (a13_evaluate_state()'s own short-
    circuit), so sweeping either stdev dial alone against DEFAULTS'
    race_scale=0 is silently uninformative -- pass `fixed={"race_scale": 4}`
    (or similar) to sweep them meaningfully. The opponent's params_b/params_a
    side is deliberately left at plain DEFAULTS regardless of `fixed`: this
    sweep is about carto's own response curve, not the opponent's."""
    jobs = []
    for v in values:
        overrides = dict(fixed or {})
        overrides[param] = v
        p = merge_params(overrides)
        for seed in seeds:
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="carto", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS), _value=v, _carto_in_a=True))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="carto",
                             params_a=dict(DEFAULTS), params_b=p, _value=v, _carto_in_a=False))
    return jobs


def parse_fixed(pairs):
    """--fixed name=value [name=value ...] -> {name: float(value)}."""
    fixed = {}
    for kv in pairs or []:
        name, _, val = kv.partition("=")
        if name not in PARAM_NAMES:
            print(f"--fixed: unknown param '{name}'", file=sys.stderr)
            sys.exit(1)
        fixed[name] = float(val)
    return fixed


def cmd_sweep(args):
    values = args.values if args.values is not None else SWEEP_DEFAULTS[args.param]
    seeds = replicate_seeds(args.base_seed, args.replicates)
    fixed = parse_fixed(args.fixed)
    jobs = build_sweep_jobs(args.param, values, args.opponent, args.numsim, seeds, fixed)

    fixed_note = f", fixed={fixed}" if fixed else ""
    print(f"Running {len(jobs)} matches "
         f"({len(values)} values x {args.replicates} replicates x 2 seats, "
         f"vs {args.opponent}{fixed_note})...", file=sys.stderr)

    df = run_many(jobs, max_workers=args.workers)
    df["carto_wins"] = np.where(df["_carto_in_a"], df["wins_a"], df["wins_b"])
    df["n"] = df["wins_a"] + df["wins_b"] + df["draws"]

    grouped = df.groupby("_value").agg(wins=("carto_wins", "sum"), n=("n", "sum")).reset_index()
    grouped["win_rate"] = grouped["wins"] / grouped["n"]
    grouped[["ci_lo", "ci_hi"]] = grouped.apply(
        lambda r: pd.Series(wilson_ci(r["wins"], r["n"])), axis=1)
    grouped = grouped.rename(columns={"_value": args.param}).sort_values(args.param)

    print(f"\nSweep of {args.param} (carto vs {args.opponent}, both seats, "
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
    ax.set_title(f"Cartographer: {param} sweep (95% Wilson CI)")
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
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="carto", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="carto",
                             params_a=dict(DEFAULTS), params_b=p))
        df = run_many_threaded(jobs, executor)
        wins, n = a13_win_rate(df)
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

    Mirrors ideas/A13 .../about.md's risk ranking: each of the four new
    layers has an independent "optimized into irrelevance" signature to
    watch for, since this agent's whole design bets on each layer being
    independently diagnosable rather than hidden inside one aggregate."""
    flags = []

    race_dials = ["race_scale", "race_stdev_ahead", "race_stdev_behind", "race_eps_gain"]
    if all(abs(best_params.get(n, 0.0)) < 0.05 for n in race_dials):
        flags.append("Layer R (race arithmetic) optimized into irrelevance -- all of "
                     f"{race_dials} collapsed near 0.")

    belief_dials = ["belief_draw_weight", "belief_reshuffle_trust",
                    "belief_opp_block_trust", "hplus_trust"]
    if all(abs(best_params.get(n, 0.0)) < 0.05 for n in belief_dials):
        flags.append("The belief layer (K-draw/D/K-block) optimized into irrelevance -- "
                     f"all of {belief_dials} collapsed near 0.")

    ahead = best_params.get("race_stdev_ahead")
    behind = best_params.get("race_stdev_behind")
    if ahead is not None and behind is not None and abs(ahead - behind) < 0.05:
        flags.append(f"race_stdev_ahead ({ahead:.4f}) and race_stdev_behind ({behind:.4f}) "
                     f"converged to each other -- Layer R's state-dependence has collapsed "
                     f"back to A7's own constant defense_stdev_mult.")

    scale = best_params.get("race_scale")
    if scale is not None and scale >= BOUNDS["race_scale"][1] - 0.05:
        flags.append(f"race_scale = {scale:.4f} is pinned at its search ceiling "
                     f"({BOUNDS['race_scale'][1]}) -- Layer R may have saturated to a "
                     f"constant rather than genuinely using the race signal.")

    # Informational only -- ai_strat_a13.h documents the EXPECTED sign
    # (ahead > 0, behind < 0: block more when ahead, less when behind), but a
    # calibration finding the opposite sign is possible and not automatically
    # wrong (same precedent as A5's gamma drift, which turned out legitimate).
    if ahead is not None and ahead < -0.05:
        flags.append(f"race_stdev_ahead = {ahead:.4f} is NEGATIVE -- contradicts "
                     f"ai_strat_a13.h's documented sign convention (expected > 0). Worth "
                     f"a second look, not necessarily wrong.")
    if behind is not None and behind > 0.05:
        flags.append(f"race_stdev_behind = {behind:.4f} is POSITIVE -- contradicts "
                     f"ai_strat_a13.h's documented sign convention (expected < 0). Worth "
                     f"a second look, not necessarily wrong.")

    return flags


def cmd_optimize(args):
    free_names = [n for n in args.params if n not in PINNED_PARAM_NAMES]
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
    for name in NEW_PARAM_NAMES + STAGE_4_EXTRA:
        marker = "" if name in free_names else "  (fixed at default)"
        print(f"  {name} = {best_params.get(name, DEFAULTS[name])}{marker}")
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
        jobs.append(dict(numsim=args.validate_numsim, seed=seed, agent_a="carto",
                         agent_b=args.opponent, params_a=best_params, params_b=dict(DEFAULTS)))
        jobs.append(dict(numsim=args.validate_numsim, seed=seed, agent_a=args.opponent,
                         agent_b="carto", params_a=dict(DEFAULTS), params_b=best_params))
    df = run_many(jobs, max_workers=args.workers)
    wins, n = a13_win_rate(df)
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
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="carto", agent_b="carto",
                             params_a=params_map[names[i]], params_b=params_map[names[j]],
                             _i=i, _j=j))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="carto", agent_b="carto",
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
            jobs.append(dict(numsim=args.numsim, seed=seed, agent_a="carto",
                             agent_b=args.opponent, params_a=params, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=args.numsim, seed=seed, agent_a=args.opponent,
                             agent_b="carto", params_a=dict(DEFAULTS), params_b=params))
        df = run_many(jobs, max_workers=args.workers)
        wins, n = a13_win_rate(df)
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
    p_sweep.add_argument("--param", choices=FREE_PARAM_NAMES, required=True)
    p_sweep.add_argument("--values", type=float, nargs="+",
                         help="default: this file's suggested sweep for the param")
    p_sweep.add_argument("--opponent", default="rand")
    p_sweep.add_argument("--fixed", nargs="+", metavar="NAME=VALUE",
                         help="hold other param(s) away from DEFAULTS for the whole sweep "
                              "-- e.g. --fixed race_scale=4 when sweeping race_stdev_ahead, "
                              "since that dial has no observable effect at race_scale=0 "
                              "(a13_evaluate_state()'s own short-circuit)")
    p_sweep.add_argument("--numsim", type=int, default=2000)
    p_sweep.add_argument("--replicates", type=int, default=4)
    p_sweep.add_argument("--base-seed", type=int, default=1337)
    p_sweep.add_argument("--workers", type=int, default=None)
    p_sweep.add_argument("--plot", action="store_true")
    p_sweep.set_defaults(func=cmd_sweep)

    p_opt = sub.add_parser("optimize", help="differential-evolution search vs a fixed opponent")
    p_opt.add_argument("--params", choices=CONTINUOUS_PARAM_NAMES, nargs="+",
                       required=True,
                       help="REQUIRED, no default -- pick one stage's free set explicitly "
                            f"(see module docstring). Stage 1: {STAGE_1_PARAMS}. "
                            f"Stage 2: {STAGE_2_PARAMS}. Stage 3: {STAGE_3_PARAMS} "
                            f"(run `sweep` on belief_opp_block_trust/hplus_trust FIRST). "
                            f"Stage 4 (optional): the above plus {STAGE_4_EXTRA}. "
                            "race_use_belief_opp is a structural bool, not searchable here "
                            "-- use `sweep --param race_use_belief_opp` instead.")
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
