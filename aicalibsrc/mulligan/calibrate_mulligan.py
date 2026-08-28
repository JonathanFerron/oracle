#!/usr/bin/env python3
"""Driver for the mulligan/seat-advantage investigation (Next Up item 2,
doc/oracle_roadmap.md).

Drives bin/calib_mulligan, which links the game engine directly and prints
one CSV result line per invocation. All the actual game simulation happens
in the compiled C binary; this script only orchestrates subprocess calls and
aggregates the results. Structure and internals (run_match's **tags-at-the-
source fix, wilson_ci, replicate_seeds, --print-defaults-sourced DEFAULT)
are carried over from aicalibsrc/balanced/calibrate_balanced.py, the newest
of the nine agent drivers.

Unlike every aicalibsrc/<agent>/calibrate_<agent>.py, this driver is not
tuning an AI agent's playing strength -- it investigates a single SHARED
game-rule parameter (mulligan_get_max_cards(), see
ai_strat_lib_heuristics.h) for fairness, so the usual sweep/optimize/
selfplay/validate shape doesn't fit. Two subcommands instead:

  seat   For one or more --agents, run a large self-mirror match (the same
         agent playing itself, both seats) at a fixed --max-cards (default:
         the shipped MULLIGAN_DEFAULT_MAX_CARDS, read from the binary) and
         report Player-A's win rate + Wilson CI. Self-mirror is the clean
         design here: both seats run the identical strategy, so any
         deviation from 50% is purely the seat/mulligan effect, not a
         confound with relative agent strength.

  sweep  For one --agent, self-mirror match at each of several --max-cards
         values, same statistics per point -- shows whether the cap
         actually controls the seat effect's size.

A10 IS-MCTS ("ismcts") is deliberately excluded from --max-cards values
other than its own fixed behavior (2): its mulligan search
(ai_strat_ismcts_flat.c's enumerate_mulligan_candidates()) is a fixed
enumeration, not driven by mulligan_get_max_cards() -- see that function's
own comment. It can still appear in `seat` at the default max_cards.

Examples:
  ./calibrate_mulligan.py seat --agents rand,balanced,heuristic,hbt \\
      --numsim 2000 --replicates 4
  ./calibrate_mulligan.py sweep --agent rand --max-cards 0 1 2 3 \\
      --numsim 2000 --replicates 4 --plot
"""

import argparse
import json
import math
import subprocess
import sys
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

import numpy as np
import pandas as pd

BINARY = Path(__file__).resolve().parent.parent.parent / "bin" / "calib_mulligan"
RESULTS_DIR = Path(__file__).resolve().parent / "results"


def _load_default_max_cards():
    if not BINARY.exists():
        # Fall back to the compiled-in value in ai_strat_lib_heuristics.c's
        # MULLIGAN_DEFAULT_MAX_CARDS, so --help and argument parsing still
        # work before `make calib_mulligan` has run. Any real command still
        # hits run_match()'s own existence check.
        return 2
    result = subprocess.run([str(BINARY), "--print-defaults"],
                            capture_output=True, text=True, check=True)
    return json.loads(result.stdout)["max_cards"]


DEFAULT_MAX_CARDS = _load_default_max_cards()


def run_match(numsim, seed, agent_a, agent_b, max_cards, **tags):
    """One call to bin/calib_mulligan. Safe to run in a worker.

    **tags are arbitrary caller bookkeeping merged straight into the
    returned dict instead of being tracked in a separate same-order list --
    run_many()'s ProcessPoolExecutor + as_completed() returns results in
    COMPLETION order, not submission order, so tagging at the source is
    what keeps this correct under real parallelism (see
    aicalibsrc/balanced/calibrate_balanced.py's identical note and
    doc/oracle_todo.md's now-closed Calibration section for the bug this
    avoids)."""
    if not BINARY.exists():
        raise FileNotFoundError(f"{BINARY} not found -- run `make calib_mulligan` first")

    args = [str(BINARY), str(numsim), str(seed), agent_a, agent_b, str(max_cards)]
    result = subprocess.run(args, capture_output=True, text=True, check=True)
    row = result.stdout.strip().split(",")
    return {
        "numsim": int(row[0]), "seed": int(row[1]),
        "agent_a": row[2], "agent_b": row[3], "max_cards": int(row[4]),
        "wins_a": int(row[5]), "wins_b": int(row[6]), "draws": int(row[7]),
        **tags,
    }


def run_many(jobs, max_workers=None, quiet=False):
    """jobs: list of kwargs dicts for run_match(). Returns a DataFrame, one row per job."""
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


# ---------------------------------------------------------------------------
# seat: self-mirror match per agent, current (or given) max_cards
# ---------------------------------------------------------------------------

def build_seat_jobs(agents, max_cards, numsim, seeds):
    jobs = []
    for agent in agents:
        for seed in seeds:
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=agent, agent_b=agent,
                             max_cards=max_cards, _agent=agent))
    return jobs


def summarize_seat(df):
    rows = []
    for agent, group in df.groupby("_agent"):
        wins_a = int(group["wins_a"].sum())
        n = int((group["wins_a"] + group["wins_b"] + group["draws"]).sum())
        rate = wins_a / n if n else float("nan")
        lo, hi = wilson_ci(wins_a, n)
        rows.append({"agent": agent, "p_a_wins": rate, "ci_lo": lo, "ci_hi": hi, "n": n})
    return pd.DataFrame(rows).sort_values("p_a_wins", ascending=False)


