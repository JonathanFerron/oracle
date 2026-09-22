# A14 AlphaOracle Prime Plus I ("PUCT + Neural Network") calibration tooling

Corpus generation, training, and calibration for `src/ai_strat/ai_strat_puct.c`
(PUCT selection over a learned policy prior, plus a two-head value/policy net
replacing A11's own single-head value net at the leaves). See
`doc/ai_agents.md`'s A14 section for the full design record and
`doc/changelog.md` for the dated write-up of whatever Stage 5 measured.

**As of 2026-09-22, `PUCT_DEFAULTS.use_puct=false` is the shipped
default** -- PUCT selection measured worse than plain UCT selection over
this agent's own net (see "The `use_puct=false` ablation" below). PUCT
selection remains fully implemented/tested; `use_puct=true` restores it as
an explicit override, just not the default a player gets.

**No ship gate here, historically.** Unlike every prior agent in this
family, *registration* never waited on what this tooling measures
(Jonathan's call, 2026-09-08) -- the search mechanism itself (a learned
prior directing PUCT selection, the first agent in this project whose tree
structure differs from plain UCT, not just its leaf evaluator) was the
milestone. That principle is unchanged; what changed 2026-09-22 is the
*default config* for an already-registered agent, based on a later
measurement this tooling made possible. The numbers are still measured and
reported honestly in `doc/ai_agents.md`, exactly like every other agent's
real result.

One subfolder per agent under `aicalibsrc/`, mirroring `aicalibsrc/hbt/` etc.
-- keep each agent's harness and driver self-contained rather than
accumulating loose files at the top level. This folder's shape is closest to
`aicalibsrc/ismctsnn/`'s (an offline training step between C harness and
Python driver, not a single C-binary + `optimize()`-search pipeline) but
adds a Stage 5 driver shaped like `aicalibsrc/carto/`'s (four free
continuous dials, needs `optimize`, not just `sweep`).

## The shared-struct gotcha -- now with THREE agents, read this first

