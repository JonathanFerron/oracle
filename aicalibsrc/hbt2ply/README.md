# A9 HBT 2-Ply ("The Grandmaster II") calibration tooling

Calibration for `src/ai_strat/ai_strat_hbt2ply.c`'s four new tunable parameters
(`HBT2PlyParams`'s `reply_trust`/`surrogate_pessimism`/`ply_energy_ceiling`/`ply_beam_width`
-- the 34 `HBTParams` fields it also carries are inherited from `A7` and hard-pinned, never
searched here). See `doc/changelog.md` for the design discussion and the calibration run
that produced the shipped values.

**2026-08-28 re-attempt (no change shipped)**: after A7's PASS-dominance defense fix was
recalibrated (rating 58 -> 65, see `aicalibsrc/hbt/README.md`), re-ran `optimize
--opponent hbt` against the new A7 to see if this agent could finally clear its own
>55% head-to-head design target -- the original 2026-08-26 diagnosis had pinned the
shortfall on A7's defense never blocking. It did not clear the target: the search
converged to `reply_trust=0.013` (i.e. "turn the ply off"), validated statistically
indistinguishable from the shipped defaults against the same new A7, and flagged by
`check_personality_flags()` as calibrating the ply into irrelevance. A follow-up
`sweep --param reply_trust --opponent hbt` confirmed this isn't a search artifact: win
rate declines monotonically as trust increases (47.64% at 0.0 down to 31.20% at 1.0).
The original "fix A7's defense first" diagnosis does not hold -- the two-ply mechanism
has no room to improve on a well-calibrated A7 via these two dials. `HBT2PLY_DEFAULTS`
unchanged; see `doc/changelog.md`'s 2026-08-28 entry for the full record.

One subfolder per agent under `aicalibsrc/`, mirroring `aicalibsrc/hbt/` -- keep each
agent's harness and driver self-contained rather than accumulating loose files at the top
level.

## Files

- `calib_hbt2ply.c` -- C calibration harness. Links the game engine directly (same pattern
  as `aicalibsrc/hbt/calib_hbt.c`), so it runs `run_simulation()` in-process with no
  subprocess-spawn or text-parsing overhead. Build with `make calib_hbt2ply` (from the repo
  root) -> `bin/calib_hbt2ply`. See the file's header comment for its CLI (four agent args,
  then 38 `HBT2PlyParams` fields for Player A -- the 34 inherited `HBTParams` fields in
  `ai_strat_hbt.h`'s declared order, then this agent's own 4 -- then the same 38 for
  Player B), or run `bin/calib_hbt2ply --print-defaults` to dump the compiled defaults as
  flat JSON.
- `calibrate_hbt2ply.py` -- Python driver on top of that binary. Orchestrates many
  calibration runs and does the statistics/search; the actual game simulation always
  happens in the C binary. Its `DEFAULTS` dict is read once, at import time, from
  `bin/calib_hbt2ply --print-defaults`, so it cannot drift from the shipped C constants.
  See the file's module docstring for the `sweep`/`optimize`/`selfplay`/`validate`
  subcommands and usage examples.

## Setup

```bash
make calib_hbt2ply      # from the repo root
```

Same Python dependencies as the other seven harnesses (`numpy`, `pandas`, `scipy`,
`matplotlib`) -- already installed if any of them has been used. On Debian/Ubuntu,
`pip install` is blocked by PEP 668 without a venv; the system packages are the simplest
path:

```bash
sudo apt install python3-pandas python3-scipy python3-matplotlib
```

## Usage

