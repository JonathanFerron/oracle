# A5 Heuristic ("Eps-Gam-Del") calibration tooling

Calibration for `src/ai_strat/ai_strat_heuristic.c`'s five tunable parameters
(`HeuristicParams`). See `doc/changelog.md` for the design discussion and the
calibration run that produced the shipped values.

One subfolder per agent under `aicalibsrc/`, mirroring `aicalibsrc/balanced/` -- keep
each agent's harness and driver self-contained rather than accumulating loose files
at the top level.

## Files

- `calib_heuristic.c` -- C calibration harness. Links the game engine directly (same
  pattern as `aicalibsrc/balanced/calib_balanced.c`), so it runs `run_simulation()`
  in-process with no subprocess-spawn or text-parsing overhead. Build with
  `make calib_heuristic` (from the repo root) -> `bin/calib_heuristic`. See the file's
  header comment for its CLI (four agent args, then five `HeuristicParams` fields for
  Player A, then the same five for Player B), or run
  `bin/calib_heuristic --print-defaults` to dump the compiled defaults as JSON.
- `calibrate_heuristic.py` -- Python driver on top of that binary. Orchestrates many
  calibration runs and does the statistics/search; the actual game simulation always
  happens in the C binary. Its `DEFAULTS` dict is read once, at import time, from
  `bin/calib_heuristic --print-defaults`, so it cannot drift from the shipped C
  constants (the `--print-defaults` discipline `aicalibsrc/balanced/` introduced). See
  the file's module docstring for the `sweep`/`optimize`/`selfplay`/`validate`
  subcommands and usage examples.

## Setup

```bash
make calib_heuristic          # from the repo root
```

Same Python dependencies as the other four harnesses (`numpy`, `pandas`, `scipy`,
`matplotlib`) -- already installed if any of them has been used. On Debian/Ubuntu,
`pip install` is blocked by PEP 668 without a venv; the system packages are the
simplest path:

```bash
sudo apt install python3-pandas python3-scipy python3-matplotlib
```

## Usage

```bash
cd aicalibsrc/heuristic

# Univariate sweep: does this one parameter actually move the needle?
./calibrate_heuristic.py sweep --param weight_cards_advantage --opponent borealis \
    --numsim 2000 --replicates 4 --plot

# Black-box search (differential evolution) vs a fixed opponent. delta
# (weight_cash_advantage) stays pinned unless explicitly named in --params (see
# "Why weight_cash_advantage is pinned" below). Keep --numsim modest here -- it's
# a search, called hundreds of times; the winner gets re-validated afterwards with
# more games automatically.
./calibrate_heuristic.py optimize --opponent borealis --identity-safe \
    --numsim 500 --replicates 2 --maxiter 15 --popsize 12

# Compare a handful of named candidates head-to-head (round-robin, Bradley-Terry
# fit) -- e.g. the shipped defaults against one or two optimize() outputs
./calibrate_heuristic.py selfplay --candidates defaults \
    results/unconstrained.json results/identity_safe.json --numsim 3000 --replicates 4

# Compare one candidate against the shipped defaults directly
./calibrate_heuristic.py validate --candidate results/identity_safe.json \
    --opponent rand --numsim 2000 --replicates 4
```

Candidate parameter sets are JSON files holding a full or partial `HeuristicParams`
dict (missing fields fall back to the compiled defaults), or the literal string
`defaults`. `optimize`'s own output file works directly as a `selfplay`/`validate`
candidate -- it nests the params under a `"best_params"` key, which both commands know
to unwrap.

Results are written to `results/*.{csv,json,png}` (gitignored) plus a stdout summary.
Pass `--plot` to `sweep` for a PNG alongside the CSV.

## Why `weight_cash_advantage` is pinned

