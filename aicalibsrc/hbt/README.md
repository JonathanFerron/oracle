# A7 Hybrid HBT ("The Grandmaster") calibration tooling

Calibration for `src/ai_strat/ai_strat_hbt.c`'s thirty-four tunable parameters
(`HBTParams`). See `doc/changelog.md` for the design discussion and the calibration
run that produced the shipped values.

One subfolder per agent under `aicalibsrc/`, mirroring `aicalibsrc/tactical/` -- keep
each agent's harness and driver self-contained rather than accumulating loose files
at the top level.

## Files

- `calib_hbt.c` -- C calibration harness. Links the game engine directly (same
  pattern as `aicalibsrc/tactical/calib_tactical.c`), so it runs `run_simulation()`
  in-process with no subprocess-spawn or text-parsing overhead. Build with
  `make calib_hbt` (from the repo root) -> `bin/calib_hbt`. See the file's header
  comment for its CLI (four agent args, then thirty-four `HBTParams` fields for
  Player A, then the same thirty-four for Player B), or run
  `bin/calib_hbt --print-defaults` to dump the compiled defaults as JSON.
- `calibrate_hbt.py` -- Python driver on top of that binary. Orchestrates many
  calibration runs and does the statistics/search; the actual game simulation always
  happens in the C binary. Its `DEFAULTS` dict is read once, at import time, from
  `bin/calib_hbt --print-defaults`, so it cannot drift from the shipped C constants.
  See the file's module docstring for the `sweep`/`optimize`/`selfplay`/`validate`
  subcommands and usage examples.

## Setup

```bash
make calib_hbt          # from the repo root
```

Same Python dependencies as the other six harnesses (`numpy`, `pandas`, `scipy`,
`matplotlib`) -- already installed if any of them has been used. On Debian/Ubuntu,
`pip install` is blocked by PEP 668 without a venv; the system packages are the
simplest path:

```bash
sudo apt install python3-pandas python3-scipy python3-matplotlib
```

## Usage

```bash
cd aicalibsrc/hbt

# Univariate sweep: does this one parameter actually move the needle?
./calibrate_hbt.py sweep --param penalty_cash_weight --opponent borealis \
    --numsim 2000 --replicates 4 --plot

# Black-box search (differential evolution) vs a fixed opponent, an explicit
# SUBSET of parameters -- see "Why calibration was staged" below for why this
# agent doesn't run one flat 32-parameter search.
./calibrate_hbt.py optimize --opponent borealis \
    --params aggr_energy_gain aggr_resource_fade critical_epsilon_mult \
             target_aggr_cash_scale target_aggr_cards_scale \
             penalty_cash_weight penalty_cards_weight defense_stdev_mult \
    --numsim 500 --replicates 2 --maxiter 15 --popsize 12

# Only run --identity-safe if check_personality_flags() actually flags the result --
# BOUNDS itself already carries non-zero floors for the fields most at risk (see
# "This agent's identity check" below), so this is a rarer escape hatch here than
# it was for A4/A5.
./calibrate_hbt.py optimize --opponent borealis --identity-safe \
    --numsim 500 --replicates 2 --maxiter 15 --popsize 12

# Compare a handful of named candidates head-to-head (round-robin, Bradley-Terry
# fit) -- e.g. the shipped defaults against one or two optimize() outputs
./calibrate_hbt.py selfplay --candidates defaults \
    results/optimize_borealis.json --numsim 2000 --replicates 4

# Compare one candidate against the shipped defaults directly
./calibrate_hbt.py validate --candidate results/optimize_borealis.json \
    --opponent rand --numsim 2000 --replicates 4
```

Candidate parameter sets are JSON files holding a full or partial `HBTParams` dict
(missing fields fall back to the compiled defaults), or the literal string
`defaults`. `optimize`'s own output file works directly as a `selfplay`/`validate`
candidate -- it nests the params under a `"best_params"` key, which both commands know
to unwrap.

Results are written to `results/*.{csv,json,png}` (gitignored) plus a stdout summary.
Pass `--plot` to `sweep` for a PNG alongside the CSV.

## Two fields are pinned in every `optimize` mode, not just `--identity-safe`

- `weight_cash_advantage` (delta) -- the same scale-invariance redundancy `A5` pins
  it for (`ai_strat_heuristic.h`): the argmax of a weighted sum is invariant to a
  positive rescaling of all three weights, so with `weight_cards_advantage` and
  `weight_energy_advantage` both free, `delta` is a redundant degree of freedom.
