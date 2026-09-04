# A13 Cartographer ("The Cartographer") calibration tooling

Calibration for `src/ai_strat/ai_strat_a13.c`'s ten new tunable parameters (`A13Params`'s
`race_scale`/`race_stdev_ahead`/`race_stdev_behind`/`race_eps_gain`/`race_use_belief_opp`
[Layer R], `belief_draw_weight`/`belief_reshuffle_trust` [Layer K-draw + Layer D],
`belief_opp_block_trust`/`hplus_trust`/`hplus_block_combo` [Layer K-block] -- the 34
`HBTParams` fields it also carries are inherited from `A7` and hard-pinned, never searched
here, except `defense_stdev_mult` which is re-fittable only at Stage 4). See
`doc/ai_agents.md's A13 section` for the full design record
and risk ranking, and `doc/changelog.md` for the calibration run(s) that produced whatever
values end up shipped.

**Status as of 2026-08-31: calibration complete, agent SHELVED.** Every mechanism measured
at parity with `A7` or worse across four independent, properly-powered searches (`hplus_trust`
conclusively harmful) -- see `ideas/A13 .../about.md`'s "Measured: shelved" section and
`doc/changelog.md`'s 2026-08-31 entry for the full record. `AI_STRATEGY_CARTOGRAPHER` was
removed from `AIStrategyType`; `carto` is not selectable anywhere. This folder is kept
buildable (`make calib_a13`) purely so the calibration record above stays reproducible --
every command below still runs, but there is no live agent for its output to ship into.

One subfolder per agent under `aicalibsrc/`, mirroring `aicalibsrc/hbt/` -- keep each
agent's harness and driver self-contained rather than accumulating loose files at the top
level.

## Files

- `calib_a13.c` -- C calibration harness. Links the game engine directly (same pattern as
  `aicalibsrc/hbt/calib_hbt.c`), so it runs `run_simulation()` in-process with no
  subprocess-spawn or text-parsing overhead. Build with `make calib_a13` (from the repo
  root) -> `bin/calib_a13`. See the file's header comment for its CLI (four agent args, then
  44 `A13Params` fields for Player A -- the 34 inherited `HBTParams` fields in
  `ai_strat_hbt.h`'s declared order, then this agent's own 10 -- then the same 44 for
  Player B), or run `bin/calib_a13 --print-defaults` to dump the compiled defaults as flat
  JSON.
- `calibrate_a13.py` -- Python driver on top of that binary. Orchestrates many calibration
  runs and does the statistics/search; the actual game simulation always happens in the C
  binary. Its `DEFAULTS` dict is read once, at import time, from `bin/calib_a13
  --print-defaults`, so it cannot drift from the shipped C constants. See the file's module
  docstring for the `sweep`/`optimize`/`selfplay`/`validate` subcommands and usage examples.

## Setup

```bash
make calib_a13      # from the repo root
```

Same Python dependencies as the other harnesses (`numpy`, `pandas`, `scipy`, `matplotlib`)
-- already installed if any of them has been used. On Debian/Ubuntu, `pip install` is
blocked by PEP 668 without a venv; the system packages are the simplest path:

```bash
sudo apt install python3-pandas python3-scipy python3-matplotlib
```

## Usage: the staged calibration plan

Per `about.md`'s risk ranking (cheapest and safest first) and the approved implementation
plan. `optimize --params` is **required, with no default free set** -- unlike every other
agent's driver, a bare `optimize` here will refuse to run, so a staged run can't
accidentally skip straight to "free everything," which is exactly the mistake that cost A7's
own stage 2 nothing but drift.

