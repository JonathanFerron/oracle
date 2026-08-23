# A2 Combo Threshold calibration tooling

Calibration for `src/ai_strat/ai_strat_combo_threshold.c`'s nine tunable parameters
(`ComboThresholdParams`). See `doc/changelog.md` for the design discussion and any
calibration run that has produced shipped values so far.

One subfolder per agent under `aicalibsrc/`, mirroring `aicalibsrc/value/` -- keep
each agent's harness and driver self-contained rather than accumulating loose files
at the top level.

## Files

- `calib_combo_threshold.c` -- C calibration harness. Links the game engine directly
  (same pattern as `aicalibsrc/value/calib_valuebased.c`), so it runs
  `run_simulation()` in-process with no subprocess-spawn or text-parsing overhead.
  Build with `make calib_combo_threshold` (from the repo root) ->
  `bin/calib_combo_threshold`. See the file's header comment for its CLI (four agent
  args, then nine `ComboThresholdParams` fields for Player A, then the same nine for
  Player B).
- `calibrate_combo_threshold.py` -- Python driver on top of that binary. Orchestrates
  many calibration runs and does the statistics/search; the actual game simulation
  always happens in the C binary. See the file's module docstring for the
  `sweep`/`optimize`/`selfplay`/`validate` subcommands and usage examples.

## Setup

```bash
make calib_combo_threshold          # from the repo root
```

Same Python dependencies as `aicalibsrc/value/` (`numpy`, `pandas`, `scipy`,
`matplotlib`) -- already installed if that harness has been used. On Debian/Ubuntu,
`pip install` is blocked by PEP 668 without a venv; the system packages are the
simplest path:

```bash
sudo apt install python3-pandas python3-scipy python3-matplotlib
```

## Usage

```bash
cd aicalibsrc/combo

# Univariate sweep: does this one parameter actually move the needle?
./calibrate_combo_threshold.py sweep --param aggression_level --opponent value \
    --numsim 2000 --replicates 4

# Black-box search (differential evolution) vs a fixed opponent. Keep --numsim
# modest here -- it's a search, called hundreds of times; the winner gets
# re-validated afterwards with more games automatically.
./calibrate_combo_threshold.py optimize --opponent value --numsim 500 \
    --replicates 2 --maxiter 10 --popsize 10

# Compare a handful of named candidates head-to-head (round-robin, Bradley-Terry
# fit) -- e.g. the shipped defaults against one or two optimize() outputs
./calibrate_combo_threshold.py selfplay --candidates defaults \
    results/optimize_value.json --numsim 2000 --replicates 4

# Compare one candidate against the shipped defaults directly
./calibrate_combo_threshold.py validate --candidate results/optimize_value.json \
    --opponent rand --numsim 2000 --replicates 4
```

Candidate parameter sets are JSON files holding a full or partial
`ComboThresholdParams` dict (missing fields fall back to the compiled defaults), or
the literal string `defaults`. `optimize`'s own output file works directly as a
`selfplay`/`validate` candidate -- it nests the params under a `"best_params"` key,
which both commands know to unwrap.

Results are written to `results/*.{csv,json,png}` (gitignored) plus a stdout summary.
Pass `--plot` to `sweep` for a PNG alongside the CSV.

## Why `optimize` targets `value`, not `rand`, by default

Phase 2 measurement (see `doc/changelog.md`) found `combo` vs `rand` is already
**ceiling-effected** at default parameters (~85-91% depending on seat) -- the same
saturation A1 hit vs Random. `combo` vs `value` (The Apprentice, A1) landed close to
50% at defaults instead, with real headroom in both directions, which is what makes
it useful as an optimization target: parameter changes actually move the number.

## Why the search space stays free, with a personality check afterwards

Per the calibration protocol agreed for this agent (2026-08-22): let `optimize`
search all nine parameters freely rather than pre-locking the defense-related ones
into a narrow band, then inspect what it finds. `optimize`'s output flags any
parameter that drifted outside its handout-specified personality band
(`defend_probability_base` outside 0.40-0.70, `defend_damage_threshold` outside
6-10) or that otherwise looks like a character change rather than a strength gain
(`combo_bonus_threshold` <= 5, i.e. even color-pair combos qualify; `combo_weight`
near 0, i.e. the agent stopped chasing combos at all). A flagged result should be
reviewed and overridden by hand before shipping, not accepted automatically -- this
mirrors what happened for A1: self-play pushed `VB_DEFEND_THRESHOLD` toward "almost
never defend," which measured stronger but eroded the agent's intended character, so
a deliberately weaker, human-chosen value was shipped instead (`doc/changelog.md`,
2026-08-21). The Showboat's threshold-gated combo-chasing and probabilistic defense
decline are the entire point of this agent (handout §3, §8) -- a calibration run that
quietly erases them isn't a win.

## selfplay vs a full grid

A1's `selfplay` round-robins a full Cartesian grid over its two parameters. A2 has
nine -- a full grid is combinatorially infeasible, so this `selfplay` instead
round-robins a small set of explicitly *named* candidates (JSON files, typically
`optimize` outputs or hand-picked alternatives) rather than every combination of
per-axis values. Use `sweep` to check one axis at a time and `optimize` to search the
full space; use `selfplay` to compare a short list of finalists head-to-head.