```bash
cd aicalibsrc/hbt2ply

# Univariate sweep: does this one parameter actually move the needle? Useful for all
# 4 of this agent's own fields, including the 2 compute-budget ones (see below).
./calibrate_hbt2ply.py sweep --param reply_trust --opponent borealis \
    --numsim 2000 --replicates 4 --plot

# Black-box search (differential evolution) vs a fixed opponent -- default free set is
# exactly the 2 real behaviour dials (OPTIMIZE_PARAM_NAMES in the script), since the 34
# inherited HBTParams fields are hard-pinned (no --identity-safe escape hatch here -- see
# "Why this agent's calibration is narrower than A7's" below) and the 2 compute-budget
# fields are meant for `sweep`, not this search.
./calibrate_hbt2ply.py optimize --opponent borealis \
    --numsim 500 --replicates 2 --maxiter 15 --popsize 12

# Compare a handful of named candidates head-to-head (round-robin, Bradley-Terry fit) --
# e.g. the shipped defaults against one or two optimize() outputs
./calibrate_hbt2ply.py selfplay --candidates defaults \
    results/optimize_borealis.json --numsim 2000 --replicates 4

# Compare one candidate against the shipped defaults directly -- against `hbt` (A7) is
# the more informative comparison for this agent than the usual `rand`/`borealis`
# baselines, since A9's whole reason for existing is to beat A7
./calibrate_hbt2ply.py validate --candidate results/optimize_borealis.json \
    --opponent hbt --numsim 2000 --replicates 4
```

Candidate parameter sets are JSON files holding a full or partial `HBT2PlyParams` dict
(missing fields fall back to the compiled defaults), or the literal string `defaults`.
`optimize`'s own output file works directly as a `selfplay`/`validate` candidate -- it
nests the params under a `"best_params"` key, which both commands know to unwrap.

Results are written to `results/*.{csv,json,png}` (gitignored) plus a stdout summary.
Pass `--plot` to `sweep` for a PNG alongside the CSV.

## Why this agent's calibration is narrower than every other agent's

Every other `aicalibsrc/` driver eventually frees some or all of its agent's own
mechanism, with an `--identity-safe` escape hatch for when a free search erodes the
designed character. This driver has neither, on purpose:

- **The 34 inherited `HBTParams` fields are hard-pinned, not just defaulted.**
  `about.md`'s framing is explicit: "the only new thing this agent adds over `A7` is the
  second ply" -- so `PINNED_PARAM_NAMES` in `calibrate_hbt2ply.py` excludes all 34 from
  both `sweep --param` and `optimize --params`' `choices`, regardless of what's requested.
  Recalibrating `A7`'s own tuned values is `aicalibsrc/hbt/calibrate_hbt.py`'s job, not
  this file's -- doing it here would mean this agent is no longer "A7 plus one ply."
- **Only 2 of the remaining 4 fields are real behaviour dials.** `reply_trust` and
  `surrogate_pessimism` are `OPTIMIZE_PARAM_NAMES`, `optimize`'s default free set.
  `ply_energy_ceiling` and `ply_beam_width` are pure compute-budget dials -- the same
  split `A8` Simple Monte Carlo's twenty parameters made (`doc/changelog.md`): more ply
  coverage is basically always at least as strong, just slower, so these are `sweep`t to
  understand their cost/strength tradeoff rather than searched for a "best" value.

## The central identity check is much narrower than A7's three-layer one

`A7`'s `check_personality_flags()` verifies three separate mechanisms (T/B/H) stay alive,
matching its "synthesis IS the personality" framing. This agent's framing is narrower --
"the personality is specifically the added ply" -- so `check_personality_flags()` here
checks exactly one thing: has the ply been calibrated into irrelevance. It flags
`reply_trust` collapsing near 0 (this agent would then be, in practice, an expensive copy
of `A7`) or `ply_energy_ceiling` collapsing low enough that it almost never gates on in a
real game. There is no multi-layer battery here because there is only one layer to lose.

## `reply_trust = 0` is this agent's regression anchor, not just a calibration bound

`reply_trust = 0` is proven (see `testsrc/test_moves.c`'s
`test_hbt2ply_reply_trust_zero_matches_a7`) to recover `A7`'s own decision bit-for-bit.
`BOUNDS["reply_trust"] = (0.0, 1.0)` therefore always includes the point at which this
agent is provably no worse than `A7` (mechanically -- not necessarily rating-wise, since
`A7`'s own defense formula has a documented PASS-dominance issue this agent's local
`hbt2ply_reply_defense_move()` corrects for internally, see `ai_strat_hbt2ply.h`). Any
`optimize` run that lands near `reply_trust = 0` is telling you the ply isn't paying for
itself at the current `surrogate_pessimism` -- worth investigating jointly rather than
assuming one dial is simply "off."