```bash
cd aicalibsrc/carto

# Stage 0 -- sanity, not calibration: confirm the neutral defaults really do recover A7.
./calibrate_a13.py validate --candidate defaults --opponent hbt --numsim 20000 --replicates 2
# Expect ~50.00% +/- noise. Anything else is a bug in the superset property, fix before
# proceeding to any stage below.

# Stage 1 -- Layer R (race arithmetic), belief-independent. Sweep FIRST, always --
# never set bounds/run optimize before seeing the shape of the response curve.
#
# race_stdev_ahead/race_stdev_behind are multiplied by a term that is always 0 whenever
# race_scale <= 0 (a13_evaluate_state()'s own short-circuit) -- sweeping either alone
# against DEFAULTS' race_scale=0 is silently uninformative (confirmed empirically: an
# initial race_scale sweep with the stdev dials still at 0 showed a perfectly flat curve,
# for the same underlying reason). --fixed holds race_scale at a nonzero reference while
# sweeping the stdev dials, and vice versa:
./calibrate_a13.py sweep --param race_stdev_ahead --fixed race_scale=4 \
    --opponent borealis --numsim 2000 --replicates 4 --plot
./calibrate_a13.py sweep --param race_stdev_behind --fixed race_scale=4 \
    --opponent borealis --numsim 2000 --replicates 4 --plot
./calibrate_a13.py sweep --param race_scale --fixed race_stdev_ahead=0.6 race_stdev_behind=-0.6 \
    --opponent borealis --numsim 2000 --replicates 4 --plot
./calibrate_a13.py optimize --opponent borealis \
    --params race_scale race_stdev_ahead race_stdev_behind \
    --numsim 500 --replicates 2 --maxiter 15 --popsize 12
# Bar: beat A7's own ~64.62% vs borealis (see doc/changelog.md's 2026-08-28 entry).

# Stage 2 -- Layer K-draw + Layer D, Layer R frozen at its stage-1 result.
./calibrate_a13.py sweep --param belief_draw_weight --opponent borealis --numsim 2000 --replicates 4 --plot
./calibrate_a13.py optimize --opponent borealis \
    --params belief_draw_weight belief_reshuffle_trust \
    --numsim 500 --replicates 2 --maxiter 15 --popsize 12

# Stage 3 -- the risky half. Run A9's sweep protocol FIRST and read it before touching
# optimize: a MONOTONIC decline (not just a flat curve) as the trust dial rises is this
# agent's exact "the mechanism is actively misleading" signature (A9's reply_trust had it).
./calibrate_a13.py sweep --param belief_opp_block_trust --opponent hbt --numsim 16000 --replicates 1 --plot
./calibrate_a13.py sweep --param belief_opp_block_trust --opponent borealis --numsim 16000 --replicates 1 --plot
./calibrate_a13.py sweep --param hplus_trust --opponent hbt --numsim 16000 --replicates 1 --plot
./calibrate_a13.py sweep --param hplus_trust --opponent borealis --numsim 16000 --replicates 1 --plot
# Only if both curves look genuinely useful (flat-or-rising, not declining):
./calibrate_a13.py optimize --opponent borealis \
    --params belief_opp_block_trust hplus_trust hplus_block_combo \
    --numsim 500 --replicates 2 --maxiter 15 --popsize 12

# Stage 4 (optional) -- joint re-fit of the 10 new dials plus defense_stdev_mult ONLY.
# Layer R turns that one field from THE value into a BASELINE of a state-dependent
# quantity -- the "parameter fit against a dead branch" hazard running in reverse. Never
# free the other 33 base fields; there is nothing of A7's own tuning to erode here.
./calibrate_a13.py optimize --opponent borealis \
    --params race_scale race_stdev_ahead race_stdev_behind race_eps_gain \
             belief_draw_weight belief_reshuffle_trust \
             belief_opp_block_trust hplus_trust hplus_block_combo \
             defense_stdev_mult \
    --numsim 500 --replicates 2 --maxiter 20 --popsize 15

# race_use_belief_opp is a structural bool, not searchable by optimize -- sweep both values
# once Stage 1's other three dials have a candidate value:
./calibrate_a13.py sweep --param race_use_belief_opp --opponent borealis --numsim 4000 --replicates 4

# At every stage: compare named candidates head-to-head...
./calibrate_a13.py selfplay --candidates defaults \
    results/optimize_borealis.json --numsim 2000 --replicates 4

# ...and validate the actual ship gate -- head-to-head vs hbt, NOT just vs borealis. A
# Borealis-relative win says nothing about this specific matchup, per A7-vs-A5 and A9-vs-A7.
./calibrate_a13.py validate --candidate results/optimize_borealis.json \
    --opponent hbt --numsim 20000 --replicates 2
```

