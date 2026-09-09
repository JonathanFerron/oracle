#!/usr/bin/env python3
"""Calibration driver for A14 AlphaOracle Prime Plus I's PUCT dials -- see
src/ai_strat/ai_strat_puct.h and doc/ai_agents.md's A14 section.

Drives bin/calib_puct, which links the game engine directly and prints one
CSV result line per invocation. All the actual game simulation happens in
the compiled C binary; this script only orchestrates subprocess calls and
aggregates/fits the results. Structure ported from
aicalibsrc/carto/calibrate_a13.py (wilson_ci, bradley_terry_fit, run_match's
tags-at-source fix, the sweep/optimize/selfplay/validate shape) and
aicalibsrc/ismctsnn/calibrate_ismctsnn.py (the two-gate measurement
framing) -- this agent's own calibration is closer to A13's shape (four
free continuous dials, needs `optimize`) than A11's (one dial, `sweep`
alone sufficed).

DEFAULTS is read once, at import time, from `bin/calib_puct --print-defaults`
(puct_get_default_ismcts_params() + puct_get_default_params()), so it can
never drift from the shipped C constants. PARAM_NAMES carries the FULL
29-field shape (21 ISMCTSParams + 8 PUCTParams, ai_strat_puct.h's own
declared order for the second block) because the C harness needs it
positionally; FREE_PARAM_NAMES is the four dials this agent's own
calibration actually searches: c_puct, fpu_reduction, prior_trust,
policy_temperature. Everything else (compute budget, rollout/mulligan
fields this agent doesn't read, root_dirichlet_alpha/root_noise_frac --
self-play-generation-only, not a real-play dial) is pinned at the shipped
default.

**No registration gate here** (Jonathan's call, 2026-09-08): this agent
ships regardless of what `validate` measures -- see doc/ai_agents.md's A14
section for whatever the real number turned out to be. The two gates below
are still the right way to MEASURE it, just not a ship/no-ship decision.

Two gates (still measured and reported, doc/ai_agents.md's A14 section):
Gate 2 (`validate`, candidate vs `ismctsnn`, both seats, Wilson CI) is the
real head-to-head bar against this agent's direct predecessor; Gate 1 is
`validate --opponent borealis` for context (an estimated Borealis rating
comparable to A11's own 74). Watch `sweep --param prior_trust` for the
A9/A13 monotonic-decline signature -- A11's own monotonic *rise* is what a
genuine win looks like; a flat or declining curve here means the learned
prior isn't adding anything over a uniform one (PUCT + uniform prior is
still a real, different agent from plain UCT, just not one the prior helps).

A weights file (aicalibsrc/puct/export_puct_weights.py's output) is
REQUIRED for every subcommand via --weights, even for e.g. puct-vs-ismcts
with the net contributing nothing -- the C harness always loads it.

Subcommands:
  sweep     Univariate diagnostic: one of the four free dials varied, puct
            vs a fixed --opponent (default "ismctsnn"), both seats, Wilson CIs.
  optimize  Differential-evolution search over --params (a subset of the
            four free dials) vs a fixed --opponent.
  selfplay  Round-robin among named candidates, puct vs puct, Bradley-Terry fit.
  validate  Candidate vs the shipped defaults, vs --opponent, both seats --
            Gate 1/Gate 2 depending on --opponent.

Examples:
  ./calibrate_puct.py sweep --weights checkpoints/full_c_weights.bin \\
      --param prior_trust --numsim 500 --replicates 4 --plot
  ./calibrate_puct.py optimize --weights checkpoints/full_c_weights.bin \\
      --params c_puct fpu_reduction policy_temperature --numsim 300 --replicates 2
  ./calibrate_puct.py validate --weights checkpoints/full_c_weights.bin \\
      --candidate defaults --opponent ismctsnn --numsim 2000 --replicates 4
  ./calibrate_puct.py validate --weights checkpoints/full_c_weights.bin \\
      --candidate defaults --opponent borealis --numsim 2000 --replicates 4
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

BINARY = Path(__file__).resolve().parent.parent.parent / "bin" / "calib_puct"
RESULTS_DIR = Path(__file__).resolve().parent / "results"

# Declared struct order: ISMCTSParams (ai_strat_ismcts1.h) then PUCTParams
# (ai_strat_puct.h) -- must match parse_ismcts_params()/parse_puct_params()/
# print_*_params_csv() in calib_puct.c exactly.
ISMCTS_PARAM_NAMES = [
    "limit_iterations", "limit_playout_steps", "limit_max_nodes",
    "limit_recall_variants", "limit_cash_variants", "limit_max_candidates",
    "search_exploration_constant", "search_use_availability", "search_expand_threshold",
    "threshold_widening_k", "threshold_widening_alpha", "prior_use_heuristic",
    "rollout_max_turns", "rollout_cutoff_depth",
    "weight_energy_advantage", "weight_cash_advantage", "weight_hand_advantage",
    "limit_flat_iterations", "limit_flat_candidates",
    "nn_value_trust", "nn_value_use_mover_seat",
]
PUCT_PARAM_NAMES = [
    "use_puct", "c_puct", "fpu_reduction", "prior_trust", "policy_temperature",
    "use_widening", "root_dirichlet_alpha", "root_noise_frac",
]
PARAM_NAMES = ISMCTS_PARAM_NAMES + PUCT_PARAM_NAMES

_INT_PARAMS = {
    "limit_iterations", "limit_playout_steps", "limit_max_nodes",
    "limit_recall_variants", "limit_cash_variants", "limit_max_candidates",
    "search_use_availability", "search_expand_threshold", "prior_use_heuristic",
    "rollout_max_turns", "rollout_cutoff_depth",
    "limit_flat_iterations", "limit_flat_candidates", "nn_value_use_mover_seat",
    "use_puct", "use_widening",  # bools -> "0"/"1", matching calib_puct.c's strtol(...) != 0
}

# The only four fields this agent's own calibration searches -- everything
# else in PARAM_NAMES is pinned at the shipped default (compute budget,
# fields A14 never reads, or root_dirichlet_alpha/root_noise_frac which are
# a self-play-GENERATION dial, not something `validate`/`sweep` here touch).
FREE_PARAM_NAMES = ["c_puct", "fpu_reduction", "prior_trust", "policy_temperature"]

# `sweep` (but deliberately NOT `optimize` -- differential_evolution needs a
# continuous dial, not an integer compute-budget one, same "budget dials are
# swept not optimized" split A8/A13's own drivers already draw) additionally
# accepts limit_iterations. Measured directly at this agent's own shipped
# 4000: mean 1.7s/decision, up to 4.6s (nowhere near the ~8%-over-A11
# estimate the plan assumed) -- this agent's argmax-based selection has no
# A10/A11-style "untried always wins" guarantee, so it genuinely descends
# deeper per iteration through already-explored branches before committing
# to expand something new; that's inherent PUCT behaviour at Oracle's ~93-
# move branching factor, not a bug (use_widening=true does NOT fix it --
# confirmed by direct A/B measurement, and it would truncate by enumeration
# order, not by prior, undermining the mechanism if it had worked). Jonathan
# confirmed 2026-09-08 that ~1.7s/decision is an acceptable interactive
# wait for the shipped 4000-iteration config; this sweep is for finding
# whether a lower budget (a real sweet-spot candidate around ~2300,
# matching A10's own historical peak-then-decline shape) trades a little
# depth for meaningfully faster real play without giving up strength -- not
# because 4000 is unusable.
SWEEPABLE_PARAM_NAMES = FREE_PARAM_NAMES + ["limit_iterations"]

# Narrowed 2026-09-08 from the four independent sweeps (n=180/point each, vs
# ismctsnn) -- all four sweeps were flat within noise (no dial cleared a
# statistically real trend), so this is a best-faith narrowing toward each
# sweep's strongest points, not a confident bound tightening:
#   c_puct: 0.5/2.0 tied best (50.0%), 3.0 clearly worst (45.6%) -> drop the
#     upper tail past 3.0, keep the rest of the original range.
#   fpu_reduction: 0.0 was the single best point across ALL A14 dial sweeps
#     (56.11%), with a mild decline as it rises (0.3->48.33%, 0.5->41.11%)
#     -> narrow toward the low end.
#   prior_trust: 0.75 best (56.11%), 0.0 worst (47.78%), no monotonic trend
#     -> narrow away from the low end that measured worst.
#   policy_temperature: 1.5/2.0 best (52.2%/51.7%), 1.0 (the old default) was
#     actually the WORST point (48.33%) -> shift the range up and off 1.0.
BOUNDS = {
    "c_puct": (0.1, 3.0),
    "fpu_reduction": (0.0, 0.3),
    "prior_trust": (0.25, 1.0),
    "policy_temperature": (1.0, 2.5),
}

SWEEP_DEFAULTS = {
    "c_puct": [0.5, 1.0, 1.5, 2.0, 3.0],
    "fpu_reduction": [0.0, 0.1, 0.2, 0.3, 0.5],
    "prior_trust": [0.0, 0.25, 0.5, 0.75, 1.0],
    "policy_temperature": [0.5, 0.75, 1.0, 1.5, 2.0],
    # Ceiling stays 4000 (the shipped default's own limit_max_nodes=4008
    # arena sizing, puct_get_default_ismcts_params(), only ever needs to fit
    # AT MOST limit_iterations nodes since at most one is created per
    # iteration) -- sweeping above 4000 would need a bigger --fixed
    # limit_max_nodes override too.
    "limit_iterations": [1500, 2000, 2300, 2600, 3000, 3500, 4000],
}


def _load_defaults_from_binary():
    if not BINARY.exists():
        # Fallback so --help works before `make calib_puct` has run -- see
        # aicalibsrc/hbt/README.md's pitfall #2 on why this rots if not kept
        # in sync; the shipped C defaults (ai_strat_puct.h) are the source
        # of truth, this is just enough to not crash argument parsing.
        return {
            "limit_iterations": 4000, "limit_playout_steps": 200, "limit_max_nodes": 4008,
            "limit_recall_variants": 2, "limit_cash_variants": 3, "limit_max_candidates": 128,
            "search_exploration_constant": 1.41421356, "search_use_availability": True,
            "search_expand_threshold": 3, "threshold_widening_k": 2.0,
            "threshold_widening_alpha": 0.5, "prior_use_heuristic": False,
            "rollout_max_turns": 500, "rollout_cutoff_depth": 0,
            "weight_energy_advantage": 0.0, "weight_cash_advantage": 0.0,
            "weight_hand_advantage": 0.0, "limit_flat_iterations": 2000,
            "limit_flat_candidates": 36, "nn_value_trust": 0.0,
            "nn_value_use_mover_seat": False,
            "use_puct": True, "c_puct": 1.5, "fpu_reduction": 0.2, "prior_trust": 1.0,
            "policy_temperature": 1.0, "use_widening": False,
            "root_dirichlet_alpha": 0.0, "root_noise_frac": 0.0,
        }
    result = subprocess.run([str(BINARY), "--print-defaults"],
                            capture_output=True, text=True, check=True)
    return json.loads(result.stdout)


DEFAULTS = _load_defaults_from_binary()


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
    a full/partial params dict, or `optimize`'s own output (nested under
    "best_params")."""
    if spec == "defaults":
        return dict(DEFAULTS)
    with open(spec) as f:
        data = json.load(f)
    return merge_params(data.get("best_params", data))


