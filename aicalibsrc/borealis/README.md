# A3 Borealis calibration tooling

Calibration for `src/ai_strat/ai_strat_borealis.c`'s six tunable parameters
(`BorealisParams`). See `doc/changelog.md` for the design discussion and any
calibration run that has produced shipped values so far.

**2026-08-28 housekeeping**: `calib_borealis.c` gained a `--print-defaults` mode
(same pattern as `aicalibsrc/balanced/`, ported into this driver too), so
`DEFAULTS` here is now read from the compiled binary rather than a hand-copied
dict. It had drifted badly -- `luna_value` was still the handout's original
pre-calibration guess (0.5), off by 9.2x from the actual shipped `4.5846`. (The
result-misattribution bug fixed the same day in `aicalibsrc/value/`/`aicalibsrc/
combo/` originated here -- this driver's `run_match(**tags)` was already correct,
see `doc/changelog.md`'s 2026-08-23 entry.)

One subfolder per agent under `aicalibsrc/`, mirroring `aicalibsrc/value/` and
`aicalibsrc/combo/` -- keep each agent's harness and driver self-contained
rather than accumulating loose files at the top level.

## Files

- `calib_borealis.c` -- C calibration harness. Links the game engine directly
  (same pattern as `aicalibsrc/combo/calib_combo_threshold.c`), so it runs
  `run_simulation()` in-process with no subprocess-spawn or text-parsing
  overhead. Build with `make calib_borealis` (from the repo root) ->
  `bin/calib_borealis`. See the file's header comment for its CLI (four agent
  args, then six `BorealisParams` fields for Player A, then the same six for
  Player B).
- `calibrate_borealis.py` -- Python driver on top of that binary. Orchestrates
  many calibration runs and does the statistics/search; the actual game
  simulation always happens in the C binary. See the file's module docstring
  for the `sweep`/`optimize`/`selfplay`/`validate` subcommands and usage
  examples.

## Setup

```bash
make calib_borealis          # from the repo root
```

Same Python dependencies as `aicalibsrc/value/`/`aicalibsrc/combo/` (`numpy`,
`pandas`, `scipy`, `matplotlib`) -- already installed if either of those
harnesses has been used. On Debian/Ubuntu, `pip install` is blocked by PEP
668 without a venv; the system packages are the simplest path:

```bash
sudo apt install python3-pandas python3-scipy python3-matplotlib
```

## Usage

```bash
cd aicalibsrc/borealis

# The headline check (handout Sec.13): win rate should be unimodal in
# luna_value. A flat curve means the cost term isn't wired in.
./calibrate_borealis.py sweep --param luna_value --opponent rand \
    --numsim 2000 --replicates 4 --plot

# Black-box search (differential evolution) vs a fixed opponent, all six
# parameters free by default. Keep --numsim modest here -- it's a search,
# called hundreds of times; the winner gets re-validated afterwards with
# more games automatically, and its lambda dial gets re-checked for
# unimodality with the other five parameters fixed at the found values.
./calibrate_borealis.py optimize --opponent combo --numsim 500 \
    --replicates 2 --maxiter 10 --popsize 10

# Compare a handful of named candidates head-to-head (round-robin, Bradley-
# Terry fit) -- e.g. the shipped defaults against one or two optimize()
# outputs
./calibrate_borealis.py selfplay --candidates defaults \
    results/optimize_combo.json --numsim 2000 --replicates 4

# Compare one candidate against the shipped defaults directly
./calibrate_borealis.py validate --candidate results/optimize_combo.json \
    --opponent rand --numsim 2000 --replicates 4
```

Candidate parameter sets are JSON files holding a full or partial
`BorealisParams` dict (missing fields fall back to the compiled defaults), or
the literal string `defaults`. `optimize`'s own output file works directly as
a `selfplay`/`validate` candidate -- it nests the params under a
`"best_params"` key, which both commands know to unwrap.

Results are written to `results/*.{csv,json,png}` (gitignored) plus a stdout
summary. Pass `--plot` to `sweep` for a PNG alongside the CSV.

## Why the personality check here is unimodality, not parameter bands

A2's `optimize` flags an optimizer result whose `defend_probability_base`/
`defend_damage_threshold` fall outside a fixed handout-specified band, or
whose `combo_bonus_threshold`/`combo_weight` drift toward "stopped chasing
combos" -- static ranges around parameters that are personality traits, not
strength knobs.

Borealis has exactly one strength knob, `luna_value`, and the property that
matters for it isn't a range -- it's *shape*. The handout's entire case for
using this agent as the Bradley-Terry anchor (Sec.1, Sec.8) rests on win rate
being unimodal in lambda: detune it in either direction from the peak and the
agent stays coherent instead of becoming exploitable. A fixed acceptable
range for lambda would say nothing about whether that property still holds
after the other five parameters move away from their handout defaults.

So `optimize` here doesn't check bands after the fact -- it re-sweeps lambda
(`LAMBDA_UNIMODALITY_GRID`, `calibrate_borealis.py`) with the other five
parameters pinned at the values the search found, and fits a quadratic to the
result (`check_unimodal_fit()`, the same technique used to characterize A1's
`VB_COST_FLOOR` calibration, see `doc/changelog.md` 2026-08-21). A concave-down
fit (negative leading coefficient) means unimodal; anything else is flagged
for manual review before shipping, following the same protocol as A1's
`VB_DEFEND_THRESHOLD` and A2's `aggression_level` -- an optimizer result that
measures stronger but erodes the property the agent exists for isn't a win.

## Why `optimize` targets `combo`, not `rand`, by default

At the handout's untuned defaults, Borealis vs `rand` is already
ceiling-effected (~92-93% both seats) the same way A1 and A2 were vs Random.
Vs `value` it lands close to parity (~49.5% averaged across seats); vs `combo`
it's actually *below* parity (~44.6%) -- Borealis, the agent meant to anchor
the scale above both, currently loses to the rung directly below it. `combo`
is therefore both the harder target and the more informative one: closing that
gap is the calibration's real job, not just moving an already-comfortable
number.

## selfplay vs a full grid

Six parameters make a full Cartesian grid more feasible than A2's nine, but
this `selfplay` still round-robins a small set of explicitly *named*
candidates (JSON files, typically `optimize` outputs or hand-picked
alternatives) rather than every combination of per-axis values, matching
`aicalibsrc/combo/`'s pattern. Use `sweep` to check one axis at a time,
`optimize` to search the full space, and `selfplay` to compare a short list
of finalists head-to-head.