**`A10`, `A11`, and `A14` all share one `ISMCTSParams` struct**
(`ai_strat_ismcts1.h`) and `A10`/`A11` share one search function
(`ismcts_search_best_move()`) -- `A14` uses the same shared struct for its
compute-budget/rollout fields but its OWN disjoint `PUCTParams`
(`ai_strat_puct.h`) for everything selection/prior-related, precisely to
avoid deepening this gotcha (see `ai_strat_puct.h`'s own header comment).

The first `calib_ismctsnn.c` (A11's own harness) copied A13's "set params on
both registries, harmless if the active agent doesn't read them" pattern --
unsafe specifically because `nn_value_trust` lives inside a struct A10 and
A11 both read, so a seat nominally playing plain `ismcts` silently inherited
whatever trust value sat in the block parsed for it. `calib_puct.c` does NOT
repeat this pattern: `apply_seat_params()` applies each seat's parsed
`ISMCTSParams`/`PUCTParams` ONLY to the registry matching that seat's REAL
agent type (`ismcts` gets `ISMCTSParams` with `nn_value_trust` forced to
`0.0f`; `ismctsnn` gets it via `ismctsnn_set_params()`; `puct` gets both its
own `ISMCTSParams` via `puct_set_ismcts_params()` and its `PUCTParams` via
`puct_set_params()`; anything else reads neither). This sidesteps the gotcha
structurally rather than needing a one-off patch per registry -- worth
copying forward if a fourth agent ever joins this struct.

**Lesson for any future harness in this shape**: prefer per-identity
application over blanket-set-every-registry the moment more than one agent
shares a struct -- verify disjointness, don't assume it.

## Files

- `gen_policy_corpus.c` -- Stage 2 self-play corpus generator. Plays real
  games under a chosen teacher (`ismctsnn`=`A11` or `puct`=`A14`, see below
  -- generalized 2026-09-22, A16 Session 2 item 2; previously hardcoded to
  `A11`) across the curated opponent pool
  (`mirror`/`vs_a7`/`vs_a3`/`vs_a4`/`vs_a6`, same as `aicalibsrc/ismctsnn/gen_corpus.c`'s
  own pool) and logs, from the teacher's own decision points only, the full
  legal-move-list + visit-fraction record this agent's policy head trains
  on -- NOT A11's own state+outcome-only format; the two corpora are not
  interchangeable even when the teacher is the same agent. A `puct` teacher
  runs whatever `PUCTParams` its module defaults currently are
  (`PUCT_DEFAULTS`) -- as of 2026-09-22 that's `use_puct=false`, i.e.
  today's real shipped `A14` config, not a forced PUCT-selection variant.
  Requires the chosen teacher's weights to load successfully (refuses to
  run otherwise -- an unloaded `ismctsnn`/`puct` net silently degrades to
  plain A10, which would corrupt the whole corpus). Build with
  `make gen_policy_corpus` -> `bin/gen_policy_corpus`.
  ```
  gen_policy_corpus <ismctsnn|puct> <weights_path> <mirror|vs_a7|vs_a3|vs_a4|vs_a6> <numgames> <seed> <output_path> [limit_iterations]
  ```
  Prints its full resolved config (teacher, weights, matchup, seed,
  `limit_iterations`, search settings, `use_puct` when the teacher is
  `puct`) to stderr once at startup (added 2026-09-22, A16 Session 2 item
  2.1) -- every per-shard worker log is self-documenting provenance,
  the source to hand-author a sidecar from once a real round ships.
  Output: a headerless flat float32 shard, 1692 floats/record as of
  2026-09-22 -- 537 (state) + 1 (outcome) + 1 (num_moves) + 1
  (total_visits, added 2026-09-22 for `--policy-target-temperature`, see
  below) + 128*9 (per-move type/count/play[3]/target[3]/visit_fraction).
  Shards from before that date (1691 floats/record, no total_visits) still
  load fine -- `train_puct_net.py`'s `load_records()` detects each file's
  own width. See the file's own header comment for the full layout and
  `ai_strat_puct_policy.h` for the catalog-index convention play[]/target[]
  use.
- `run_selfplay.sh` -- fans `gen_policy_corpus` out across several
  background workers (process-level parallelism), bounded by wall-clock
  rather than a fixed game count, ported from
  `aicalibsrc/ismctsnn/run_selfplay.sh` (same `corpus/seed_ledger.tsv`
  no-seed-reuse guarantee, same CPU/corpus-size monitor -- `seed_ledger.tsv`
  gained a trailing `teacher` column 2026-09-22; pre-existing rows without
  it are implicitly `ismctsnn`, A11 was the only teacher before that date).
  No longer needs `cd` to the repo root (fixed 2026-09-22 alongside the
  teacher generalization below) -- every path it builds is already
  absolute, and `gen_policy_corpus.c` now takes an explicit weights path
  instead of a fixed repo-root-relative one.
  ```
  ./run_selfplay.sh <label> <duration_seconds> <teacher: ismctsnn|puct> [weights_path] [workers] [limit_iterations] [matchups_csv]
  ```
  `teacher` is required (no default -- silently generating against the
  wrong teacher would corrupt the corpus); `weights_path` defaults
  per-teacher if omitted or passed as `''`.
- `train_puct_net.py` -- PyTorch (CPU) training script. Two-head net
  (537->256->128->64 shared trunk, BatchNorm+dropout, then a value head and
  a policy head as separate `nn.Linear` attributes -- not buried in one
  `nn.Sequential` the way A11's single head is, so this exporter doesn't
  need A11's own dropout-shifts-Sequential-indices care for the heads).
  Policy loss is soft-target cross-entropy against `visit_fraction`
  (AlphaZero's own `-pi^T log(p)`), computed via `compose_policy_scores()`
  -- a torch-vectorized mirror of `puct_move_score()`
  (`ai_strat_puct_policy.c`) that must stay in sync with it and with
  `ai_strat_puct_net.c`'s C forward pass; three independent implementations
  of the same formula. Carries A11's hard-won regularization defaults
  (`dropout=0.4`, `weight_decay=1e-3`, `lr=3e-4`), the same shard-level
  train/val split, `--val-seeds`, `--max-train-records`. **Watch both loss
  components separately, not just the sum** -- they converge at very
  different scales and rates (this agent's own first real run: value MSE
  plateaued by epoch ~5-6, policy loss similarly, while train MSE kept
  falling for 70+ more epochs -- protected by `best_state` tracking on the
  combined validation loss, same overfit-protection A11's own trainer uses).
  As of 2026-09-22 (A16 Session 1, items 1.2/1.3) it also prints a uniform-
  prior CE baseline and the target distribution's own entropy alongside
  `val_p_loss`, so the loss reads as "fraction of headroom captured"
  (`headroom_captured` in the per-epoch and per-matchup lines) instead of a
  bare number, and takes `--policy-target-temperature` (`pi_i ~
  fraction_i^(1/tau)`, reshaping the visit_fraction TARGET at training
  time, default 1.0 = unchanged) -- not the same knob as
  `PUCTParams.policy_temperature`, which reshapes the NET's own predictions
  at inference time and this script never touches.
- `export_puct_weights.py` -- exports a trained `.pt` checkpoint to the flat
  headerless float32 format `ai_strat_puct_net.h` expects (`W1,b1,W2,b2,W3,b3`
  fused trunk, then `Wv,bv` value head, then `Wp,bp` policy head), fusing the
  trained `BatchNorm1d` into the trunk's first `Linear` layer (same algebra
  as A11's own `export_weights.py`) and verifying the fused forward pass
  against the live PyTorch model on both heads' raw outputs before writing.
  ```
  ./export_puct_weights.py <checkpoint.pt> <sample_corpus_shard.bin> -o <out.bin> [--tol TOL]
  ```
- `calib_puct.c` -- Stage 5 calibration harness. Same in-process
  `run_simulation()` pattern as every other `CALIB_*` target. Build with
  `make calib_puct` -> `bin/calib_puct`. Takes the weights path, then 21
  `ISMCTSParams` + 8 `PUCTParams` fields per seat (`ai_strat_ismcts1.h`/
  `ai_strat_puct.h`'s own declared order) -- see the file's header for the
  full CLI, or run `bin/calib_puct --print-defaults` to dump the shipped
  defaults as JSON. **Read the shared-struct section above before touching
  this file.**
- `calib_puct_timing.c` -- per-decision timing harness, mirroring
  `aicalibsrc/ismctsnn/calib_ismctsnn_timing.c`'s structure but with no
  trust dial to sweep (this agent's leaf evaluation always pays the same
  one-shared-forward-pass cost whenever it's active). Answers "does this
  agent's cost stay near A11's own measured 439ms" *before* committing to a
  large-n Stage 5 run. Build with `make calib_puct_timing` -> `bin/calib_puct_timing`.
  ```
  calib_puct_timing <weights_path> <limit_iterations> <numgames> <seed>
  ```
- `calibrate_puct.py` -- Python driver. `DEFAULTS` read once, at import
  time, from `bin/calib_puct --print-defaults`, so it cannot drift from the
  shipped C constants. Four subcommands (see the module docstring for full
  detail):
  - `sweep` -- univariate diagnostic: one of `c_puct`/`fpu_reduction`/
    `prior_trust`/`policy_temperature` varied vs a fixed `--opponent`
    (default `ismctsnn`), both seats, Wilson CIs. Watch for the `A9`/`A13`
    monotonic-decline signature on `prior_trust` specifically -- `A11`'s own
    `nn_value_trust` sweep instead rose monotonically, the mirror image.
  - `optimize` -- differential-evolution search over a chosen subset of the
    four free dials vs a fixed opponent, with a personality-flag check
    (`prior_trust` collapsing near 0, `c_puct`/`policy_temperature` pinned
    at search bounds).
  - `selfplay` -- round-robin among named dial-configuration candidates,
    `puct` vs `puct`, Bradley-Terry fit.
  - `validate` -- candidate vs the shipped defaults, vs `--opponent` --
    `--opponent ismctsnn` is Gate 2 (the real head-to-head bar against this
    agent's direct predecessor), `--opponent borealis` is Gate 1 (context,
    an estimated Borealis rating comparable to A11's own 74). Both are
    measured and reported, neither gates registration (see above).

  A weights file (`export_puct_weights.py`'s output) is REQUIRED for every
  subcommand via `--weights`, even for a puct-vs-ismcts sanity check -- the
  C harness always loads it.

## Setup

```bash
make gen_policy_corpus calib_puct calib_puct_timing   # from the repo root
```

The Python pipeline (`train_puct_net.py`, `export_puct_weights.py`,
`calibrate_puct.py`) needs `torch`, `numpy`, `pandas`, `scipy`, `matplotlib`.
This agent's own tooling deliberately does NOT set up its own `.venv/` --
`aicalibsrc/ismctsnn/.venv/` already has the identical dependency set (torch
2.14.0+cpu, numpy, pandas, scipy, matplotlib) and duplicating a ~1.3GB
install for an identical requirements list bought nothing. Use it directly:

```bash
cd aicalibsrc/puct
../ismctsnn/.venv/bin/python3 train_puct_net.py corpus --label full
```

If that venv is ever removed, set up a fresh one the same way
`aicalibsrc/ismctsnn/README.md` describes.

## Usage

```bash
cd aicalibsrc/puct

# Stage 2 -- generate a corpus (see run_selfplay.sh's header for sizing)
./run_selfplay.sh full 43200 ismctsnn '' 15   # A11-taught (the original round)
./run_selfplay.sh round2 43200 puct '' 15     # A14-taught (added 2026-09-22, A16 Session 2)

# Stage 3 -- train (corpus_dir is a directory, not a glob -- --label selects shards)
../ismctsnn/.venv/bin/python3 train_puct_net.py corpus --label full

# Stage 3 -- export to the C inference format
../ismctsnn/.venv/bin/python3 export_puct_weights.py checkpoints/full_puct_net.pt \
    corpus/full_vs_a3_seed3.bin -o checkpoints/full_c_weights.bin

# Stage 5 -- measure (from the repo root)
./calibrate_puct.py sweep --weights checkpoints/full_c_weights.bin \
    --param prior_trust --numsim 500 --replicates 4 --plot
./calibrate_puct.py validate --weights checkpoints/full_c_weights.bin \
    --candidate defaults --opponent ismctsnn --numsim 2000 --replicates 4
./calibrate_puct.py validate --weights checkpoints/full_c_weights.bin \
    --candidate defaults --opponent borealis --numsim 2000 --replicates 4
```

## Shipped weights

The packaged weights the binary actually loads
(`assets/puct/plus1_weights.bin` + its `.json` provenance sidecar, once
Stage 5 is done) will follow `assets/ismctsnn/prime_657k_weights.json`'s
exact shape (architecture, training/corpus provenance, measured results).
See `doc/ai_agents.md`'s A14 section for the narrative once it exists;
`doc/changelog.md` for the dated record.

## The `use_puct=false` ablation (2026-09-22) — now the shipped default

`PUCTParams.use_puct=false` isolates the retrained two-head net from PUCT's
selection rule (plain UCT selection, same net at the leaves) — implemented
since `A14`'s own registration but not measured until `A16` Session 1.
`use_puct_false.json` is the one-key candidate file, still useful for
re-running or comparing against `use_puct=true` (now the non-default
config):

```bash
./calibrate_puct.py validate --weights checkpoints/full_c_weights.bin \
    --candidate use_puct_false.json --opponent ismctsnn \
    --numsim 137 --replicates 15   # n=4110, matches the gate logs above
./calibrate_puct.py validate --weights checkpoints/full_c_weights.bin \
    --candidate use_puct_false.json --opponent borealis \
    --numsim 137 --replicates 15
```

Result: a decisive 57.15% [55.63%, 58.66%] head-to-head win over `A11`
(`A14`'s own null result decomposes into "good net, bad selection rule"),
softer on the Borealis-anchored rating (~75, tied with `A11`'s own 74 within
CI). Shipped as `A14`'s new default the same day (Jonathan's call) — see
`src/ai_strat/ai_strat_puct.h`'s `PUCT_DEFAULTS`. Full writeup:
`doc/ai_agents.md`'s A14 section, 2026-09-22 addendum; raw output in
`ablation_use_puct_run.log` / `ablation_use_puct_borealis_run.log`.