# ---------------------------------------------------------------------------
# Core: one match, many matches, confidence intervals
# ---------------------------------------------------------------------------

def run_match(weights, numsim, seed, agent_a, agent_b, params_a, params_b, **tags):
    """One call to bin/calib_puct. Safe to run in a worker.

    **tags are arbitrary caller bookkeeping merged into the returned dict at
    the source -- run_many()'s ProcessPoolExecutor + as_completed() returns
    results in COMPLETION order, not submission order (aicalibsrc/hbt/README.md's
    pitfall #1)."""
    if not BINARY.exists():
        raise FileNotFoundError(f"{BINARY} not found -- run `make calib_puct` first")

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


def puct_win_rate(df):
    in_a = df["agent_a"] == "puct"
    wins = int(np.where(in_a, df["wins_a"], df["wins_b"]).sum())
    n = int((df["wins_a"] + df["wins_b"] + df["draws"]).sum())
    return wins, n


# ---------------------------------------------------------------------------
# sweep: one free dial varied, puct vs a fixed opponent, both seats
# ---------------------------------------------------------------------------

def parse_fixed(pairs):
    fixed = {}
    for kv in pairs or []:
        name, _, val = kv.partition("=")
        if name not in PARAM_NAMES:
            print(f"--fixed: unknown param '{name}'", file=sys.stderr)
            sys.exit(1)
        fixed[name] = float(val)
    return fixed


