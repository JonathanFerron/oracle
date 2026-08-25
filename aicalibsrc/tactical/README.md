# A6 Tactical ("Pressure Cooker") calibration tooling

Calibration for `src/ai_strat/ai_strat_tactical.c`'s sixteen tunable parameters
(`TacticalParams`). See `doc/changelog.md` for the design discussion and the
calibration run that produced the shipped values.

One subfolder per agent under `aicalibsrc/`, mirroring `aicalibsrc/heuristic/` -- keep
each agent's harness and driver self-contained rather than accumulating loose files
at the top level.

## Files

- `calib_tactical.c` -- C calibration harness. Links the game engine directly (same
  pattern as `aicalibsrc/heuristic/calib_heuristic.c`), so it runs `run_simulation()`
  in-process with no subprocess-spawn or text-parsing overhead. Build with
  `make calib_tactical` (from the repo root) -> `bin/calib_tactical`. See the file's
  header comment for its CLI (four agent args, then sixteen `TacticalParams` fields
  for Player A, then the same sixteen for Player B), or run
  `bin/calib_tactical --print-defaults` to dump the compiled defaults as JSON.
- `calibrate_tactical.py` -- Python driver on top of that binary. Orchestrates many
  calibration runs and does the statistics/search; the actual game simulation always
  happens in the C binary. Its `DEFAULTS` dict is read once, at import time, from
  `bin/calib_tactical --print-defaults`, so it cannot drift from the shipped C
  constants. See the file's module docstring for the `sweep`/`optimize`/`selfplay`/
  `validate` subcommands and usage examples.

## Setup

```bash
make calib_tactical          # from the repo root
```

Same Python dependencies as the other five harnesses (`numpy`, `pandas`, `scipy`,
`matplotlib`) -- already installed if any of them has been used. On Debian/Ubuntu,
`pip install` is blocked by PEP 668 without a venv; the system packages are the
simplest path:

```bash
sudo apt install python3-pandas python3-scipy python3-matplotlib
```

## Usage

```bash
cd aicalibsrc/tactical

# Univariate sweep: does this one parameter actually move the needle?
./calibrate_tactical.py sweep --param aggression_energy_diff_weight --opponent borealis \
    --numsim 2000 --replicates 4 --plot

# Black-box search (differential evolution) vs a fixed opponent, all sixteen
# parameters free (nothing is pinned here the way A5's delta is -- see
# ai_strat_heuristic.h for why that agent needed a pin and this one doesn't).
./calibrate_tactical.py optimize --opponent borealis --numsim 500 --replicates 2 \
    --maxiter 10 --popsize 10

# Only run --identity-safe if check_personality_flags() actually flags the result --
# see "This agent's identity check" below.
./calibrate_tactical.py optimize --opponent borealis --identity-safe \
    --numsim 500 --replicates 2 --maxiter 15 --popsize 12

# Compare a handful of named candidates head-to-head (round-robin, Bradley-Terry
# fit) -- e.g. the shipped defaults against one or two optimize() outputs
./calibrate_tactical.py selfplay --candidates defaults \
    results/optimize_borealis.json --numsim 2000 --replicates 4

# Compare one candidate against the shipped defaults directly
./calibrate_tactical.py validate --candidate results/optimize_borealis.json \
    --opponent rand --numsim 2000 --replicates 4
```

Candidate parameter sets are JSON files holding a full or partial `TacticalParams`
dict (missing fields fall back to the compiled defaults), or the literal string
`defaults`. `optimize`'s own output file works directly as a `selfplay`/`validate`
candidate -- it nests the params under a `"best_params"` key, which both commands know
to unwrap.

Results are written to `results/*.{csv,json,png}` (gitignored) plus a stdout summary.
Pass `--plot` to `sweep` for a PNG alongside the CSV.

## This agent's identity check is different from A4's/A5's

`about.md` names this agent's exact failure mode: "a static version of this agent is a
worse Heuristic, not a Tactical agent." That isn't a per-parameter drift question (is
this one weight far from its spec default?) -- it's a question about whether the
*aggression factor as a whole* still responds to the position. `check_personality_flags()`
therefore computes `aggression_factor` at `AGGRESSION_BATTERY`, a small set of synthetic
`(own_energy, opp_energy, my_hand_power, opp_estimated_power, own_cash)` tuples spanning
the input space, and flags a candidate whose range across that battery collapses below
0.15 -- i.e. the position stops mattering. It also flags an inverted phase-threshold
ordering (`phase_critical_threshold >= phase_late_threshold`, etc.), since that both
breaks `GamePhase()`'s intended classification and the `opp_energy_floor` reuse for
`try_play_draw_card()`.

`optimize --identity-safe` exists for if this ever flags: `BOUNDS_IDENTITY_SAFE` keeps
every aggression-formula term at a meaningful non-zero floor rather than letting the
search collapse them all toward 0 (which would make `aggression_factor` sit permanently
near its 0.5 baseline, exactly the "worse Heuristic" failure mode).

## Why sixteen parameters, and why `optimize` was fast anyway

Sixteen free parameters is the largest search space of any agent calibrated so far
(`A4`'s ten was the previous max). In practice a `maxiter=10, popsize=10` run against
`borealis` at `numsim=500` finished in about 20 seconds (the engine itself is fast;
each generation's ~160-320 games/evaluation dominates nothing). If a future run does
turn out slow, `--params` lets the aggression-formula terms and the defense-EV terms be
calibrated as separate subsets first, then joined in a final pass to catch cross-term
interactions -- the option exists in the driver even though the first run didn't need it.

## Why the shipped result has no `--identity-safe` candidate

Unlike `A4` (two free runs eroded its resource-target slopes to near-zero) and `A5`
(one free run pushed a weight to 60x its spec default, though playtracing found that
one legitimate rather than degenerate), the first unconstrained `optimize` run here
came back with `personality_flags: []` -- phase ordering intact, aggression range
healthy. There was nothing to run `--identity-safe` against. The shipped defaults are
that first run's validated result: 53.56% vs `borealis` (40,000 games), the second
agent after `A5` to measure above the Borealis anchor.

## selfplay vs a full grid

Sixteen parameters make a full Cartesian grid infeasible, so `selfplay` round-robins a
small set of explicitly *named* candidates (JSON files, typically `optimize` outputs)
rather than every combination, matching every other driver's pattern. Use `sweep` to
check one axis at a time, `optimize` to search, and `selfplay` to compare a short list
of finalists head-to-head once more than one credible candidate exists.