def cmd_seat(args):
    agents = args.agents.split(",")
    max_cards = args.max_cards if args.max_cards is not None else DEFAULT_MAX_CARDS
    seeds = replicate_seeds(args.base_seed, args.replicates)
    jobs = build_seat_jobs(agents, max_cards, args.numsim, seeds)

    print(f"Running {len(jobs)} self-mirror matches "
         f"({len(agents)} agents x {args.replicates} replicates, "
         f"max_cards={max_cards})...", file=sys.stderr)
    df = run_many(jobs, max_workers=args.workers)

    summary = summarize_seat(df)
    print(f"\nSeat advantage per agent (self-mirror, max_cards={max_cards}, "
         f"P(A wins), 0.5 = no seat effect):")
    print(summary.to_string(index=False, float_format=lambda x: f"{x:.4f}"))

    # Named after the actual agent list, not just max_cards -- otherwise a
    # second `seat` run with a different --agents subset at the same
    # max_cards silently clobbers the first run's results (found running
    # this investigation's own roster in two batches, closed-form agents
    # then the costlier search-based ones).
    tag = "_".join(agents)
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    out_path = RESULTS_DIR / f"seat_max_cards_{max_cards}_{tag}.csv"
    summary.to_csv(out_path, index=False)
    df.to_csv(RESULTS_DIR / f"seat_max_cards_{max_cards}_{tag}_raw.csv", index=False)
    print(f"\nSaved: {out_path}")


# ---------------------------------------------------------------------------
# sweep: one agent, self-mirror match across several max_cards values
# ---------------------------------------------------------------------------

def build_sweep_jobs(agent, max_cards_values, numsim, seeds):
    jobs = []
    for mc in max_cards_values:
        for seed in seeds:
            jobs.append(dict(numsim=numsim, seed=seed, agent_a=agent, agent_b=agent,
                             max_cards=mc))
    return jobs


def summarize_sweep(df):
    rows = []
    for mc, group in df.groupby("max_cards"):
        wins_a = int(group["wins_a"].sum())
        n = int((group["wins_a"] + group["wins_b"] + group["draws"]).sum())
        rate = wins_a / n if n else float("nan")
        lo, hi = wilson_ci(wins_a, n)
        rows.append({"max_cards": mc, "p_a_wins": rate, "ci_lo": lo, "ci_hi": hi, "n": n})
    return pd.DataFrame(rows).sort_values("max_cards")


def cmd_sweep(args):
    seeds = replicate_seeds(args.base_seed, args.replicates)
    jobs = build_sweep_jobs(args.agent, args.max_cards, args.numsim, seeds)

    print(f"Running {len(jobs)} self-mirror matches "
         f"({len(args.max_cards)} max_cards values x {args.replicates} replicates, "
         f"agent={args.agent})...", file=sys.stderr)
    df = run_many(jobs, max_workers=args.workers)

    summary = summarize_sweep(df)
    print(f"\nSweep of max_cards ({args.agent} self-mirror, P(A wins), "
         f"0.5 = no seat effect):")
    print(summary.to_string(index=False, float_format=lambda x: f"{x:.4f}"))

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    out_path = RESULTS_DIR / f"sweep_{args.agent}.csv"
    summary.to_csv(out_path, index=False)
    df.to_csv(RESULTS_DIR / f"sweep_{args.agent}_raw.csv", index=False)
    print(f"\nSaved: {out_path}")

    if args.plot:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt

        fig, ax = plt.subplots(figsize=(7, 5))
        ax.errorbar(summary["max_cards"], summary["p_a_wins"],
                   yerr=[summary["p_a_wins"] - summary["ci_lo"],
                         summary["ci_hi"] - summary["p_a_wins"]],
                   fmt="o-", capsize=4)
        ax.axhline(0.5, color="gray", linestyle="--", linewidth=1)
        ax.set_xlabel("mulligan max_cards")
        ax.set_ylabel("P(Player A wins)")
        ax.set_title(f"Seat advantage vs mulligan cap ({args.agent} self-mirror)")
        fig.tight_layout()
        plot_path = RESULTS_DIR / f"sweep_{args.agent}.png"
        fig.savefig(plot_path, dpi=120)
        print(f"Saved: {plot_path}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Mulligan/seat-advantage investigation driver",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    p_seat = subparsers.add_parser("seat", help="Seat advantage per agent (self-mirror)")
    p_seat.add_argument("--agents", required=True,
                        help="Comma-separated agent shorthands, e.g. rand,balanced,hbt")
    p_seat.add_argument("--max-cards", type=int, default=None,
                        help=f"Mulligan cap to test (default: shipped default, {DEFAULT_MAX_CARDS})")
    p_seat.add_argument("--numsim", type=int, default=2000)
    p_seat.add_argument("--replicates", type=int, default=4)
    p_seat.add_argument("--base-seed", type=int, default=1337)
    p_seat.add_argument("--workers", type=int, default=None)
    p_seat.set_defaults(func=cmd_seat)

    p_sweep = subparsers.add_parser("sweep", help="Sweep max_cards for one agent")
    p_sweep.add_argument("--agent", required=True)
    p_sweep.add_argument("--max-cards", type=int, nargs="+", default=[0, 1, 2, 3])
    p_sweep.add_argument("--numsim", type=int, default=2000)
    p_sweep.add_argument("--replicates", type=int, default=4)
    p_sweep.add_argument("--base-seed", type=int, default=1337)
    p_sweep.add_argument("--workers", type=int, default=None)
    p_sweep.add_argument("--plot", action="store_true")
    p_sweep.set_defaults(func=cmd_sweep)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
