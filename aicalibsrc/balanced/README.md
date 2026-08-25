# A4 Balanced Rules calibration tooling

Calibration for `src/ai_strat/ai_strat_balanced_rules.c`'s ten tunable parameters
(`BalancedRulesParams`). See `doc/changelog.md` for the design discussion and the
calibration run that produced the shipped values (2026-08-24).

One subfolder per agent under `aicalibsrc/`, mirroring `aicalibsrc/borealis/` -- keep
each agent's harness and driver self-contained rather than accumulating loose files
at the top level.

## Files

- `calib_balanced.c` -- C calibration harness. Links the game engine directly (same
  pattern as `aicalibsrc/borealis/calib_borealis.c`), so it runs `run_simulation()`
  in-process with no subprocess-spawn or text-parsing overhead. Build with
  `make calib_balanced` (from the repo root) -> `bin/calib_balanced`. See the file's
  header comment for its CLI (four agent args, then ten `BalancedRulesParams` fields
  for Player A, then the same ten for Player B), or run
  `bin/calib_balanced --print-defaults` to dump the compiled defaults as JSON.
- `calibrate_balanced.py` -- Python driver on top of that binary. Orchestrates many
  calibration runs and does the statistics/search; the actual game simulation always
  happens in the C binary. Unlike the three earlier drivers, its `DEFAULTS` dict is
  never hand-copied -- it's read once, at import time, from
  `bin/calib_balanced --print-defaults`, so it cannot drift from the shipped C
  constants the way `aicalibsrc/value/`'s, `aicalibsrc/combo/`'s and
  `aicalibsrc/borealis/`'s copies have (`doc/oracle_todo.md` tracks that as an open
  item for those three). See the file's module docstring for the
  `sweep`/`optimize`/`selfplay`/`validate` subcommands and usage examples.

## Setup

```bash
make calib_balanced          # from the repo root
```

Same Python dependencies as the other three harnesses (`numpy`, `pandas`, `scipy`,
`matplotlib`) -- already installed if any of them has been used. On Debian/Ubuntu,
`pip install` is blocked by PEP 668 without a venv; the system packages are the
simplest path:

```bash
sudo apt install python3-pandas python3-scipy python3-matplotlib
```

## Usage

```bash
cd aicalibsrc/balanced

# Univariate sweep: does this one parameter actually move the needle? Check
# defense_beta first -- a flat curve means the variance term isn't wired in.
./calibrate_balanced.py sweep --param defense_beta --opponent borealis \
    --numsim 2000 --replicates 4 --plot

# Black-box search (differential evolution) vs a fixed opponent, respecting this
# agent's identity constraints (see "Why --identity-safe exists" below). Keep
# --numsim modest here -- it's a search, called hundreds of times; the winner
# gets re-validated afterwards with more games automatically.
./calibrate_balanced.py optimize --opponent borealis --identity-safe \
    --numsim 500 --replicates 2 --maxiter 15 --popsize 12

# Compare a handful of named candidates head-to-head (round-robin, Bradley-Terry
# fit) -- e.g. the shipped defaults against one or two optimize() outputs
./calibrate_balanced.py selfplay --candidates defaults \
    results/candidate_vs_borealis.json --numsim 2000 --replicates 4

# Compare one candidate against the shipped defaults directly
./calibrate_balanced.py validate --candidate results/candidate_vs_borealis.json \
    --opponent rand --numsim 2000 --replicates 4
```

Candidate parameter sets are JSON files holding a full or partial
`BalancedRulesParams` dict (missing fields fall back to the compiled defaults), or
the literal string `defaults`. `optimize`'s own output file works directly as a
`selfplay`/`validate` candidate -- it nests the params under a `"best_params"` key,
which both commands know to unwrap.

Results are written to `results/*.{csv,json,png}` (gitignored) plus a stdout summary.
Pass `--plot` to `sweep` for a PNG alongside the CSV.

## Why `optimize` targets `borealis` by default -- and why it's still a hard fight

Balanced Rules' design-intent rating (62, `ideas/A4 .../about.md`) *is* its expected
win rate against Borealis, the rating-50 anchor, so `borealis` is both the natural and
the most informative opponent to calibrate against, unlike `rand` (ceiling-effected at
~98%+ for every implemented agent) or `value`/`combo` (both land close to parity for
this agent, real headroom, but neither is the number that actually gets reported).
Measure at defaults first if calibrating a future revision -- if `borealis` turns out
to be a pure floor effect for a particular parameter region, `combo` is the fallback
target, the same way A1/A2/A3 each picked whichever opponent had real two-sided
headroom.

Shipping a rating below 50 against Borealis is an accepted, legitimate outcome here,
not a bug to chase away -- see `doc/changelog.md`'s 2026-08-24 entry and the
implementation plan's Finding 9. A closed-form, no-search resource-conservation agent
is not guaranteed to beat a lambda-tuned exhaustive 0-3-champion enumerator, and
calibration measured that it doesn't (rating ~36 on the roster-wide `--stda.rating`
fit). The job of `optimize` here is to find the best version of *this* agent, not to
force a number above 50.

## Why `--identity-safe` exists

Two free `optimize` runs (all ten parameters, `--opponent combo`, 2026-08-24) each
independently drove `target_cash_slope`/`target_cards_slope` toward 0 and
`defense_beta` past 2.0 -- i.e. "spend everything regardless of opponent energy" and
"rarely defend." Both measured *stronger* (up to 70.7% vs `combo`) but erode exactly
the traits `check_personality_flags()` exists to protect: obsessive, opponent-energy-
scaled resource accounting is this agent's entire identity (`about.md`), not a
tunable weighted sum to be optimized away. This mirrors A2's rejected
`aggression_level=2.21` precedent (`doc/changelog.md`, 2026-08-22) -- a result that
measures stronger by eroding the agent's designed character is not a win.

Rather than hand-pick a compromise from limited sweep data, `--identity-safe` re-runs
the search inside `BOUNDS_IDENTITY_SAFE` (`calibrate_balanced.py`), which keeps both
resource-target slopes non-degenerate (matching `check_personality_flags()`'s own
thresholds) and `defense_beta` inside `[0.25, 2.0]` by construction, and always keeps
`combo_weight` fixed at its blind default regardless of `--params` -- combo-blindness
is this agent's scope boundary (`about.md`: combo scoring as a primary signal belongs
to A2/A3), not a personality trait to negotiate with a search. Two `--identity-safe`
runs, targeting `combo` and then `borealis` separately, converged to statistically
indistinguishable ~34-35% win rates vs `borealis` from different starting points --
a stable result, not an artifact of one particular search.

`check_personality_flags()` and the `defense_beta` re-sweep still run under
`--identity-safe` and should report clean (no flags) by construction; they're kept as
a sanity check and because the plain `optimize` mode (without `--identity-safe`)
remains available for exploring outside the safe region, e.g. to characterize how much
strength is being traded away for identity.

## selfplay vs a full grid

Ten parameters make a full Cartesian grid infeasible, so `selfplay` round-robins a
small set of explicitly *named* candidates (JSON files, typically `optimize` outputs
or hand-picked alternatives) rather than every combination of per-axis values,
matching `aicalibsrc/combo/`'s and `aicalibsrc/borealis/`'s pattern. Use `sweep` to
check one axis at a time, `optimize --identity-safe` to search the full
character-respecting space, and `selfplay` to compare a short list of finalists
head-to-head -- this is exactly how the shipped defaults were chosen: two
`--identity-safe` candidates (targeting `combo` and `borealis` respectively) plus the
original spec-derived defaults, round-robinned at 24,000 games per pairing.
