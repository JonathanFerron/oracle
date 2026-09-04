#!/usr/bin/env python3
"""Calibration driver for A9 HBT 2-Ply ("Grandmaster II")'s four new
parameters (HBT2PlyParams -- see src/ai_strat/ai_strat_hbt2ply.h).

Drives bin/calib_hbt2ply, which links the game engine directly and prints
one CSV result line per invocation. All the actual game simulation happens
in the compiled C binary; this script only orchestrates subprocess calls
and aggregates/fits/searches the results. Structure and internals (the
subcommand shape, run_match's tags-at-the-source fix, the seed-offset
convention, --print-defaults-sourced DEFAULTS) are carried over from
aicalibsrc/hbt/calibrate_hbt.py, adapted for a much smaller search space.

DEFAULTS below is read once, at import time, from `bin/calib_hbt2ply
--print-defaults`, so it can never drift from the shipped compiled values.

Unlike every other agent's driver, this one's PARAM_NAMES carries the full
38-field HBT2PlyParams shape (the 34 HBTParams fields inherited from A7,
plus this agent's own 4) because the C harness needs all 38 positionally --
but the 34 inherited fields are HARD PINNED (PINNED_PARAM_NAMES), not just
defaulted: about.md's framing is "the only new thing this agent adds over
A7 is the second ply," so this driver never re-derives A7's own tuned
values, and `--param`/`--params` only ever accept this agent's own 4 fields
(FREE_PARAM_NAMES) -- there is no `--identity-safe` escape hatch here,
because there is nothing to erode: A7's own calibration is calibrate_hbt.py's
job, not this file's.

Of those 4, only 2 are real behaviour dials searched by `optimize`'s default
(OPTIMIZE_PARAM_NAMES): reply_trust and surrogate_pessimism. The other 2
(ply_energy_ceiling, ply_beam_width) are pure compute-budget dials -- same
split as A8 Simple Monte Carlo's twenty parameters (doc/changelog.md): "more
[ply] coverage is basically always at least as strong, just slower," so
these are SWEPT via `sweep`, not searched via `optimize`, though `--params`
will let you force either into an `optimize` run if you have a specific
reason to.

Per the staged calibration plan (doc/changelog.md, aicalibsrc/hbt2ply/
README.md), stage 1 -- the only planned stage -- frees exactly
reply_trust and surrogate_pessimism against `borealis`; a joint re-fit of
A7's 34 inherited fields is explicitly out of scope (this agent's framing
is "A7 plus one ply," not a fresh fit).

check_personality_flags() is correspondingly narrow: about.md's identity is
specifically "the added ply," so the one thing worth flagging is the ply
having been optimized into irrelevance (reply_trust collapsed near 0, or
ply_energy_ceiling collapsed to a value that gates it off in every
realistic position).

Four subcommands, same as every other driver:

  sweep     Univariate diagnostic: hold every parameter at its default
            except one, vary that one, play vs a fixed --opponent (default
            "rand"), both seats, with binomial confidence intervals.

  optimize  Black-box search (scipy.optimize.differential_evolution) over
            some or all free parameters, maximizing win rate against a
            fixed --opponent (default "borealis" -- the rating-50 anchor).

  selfplay  Round-robin among a small set of NAMED candidate parameter sets,
            both seat orders, --ai.a=hbt2ply vs --ai.b=hbt2ply. Reports a
            Bradley-Terry fit.

  validate  Compare one candidate parameter set against the shipped
            defaults, vs a chosen opponent, both seats.

Candidate parameter sets are given as JSON files (a full or partial
HBT2PlyParams dict; missing fields fall back to the compiled defaults) or
the literal string "defaults". `optimize`'s own output file is directly
usable as a `selfplay`/`validate` candidate (it has a "best_params" key,
which these commands know to unwrap).

Examples:
  ./calibrate_hbt2ply.py sweep --param ply_beam_width --opponent borealis \\
      --numsim 2000 --replicates 4 --plot
  ./calibrate_hbt2ply.py optimize --opponent borealis \\
      --numsim 500 --replicates 2 --maxiter 15 --popsize 12
  ./calibrate_hbt2ply.py selfplay --candidates defaults results/optimize_borealis.json
  ./calibrate_hbt2ply.py validate --candidate results/optimize_borealis.json --opponent hbt
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

BINARY = Path(__file__).resolve().parent.parent.parent / "bin" / "calib_hbt2ply"
RESULTS_DIR = Path(__file__).resolve().parent / "results"

# Declared struct order in HBT2PlyParams: the 34 HBTParams fields (base),
# then this agent's own 4 -- must match parse_params() in calib_hbt2ply.c.
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
    "reply_trust", "surrogate_pessimism", "ply_energy_ceiling", "ply_beam_width",
]
PARAM_NAMES = BASE_PARAM_NAMES + NEW_PARAM_NAMES

_INT_PARAMS = {
    "phase_mid_threshold", "phase_late_threshold", "phase_critical_threshold",
    "aggression_cash_surplus_threshold", "lethal_horizon",
    "hold_lethal_combos", "lethal_combo_bonus", "lethal_hold_ceiling",
    "ply_energy_ceiling", "ply_beam_width",
}

# Hard pin: A9 re-derives none of A7's 34 fields (about.md). Not an
# "identity-safe" soft constraint the way other agents' PINNED_PARAM_NAMES
# entries are -- there is no escape hatch for these here.
PINNED_PARAM_NAMES = set(BASE_PARAM_NAMES)
FREE_PARAM_NAMES = [n for n in PARAM_NAMES if n not in PINNED_PARAM_NAMES]

# The 2 real behaviour dials `optimize` searches by default. The other 2
# free names (ply_energy_ceiling, ply_beam_width) are compute-budget dials,
# swept via `sweep` instead -- see module docstring.
OPTIMIZE_PARAM_NAMES = ["reply_trust", "surrogate_pessimism"]
BUDGET_PARAM_NAMES = ["ply_energy_ceiling", "ply_beam_width"]


def _load_defaults_from_binary():
    if not BINARY.exists():
        # Fall back to the compiled-in values (ai_strat_hbt2ply.c's
        # hbt2ply_get_default_params(), and A7's own HBT_DEFAULTS for the
        # 34 base fields) so --help and argument parsing still work before
        # `make calib_hbt2ply` has run.
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
            "aggr_energy_gain": 0.16112329, "aggr_resource_fade": 0.07170858,
            "critical_epsilon_mult": 2.46429341,
            "target_cash_slope": 0.08096868, "target_cash_intercept": -2.72849536,
            "target_cards_slope": 0.03572451, "target_cards_intercept": -0.99130504,
            "late_game_aggro": 2.09102475, "lethal_horizon": 9,
            "target_aggr_cash_scale": 0.09638681, "target_aggr_cards_scale": 0.20156727,
            "penalty_cash_weight": 0.74931590, "penalty_cards_weight": 0.86105139,
            "hold_lethal_combos": True, "lethal_combo_bonus": 24,
            "lethal_hold_ceiling": 38, "defense_stdev_mult": 0.71117494,
            "reply_trust": 1.0, "surrogate_pessimism": 1.0,
            "ply_energy_ceiling": 99, "ply_beam_width": 0,
        }
    result = subprocess.run([str(BINARY), "--print-defaults"],
                            capture_output=True, text=True, check=True)
    return json.loads(result.stdout)


DEFAULTS = _load_defaults_from_binary()

# Search space for `optimize` and default sweep grids for `sweep`, this
# agent's own 4 fields only -- the 34 pinned base fields need neither
# (calibrate_hbt.py already owns that space).
BOUNDS = {
    "reply_trust": (0.0, 1.0),
    "surrogate_pessimism": (0.0, 1.0),
    "ply_energy_ceiling": (0, 99),
    "ply_beam_width": (0, 298),  # C(12,3)+C(12,2)+C(12,1), the true max candidate count
}

SWEEP_DEFAULTS = {
    "reply_trust": [0.0, 0.25, 0.5, 0.75, 1.0],
    "surrogate_pessimism": [0.0, 0.25, 0.5, 0.75, 1.0],
    # A7's own phase thresholds (18/41/67) are natural sweep points for
    # "how much of the game gets the ply": 99 = about.md's "every candidate
    # move" reading, 18 reproduces doc/ai_agents.md's A7 section's PHASE_CRITICAL gate.
    "ply_energy_ceiling": [0, 18, 41, 67, 99],
    "ply_beam_width": [0, 5, 10, 20, 50, 100],
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
    a full/partial HBT2PlyParams dict, or `optimize`'s own output (which
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
    """One call to bin/calib_hbt2ply. Safe to run in a worker.

    **tags are arbitrary caller bookkeeping (e.g. sweep's `_value`,
    selfplay's `_i`/`_j`) merged straight into the returned dict instead of
    being tracked in a separate same-order list -- run_many()'s
    ProcessPoolExecutor + as_completed() returns results in COMPLETION
    order, not submission order, so tagging at the source (rather than
    reattaching by list position afterward) is what keeps this correct
    under real parallelism.
    """
    if not BINARY.exists():
        raise FileNotFoundError(f"{BINARY} not found -- run `make calib_hbt2ply` first")

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


def hbt2ply_win_rate(df):
    in_a = df["agent_a"] == "hbt2ply"
    wins = int(np.where(in_a, df["wins_a"], df["wins_b"]).sum())
    n = int((df["wins_a"] + df["wins_b"] + df["draws"]).sum())
    return wins, n


# ---------------------------------------------------------------------------
# sweep: one param varied, hbt2ply vs a fixed opponent, both seats
# ---------------------------------------------------------------------------

def build_sweep_jobs(param, values, opponent, numsim, seeds):
    jobs = []
    for v in values:
        p = merge_params({param: v})
        for seed in seeds:
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="hbt2ply", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS), _value=v, _hbt2ply_in_a=True))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="hbt2ply",
                             params_a=dict(DEFAULTS), params_b=p, _value=v, _hbt2ply_in_a=False))
    return jobs


def cmd_sweep(args):
    values = args.values or SWEEP_DEFAULTS[args.param]
    seeds = replicate_seeds(args.base_seed, args.replicates)
    jobs = build_sweep_jobs(args.param, values, args.opponent, args.numsim, seeds)

    print(f"Running {len(jobs)} matches "
         f"({len(values)} values x {args.replicates} replicates x 2 seats, "
         f"vs {args.opponent})...", file=sys.stderr)

    df = run_many(jobs, max_workers=args.workers)
    df["hbt2ply_wins"] = np.where(df["_hbt2ply_in_a"], df["wins_a"], df["wins_b"])
    df["n"] = df["wins_a"] + df["wins_b"] + df["draws"]

    grouped = df.groupby("_value").agg(wins=("hbt2ply_wins", "sum"), n=("n", "sum")).reset_index()
    grouped["win_rate"] = grouped["wins"] / grouped["n"]
    grouped[["ci_lo", "ci_hi"]] = grouped.apply(
        lambda r: pd.Series(wilson_ci(r["wins"], r["n"])), axis=1)
    grouped = grouped.rename(columns={"_value": args.param}).sort_values(args.param)

    print(f"\nSweep of {args.param} (hbt2ply vs {args.opponent}, both seats, "
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
    ax.set_title(f"Grandmaster II: {param} sweep (95% Wilson CI)")
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
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="hbt2ply", agent_b=opponent,
                             params_a=p, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=opponent, agent_b="hbt2ply",
                             params_a=dict(DEFAULTS), params_b=p))
        df = run_many_threaded(jobs, executor)
        wins, n = hbt2ply_win_rate(df)
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

    about.md's identity for this agent is narrow -- "the distinct
    personality here is specifically the added ply, not a new evaluation
    mechanism" -- so the one thing worth checking is whether the ply
    survived calibration at all, not a multi-layer check like A7's."""
    flags = []

    if best_params["reply_trust"] < 0.05:
        flags.append(f"reply_trust optimized down to {best_params['reply_trust']:.4f} "
                     f"(< 0.05) -- the second ply has been calibrated into irrelevance; "
                     f"this agent would be, in practice, an expensive copy of A7.")

    ceiling = best_params["ply_energy_ceiling"]
    if ceiling <= best_params.get("phase_critical_threshold", 18) // 4:
        flags.append(f"ply_energy_ceiling = {ceiling} is so low the ply would almost never "
                     f"gate on in a real game (opponent energy rarely lingers this close to "
                     f"zero) -- effectively the same as disabling it.")

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
    for name in NEW_PARAM_NAMES:
        marker = "" if name in free_names else "  (fixed at default)"
        print(f"  {name} = {best_params[name]}{marker}")
    print("  (34 base HBTParams fields fixed at A7's shipped defaults -- see module docstring)")
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
        jobs.append(dict(numsim=args.validate_numsim, seed=seed, agent_a="hbt2ply",
                         agent_b=args.opponent, params_a=best_params, params_b=dict(DEFAULTS)))
        jobs.append(dict(numsim=args.validate_numsim, seed=seed, agent_a=args.opponent,
                         agent_b="hbt2ply", params_a=dict(DEFAULTS), params_b=best_params))
    df = run_many(jobs, max_workers=args.workers)
    wins, n = hbt2ply_win_rate(df)
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
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="hbt2ply", agent_b="hbt2ply",
                             params_a=params_map[names[i]], params_b=params_map[names[j]],
                             _i=i, _j=j))
            jobs.append(dict(numsim=numsim, seed=seed, agent_a="hbt2ply", agent_b="hbt2ply",
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
            jobs.append(dict(numsim=args.numsim, seed=seed, agent_a="hbt2ply",
                             agent_b=args.opponent, params_a=params, params_b=dict(DEFAULTS)))
            jobs.append(dict(numsim=args.numsim, seed=seed, agent_a=args.opponent,
                             agent_b="hbt2ply", params_a=dict(DEFAULTS), params_b=params))
        df = run_many(jobs, max_workers=args.workers)
        wins, n = hbt2ply_win_rate(df)
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
    p_sweep.add_argument("--numsim", type=int, default=2000)
    p_sweep.add_argument("--replicates", type=int, default=4)
    p_sweep.add_argument("--base-seed", type=int, default=1337)
    p_sweep.add_argument("--workers", type=int, default=None)
    p_sweep.add_argument("--plot", action="store_true")
    p_sweep.set_defaults(func=cmd_sweep)

    p_opt = sub.add_parser("optimize", help="differential-evolution search vs a fixed opponent")
    p_opt.add_argument("--params", choices=FREE_PARAM_NAMES, nargs="+",
                       help="default: reply_trust and surrogate_pessimism only "
                            "(OPTIMIZE_PARAM_NAMES) -- ply_energy_ceiling/ply_beam_width "
                            "are compute-budget dials meant for `sweep`, not this search, "
                            "though you can force either in here explicitly")
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
