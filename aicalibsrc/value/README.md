# A1 Value Based calibration tooling

Calibration for `src/ai_strat/ai_strat_valuebased.c`'s two tunable parameters
(`VB_COST_FLOOR`, `VB_DEFEND_THRESHOLD`). See `doc/changelog.md` (2026-08-21
entries) for the design discussion and the calibration run that produced the
currently shipped values.

**2026-08-28 housekeeping**: fixed two bugs (ported from `aicalibsrc/balanced/`'s
already-correct patterns) that predate this note but affected every run until now.
(1) `run_match()` now takes `**tags`, so `selfplay`'s per-candidate `_i`/`_j`
bookkeeping travels with each result instead of being reattached from a
submission-order list after `run_many()`'s multi-worker pool returns results in
completion order -- previously silently scrambled BT-fit attribution under real
concurrency. (2) `--print-defaults` (via a new `value_based_get_default_params()`
accessor in `ai_strat_valuebased.c`/`.h`) means `DEFAULTS` here is read from the
compiled binary rather than a hand-copied dict, which had drifted to the
pre-calibration values (1.0/0.5 instead of the shipped 1.3/0.8). Re-validated
`VB_COST_FLOOR` with both fixed: a 10-point grid's quadratic fit R² jumped from the
original ~0.25-0.49 to 0.84, but the shipped `1.3` remains statistically tied with
the fit's top candidates (`1.5`/`1.7`/`2.0`) in a focused, larger-sample comparison
-- not re-shipped. See `doc/changelog.md`'s 2026-08-28 entry.

One subfolder per agent under `aicalibsrc/` as more agents get calibration
tooling (A2 Combo Threshold, A3 Borealis are the likely next ones) -- keep
each agent's harness and driver self-contained in its own subfolder rather
than accumulating loose files at the top level.

## Files

- `calib_valuebased.c` -- C calibration harness. Links the game engine
  directly (same pattern as `testsrc/test_recall.c` etc.), so it runs
  `run_simulation()` in-process with no subprocess-spawn or text-parsing
  overhead. Build with `make calib_valuebased` (from the repo root) ->
  `bin/calib_valuebased`. See the file's header comment for its CLI.
- `calibrate_valuebased.py` -- Python driver on top of that binary.
  Orchestrates many parallel calibration runs and does the statistics; the
  actual game simulation always happens in the C binary. See the file's
  module docstring for the `sweep`/`selfplay`/`validate` subcommands and
  usage examples.

## Setup

```bash
make calib_valuebased          # from the repo root
```

The Python driver needs `numpy`, `pandas`, `scipy`, `matplotlib`, and
`scikit-optimize`. On Debian/Ubuntu, `pip install` is blocked by PEP 668
(externally managed environment) without a venv, and building a venv itself
needs `python3.14-venv` from apt -- simplest path is the system packages:

```bash
sudo apt install python3-pandas python3-scipy python3-matplotlib python3-scikit-optimize
```

(`numpy` is a transitive dependency of those and comes along with them.)

## Usage

```bash
cd aicalibsrc/value

# Univariate sweep vs Random (matches value_based_handout.md Sec 11)
./calibrate_valuebased.py sweep --param defend_threshold --numsim 2500 --replicates 4

# Self-play round-robin over a parameter grid, Bradley-Terry fit,
# automatic best-vs-default-vs-Random validation
./calibrate_valuebased.py selfplay --cost-floors 0.5 1.0 1.5 2.0 \
    --defend-thresholds 0.25 0.5 0.8 1.0 --numsim 4000 --replicates 8

# Compare one specific candidate against the shipped defaults
./calibrate_valuebased.py validate --cost-floor 1.3 --defend-threshold 0.8 \
    --numsim 4000 --replicates 8
```

Results are written to `results/*.csv` (gitignored) plus a stdout summary.
Pass `--plot` to `sweep`/`selfplay` for a PNG alongside the CSV.

## Why self-play, not just vs-Random

The vs-Random comparison used to validate A1 in the first place is
**ceiling-effected**: Value Based beats Random ~90%+ almost regardless of
exact parameter values in a reasonable range, so nearby candidates are hard
to tell apart that way. Self-play (the same agent, two different parameter
sets, head-to-head in one game) doesn't have that ceiling -- small parameter
differences show up as deviations from 50% instead of getting drowned out
near saturation. `value_based_set_params(PlayerID, ...)` is what makes that
possible: it lets each seat run a different parameter set in the same game,
which the compile-time `#define`s alone could not do.

Caveat: pairwise self-play results can be intransitive (A beats B, B beats C,
C beats A) in an adversarial game, so single head-to-head verdicts aren't
trusted directly -- `selfplay` runs a full round-robin and fits a
Bradley-Terry model to get one relative-strength number per parameter combo.