def build_sweep_jobs(weights, param, values, opponent, numsim, seeds, fixed=None):
    jobs = []
    for v in values:
        overrides = dict(fixed or {})
        overrides[param] = v
        p = merge_params(overrides)
        for seed in seeds:
            jobs.append(dict(weights=weights, numsim=numsim, seed=seed, agent_a="puct",
                             agent_b=opponent, params_a=p, params_b=dict(DEFAULTS),
                             _value=v, _puct_in_a=True))
            jobs.append(dict(weights=weights, numsim=numsim, seed=seed, agent_a=opponent,
                             agent_b="puct", params_a=dict(DEFAULTS), params_b=p,
                             _value=v, _puct_in_a=False))
    return jobs


def cmd_sweep(args):
    values = args.values if args.values is not None else SWEEP_DEFAULTS[args.param]
    seeds = replicate_seeds(args.base_seed, args.replicates)
    fixed = parse_fixed(args.fixed)
    jobs = build_sweep_jobs(args.weights, args.param, values, args.opponent,
                            args.numsim, seeds, fixed)

    fixed_note = f", fixed={fixed}" if fixed else ""
    print(f"Running {len(jobs)} matches "
         f"({len(values)} values x {args.replicates} replicates x 2 seats, "
         f"vs {args.opponent}{fixed_note})...", file=sys.stderr)

    df = run_many(jobs, max_workers=args.workers)
    df["puct_wins"] = np.where(df["_puct_in_a"], df["wins_a"], df["wins_b"])
    df["n"] = df["wins_a"] + df["wins_b"] + df["draws"]

    grouped = df.groupby("_value").agg(wins=("puct_wins", "sum"), n=("n", "sum")).reset_index()
    grouped["win_rate"] = grouped["wins"] / grouped["n"]
    grouped[["ci_lo", "ci_hi"]] = grouped.apply(
        lambda r: pd.Series(wilson_ci(r["wins"], r["n"])), axis=1)
    grouped = grouped.rename(columns={"_value": args.param}).sort_values(args.param)

    print(f"\nSweep of {args.param} (puct vs {args.opponent}, both seats, "
         f"{args.numsim * args.replicates * 2} games/value):")
    print(grouped.to_string(index=False, float_format=lambda x: f"{x:.4f}"))

    is_monotonic_decline = grouped["win_rate"].is_monotonic_decreasing and len(grouped) > 2
    if is_monotonic_decline:
        print("\n*** WARNING: win rate declines MONOTONICALLY as this dial rises -- the "
             "A9 reply_trust / A13 hplus_trust failure signature. A11's own trust sweep "
             "instead rose monotonically; that's what a genuine win looks like. ***")

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
    ax.set_title(f"AlphaOracle Prime Plus I: {param} sweep (95% Wilson CI)")
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    print(f"Saved: {out_path}")