The argmax of a weighted sum of three terms is invariant to a positive rescaling of
all three weights together, so with three free weights one degree of freedom is
redundant -- the same conclusion `ideas/G2 .../calibration_example.txt` reaches ("keep
delta fixed at 1.0"). Pinning delta at its spec default and searching the other four
(epsilon, gamma, the taper exponent, the opponent-hand-size discount) makes epsilon and
gamma well-defined ratios against a fixed reference, rather than leaving the search free
to wander along the whole scale-invariant ridge. `--params` can still name
`weight_cash_advantage` explicitly to override this in plain (non-`--identity-safe`)
mode; `--identity-safe` always excludes it regardless, the same way `aicalibsrc/balanced/`
always excludes `combo_weight`.

## Why `optimize` targets `borealis` by default

`doc/oracle_todo.md`'s original "calibrate against `A4` Balanced Rules" note is
superseded: `A4` itself measured rating 36, below the anchor, so tuning against it risks
a `balanced`-specific counter-strategy that doesn't generalise. `borealis` is the
rating-50 anchor and the natural, most informative target, unlike `rand`
(ceiling-effected at ~90-99%+ for every implemented agent).

## Why the ceiling on `weight_cards_advantage` was widened mid-calibration

A manual univariate sweep (gamma at the spec default of 0.15, then 1, 2, 3, 4, 6, 8, 12,
everything else at spec defaults, vs `borealis`) found the useful range extends far past
the initial `BOUNDS` ceiling of 2.0: win rate climbed from 26.8% at gamma=1 to a peak of
~48.7% around gamma=6-8, then collapsed to 19.1% at gamma=12 -- a clear unimodal shape,
not runaway growth. `BOUNDS["weight_cards_advantage"]` was widened to `(0.0, 15.0)`
before running `optimize` so the search could actually reach that peak instead of being
silently capped short of it (the same lesson `aicalibsrc/borealis/`'s `luna_value` bound
and `aicalibsrc/balanced/`'s `target_cash_slope` bound both record for their own agents).

## Why `--identity-safe` exists, and why the found gamma still shipped

An unconstrained `optimize` run (all four free params, `--opponent borealis`) found
`weight_cards_advantage=9.815` -- more than 60x the spec's illustrative 0.15 -- at
59.67% [59.18%, 60.14%] validated vs `borealis` (40,000 games). `check_personality_flags()`
flags any gamma above 1.0 as worth reviewing, the same "measured stronger by eroding
character is not automatically a win" policy A1-A4 all apply.

Unlike those precedents, though, playtracing this candidate (turn-count histograms via
`bin/oracle -sa -p`, not just the aggregate win rate) ruled out the failure mode the flag
exists to catch: it still finishes nearly every game in under 20 turns and wins 99.8% vs
`rand` -- a fast, decisive strategy, not a "hoard forever, never attack" stall. A high
gamma changes *which* moves this agent's one weighted-sum mechanism prefers; it does not
disable a rule or add a new mechanism the way A2's/A4's rejected extremes did (those
broke an explicit, named rule -- "wait for a combo above the threshold," "never spend
below the resource target" -- that the agent's identity depended on). `about.md`'s own
statement of this agent's identity is exactly "its entire identity is its three
weights," so a large but still-single-mechanism weight is a legitimate calibration
finding, not erosion.

`optimize --identity-safe` (gamma capped at 2.0, epsilon kept away from 0, the taper
exponent capped at 2.0) was still run as the character-preserving comparison, per the
same protocol A4 established. It converged to `weight_cards_advantage=1.962` --
pinned against its own ceiling -- at a statistically indistinguishable 58.99%
[58.51%, 59.47%] vs `borealis`. A 3-way self-play round-robin (defaults vs both
candidates, 48,000 games/pairing) confirmed the two are inseparable (Bradley-Terry
strength 1.1294 vs 1.1098) and both far ahead of the spec defaults (0.0) -- so the
identity-safe candidate shipped: same measured strength, every weight closer to the
stub's own illustrative numbers.

## selfplay vs a full grid

Five parameters make a full Cartesian grid feasible in principle but still noisier than
a short, deliberately-chosen list -- `selfplay` round-robins a small set of explicitly
*named* candidates (JSON files, typically `optimize` outputs) rather than every
combination, matching `aicalibsrc/balanced/`'s pattern. Use `sweep` to check one axis at
a time, `optimize`/`optimize --identity-safe` to search, and `selfplay` to compare a
short list of finalists head-to-head -- this is exactly how the shipped defaults were
chosen: the unconstrained candidate, the identity-safe candidate, and the spec-derived
defaults, round-robinned at 48,000 games per pairing.
