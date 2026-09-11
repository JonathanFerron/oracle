# A15 Risk Threshold ("The Daredevil") calibration tooling

Calibration for `src/ai_strat/ai_strat_a15.c`'s 6-field `A15Params`
(`defense_loss_threshold`, `endgame_q1`-`endgame_q4`, `endgame_enabled`). See
`doc/ai_agents.md`'s A15 section and `doc/changelog.md` for the design record.

**No ship gate here.** Unlike a design-target agent, A15 is a direct transcription
of Jonathan's own real-table decision procedure (`ai_strat_a15.h`'s header comment)
-- its measured Borealis rating is diagnostic, not a pass/fail bar. Calibration's job
is to find defensible values for the 5 real dials and to answer one honest empirical
question about R8 (see "What's actually being asked" below), not to chase a target
number.

One subfolder per agent under `aicalibsrc/`, mirroring `aicalibsrc/hbt2ply/` -- keep
each agent's harness and driver self-contained rather than accumulating loose files
at the top level.

## 2026-09-10 calibration run (shipped) -- two passes, R8's trigger redesigned mid-way

**Pass 1** (hand-size-derived horizon -- since superseded): per-dial sweeps vs
`borealis` found the only winning configuration was `endgame_q2`/`q3`/`q4`
pinned to a near-zero epsilon (`0.01`), which measured 44.98% `[44.44%,
45.53%]` (n=32,000) but meant R8 was never actually gating on confidence.
Jonathan pushed back on this directly -- a near-zero threshold reads as
"attack almost unconditionally," not "wait for a well-justified chance"
(`ai_strat_a15.h`'s own naming rationale for "calculated, not reckless"). A
direct diagnostic (logging every `P(finish)` the old mechanism evaluated
across 200 games) confirmed the concern: **over 90% of evaluations landed
below p=0.2 regardless of horizon** -- "my hand has thinned to N attacks'
worth of champions" turns out to be almost completely decoupled from "the
opponent is actually close to dying." A genuinely selective q on that
hand-size-derived horizon fired essentially never (~0.5 times/game at
q=0.5, vs ~1.5 times/game at the near-zero q=0.01) because the hand-size
pre-filter had already discarded nearly every real opportunity along with
the bad ones.

**Pass 2** (current, `find_triggering_horizon()` in
`ai_strat_a15_endgame.c`): the hand-size proxy was dropped entirely. Every
attack turn now checks `P(finish within N)` directly for **all four**
horizons N=1-4, firing on the smallest N that clears its own `q_N` -- no
resource-state pre-filter. Recalibrated from scratch (same sweep ->
optimize order below), this produced a materially different and more
interpretable answer:

- **R8 is still load-bearing** (true under both mechanisms): ablation
  (`endgame_enabled=False`, same `defense_loss_threshold`) measures **7.06%**
  vs `borealis`, n=32,000. With R8 on and calibrated: **46.56% [46.02%,
  47.11%]**, n=32,000.
- **`endgame_q2`/`q3`/`q4` are now genuine, high confidence bars** --
  `0.78`/`0.75`/`0.69` -- exactly the "wait for a well-justified chance"
  character the design called for. This is the result that vindicates
  dropping the hand-size proxy: once the check reflects real finish
  probability instead of resource state, selectivity measures *as well as*
  the old near-zero hack did, while actually meaning what it says.
- **`endgame_q1` (the 4-attack horizon) is the one deliberate exception**,
  shipped at a small epsilon (`0.01`) rather than a real confidence bar --
  `P(finish within 4)` is checked LAST (only once the genuine confidence
  gates at horizons 1-3 have all failed) and is the loosest of the four
  cumulative probabilities, so its role isn't "confidently predict a win in
  4 attacks," it's "don't prematurely rule out pursuing one at all." Raising
  it to a real bar (tested at `0.5`) measured 31.38% on otherwise-identical
  q2-q4 -- a large, real regression, confirming this asymmetry is load-
  bearing too, not cosmetic.

Shipped: `defense_loss_threshold=0.20`, `endgame_q1=0.01`,
`endgame_q2=0.78`, `endgame_q3=0.75`, `endgame_q4=0.69`,
`endgame_enabled=True`. Measured **46.56% vs `borealis`** -- see
`doc/ai_agents.md`'s A15 section for the full writeup including the
roster-context rating, and `ai_strat_a15_endgame.h`'s header comment for the
mechanism redesign itself.

## Files

- `calib_daredevil.c` -- C calibration harness. Links the game engine directly (same
  pattern as every other `aicalibsrc/*/calib_*.c`), so it runs `run_simulation()`
  in-process with no subprocess-spawn or text-parsing overhead. Build with
  `make calib_daredevil` (from the repo root) -> `bin/calib_daredevil`. See the
  file's header comment for its CLI (four fixed args, then 6 `A15Params` fields for
  Player A in `ai_strat_a15.h`'s declared order, then the same 6 for Player B), or
  run `bin/calib_daredevil --print-defaults` to dump the compiled defaults as flat
  JSON.
- `calibrate_daredevil.py` -- Python driver on top of that binary. Orchestrates many
  calibration runs and does the statistics/search; the actual game simulation always
  happens in the C binary. Its `DEFAULTS` dict is read once, at import time, from
  `bin/calib_daredevil --print-defaults`, so it cannot drift from the shipped C
  constants. See the file's module docstring for the `sweep`/`optimize`/`selfplay`/
  `validate` subcommands and usage examples.

## Setup

```bash
make calib_daredevil    # from the repo root
```

Same Python dependencies as every other harness (`numpy`, `pandas`, `scipy`,
`matplotlib`) -- already installed if any of them has been used. On Debian/Ubuntu,
`pip install` is blocked by PEP 668 without a venv; the system packages are the
simplest path:

```bash
sudo apt install python3-pandas python3-scipy python3-matplotlib
```

## Usage

```bash
cd aicalibsrc/daredevil

# Univariate sweep: does this one parameter actually move the needle? Useful for
# all 5 real dials plus the endgame_enabled ablation (values 0/1).
./calibrate_daredevil.py sweep --param defense_loss_threshold --opponent borealis \
    --numsim 2000 --replicates 4 --plot

# Black-box search (differential evolution) over all 5 continuous dials vs a fixed
# opponent -- endgame_enabled is not searched here (it's a boolean ablation switch,
# not a continuous dial); use two validate()/selfplay() runs instead (see below).
./calibrate_daredevil.py optimize --opponent borealis \
    --numsim 500 --replicates 2 --maxiter 15 --popsize 12

# Compare a handful of named candidates head-to-head (round-robin, Bradley-Terry
# fit) -- e.g. the shipped defaults against one or two optimize() outputs
./calibrate_daredevil.py selfplay --candidates defaults \
    results/optimize_borealis.json --numsim 2000 --replicates 4

# Compare one candidate against the shipped defaults directly
./calibrate_daredevil.py validate --candidate results/optimize_borealis.json \
    --opponent borealis --numsim 2000 --replicates 4
```

Candidate parameter sets are JSON files holding a full or partial `A15Params` dict
(missing fields fall back to the compiled defaults), or the literal string
`defaults`. `optimize`'s own output file works directly as a `selfplay`/`validate`
candidate -- it nests the params under a `"best_params"` key, which both commands
know to unwrap.

Results are written to `results/*.{csv,json,png}` (gitignored) plus a stdout
summary. Pass `--plot` to `sweep` for a PNG alongside the CSV.

## No `--identity-safe` mode: this driver frees all 6 fields, on purpose

Every inherited-parameter agent's driver (`A9`/`A13`/`A14`) hard-pins the fields it
inherits from an earlier agent and frees only its own new ones. `A15` inherits
nothing -- `ai_strat_a15.h`'s entire rule chain (R1-R9) is a from-scratch
transcription with exactly 5 free calibration targets (R4's
`defense_loss_threshold`, R8's `endgame_q1`-`q4`) and one structural switch
(`endgame_enabled`). `PINNED_PARAM_NAMES` in `calibrate_daredevil.py` is
deliberately empty, and there is no `--identity-safe` escape hatch, matching the
`hbt2ply`/`carto`/`puct` precedent for an agent with no inherited character to
erode -- there's simply nothing here to protect a free search from.

## What's actually being asked: is R8 worth its own approximation error?

Every prior agent's `check_personality_flags()` protects a DESIGNED character from
erosion by a free search. A15 has no such character to protect (it's a
transcription, not a design) -- so this driver's version checks something
different: whether R8, the one genuinely speculative piece of this agent
(`ai_strat_a15_prob.c`'s `a15_p_finish_within()` is a normal approximation with a
fixed, unmeasured opponent-block heuristic, not R4's exact convolution -- see that
file's header comment), is actually contributing anything once calibrated, or
whether it's been optimized into a corner that's functionally equivalent to
`endgame_enabled=False`. It flags: all four `endgame_q1`-`q4` landing at or above
0.95 (R8 would almost never fire -- compare directly against an
`endgame_enabled=False` run), and `defense_loss_threshold` landing at or above 0.95
(R4 would almost never defend, a large shift from the design doc's own ~30%
starting estimate, worth a manual read even if it measures stronger).

**The direct ablation test this agent's own design calls for** (not automated by
any single subcommand -- run both and compare):

```bash
# endgame_enabled = True (the calibrated/default config)
./calibrate_daredevil.py validate --candidate defaults --opponent borealis \
    --numsim 2000 --replicates 4

# endgame_enabled = False (R8 entirely off) -- write a JSON candidate with just
# {"endgame_enabled": false} and pass it as --candidate
echo '{"endgame_enabled": false}' > results/no_endgame.json
./calibrate_daredevil.py validate --candidate results/no_endgame.json \
    --opponent borealis --numsim 2000 --replicates 4
```

If `endgame_enabled=False` matches or beats `True` at the calibrated
`defense_loss_threshold`, that is the single most important finding this
calibration pass can produce -- R8's approximation isn't paying for itself, and
that's a legitimate, reportable result (same epistemic status as A8's rollout-bias
diagnosis or A13's `hplus_trust` null result), not a failure of calibration.

## Recommended calibration order

1. `sweep --param defense_loss_threshold --opponent borealis` -- R4's dial is exact
   (no approximation error to worry about), so this sweep is the cleanest signal in
   the whole parameter set.
2. `sweep --param endgame_qN --opponent borealis` for each of the 4 horizons --
   individually, since a joint search can mask a genuinely dead dial the way it did
   for `A13`'s `hplus_trust` before that sweep isolated it.
3. The direct ablation test above, at whatever `defense_loss_threshold` step 1
   suggests.
4. `optimize --opponent borealis` over all 5 continuous dials jointly, only after
   the per-dial sweeps have a story to compare it against.
5. `validate`/`selfplay` the optimizer's winner against `defaults` and against a
   few other roster agents (`hbt`, `carto`) for the qualitative record --
   remembering that for this agent, "loses to `hbt`" is not itself evidence of a
   bug the way it would be for a design-target agent.