# ---------------------------------------------------------------------------
# optimize: differential evolution vs a fixed opponent
# ---------------------------------------------------------------------------

def vector_to_params(x, free_names, fixed):
    p = dict(fixed)
    for name, val in zip(free_names, x):
        p[name] = val
    return merge_params(p)


def make_objective(weights, free_names, fixed, opponent, numsim, seeds, executor, progress):
    def objective(x):
        p = vector_to_params(x, free_names, fixed)
        jobs = []
        for seed in seeds:
            jobs.append(dict(weights=weights, numsim=numsim, seed=seed, agent_a="puct",
                             agent_b=opponent, params_a=p, params_b=dict(DEFAULTS)))
            jobs.append(dict(weights=weights, numsim=numsim, seed=seed, agent_a=opponent,
                             agent_b="puct", params_a=dict(DEFAULTS), params_b=p))
        df = run_many_threaded(jobs, executor)
        wins, n = puct_win_rate(df)
        rate = wins / n if n else 0.0
        progress["evals"] += 1
        print(f"\r  eval {progress['evals']}: win_rate={rate:.4f}",
             end="", file=sys.stderr, flush=True)
        return -rate  # differential_evolution minimizes
    return objective


def check_personality_flags(best_params):
    """Human-readable warnings for anything an optimizer result should not
    silently ship with -- never auto-rejects, flags for review, same policy
    as every other agent's precedent (A4/A5/A13)."""
    flags = []

    pt = best_params.get("prior_trust")
    if pt is not None and pt < 0.05:
        flags.append(f"prior_trust = {pt:.4f} collapsed near 0 -- the optimizer found the "
                     "learned policy prior isn't helping (PUCT degenerated toward uniform-"
                     "prior exploration). A real finding if confirmed, not necessarily a bug.")

    cp = best_params.get("c_puct")
    if cp is not None and (cp <= BOUNDS["c_puct"][0] + 0.05 or cp >= BOUNDS["c_puct"][1] - 0.05):
        flags.append(f"c_puct = {cp:.4f} is pinned at a search bound "
                     f"({BOUNDS['c_puct']}) -- widen the bound and re-run before trusting this.")

    pterm = best_params.get("policy_temperature")
    if pterm is not None and pterm >= BOUNDS["policy_temperature"][1] - 0.1:
        flags.append(f"policy_temperature = {pterm:.4f} is pinned near its search ceiling "
                     f"({BOUNDS['policy_temperature'][1]}) -- effectively flattening the "
                     "prior toward uniform; may be the same finding as prior_trust collapsing.")

    return flags


