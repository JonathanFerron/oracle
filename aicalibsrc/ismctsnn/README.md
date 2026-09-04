# A11 IS-MCTS + NN ("AlphaOracle Prime") calibration tooling

Corpus generation, training, and calibration for `src/ai_strat/ai_strat_ismctsnn.c`
(value-net leaf evaluation blended into `A10`'s IS-MCTS tree). See
`ideas/A11 ai agent is-mcts + nn (alphaoracle prime)/about.md` for the full design
record and `doc/changelog.md` for the dated write-up of the measurement that shipped
this agent: **both ship gates PASS** at `nn_value_trust=1.0` -- 58.44% head-to-head vs
`A10` (Gate 2, the real bar), ~74 estimated Borealis rating (Gate 1, context).

**2026-09-04 -- "bigger training corpus" follow-up: tried, falsified.** Two independent
widening axes (raw record volume via `--max-train-records`; opponent diversity via two
new curated opponents, `vs_a4`/`vs_a6`) both landed on the same ~0.1705-0.1706 val MSE
floor as the shipped 657K-record net -- see `doc/changelog.md`'s 2026-09-04 entry and
`ideas/A11 .../about.md` for the full record. No retrain shipped. The tooling added for
this check (`train_value_net.py`'s comma-separated `--label`, `--val-seeds`,
`--max-train-records`; `gen_corpus.c`'s `vs_a4`/`vs_a6` matchups; `run_selfplay.sh`'s
matchup-list parameter) stays below as reusable infrastructure for any future revisit --
this data-size-curve methodology is worth re-running before committing to a large
generation run, not just for this agent.

One subfolder per agent under `aicalibsrc/`, mirroring `aicalibsrc/hbt/` etc. -- keep
each agent's harness and driver self-contained rather than accumulating loose files
at the top level. This folder's shape is different from every prior agent's, though:
it's the first with an offline training step between C harness and Python driver
(corpus generation -> PyTorch training -> weight export -> calibration), not a single
C-binary + `optimize()`-search pipeline.

## The shared-struct gotcha -- read this before writing any new harness here

**`A10` and `A11` share one `ISMCTSParams` struct (`ai_strat_ismcts1.h`) and one
`ismcts_search_best_move()` function** (`ai_strat_ismcts_search.c`) -- unlike, say,
`A13Params`/`HBTParams`, which are genuinely disjoint structs read by disjoint code.
The first `calib_ismctsnn.c` copied `A13`'s `calib_a13.c` pattern of "set params on
both registries, harmless if the active agent doesn't read them." That's **unsafe
here specifically**: a seat nominally playing plain `ismcts` was also silently
inheriting whatever `nn_value_trust` sat in its own parsed params block (e.g. the
sweep/validate baseline's `dict(DEFAULTS)` carries `nn_value_trust=1.0`, since that's
`ismctsnn_get_default_params()`'s own starting point). Every measurement before the
fix was invalid -- it was comparing candidate-trust vs a mislabeled second NN variant,
not vs real `A10` -- and ran ~2x slower besides (both seats paying the expensive
NN-forward-pass cost instead of one).

Fixed by forcing `nn_value_trust=0.0f` specifically on the copy passed to
`ismcts_set_params()`, regardless of what the parsed block otherwise contains (see
`calib_ismctsnn.c`'s header comment and its call site).

**Lesson for any future harness in this shape**: "harmless to set params another
agent won't read" only holds when the structs are actually disjoint -- verify, don't
assume, whenever two agents share one struct/function.

## Files

- `gen_corpus.c` -- Stage 1 self-play corpus generator. Plays real `A10` games across a
  curated opponent pool -- mirror self-play, vs `A7`, vs `A3` (the original three, see
  about.md for why this pool, not pure mirror self-play), plus `vs_a4`/`vs_a6` (added
  2026-09-04 for a recipe-diversity check, both meeting a Borealis-rating>=35 floor for
  "a useful proxy for real play") -- and logs `(info-set state, outcome)` pairs from
  `A10`'s own decision points only. Not a params sweep/optimize harness, so it doesn't
  follow the `calibrate_*.py` driver pattern the other `CALIB_*` targets do -- run it
  directly. Build with `make gen_corpus` -> `bin/gen_corpus`.
  ```
  gen_corpus <mirror|vs_a7|vs_a3|vs_a4|vs_a6> <numgames> <seed> <output_path> [limit_iterations]
  ```
  Output: a headerless flat-float32 shard, 538 floats/record (537-float state +
  1-float outcome, see `ai_strat_ismctsnn_state.h`).
- `run_selfplay.sh` -- fans `gen_corpus` out across several background workers
  (process-level parallelism), bounded by wall-clock rather than a fixed game count,
  so the same script serves both a quick pilot and a much longer full run. Maintains
  `corpus/seed_ledger.tsv` so concurrent/future runs never reuse an RNG seed. The
  optional 5th argument (added 2026-09-04) selects the opponent pool as a
  comma-separated list, defaulting to the original `mirror,vs_a7,vs_a3`.
  ```
  ./run_selfplay.sh <label> <duration_seconds> [workers] [limit_iterations] [matchups_csv]
  ```
- `train_value_net.py` -- PyTorch (CPU) training script. Small MLP
  (537->256->128->64->1, BatchNorm+dropout) on the corpus shards. **Use
  dropout/weight_decay/a lower lr** -- an early unregularized run overfit
  catastrophically past epoch 1 (best val MSE at epoch 1, then monotonically worse
  for 1000+ epochs after); dropout=0.4 + weight_decay=1e-3 + lr=3e-4 fixed it (best
  epoch moved to 4, ~30% MSE reduction, graceful degradation after). **Always check
  the epoch of the best checkpoint, not just its value** -- "best at epoch 1, all
  downhill after" is a red flag even when that epoch's number looks fine. Writes a
  `.pt` checkpoint + a `.json` sidecar (architecture, hyperparameters, val MSE,
  corpus label) to `checkpoints/` (gitignored). `--label` accepts a comma-separated
  list (pool multiple corpus generations together); `--val-seeds` pins validation to
  specific per-matchup seeds regardless of what else is in the corpus dir (needed to
  keep a val MSE comparable across corpus sizes -- the default "highest seed per
  matchup" silently drifts as new, higher-seeded shards are added); `--max-train-records`
  randomly subsamples the training set only, for building a data-size learning curve on
  a single corpus (all three added 2026-09-04, see the note above).
- `export_weights.py` -- exports a trained `.pt` checkpoint to the flat headerless
  float32 format `ai_strat_ismctsnn_net.h` expects, fusing the trained BatchNorm1d
  into `Linear1`'s weights (an exact transform in eval mode) and verifying the fused
  forward pass against the live PyTorch model on a sample corpus shard before
  writing.
  ```
  ./export_weights.py <checkpoint.pt> <sample_corpus_shard.bin> -o <out.bin> [--tol TOL]
  ```
- `calib_ismctsnn.c` -- Stage 3 calibration harness. Same in-process `run_simulation()`
  pattern as every other `CALIB_*` target (links the game engine directly, no
  subprocess/text-parsing overhead). Build with `make calib_ismctsnn` ->
  `bin/calib_ismctsnn`. See the file's header for its CLI (weights path, then the 20
  `ISMCTSParams` fields per seat), or run `bin/calib_ismctsnn --print-defaults` to
  dump `ismctsnn_get_default_params()` as JSON. **Reads the shared-struct gotcha
  above before touching this file.**
- `calibrate_ismctsnn.py` -- Python driver on top of `calib_ismctsnn`. Its `DEFAULTS`
  dict is read once, at import time, from `bin/calib_ismctsnn --print-defaults`, so
  it cannot drift from the shipped C constants. Three subcommands (see the module
  docstring for full detail):
  - `sweep` -- univariate diagnostic: `nn_value_trust` varied, `ismctsnn` vs a fixed
    `--opponent` (default `ismcts`), both seats, Wilson CIs. Watch for `A9`/`A13`'s
    monotonic-*decline* signature -- `A11`'s own sweep instead rose monotonically
    with trust, the mirror image.
  - `validate` -- candidate trust vs the shipped baseline (`nn_value_trust=0`, i.e.
    pure `A10`) and vs `--opponent`, both seats -- **this is Gate 2**, the real ship
    bar.
  - `selfplay` -- round-robin among a small set of named trust candidates,
    `ismctsnn` vs `ismctsnn`, Bradley-Terry fit among them. Narrower than `A13`'s own
    `selfplay`.

  There is deliberately no `optimize` (differential-evolution) subcommand, unlike
  most other agents' drivers: `nn_value_trust` is a single continuous dial, and
  `sweep` already finds the best point on the response curve directly.
- `calib_ismctsnn_timing.c` -- per-decision timing harness, mirroring
  `aicalibsrc/ismcts/calib_ismcts_timing.c`'s structure. Answers "does NN-eval-per-leaf
  cost more than heuristic-rollout-to-terminal" *before* committing to a large-n
  Stage 3 run, not after. Build with `make calib_ismctsnn_timing` ->
  `bin/calib_ismctsnn_timing`.
  ```
  calib_ismctsnn_timing <weights_path> <trust> <limit_iterations> <numgames> <seed>
  ```
  Reports mean/median/p95/min/max, split ALL / EARLY (turn<=2) / LATE (turn>=15).
  Measured (unoptimized `-Og`, project default): **0.496s mean/decision at
  trust=1.0**, about **16x** `A10`'s own plain-rollout cost (~0.031s/decision) --
  the NN forward pass dominates (trust=1.0, rollout skipped, costs about the same as
  trust=0.5, both paid). At `-O2` (`make release`): **0.439s mean/decision**, only
  ~11.5% faster -- `linear_relu()` (`ai_strat_ismctsnn_net.c`) is a naive
  unvectorized triple loop under strict FP semantics, and plain `-O2` (no
  `-ffast-math`/`-march=native`, kept portable) doesn't autovectorize it much. See
  `doc/changelog.md` and `assets/ismctsnn/prime_657k_weights.json`'s `timing` block
  for both figures.

## Setup

```bash
make gen_corpus calib_ismctsnn calib_ismctsnn_timing   # from the repo root
```

The Python pipeline (`train_value_net.py`, `export_weights.py`,
`calibrate_ismctsnn.py`) needs `torch`, `numpy`, `pandas`, `scipy`, `matplotlib`. A
project-local venv at `.venv/` (gitignored) was used for the shipped weights:

```bash
cd aicalibsrc/ismctsnn
python3 -m venv .venv
.venv/bin/pip install torch numpy pandas scipy matplotlib
```

## Usage

```bash
cd aicalibsrc/ismctsnn

# Stage 1 -- generate a corpus (see run_selfplay.sh's header for the full-run sizing)
./run_selfplay.sh pilot 3600 12

# Stage 1 -- train (corpus_dir is a directory, not a glob -- --label selects shards)
.venv/bin/python train_value_net.py corpus --label pilot --run-name my_run

# Stage 2 -- export to the C inference format
.venv/bin/python export_weights.py checkpoints/my_run_value_net.pt \
    corpus/pilot_mirror_seed1.bin -o checkpoints/my_run_c_weights.bin

# Stage 3 -- measure (from the repo root)
./calibrate_ismctsnn.py sweep --weights checkpoints/my_run_c_weights.bin \
    --numsim 500 --replicates 4 --plot
./calibrate_ismctsnn.py validate --weights checkpoints/my_run_c_weights.bin \
    --trust 1.0 --numsim 2000 --replicates 4
```

## Shipped weights

The packaged weights the binary actually loads (`assets/ismctsnn/prime_657k_weights.bin`
+ its `.json` provenance sidecar) come from the `reg_strong_value_net.pt` checkpoint
in this folder's (gitignored) `checkpoints/`, trained on the 1-hour/657K-record pilot
corpus -- verified byte-for-byte against `export_weights.py`'s own output on that
checkpoint. See the sidecar `.json` for the full training/corpus/measurement record;
`ideas/A11 .../about.md` for the narrative.