Candidate parameter sets are JSON files holding a full or partial `A13Params` dict (missing
fields fall back to the compiled defaults), or the literal string `defaults`. `optimize`'s
own output file works directly as a `selfplay`/`validate` candidate -- it nests the params
under a `"best_params"` key, which both commands know to unwrap.

Results are written to `results/*.{csv,json,png}` (gitignored) plus a stdout summary. Pass
`--plot` to `sweep` for a PNG alongside the CSV.

## Two ship gates, not one

A `--stda.rating` fit above `A7`'s 65 is necessary but **not sufficient**. `A7` itself rated
above `A5` in the roster-wide fit while losing 26.0% head-to-head to `A5` specifically; `A9`
rated 59 (above `A6`'s 52) while measuring only 47.2% head-to-head against `A7`, the exact
opponent its own mechanism targeted. The actual ship gate for this agent is **both**: (1) a
roster-wide rating above 65, and (2) a 40,000-game, both-seats head-to-head against `hbt`
with a Wilson 95% CI lower bound strictly above 50%. Gate (2) is the one `validate
--opponent hbt` above is for -- run it at every stage, not just at the end.

## Why this agent's calibration is narrower than most agents' -- but wider than A9's

Like `A9` HBT 2-Ply, the 34 inherited `HBTParams` fields are hard-pinned (`PINNED_PARAM_NAMES`
in `calibrate_a13.py`), not just defaulted, and there is no `--identity-safe` escape hatch:
there is nothing of `A7`'s own tuning to erode here. Unlike `A9`, one inherited field --
`defense_stdev_mult` -- IS re-fittable, but only at Stage 4, and only because Layer R
structurally changes what that field means (a baseline rather than the whole value). See
`ai_strat_a13.h`'s struct comment on `A13Params.base` for the full reasoning.

## `check_personality_flags()` mirrors the four-layer risk ranking, not a single check

Unlike `A9`'s narrow "has the ply been optimized into irrelevance" check (one mechanism, one
flag), this agent's identity is four independent layers, each with its own documented
failure signature (`about.md`'s risk ranking): Layer R collapsing to 0, the belief layer
(K-draw/D/K-block) collapsing to 0, the two race-variance dials converging to each other
(state-dependence collapsing back to `A7`'s own constant), `race_scale` saturating at its
search ceiling, and a sign-convention check on `race_stdev_ahead`/`race_stdev_behind`
(informational, not an auto-reject -- a calibration finding the opposite of the documented
intuition is possible and not automatically wrong, same precedent as `A5`'s gamma drift).

## The all-neutral configuration is this agent's regression anchor, not just a calibration bound

Every new dial at its default (0, or `false` for `race_use_belief_opp`) is proven (see the
whole-game bit-for-bit check run during implementation, and `testsrc/test_moves.c`'s planned
`test_a13_neutral_matches_a7`) to recover `A7`'s own decisions exactly, at both A7's own
cost. Any stage whose `optimize` result lands back near all-neutral is telling you that
layer isn't paying for itself -- pin it there and move to the next stage rather than forcing
a nonzero value. If every stage ends this way, the correct outcome (Jonathan's call,
2026-08-31) is to shelve this agent entirely rather than register a second copy of `A7`
under a new name -- see the top-level plan's "What ships in each outcome" section.