def cmd_optimize(args):
    free_names = args.params
    fixed = {n: DEFAULTS[n] for n in PARAM_NAMES if n not in free_names}
    bounds = [BOUNDS[n] for n in free_names]
    seeds = replicate_seeds(args.base_seed, args.replicates)

    print(f"Optimizing {len(free_names)} param(s) vs {args.opponent}: {free_names}",
         file=sys.stderr)
    print(f"({len(seeds) * 2} matches x {args.numsim} games per evaluation)", file=sys.stderr)

    progress = {"evals": 0}
    with ThreadPoolExecutor(max_workers=args.workers or 8) as executor:
        objective = make_objective(args.weights, free_names, fixed, args.opponent,
                                   args.numsim, seeds, executor, progress)
        result = differential_evolution(objective, bounds, maxiter=args.maxiter,
                                        popsize=args.popsize, seed=args.opt_seed,
                                        tol=0.01, mutation=(0.5, 1.0),
                                        recombination=0.7, polish=False)
    print(file=sys.stderr)

    best_params = vector_to_params(result.x, free_names, fixed)
    print("\nBest parameters found:")
    for name in FREE_PARAM_NAMES:
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
        jobs.append(dict(weights=args.weights, numsim=args.validate_numsim, seed=seed,
                         agent_a="puct", agent_b=args.opponent, params_a=best_params,
                         params_b=dict(DEFAULTS)))
        jobs.append(dict(weights=args.weights, numsim=args.validate_numsim, seed=seed,
                         agent_a=args.opponent, agent_b="puct", params_a=dict(DEFAULTS),
                         params_b=best_params))
    df = run_many(jobs, max_workers=args.workers)
    wins, n = puct_win_rate(df)
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

