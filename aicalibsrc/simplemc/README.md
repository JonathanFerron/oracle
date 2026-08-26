# A8 Simple Monte Carlo ("The Soothsayer") calibration tooling

Calibration for `src/ai_strat/ai_strat_simplemc1.c`'s twenty tunable parameters
(`SimpleMcParams`). See `doc/changelog.md` for the design discussion and whatever
calibration run produced the shipped values.

One subfolder per agent under `aicalibsrc/`, mirroring `aicalibsrc/tactical/`/
`aicalibsrc/hbt/` -- keep each agent's harness and driver self-contained rather than
accumulating loose files at the top level.

## Files

- `calib_simplemc.c` -- C calibration harness. Links the game engine directly (same
  pattern as `aicalibsrc/hbt/calib_hbt.c`), so it runs `run_simulation()` in-process with
  no subprocess-spawn or text-parsing overhead. Build with `make calib_simplemc` (from the
  repo root) -> `bin/calib_simplemc`. See the file's header comment for its CLI (four
  agent args, then twenty `SimpleMcParams` fields for Player A, then the same twenty for
  Player B), or run `bin/calib_simplemc --print-defaults` to dump the compiled defaults as
  JSON.
- `calibrate_simplemc.py` -- Python driver on top of that binary. Orchestrates many
  calibration runs and does the statistics/search; the actual game simulation always
  happens in the C binary. Its `DEFAULTS` dict is read once, at import time, from
  `bin/calib_simplemc --print-defaults`, so it cannot drift from the shipped C constants.
  See the file's module docstring for the `sweep`/`optimize`/`selfplay`/`validate`
  subcommands and usage examples.

## Setup

```bash
make calib_simplemc          # from the repo root
```

Same Python dependencies as the other seven harnesses (`numpy`, `pandas`, `scipy`,
`matplotlib`) -- already installed if any of them has been used. On Debian/Ubuntu,
`pip install` is blocked by PEP 668 without a venv; the system packages are the
simplest path:

```bash
sudo apt install python3-pandas python3-scipy python3-matplotlib
```

## This agent's parameters are a different shape than A1-A7's -- read this first