- `hold_lethal_combos` -- this agent exists to be combo-aware "to a good extent" (the
  design decision that pulled `A3`'s hold mechanism into this agent's scope at all,
  see `about.md`); it is a fixed design choice, not a dial a search should be free to
  switch off.

`FREE_PARAM_NAMES` in `calibrate_hbt.py` is `PARAM_NAMES` minus these two; `optimize`
without `--params` searches exactly that set.

## Why calibration was staged, not one flat search

Thirty-four parameters is roughly double `A6`'s sixteen (the previous largest space).
Rather than trust one unconstrained differential-evolution pass over 32 free
parameters, calibration ran in stages, each building on the last:

1. **Stage 1** froze every parameter this agent inherited from `A3`/`A4`/`A5`/`A6` at
   that SOURCE AGENT'S OWN shipped default, and freed only the eight parameters new
   to this agent (the T->H and B->H coupling terms, the two target-aggression
   scales, `defense_stdev_mult`) against `borealis`. This is the
   `optimize --params aggr_energy_gain ...` invocation shown above. Result: 60.96%
   [60.48%, 61.43%] vs `borealis` (40,000 games, validated), no personality flags.
2. **Stage 2** additionally freed `A5`'s own four non-pinned weights (twelve free
   total), re-deriving them jointly rather than reusing `A5`'s shipped values.
   Statistically indistinguishable from stage 1 (61.36% [60.88%, 61.83%]; a direct
   stage-1-vs-stage-2 head-to-head measured 49.11% [48.56%, 49.66%], a tie within
   noise) at the cost of `weight_cards_advantage` drifting to 10.72 (vs `A5`'s own
   1.96) and `opp_card_discount` to 2.75 (near its 3.0 bound) -- no measurable gain
   for abandoning "H ranks with `A5`'s own tuned advantage function," so **stage 1
   shipped**.
3. **Stage 3** (jointly freeing `A6`'s twelve aggression/phase fields too) was never
   run: stage 1 already clears all three source agents (`A4`, `A5`, `A6`) in the
   roster-wide `--stda.rating` fit without touching their own tuned values, and
   `about.md`'s framing is a synthesis of three ALREADY-CALIBRATED agents, not a
   fourth from-scratch fit.

If a future recalibration is needed, `--params` supports repeating this staged
approach; `PARAM_NAMES`' declared order groups fields by layer (H, then T's
phase/aggression, then the T->H coupling, then B's targets, then the B->H coupling,
then combo hold, then defense) specifically so a subset like stage 1's is easy to
name.

## This agent's identity check verifies all THREE layers, not one

`about.md`'s framing is "the personality IS the fixed three-layer synthesis," so
`check_personality_flags()` checks each layer independently rather than a single
per-agent metric:

- **T alive**: `aggression_factor`'s range across `AGGRESSION_BATTERY` (`A6`'s own
  synthetic-position test, ported verbatim) must stay >= 0.15, and the three phase
  thresholds must stay correctly ordered.
- **B alive**: neither `penalty_cash_weight`/`penalty_cards_weight` nor
  `target_cash_slope`/`target_cards_slope` may sit at its `BOUNDS` lower bound --
  `A4`'s own documented failure mode (two free searches drove its resource-target
  slopes toward 0, see `aicalibsrc/balanced/README.md`) is exactly what these floors
  exist to prevent here.
- **H alive**: `weight_energy_advantage`/`weight_cash_advantage` must stay >= 0.2 and
  `weight_taper_exponent` must stay <= 3.0 (`A5`'s own ratio checks).

Unlike `A4`/`A5`/`A6`, this agent's `BOUNDS` (not just `BOUNDS_IDENTITY_SAFE`) already
carries non-zero floors for every field the B-alive and T->H-coupling checks watch --
see `ai_strat_hbt.h`'s "commit to the synthesis" design decision. This is a starting
choice, not a reaction to a free search eroding something; `--identity-safe` is kept
as an escape hatch for if a future recalibration ever does trigger a flag despite
those floors, but stage 1's actual run needed neither the floors nor the escape hatch
to bind (see "Why calibration was staged" above).

## selfplay vs a full grid

Thirty-four parameters make a full Cartesian grid infeasible, so `selfplay`
round-robins a small set of explicitly *named* candidates (JSON files, typically
`optimize` outputs) rather than every combination, matching every other driver's
pattern. Use `sweep` to check one axis at a time, `optimize --params <subset>` to
search a stage, and `selfplay` to compare a short list of finalists head-to-head once
more than one credible candidate exists.