def build_selfplay_jobs(weights, names, params_map, numsim, seeds, include_mirror):
    jobs = []
    pairs = [(i, j) for i in range(len(names)) for j in range(len(names))
             if i < j or (include_mirror and i == j)]
    for i, j in pairs:
        for seed in seeds:
            jobs.append(dict(weights=weights, numsim=numsim, seed=seed, agent_a="puct",
                             agent_b="puct", params_a=params_map[names[i]],
                             params_b=params_map[names[j]], _i=i, _j=j))
            jobs.append(dict(weights=weights, numsim=numsim, seed=seed, agent_a="puct",
                             agent_b="puct", params_a=params_map[names[j]],
                             params_b=params_map[names[i]], _i=j, _j=i))
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
        print("Need at least 2 --candidates (e.g. defaults plus one JSON file)", file=sys.stderr)
        sys.exit(1)

    names = [Path(c).stem if c != "defaults" else "defaults" for c in args.candidates]
    if len(set(names)) != len(names):
        print("Candidate names (filename stems) must be unique", file=sys.stderr)
        sys.exit(1)
    params_map = {name: load_candidate(spec) for name, spec in zip(names, args.candidates)}

    seeds = replicate_seeds(args.base_seed, args.replicates)
    jobs = build_selfplay_jobs(args.weights, names, params_map, args.numsim, seeds,
                               args.include_mirror)

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
            jobs.append(dict(weights=args.weights, numsim=args.numsim, seed=seed,
                             agent_a="puct", agent_b=args.opponent, params_a=params,
                             params_b=dict(DEFAULTS)))
            jobs.append(dict(weights=args.weights, numsim=args.numsim, seed=seed,
                             agent_a=args.opponent, agent_b="puct", params_a=dict(DEFAULTS),
                             params_b=params))
        df = run_many(jobs, max_workers=args.workers)
        wins, n = puct_win_rate(df)
        lo, hi = wilson_ci(wins, n)
        return wins, n, (wins / n if n else 0.0), lo, hi

    print(f"Validating vs {args.opponent} ({len(seeds) * 2} matches per config)...",
         file=sys.stderr)
    d_wins, d_n, d_rate, d_lo, d_hi = vs_opponent(dict(DEFAULTS))
    if candidate == dict(DEFAULTS):
        # --candidate defaults (or an explicit file that happens to match
        # every default exactly) would otherwise run the IDENTICAL set of
        # matches twice -- real, non-trivial waste at this agent's own
        # per-decision cost. Reuse the baseline result instead of
        # re-measuring "defaults vs defaults".
        print("  (candidate == defaults -- reusing the baseline result instead of "
             "re-measuring an identical config)", file=sys.stderr)
        c_wins, c_n, c_rate, c_lo, c_hi = d_wins, d_n, d_rate, d_lo, d_hi
    else:
        c_wins, c_n, c_rate, c_lo, c_hi = vs_opponent(candidate)

    print(f"\nWin rate vs {args.opponent} ({d_n} games each):")
    print(f"  defaults:  {d_rate:.4f} [{d_lo:.4f}, {d_hi:.4f}]")
    print(f"  candidate: {c_rate:.4f} [{c_lo:.4f}, {c_hi:.4f}]")
    print(f"  delta: {(c_rate - d_rate) * 100:+.2f} percentage points")
    if args.opponent == "ismctsnn":
        print(f"\nGate 2 (measured, not a ship gate -- see doc/ai_agents.md's A14 section): "
             f"{'PASS' if c_lo > 0.5 else 'FAIL'} -- Wilson CI lower bound {c_lo:.4f} "
             f"{'>' if c_lo > 0.5 else '<='} 0.50")
    elif args.opponent == "borealis":
        print(f"\nGate 1 context: estimated Borealis rating ~{round(c_rate * 100)} "
             f"(A11's own measured rating: 74)")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    p_sweep = sub.add_parser("sweep", help="univariate sweep vs a fixed opponent")
    p_sweep.add_argument("--weights", required=True)
    p_sweep.add_argument("--param", choices=SWEEPABLE_PARAM_NAMES, required=True)
    p_sweep.add_argument("--values", type=float, nargs="+",
                         help="default: this file's suggested sweep for the param")
    p_sweep.add_argument("--opponent", default="ismctsnn")
    p_sweep.add_argument("--fixed", nargs="+", metavar="NAME=VALUE")
    p_sweep.add_argument("--numsim", type=int, default=2000)
    p_sweep.add_argument("--replicates", type=int, default=4)
    p_sweep.add_argument("--base-seed", type=int, default=1337)
    p_sweep.add_argument("--workers", type=int, default=None)
    p_sweep.add_argument("--plot", action="store_true")
    p_sweep.set_defaults(func=cmd_sweep)

    p_opt = sub.add_parser("optimize", help="differential-evolution search vs a fixed opponent")
    p_opt.add_argument("--weights", required=True)
    p_opt.add_argument("--params", choices=FREE_PARAM_NAMES, nargs="+", required=True)
    p_opt.add_argument("--opponent", default="ismctsnn")
    p_opt.add_argument("--numsim", type=int, default=300,
                       help="games per match during search (keep modest -- this runs "
                            "many times); increase for the final validation instead")
    p_opt.add_argument("--replicates", type=int, default=2)
    p_opt.add_argument("--base-seed", type=int, default=1337)
    p_opt.add_argument("--opt-seed", type=int, default=42, help="DE's own RNG seed")
    p_opt.add_argument("--maxiter", type=int, default=15)
    p_opt.add_argument("--popsize", type=int, default=12)
    p_opt.add_argument("--workers", type=int, default=8)
    p_opt.add_argument("--validate-numsim", type=int, default=2000)
    p_opt.add_argument("--validate-replicates", type=int, default=4)
    p_opt.set_defaults(func=cmd_optimize)

    p_self = sub.add_parser("selfplay", help="round-robin among named candidates, BT fit")
    p_self.add_argument("--weights", required=True)
    p_self.add_argument("--candidates", nargs="+", required=True,
                        help="'defaults' and/or paths to JSON param files "
                             "(optimize's own output works directly)")
    p_self.add_argument("--numsim", type=int, default=2000)
    p_self.add_argument("--replicates", type=int, default=4)
    p_self.add_argument("--base-seed", type=int, default=1337)
    p_self.add_argument("--workers", type=int, default=None)
    p_self.add_argument("--include-mirror", action="store_true")
    p_self.set_defaults(func=cmd_selfplay)

    p_val = sub.add_parser("validate", help="one candidate vs the shipped defaults")
    p_val.add_argument("--weights", required=True)
    p_val.add_argument("--candidate", required=True, help="'defaults' or a JSON param file")
    p_val.add_argument("--opponent", default="ismctsnn",
                       help="'ismctsnn' = Gate 2 (real bar); 'borealis' = Gate 1 (context)")
    p_val.add_argument("--numsim", type=int, default=2000)
    p_val.add_argument("--replicates", type=int, default=4)
    p_val.add_argument("--base-seed", type=int, default=1337)
    p_val.add_argument("--workers", type=int, default=None)
    p_val.set_defaults(func=cmd_validate)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