A1-A7 tune decision-quality **weights** for a fixed-cost algorithm: calibration finds the
one best value for each weight. Simple Monte Carlo is a search, not a weighted-sum
heuristic -- most of `SimpleMcParams` controls how much this agent *searches*, and more
searching is basically always at least as strong, just slower. There is no interior
optimum to find for those fields, only a cost/strength curve. The twenty fields split
three ways (see `calibrate_simplemc.py`'s module docstring for the full breakdown):

- **`BUDGET_PARAMS`** (7 fields: `rollout_seed_simulations`, `rollout_round_simulations`,
  `limit_stage1/2/3_simulations`, `limit_max_simulations`, `limit_total_rollouts`) -- pure
  compute-budget dials. `sweep` these to see the cost/strength curve and pick a shipped
  budget; `optimize` does **not** search them by default (it would just walk them to their
  upper bound, which isn't a meaningful search).
- **`EFFICIENCY_PARAMS`** (11 fields: the enumeration caps, the pruning confidence level,
  the stage keep-ratios/hard-caps, `prune_zero_win_seed`) -- these trade real risk for
  real speed, so there *is* a genuine interior optimum here. `optimize` searches exactly
  these by default, at a budget fixed at the shipped defaults.
- **Everything else** (2 fields): `rollout_determinize` is a binary research question
  (how much is hidden-information reasoning worth to this agent?), not a continuous dial
  -- A/B it via `validate --candidate <a JSON file of just {"rollout_determinize": 0}>`
  rather than searching it (see "A/B determinization" below). `rollout_max_turns` is
  inert (games never come close to `MAX_NUMBER_OF_TURNS`) and isn't touched anywhere.

## Usage

```bash
cd aicalibsrc/simplemc

# Cost/strength curve for a BUDGET_PARAMS field -- how much does spending more
# rollouts-per-candidate actually buy?
./calibrate_simplemc.py sweep --param rollout_round_simulations --opponent rand \
    --numsim 20 --replicates 2

# Black-box search over EFFICIENCY_PARAMS only (the default), vs a fixed opponent.
./calibrate_simplemc.py optimize --opponent borealis --numsim 20 --replicates 1 \
    --maxiter 4 --popsize 6

# Search a different subset explicitly (e.g. to also let a BUDGET_PARAMS field move) --
# --params always overrides the default EFFICIENCY_PARAMS list.
./calibrate_simplemc.py optimize --opponent borealis \
    --params threshold_confidence_level limit_stage2_keep rollout_round_simulations

# Compare a handful of named candidates head-to-head (round-robin, Bradley-Terry fit).
./calibrate_simplemc.py selfplay --candidates defaults results/optimize_borealis.json \
    --numsim 20 --replicates 2

# Compare one candidate against the shipped defaults directly.
./calibrate_simplemc.py validate --candidate results/optimize_borealis.json \
    --opponent rand --numsim 20 --replicates 2
```

Candidate parameter sets are JSON files holding a full or partial `SimpleMcParams` dict
(missing fields fall back to the compiled defaults), or the literal string `defaults`.
`optimize`'s own output file works directly as a `selfplay`/`validate` candidate -- it
nests the params under a `"best_params"` key, which both commands know to unwrap.

Results are written to `results/*.{csv,json,png}` (gitignored) plus a stdout summary.
Pass `--plot` to `sweep` for a PNG alongside the CSV.

### A/B determinization

`rollout_determinize` (default `true`) controls whether each simulation re-deals the
information hidden from this agent (`ai_strat_playout.h`'s `mc_determinize()`) before
rolling out, or plays every rollout on the exact root clone (cheating -- it can see the
opponent's hand and both deck orders). Measure what it's actually worth:

```bash
echo '{"rollout_determinize": 0}' > /tmp/no_determinize.json
./calibrate_simplemc.py validate --candidate /tmp/no_determinize.json --opponent borealis \
    --numsim 500 --replicates 4
```

A large positive delta for the determinizing default confirms the hidden-information
reasoning is pulling its weight; a small or negative delta would be a genuinely
interesting finding (and a datapoint for `A10`'s reshuffle-aware refinement of the same
mechanism).

## Runtime: this agent costs ~100x more per game than A1-A7

Measured directly (isolated timing of a single decision at the shipped default budget,
`limit_total_rollouts=25000`): **2.8-4.7ms average per decision**, worst observed
~45ms, vs a ~250ms theoretical ceiling if confidence-interval pruning never converged
before exhausting the budget (in practice it always does, well before that). At the
whole-game level this is roughly **~100ms/game** (vs `rand`; A1-A7 cost well under 1ms/
game). Every default above (`--numsim=20`, small `--maxiter`/`--popsize`) is sized for a
**~30-second prelim run**, not a final calibration. Scale `--numsim`/`--replicates`/
`--maxiter`/`--popsize` up deliberately only for the one run whose result actually ships
-- that one can run up to **~20 minutes** (Jonathan's guidance, 2026-08-25). Don't scale
up prelim/exploratory runs as a matter of course; the whole point of the small defaults
is to keep the edit-measure loop fast during search-mechanism iteration.

## Search-health flags, not personality flags

Every other agent's driver has a `check_personality_flags()` that catches a
decision-quality mechanism collapsing toward a static/degenerate value (A6's about.md:
"a static version of this agent is a worse Heuristic, not a Tactical agent"). This agent
has no strategic weights to collapse -- what can go wrong instead is the **search**
becoming unsound:

- `threshold_confidence_level` (z) below `1.0` -- pruning that aggressive risks discarding
  the true best move on noisy early samples (a <68% confidence band, not the ~95%+ a
  normal approximation to the binomial should use for this).
- `threshold_stage1/2/3_keep_ratio` or `limit_stage1/2/3_keep` not monotonically
  decreasing -- a later stage cap would stop narrowing (or would re-widen) the survivor
  set instead of progressively pruning it, defeating the whole point of staged pruning.

Same policy as every other agent: these never auto-reject a candidate, only flag it for
review before shipping.
