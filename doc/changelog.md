# Oracle Changelog

Completed work, most recent first. `doc/oracle_todo.md` tracks what's still open;
this file is where finished items go so the todo list doesn't keep growing.

---

## 2026-09-04 — `A13` Cartographer registered for real play -- shipped for its character, not its strength

Shelved 2026-08-31 (see that date's entry below) after four independent calibration
searches all measured at parity with `A7` or worse, never a strength improvement.
Jonathan's call today: register it anyway, since the interesting part was never
its strength but its distinct playing character -- race-aware risk appetite
(Layer R turns `A7`'s fixed `defense_stdev_mult` into a state-dependent one,
deflating variance when ahead in the turns-to-kill race, inflating it when
behind) and deck-aware draw timing (Layer K-draw/D value a DRAW candidate
against the live unseen-pool mean and the reshuffle boundary, not a flat
average).

Reversed the 2026-08-31 shelving precisely: `AI_STRATEGY_CARTOGRAPHER`
re-added to `AIStrategyType` (`game_types.h`) -- appended after
`AI_STRATEGY_CLAIRVOYANT`, not restored to a "13th slot" mid-list, since enum
ordinal is a menu/table index everywhere; `STRATEGY_REGISTRY` entry restored
in `ai_strategy.c` (`a13_attack_strategy`/`a13_defense_strategy`/
`a13_mulligan`/`a13_discard_to_7`); `player_config.c` regained its shorthand
(`carto`), menu label, display name (all three languages), and a
`AI_STRATEGY_RATINGS[]` entry.

**Ships with the Stage 4 joint search's calibrated config, not the neutral/
`A7`-recovering one** (`aicalibsrc/carto/results/optimize_borealis.json`):
`race_scale=8.147`, `race_stdev_ahead=0.868`, `race_stdev_behind=-0.759`,
`race_eps_gain=-0.0843`, `belief_draw_weight=-1.102`,
`belief_reshuffle_trust=0.431`, `belief_opp_block_trust=0.225`,
`defense_stdev_mult=1.290` (overriding `A7`'s own 0.2156, the one inherited
field this agent ever re-fits) -- `hplus_trust`/`hplus_block_combo` stay
pinned at neutral (conclusively harmful, per the 2026-08-31 entry).
Deliberately not the bit-for-bit-`A7` configuration the superset guarantee
still allows: shipping neutral would make this a literal `A7` clone under a
new name, exactly what the original shelving decision reasoned against.
Validated 64.88% [64.36%, 65.41%] vs `borealis` (32,000 games) -- **rating
65, statistically tied with `A7`'s own 65**, confirmed by a fresh spot-check
this session (2,000 games: 66.15% vs `borealis`, ≈45.65% vs `hbt`, both
consistent with the 32k-game measurement within sampling noise).

**Verification**: clean `make` build, no new warnings; `./bin/oracle -a -p`
byte-identical to `bin/expectedresults.txt` (the enum append is additive,
doesn't renumber any existing agent); `valgrind --leak-check=full` clean on
a 20-game `carto` vs `hbt` run.

Not a reversal of the calibration verdict -- see the entry below for that
record unchanged. This is a distinct decision: strength parity was already
established; today's call was that a statistically-tied-but-mechanically-
different agent still adds roster value as a second, differently-flavoured
deterministic opponent.

---

## 2026-09-04 — AlphaOracle Prime "bigger training corpus" (Next Up item 2): falsified on two independent axes -- no retrain, tooling kept

Full design record: `doc/ai_agents.md`'s A11 section.
The roadmap's premise for this item was that the shipped value net (`reg_strong_value_net.pt`,
1-hour/657K-record pilot corpus) was data-starved: it needed aggressive regularization
(`dropout=0.4`, `weight_decay=1e-3`) just to survive past epoch 1, and its best val MSE
(0.17051) had not visibly plateaued when training stopped. Before committing to the
originally-planned 12-hour generation run, measured a data-size learning curve directly
on the existing pilot corpus instead of assuming it.

**Axis 1 -- raw volume.** Trained at 60K/120K/240K/360K/488K (full pilot) training
records, fixed val set (see below), fixed regularization matching the shipped config:
val MSE 0.175554 -> 0.173111 -> 0.171251 -> 0.170419 -> 0.170512. The curve flattens by
~240-360K and the full 488K pilot training set is statistically indistinguishable from
360K (even fractionally worse) -- **not** "still descending." Ruled out one confound
before trusting this: relaxing regularization at the same full 488K record count made
val MSE *worse*, not better (0.4/1e-3 -> 0.170512; 0.1/1e-4 -> 0.175361; 0/0 -> 0.177736),
so the plateau is not an over-regularization artifact -- the shipped hyperparameters are
already close to the right point for this data volume.

**Axis 2 -- opponent diversity.** Extended `gen_corpus.c`'s curated pool with two new
opponents, both meeting Jonathan's Borealis-rating>=35 floor for "a useful proxy for real
play": `vs_a4` (`AI_STRATEGY_BALANCED`, rating 36) and `vs_a6` (`AI_STRATEGY_TACTICAL`,
rating 52) -- still deliberately excluding `A5` (already `A10`'s own rollout policy).
Generated 214,660 records in a 20-minute/12-worker run at the real `limit_iterations=4000`
budget (`run_selfplay.sh widen 1200 12 0 vs_a4,vs_a6`, ~644K records/hour, matching the
pilot's own throughput) and trained on pilot+widen combined (702,976 training records, a
44% increase) against the *same* pinned validation set: val MSE 0.170579 -- statistically
indistinguishable from the 488K-only result, not an improvement. Per-matchup breakdown is
informative about *why*: `vs_a3` (weaker opponent) is far more predictable (val MSE
0.150, 78.1% directional accuracy) than `mirror` (`A10` self-play, val MSE 0.186, 71.7%)
-- the aggregate ceiling looks driven by genuine outcome variance in close self-play
games, not a data gap.

**Validation methodology, load-bearing for both measurements**: pinned validation to the
pilot's original three highest-seeded holdout shards (`pilot_mirror_seed10`,
`pilot_vs_a7_seed11`, `pilot_vs_a3_seed12`, 168,904 records) via a new `--val-seeds`
flag on `train_value_net.py`, rather than letting `split_train_val()`'s default
"highest seed per matchup" silently drift onto new, higher-seeded shards as the corpus
grew -- without this, the two runs' val MSE would not have been comparable to each
other or to the shipped net's own reported 0.17051.

**Verdict (Jonathan's call, 2026-09-04): stop, do not retrain, no new agent.** Two
independent widening axes -- volume and opponent diversity -- both landed on the same
~0.1705-0.1706 val MSE floor with the same architecture/features/regularization. This is
a genuine negative result on the "bigger corpus" hypothesis specifically (matching `A9`'s
`reply_trust` and `A13`'s `hplus_trust` pattern: a mechanism that measured cleanly null
rather than positive or ambiguous), not a case for spending the originally-planned
3.5-12 hours generating more of the same distribution. The shipped `A11` weights
(`assets/ismctsnn/prime_657k_weights.bin`, rating 74) are unchanged; no second NN agent
was registered, so the "AlphaOracle Prime Plus I" name stays reserved for a future
Stage 4 (PUCT + policy head) agent exactly as `about.md`'s naming decision already
specified --
this experiment changed the mechanism's *inputs*, never its outputs, so no naming
conflict arose. **Tooling kept as reusable infrastructure**: `gen_corpus.c`'s `vs_a4`/
`vs_a6` matchups, `run_selfplay.sh`'s `matchups_csv` parameter, and
`train_value_net.py`'s `--label` (now comma-separated), `--val-seeds`, and
`--max-train-records` flags all remain available for a future revisit -- e.g. if Stage 4
(PUCT + policy head) changes what's worth training on, this data-size-curve methodology
is the right first check to run again before any large generation commitment, not just
for this agent. The 214,660-record `widen` corpus and the `curve_*`/`reg_check_*`
checkpoints stay on disk (both gitignored) as the record backing this write-up. Closes
this "Next Up" item; next per `doc/oracle_roadmap.md` is Stage 4 (PUCT + policy head,
"AlphaOracle Prime Plus I").

## 2026-09-03 — `A11` IS-MCTS + NN ("AlphaOracle Prime"): registered and shipping -- new roster ceiling, rating 74

Full design record: `doc/ai_agents.md`'s A11 section.
Built 2026-09-01/02, this session (2026-09-03) closes out the three registration steps
its own "Next session's work" left open, so the agent goes from "both ship gates PASS,
not yet registered" to a real, shipped roster agent.

**The staged design (`about.md`'s "Confirmed plan", 2026-09-01):** value net first,
policy head gated on results. Stage 1 (self-play data + training) and Stage 2 (C
inference + tree integration) had to clear both Stage 3 ship gates before Stage 4
(PUCT + policy head) would even be attempted -- two simultaneous changes would make a
null result hard to attribute, and `A10`'s own Phase 6 finding was that rollout
*policy* (not tree scoring) was the dominant lever, so the cheaper/lower-risk
mechanism was staged first, the same ordering `A13` Cartographer used.

**Stage 1 -- self-play corpus.** `A10`'s own `limit_iterations=4000` games, logged
from `A10`'s own decision points only (never the opponent's), across a **curated
opponent pool, not pure mirror self-play**: `A10` vs itself, vs `A7` (Grandmaster --
strongest deterministic synthesis, structurally nothing like tree search), vs `A3`
(Borealis -- the fixed rating anchor). Deliberately excludes `A5` as an explicit
opponent despite being a strong closed-form agent in its own right -- it's already
`A10`'s own internal rollout policy (redundant diversity). A 1-hour, 12-worker pilot
(`aicalibsrc/ismctsnn/gen_corpus.c` + `run_selfplay.sh`, headerless flat-float32
records, a seed ledger preventing RNG-seed collisions across pilot/full-run
generations) produced 657,220 records / 1.32GB, split evenly across the three
matchups (4 shards each). Real throughput came in at ~44% of the naive single-thread
extrapolation -- see `feedback_concurrent_worker_timing_estimates` (memory) and
`local_training_plan.md`.

**Training** (`train_value_net.py`, PyTorch CPU): a small MLP
(537-float state -> 256 -> 128 -> 64 -> 1). The 537-float state encoder
(`ai_strat_ismctsnn_state.h/.c`) is 5 catalog count-vectors (102 champions + Draw-2 +
Draw-3 + Cash, one vector each for own hand, own/opp discard, own/opp combat zone)
plus 12 scalars (energy, cash, turn, phase, deck-remaining, opp hand size, combo-bonus
one-hots) -- relativized to the observing player, encoding no deck *contents*
(anti-clairvoyance, matching `mc_determinize()`), verified against a real
`setup_game()` state. **First run overfit catastrophically past epoch 1** (best val
MSE at epoch 1, then monotonically worse for 1000+ epochs after -- likely cause: many
correlated decision-records from a modest number of distinct games, no
regularization). Fixed with `dropout=0.4`, `weight_decay=1e-3`, lower `lr=3e-4`: best
checkpoint moved to epoch 4-14 depending on the run, ~28-30% MSE reduction over
baseline, graceful degradation after. **Lesson: always check the epoch of the best
checkpoint, not just its value** -- "best at epoch 1, all downhill after" is a red
flag even when that epoch's number looks fine. The weights that shipped
(`assets/ismctsnn/prime_657k_weights.bin`) come from the `reg_strong_value_net.pt`
checkpoint (dropout=0.4/weight_decay=1e-3/lr=3e-4, 14 epochs, val MSE 0.17051 vs
baseline 0.24508) -- confirmed by re-exporting and comparing byte-for-byte against the
shipped file, since two regularized checkpoints existed and the filename alone didn't
disambiguate which one was actually measured.

**Stage 2 -- C inference + integration.** Training stays Python/PyTorch, offline;
inference is a hand-written plain-C forward pass (`ai_strat_ismctsnn_net.h/.c`), no
external ML runtime -- weights exported as a raw float array
(`export_weights.py`, fusing the trained BatchNorm1d into `Linear1` at export time, an
exact transform in eval mode), the same `rating_csv.c`-style persistence precedent
every other file-I/O feature in this codebase follows. Verified to `2.4e-7` max diff
against the live PyTorch model. A `nn_value_trust` dial added to the *shared*
`ISMCTSParams` struct (`ai_strat_ismcts1.h`) is consumed by a new `leaf_value()`
helper in `ai_strat_ismcts_search.c` (0.0 = pure `A10` rollout-to-terminal, bit-for-bit
recoverable; 1.0 = pure NN value, skips the rollout entirely). New agent file
`ai_strat_ismctsnn.h/.c` reuses `ismcts_search_best_move()` completely unchanged --
UCT selection, determinization, and rollout policy (`A5` Heuristic on both seats) are
all identical to `A10`; this agent only changes what evaluates a leaf. The superset
guarantee was verified empirically, not just by inspection: `--ai.a=ismcts` vs
`--ai.a=ismctsnn` at trust=0 produced byte-identical `stda.auto` output.

**A real methodological gotcha, worth remembering for any future harness in this
shape:** the first `calib_ismctsnn.c` copied `A13`'s "set params on both registries,
harmless if unread" pattern from `calib_a13.c`. That's unsafe here specifically --
`A10`/`A11` share one `ISMCTSParams` struct and one `ismcts_search_best_move()`
function (unlike `A13Params`/`HBTParams`, genuinely disjoint), so an "ismcts" seat was
silently inheriting whatever `nn_value_trust` sat in its parsed params block. Every
measurement before the fix was invalid. Fixed by forcing `nn_value_trust=0.0f`
specifically on the copy passed to `ismcts_set_params()`. **Lesson: "harmless to set
params another agent won't read" only holds when the structs are actually disjoint --
verify, don't assume, whenever two agents share one struct/function.** See
`aicalibsrc/ismctsnn/README.md`.

**Stage 3 -- measurement, both gates PASS.** Exploratory sweep (`nn_value_trust` vs
`ismcts`, n=1040/point) showed win rate rising **monotonically** with trust (0.50 at
trust=0 up to 0.60 at trust=1.0) -- the mirror image of `A9`'s `reply_trust`/`A13`'s
`hplus_trust` decline signature, not a repeat of it. Full-rigor
`validate --trust 1.0` (n=4,110 games each, ~1.5pp Wilson CI half-width, 30 jobs / 15
workers, two full rounds, no idle tail):

- **Gate 2 (the real bar) -- vs `ismcts` (`A10`), both seats**: baseline (`trust=0.0`)
  read exactly 0.5000 (superset guarantee reconfirmed at this n); candidate
  (`trust=1.0`) **58.44%**, 95% CI **[56.93%, 59.94%]**. **PASS.**
- **Gate 1 (context) -- vs `borealis` (`A3`)**: baseline (`trust=0.0`, i.e. plain
  `A10`) read **67.88%** [66.44%, 69.29%] -- matches `A10`'s own documented measured
  rating (67.6-69.5) almost exactly, a free cross-check that the harness measures
  correctly, not just consistently. Candidate (`trust=1.0`) **74.04%**, 95% CI
  **[72.68%, 75.36%]** -- an estimated **Borealis rating of ~74**, about +5 over
  `A10`'s 69.

This is a genuine positive result, not the null result the shelve-on-null policy
(matching `A13`'s precedent exactly) was written for -- `nn_value_trust` did not
calibrate to 0, and `ismctsnn_get_default_params()`'s existing default
(`nn_value_trust=1.0`) is already the best-measured point, so no further trust-value
tuning is indicated.

**Timing** (`aicalibsrc/ismctsnn/calib_ismctsnn_timing.c`): the hand-written C forward
pass (~179K params, called up to `limit_iterations=4000` times/decision) costs
**0.496s mean/decision at `-Og`** (project default) -- about **16x** `A10`'s own
plain-rollout cost (~0.031s/decision). The NN forward pass dominates: trust=1.0
(rollout skipped entirely) costs about the same as trust=0.5 (both rollout and NN
paid). A new `make release` (`-O2`, this session) target measures **0.439s
mean/decision -- only ~11.5% faster**, since `linear_relu()`
(`ai_strat_ismctsnn_net.c`) is a naive unvectorized triple loop under strict FP
semantics and plain `-O2` (no `-ffast-math`/`-march=native`, kept portable) doesn't
autovectorize it much. Both `-a -p` regression baselines (`-Og` and `-O2`) matched
`bin/expectedresults.txt` exactly -- `-O2` doesn't change game outcomes here.

**Registration (this session, 2026-09-03) -- the three steps `about.md`'s "Next
session's work" left open:**

1. **Rating table**: `player_config.c`'s `AI_STRATEGY_RATINGS[AI_STRATEGY_ISMCTS_NN]`
   updated from the pre-measurement `{97, false}` to the real `{74, true}` -- the new
   roster ceiling, above `A10`'s 69.
2. **Weights packaged as a shipped asset**: new top-level `assets/` directory
   (sibling to `src/`/`bin/`/`doc/`, category-scoped per subfolder --
   `assets/ismctsnn/` today, with the future SDL3 GUI's champion artwork expected to
   be the next subfolder). The pilot-corpus-trained weights ship as-is (not blocked on
   a bigger corpus, Jonathan's explicit call) at
   `assets/ismctsnn/prime_657k_weights.bin`, with a `.json` sidecar recording
   architecture, training hyperparameters, corpus composition, and the measured Gate
   1/2 numbers.
3. **Default weight load wired into real play**: `main.c` now calls
   `ismctsnn_load_weights()` once before mode dispatch, covering CLI/TUI/`stda.auto`/
   `stda.rating` alike, with a new `--ai.weights=PATH` override
   (`ISMCTSNN_DEFAULT_WEIGHTS_PATH` otherwise). A missing/corrupt file prints one
   `stderr` warning and leaves `decide_and_apply()`'s own trust=0 fallback in place --
   never worse than plain `A10`, just not real AlphaOracle Prime play. A related
   latent bug was fixed alongside this: `g_params[]` previously started at
   `nn_value_trust=0.0f` even with weights loaded (only calibration harnesses called
   `ismctsnn_set_params()`), so real play would have stayed silently at plain `A10`
   even after this wiring landed -- `ismctsnn_load_weights()` now promotes
   `g_params[]` to `ismctsnn_get_default_params()` (trust=1.0) on success, and
   `ismctsnn_reset_params()` was changed to reset to this agent's own default rather
   than `A10`'s, for the same reason.

Also added this session, found useful/needed while shipping the above:

- **`--rating.agents=LIST`** (`cmdline.c`/`stda_rating.c`): restricts
  `--stda.rating`'s round-robin to a comma-separated shorthand list -- without it,
  `register_implemented_agents()` auto-enrols every implemented agent including the
  now-registered, ~16x-slower-per-decision `A11` (and `A10`), making a full-roster fit
  impractical. An unknown or unimplemented shorthand in the list is an error, not a
  silent skip (same reasoning as `parse_agent_option()`).
- **`make release`** (`-O2`, no debug symbols): the only build configuration besides
  the `-Og` default/`debug`. Used to produce the timing numbers above; kept as a
  general-purpose target for long simulation runs and the tree-search agents
  (`A8`/`A10`/`A11`).

**Verification**: `-a -p` regression baseline identical to `bin/expectedresults.txt`
at both `-Og` and `-O2`; `--ai.a=ismctsnn --ai.b=ismcts` with and without a valid
weights file produces genuinely different, direction-correct outcomes (confirming the
agent is no longer silently inert); `test_combo`/`test_recall`/`test_cash_exchange`/
`test_rating` all pass unchanged; `valgrind --leak-check=full` clean on the new
weight-loading path.

Closes `doc/oracle_roadmap.md`'s "Next Up" item 4. `A11` was the last rung on the
original `A1`-`A11` ladder; `A12` Clairvoyant and the shelved `A13` Cartographer sit
outside that ladder's enum ordinal sequence. Stage 4 (PUCT + policy head, "AlphaOracle
Prime Plus I" -- renamed from an earlier "Prime II" placeholder 2026-09-04, see that
date's entry -- if it ever ships) is technically unlocked but undesigned and not
scheduled.

---

## 2026-08-31 — `A13` Cartographer (Next Up item 3): implemented, calibrated, shelved -- nothing beat A7

Full design record: `doc/ai_agents.md`'s A13 section. Built as
A7's exact three-layer synthesis (inherited verbatim as `.base`) plus four new deterministic
layers, all closed-form over the exact unseen-card pool derived by subtraction from the
known 120-card deck (never sampling, never fabricating a single hidden hand -- a direct,
structural answer to `A9`'s failure): Layer R (race/turns-to-kill arithmetic turning `A7`'s
fixed signed `defense_stdev_mult` into a state-dependent one), Layer K-draw + Layer D
(deck-aware draw valuation, reshuffle-boundary awareness), Layer K-block (Jensen-corrected
expected block, `E[net(S)] = Sum_k P(K=k)*min(max(raw(S)-E[block|k],0),opp_energy)`, the
non-degenerate alternative to the naive block-subtraction model that doomed `A9`). Ten new
`A13Params` fields (originally eleven; `belief_opp_power_trust` was dropped mid-implementation
as a redundant, dimensionally-mismatched third tier on a quantity `race_use_belief_opp`
already selects between). Six new files: `ai_strat_a13.{c,h}` (orchestration, params,
mulligan/discard thunks onto `A7`'s own `hbt_*_with()`), `ai_strat_a13_belief.{c,h}` (the
closed-form pool/hypergeometric machinery), `ai_strat_a13_state.{c,h}` (Layer R),
`ai_strat_a13_enum.{c,h}` (move enumeration/scoring, reusing `A7`'s `hbt_advantage()`/
`predicted_damage()`/`is_held_combo()`/`evaluate_defense_subset()` verbatim).

**The superset guarantee held throughout.** One parameter vector (every new dial at its
neutral value) provably recovers `A7` bit-for-bit -- verified not just at the unit level but
via whole-game bit-for-bit replay (5 seeds x 40 games each, every single game's outcome and
turn count identical) at every stage of implementation, including after two real bugs were
found and fixed in the belief module.

**Calibration, staged per the approved plan (`aicalibsrc/carto/calib_a13.c` +
`calibrate_a13.py`, same in-process harness pattern as every other agent):**

- **Stage 1 (Layer R alone).** A first `optimize` attempt at too small a per-evaluation
  budget (2000 games/eval) found what looked like a 66%+ candidate that evaporated to 64%
  once re-measured with more games at the same seeds -- textbook differential-evolution
  overfitting to noise, not a real signal. Re-run properly powered (16,000 games/eval):
  converged to sign-correct values matching the documented convention, validated 64.82% vs
  `borealis` (40k games) and 49.93% vs `hbt` -- both statistically identical to neutral,
  no gain either way.
- **Stage 2 (Layer K-draw + D).** A univariate sweep of `belief_draw_weight` surfaced a real
  bug: `pool_mean_power()` averaged over the *whole* unseen pool including non-champion
  cards (draw/cash cards carry much lower `power` than champions), diluting `draw_value`
  against its `AVERAGE_POWER_FOR_MULLIGAN` comparison baseline by a large, roughly-constant
  amount unrelated to real pool depletion -- produced a spurious catastrophic collapse
  (34.7% win rate) at strongly positive weight, a bug signature rather than a finding. Fixed
  to champion-only averaging; the sweep flattened but stayed within noise. A second
  refinement followed Jonathan's clarification that `power` (his own straight 50/50 average
  of `attack_efficiency`/`defense_efficiency`) and every other derived `fullDeck[]` field are
  early heuristic guesses, not authoritative -- and that a 50/50 attack/defense weighting had
  no basis. Measured the real split directly (a scratch harness replicating `play_turn()`'s
  exact sequence, instrumented between `defense_phase()` and `resolve_combat()`, ~8000 games
  across `hbt`/`borealis`/`heuristic` pairings): **78.21% attack / 21.79% defense**,
  consistent within a few points across every pairing. Rebuilt the value function around
  `0.7821*attack_efficiency + 0.2179*defense_efficiency` (new deck-wide baseline
  `A13_AVERAGE_CARD_VALUE=5.51528`, replacing `AVERAGE_POWER_FOR_MULLIGAN`). Result:
  flatter, not better -- 64.68% vs `borealis`, 49.93% vs `hbt`, still parity, ruling out
  "wrong value function" as the explanation.
- **Stage 3 (Layer K-block), A9's sweep protocol first.** `hplus_trust`: clean monotonic
  decline vs both opponents (vs `hbt`: 50.00%->42.09%->31.94%->23.84%->20.34%; vs
  `borealis`: 64.68%->50.96%->33.91%->21.82%->17.18%, trust 0/0.25/0.5/0.75/1.0, tight
  non-overlapping CIs) -- `A9`'s exact `reply_trust` failure signature, even sharper.
  Pinned to 0, conclusively. `belief_opp_block_trust`: a first sweep was flat because the
  dial only reaches `a13_evaluate_state()`'s output by first flowing through the race-scale
  machinery, which was still pinned off from Stage 1 -- a direct, concrete example of the
  cross-layer coupling risk flagged going in. Re-swept with `race_scale`/`race_stdev_ahead`/
  `race_stdev_behind` held live: still flat, non-monotonic, no exploitable signal, but this
  time for the right reason (the diffuse opponent-hand belief, not dead wiring).
- **Broad joint search (Layer R + K-draw + `belief_opp_block_trust`, 630 evaluations, no
  early convergence)**, run specifically to test whether the per-stage null results were a
  coordinate-descent artifact rather than a real absence of signal: validated 65.18% vs
  `borealis` (60k games) and 49.80% vs `hbt` -- still statistically identical to neutral. The
  coordinate-descent hypothesis does not appear to explain the pattern.
- **Stage 4 (+ `defense_stdev_mult` free)**, under a hard sub-1-minute search budget: a
  driver bug initially made `defense_stdev_mult` silently stay pinned despite being
  explicitly requested (`PINNED_PARAM_NAMES` included all 34 base fields unconditionally,
  with no carve-out for the one documented exception; a second latent bug -- no `BOUNDS`
  entry for it at all -- sat behind the first). Fixed both. Re-run at a guaranteed-fast
  budget (measured actual per-batch wall time directly rather than re-guessing after one
  overrun): `defense_stdev_mult=1.29` (far from `A7`'s shipped 0.2156) alongside the usual
  race/belief values, validated 64.88% vs `borealis` and 49.91% vs `hbt` -- still parity.

**Verdict (Jonathan's call, 2026-08-31): shelved, not registered.** `AI_STRATEGY_CARTOGRAPHER`
was removed from `AIStrategyType`, its `STRATEGY_REGISTRY` entry and the four
`player_config.c` table entries (menu label, rating, display name, shorthand) reverted --
`carto` is no longer selectable anywhere (CLI, TUI, `--stda.rating`). A second copy of `A7`
under a new name -- and, had `hplus_trust` shipped, a net-negative one -- would pollute the
Bradley-Terry fit, the same reasoning already applied to `A9`'s own below-target 2026-08-28
re-attempt, here applied to a fully-null rather than partially-successful result. The source
files remain on disk as reference (not compiled into `STRATEGY_REGISTRY`, harmless dead code
in `bin/oracle`'s build); `ai_strat_a13_belief.{c,h}` (the closed-form pool/hypergeometric
machinery) is explicitly kept as reusable infrastructure for a future `A11` feature-extraction
pass. `aicalibsrc/carto/` stays buildable (`make calib_a13`) so this record stays
reproducible. Closes this "Next Up" item; next per `doc/oracle_roadmap.md` is `A11` IS-MCTS+NN.

## 2026-08-28 — Mulligan / seat-advantage investigation (Next Up item 2): real effect, but agent-dependent in both size and direction -- shipped rule unchanged

Consolidated the mulligan card-count cap first: it was duplicated as a local
`uint8_t max_nbr_cards_to_mulligan = 2;` in three separate agent files
(`ai_strat_lib_heuristics.c`'s shared default, `ai_strat_borealis.c`, `ai_strat_hbt_cards.c`)
plus the interactive human path (`game_commands.c`), all doing the identical
below-`AVERAGE_POWER_FOR_MULLIGAN` counting loop. Now one shared accessor,
`mulligan_get_max_cards()` (`ai_strat_lib_heuristics.c`/`.h`), with a
calibration-only `mulligan_set_max_cards()`/`_reset_max_cards()` override pair
(mirrors `value_based_set_params()`/`_reset_params()`'s shape) -- a shared
game-rule parameter, not a per-agent dial, so one global value rather than
per-player. `game_commands.c`'s fixed-size `indices[2]` stack array needed care,
not a literal swap: grew to `indices[MULLIGAN_HARD_CAP]` (4) since a runtime
value can't size a C array. `A10` IS-MCTS's mulligan search
(`ai_strat_ismcts_flat.c`) stays a fixed 0/1/2-card enumeration, not driven by
this value -- extending it needs a real `enumerate_triples()`, out of scope
here. Default stays 2; `-a -p` regression baseline unaffected by construction.

Built `aicalibsrc/mulligan/` (`calib_mulligan.c` + `calibrate_mulligan.py`) to
measure the effect -- a different shape from every `aicalibsrc/<agent>/` driver,
since this investigates a shared game-rule parameter's fairness rather than an
agent's playing strength. `seat` runs a large **self-mirror** match (an agent
against itself, both seats) and reports Player A's win rate: since both seats
run the *identical* strategy, any deviation from 50% is purely the seat/mulligan
effect, not confounded with relative agent strength -- the clean design this
question needed, and cleaner than the original cross-agent observation
(`combo` vs `rand`, 84.85%/91.11%) that started this investigation.

**Result, current shipped rule (`max_cards=2`), self-mirror, n=24,000/agent for
the closed-form roster (`aicalibsrc/mulligan/results/`):**

| Agent | P(A wins) | 95% CI |
|---|---|---|
| `borealis` | 56.83% | [56.21%, 57.46%] |
| `balanced` | 54.22% | [53.59%, 54.85%] |
| `rand` | 49.65% | [49.01%, 50.28%] |
| `heuristic` | 48.40% | [47.77%, 49.03%] |
| `hbt2ply` | 48.18% | [47.54%, 48.81%] |
| `value` | 46.58% | [45.95%, 47.21%] |
| `hbt` | 45.45% | [44.82%, 46.08%] |
| `combo` | 45.16% | [44.53%, 45.79%] |
| `tactical` | 40.16% | [39.54%, 40.78%] |

Plus the costlier search-based agents at n=900 (smaller sample given their
per-game cost): `ismcts` 37.89% [34.78%, 41.10%] (clear second-seat effect,
larger than `tactical`'s), `simplemc` 47.11% [43.87%, 50.38%] and `clairvoy`
48.44% [45.19%, 51.71%] (both consistent with no effect at this sample size).

**The seat effect is real for most agents (CI excludes 50% for 8 of 9
closed-form agents plus `ismcts`), but its size AND DIRECTION are agent-dependent,
not a single global bias**: `borealis`/`balanced` show a genuine *first*-player
advantage; `tactical`/`combo`/`hbt`/`value`/`hbt2ply`/`heuristic`/`ismcts` show a
*second*-player advantage, ranging from small (`heuristic`, ~1.6pp) to large
(`ismcts` ~12pp, `tactical` ~10pp); `rand` is essentially neutral. This
supersedes the original `A1`/`A2`-era observation and resolves `A3` Borealis's
own earlier conflicting mirror-match evidence (`doc/changelog.md`'s 2026-08-23
entry) -- both were measured before the 2026-08-28 combo-bonus/PASS-dominance
fixes and are superseded by this clean re-measurement.

**Sweep of `max_cards` (0-4) for three representative agents** confirmed the
cap is a real lever, but not a uniform one:
- `rand` (control): stays close to neutral throughout (49.65%-52.36%), with a
  small, non-monotonic wobble -- `AVERAGE_POWER_FOR_MULLIGAN`'s count-trigger
  design means the cap only matters up to how many below-average cards a hand
  actually holds, so raising it past ~2-3 has diminishing and sometimes
  reversing effect even for a strategy-blind agent.
- `tactical` (largest second-seat effect): cap 0 -> 46.02%, cap 1 -> 40.19%,
  cap 2 -> 40.16% (shipped), cap 3 -> 42.77%, cap 4 -> 44.63%. Raising the cap
  *would* move this agent back toward fairness.
- `borealis` (first-seat effect): cap 0 -> 48.29% (no effect), cap 1 -> 55.17%,
  cap 2 -> 56.83% (shipped), cap 3 -> 55.10%, cap 4 -> 54.76%. The advantage
  appears entirely *because* mulligan exists at all (jumps in the moment
  `max_cards` goes from 0 to 1) and stays roughly flat regardless of the exact
  cap thereafter -- for this agent, the imbalance isn't about how many cards
  Player B can swap, it's that giving Player B *any* mulligan interacts badly
  with Borealis's own combo-hold mulligan override (`borealis_mulligan()`).

**Decision: `MULLIGAN_DEFAULT_MAX_CARDS` stays 2, not changed.** No single
alternate value clearly helps across the roster -- raising the cap helps
`tactical` but doesn't meaningfully move `borealis`'s imbalance (which is about
mulligan's mere existence, not its size), and `rand`'s own response is small
and non-monotonic. The seat effect turns out to be substantially about
agent-specific play style (tempo-oriented agents favor going first; card/resource-
oriented agents favor the extra mulligan cards) rather than a pure rule defect a
single shared dial can correct for every agent at once. 2 also matches the
original design intent (`doc/game_rules_doc.md`) and, per Jonathan, "feels fun
and fair" in actual casual play -- consistent with what the data shows once the
effect is understood as agent-dependent rather than a uniform unfairness.
Revisit per-agent if a specific agent's own calibration ever specifically
targets seat balance (e.g. `borealis`'s own mulligan override could be
re-examined on its own, separately from the shared cap).

Two things confirmed as intentional while investigating, not gaps: interactive
mode already has its own random-first-player option (`player_config.c`'s
`get_player_assignment()`, `ASSIGN_RANDOM`); batch mode's `setup_game()`
hardcoding Player A first is deliberate, matching how every calibration/rating
tool already measures both seat orientations explicitly rather than relying on
coin-flip noise that would need a larger sample to average out the same
information.

`ideas/4 match results export/`'s interactive-mode CSV exporter was *not*
built as part of this item -- its real value (mining heuristics from true human
play, feeding `A11`'s future neural network, per Jonathan) is independent of
the seat question and it can't produce batch data anyway. Rescoped and
scheduled as its own future roadmap item, naturally timed alongside `A11`.

All 8 test suites (200 assertions) green; `./bin/oracle -a -p` byte-identical
to `bin/expectedresults.txt`; `make format` clean.

## 2026-08-28 — Housekeeping bug fixes (Next Up item 1): combo-bonus table plumbing, calibration-driver bugs, A1 re-validated

First item in the agreed 2026-08-28 project sequencing (`doc/oracle_roadmap.md`'s
"Next Up"), ahead of the mulligan investigation and `A13`/`A11` since each protects
the integrity of work later items build on. Three sub-fixes, plus a rename.

**1. `combat.c`'s combo-bonus table selection, and a rename.**
`calculate_total_attack()`/`calculate_total_defense()`/`resolve_combat_with_details()`
all hardcoded `calculate_combo_bonus()`'s table argument to `DECK_RANDOM` regardless
of the actual deck in play -- found while writing `ideas/G3 ai agent deck
construction/custom_deck_construction_handout.md`, and a real correctness bug today
(not just future-facing prep), blocking for `G3` whenever it's picked up.
`combo_bonus.c` itself already branched correctly (`calc_random_bonus()` vs
`calc_prebuilt_bonus()`); the bug was purely in what got passed in. Fixed by adding
`combo_bonus_table` to `struct gamestate`, defaulted to the random table in
`setup_game()`, and reading it at all four call sites instead of the literal --
zero call-site churn (only `resolve_combat()` calls the two `calculate_total_*()`
functions, and `struct gamestate` is copied by value everywhere MC/ISMCTS
simulations fork state) and a byte-identical `-a -p` baseline (no `--deck` option
exists anywhere, so every game today already is a random-distribution game).

Renamed the whole enum along the way (Jonathan, 2026-08-28): `DeckType`/
`DECK_RANDOM`/`DECK_MONOCHROME`/`DECK_CUSTOM` conflated "how the deck was built"
with "which combo-bonus table applies." Per `ideas/10 Draft Format and Game Depth
Addition Ideas/oracle-draft-formats-complete.md`'s ~15 planned deck-building
approaches, several are expected to share one of a handful of scoring tables, so the
type needed to name the table, not the construction method. Renamed to
`ComboBonusTable`/`COMBO_BONUS_RANDOM`/`COMBO_BONUS_MONOCHROME`/`COMBO_BONUS_CUSTOM`
while the type still had only 6 usage sites -- cheap now, much more expensive after
`G3`/draft-format work spreads it further. `ai_strat_common.c`'s
`combo_bonus_for_selection()`/`expected_incoming_attack()` keep their own hardcoded
`COMBO_BONUS_RANDOM` deliberately (an AI-side estimate, not authoritative
resolution, already documented as such) -- not touched.

New test `testsrc/test_combat.c` (`make test_combat`, instantiating a scaffold the
makefile had carried commented-out since early on): no prior test exercised
`combat.c`'s combo-bonus integration at all (only `combo_bonus.c`'s own routing),
which is exactly why the bug survived. Confirms `setup_game()`'s default and that
`calculate_total_attack()` actually reads the table (two identically-seeded contexts,
same combat-zone cards, table forced to random vs custom, delta matches the known
10-vs-7 same-species combo difference exactly).

**2. Calibration-driver `DEFAULTS` drift**, ported from `aicalibsrc/balanced/`'s
already-fixed pattern into the three remaining harnesses. `calib_valuebased.c`,
`calib_combo_threshold.c`, `calib_borealis.c` each gained a `--print-defaults` mode
dumping the live compiled struct as JSON (`ai_strat_valuebased.c`/`.h` gained a new
`value_based_get_default_params()` accessor to support it -- its two tunables were
private `#define`s with no accessor before); all three Python drivers now read
`DEFAULTS` from the binary at import time instead of a hand-copied dict. Measured
drift before the fix: `calibrate_valuebased.py`'s `cost_floor`/`defend_threshold`
were the exact pre-calibration values (1.0/0.5 vs shipped 1.3/0.8);
`calibrate_combo_threshold.py` had drifted on 7 of 9 fields; `calibrate_borealis.py`'s
`luna_value` was off by 9.2x (0.5 vs shipped 4.5846).

**3. Calibration-driver result misattribution**, ported the same way from
`aicalibsrc/balanced/`. `run_match()` in `calibrate_valuebased.py` and
`calibrate_combo_threshold.py` now takes `**tags`, merged straight into the returned
dict, so results are self-describing -- `cmd_sweep()`/`cmd_selfplay()` no longer
reattach per-job bookkeeping (`_i`/`_j`, `_value`/`_combo_in_a`) from a
submission-order list after `run_many()`'s `ProcessPoolExecutor` returns results in
completion order, which previously scrambled attribution under real concurrency
(the original repro: a `luna_value`-style sweep going jagged/non-monotone with
multiple workers, smooth with `--workers 1`). `calibrate_valuebased.py`'s
`summarize_selfplay()` also carried a transpose-based win-rate bug
`calibrate_combo_threshold.py`'s own version had already fixed (`wins.T.sum(axis=1)`
sums *opponents'* wins in games where a candidate was on defense, not the
candidate's own wins) -- ported that fix too while in the same function.

**A1's `VB_COST_FLOOR` re-validated with both bugs fixed, not re-shipped.** A
10-point `cost_floor` selfplay grid (0.5-3.0, `defend_threshold` pinned at its
shipped `0.8`, 720,000 games) fit a quadratic through (cost_floor, strength) at
**R² = 0.84** -- dramatically higher than the original 2026-08-21 calibration's
unexplained R² ≈ 0.25-0.49, confirming the misattribution bug was a real,
significant noise contributor to that oddity, not just the "mild effect" the
handout predicted. However, a focused, larger-sample head-to-head restricted to the
shipped `1.3` against the fit's top candidates (`1.5`/`1.7`/`2.0`) came back
statistically tied (overall win rates 0.4965-0.5027 across 144,000 games each, no
candidate clearly dominates). **The bug explains the previously-mysterious low R²,
but doesn't change the ship decision**: `1.3` remains a defensible value inside the
same noise-dominated cluster the original calibration identified (the same
conclusion, on cleaner evidence) -- `VB_COST_FLOOR` is unchanged.

All eight test suites (`test_combo` `test_recall` `test_cash_exchange` `test_rating`
`test_hbt2ply_reply` `test_moves` `test_ismcts` `test_combat`, 200 assertions) green;
`./bin/oracle -a -p` byte-identical to `bin/expectedresults.txt`;
`make calib_valuebased calib_combo_threshold calib_borealis` all build with
`--print-defaults` output verified against the actual shipped C constants;
`make format` unchanged.

## 2026-08-28 — A7 Hybrid HBT re-optimized with the PASS-dominance fix in place: 58 → 65, closes the follow-up task

Answers the open Bug Tracker question from the 2026-08-27 A5/A7 defense fix (see that
entry below and the `project_a5_a7_defense_pass_dominance` memory): why did an identical
PASS-dominance fix improve A5 (60→64) but hurt A7 (62→58)? Leading hypothesis (user,
2026-08-27) was to re-optimize A7's `HBTParams` with the fix in place rather than revert
it — this is that re-optimization.

**Root cause**: `defense_stdev_mult` (A7's only defense-exclusive dial, feeding
`variance_aware_incoming()`) was dead weight before the fix -- PASS strictly dominated
every block regardless of the incoming-attack estimate, so this parameter's shipped
+0.711 (an A6-style inflation) was fit to noise. Post-fix it became live for the first
time, and confirmed via a univariate sweep vs `borealis`
(`aicalibsrc/hbt/results/sweep_defense_stdev_mult.csv`): win rate fell monotonically
from 60.96% at -2.0 to 57.53% at +2.0 -- the shipped +0.711 was now actively biasing
toward over-blocking, which cost more than it saved. A second sweep on
`penalty_cards_weight` (the "double-charges card spending on defense" hypothesis) showed
no comparable monotonic effect -- a weaker, unconfirmed contributor at best.

**Recalibration**: re-ran `optimize()` over the same 12 free parameters stage 1/2 used
originally (the 8 stage-1 coupling/defense fields plus H's own 4 weights) vs `borealis`,
three ways:
- Unconstrained `BOUNDS`: 64.15% [63.68%, 64.62%] (40,000 games), no personality flags,
  but `weight_cards_advantage` drifted to 12.68 -- the same known failure mode from the
  original (pre-fix) stage 2 search (which drifted to 10.72), re-appearing and slightly
  worse.
- `--identity-safe` over its full bounded-field table (not restricted to the same 12):
  63.49% -- discarded on inspection, since `BOUNDS_IDENTITY_SAFE` has no entries for
  `target_aggr_cash_scale`/`target_aggr_cards_scale`, so omitting `--params` silently
  re-opened T's whole aggression/phase battery instead of giving a tamer version of the
  intended 12-param search -- a scope mismatch, not a fair comparison.
- `--identity-safe` restricted to the 10 of those 12 fields `BOUNDS_IDENTITY_SAFE`
  actually covers (the 2 excluded fields held at their existing values): **64.62%
  [64.15%, 65.09%] (40,000 games), no personality flags, `weight_cards_advantage` landed
  at 1.95** -- essentially A5's own shipped 1.96, no drift at all. Best win rate of the
  three candidates AND the only one that stayed true to "H ranks with A5's own tuned
  advantage function" -- also won a direct three-way self-play round-robin against both
  the unconstrained candidate and the shipped defaults
  (`aicalibsrc/hbt/results/selfplay_named.csv`). **Shipped.**

Only 10 of the 34 `HBTParams` fields changed from the original 2026-08-25 stage-1 fit:
`weight_energy_advantage`, `weight_cards_advantage`, `weight_taper_exponent`,
`opp_card_discount`, `aggr_energy_gain`, `aggr_resource_fade`, `critical_epsilon_mult`,
`penalty_cash_weight`, `penalty_cards_weight`, `defense_stdev_mult` (0.711 → 0.216 --
still net inflating the incoming-attack estimate, just far less). Every other field --
T's whole aggression/phase battery, B's resource targets, the combo-hold trio -- is
untouched. See `ai_strat_hbt.c`'s `HBT_DEFAULTS` comment for the exact before/after
values.

**Measured** (both seats, `-a -p --ai.a=hbt --ai.b=borealis`): 2650/4000 = 66.25%
(n=4000, matching the exact convention the 57.5%/58 pre-recalibration figure used);
5257/8000 = 65.71% (n=8000, a larger same-method sample). All three measurements (harness
40,000-game validated 64.62%, and these two direct-binary samples) agree within their
respective confidence intervals. **Shipped rating: 65** (from the most precise,
40,000-game figure), a gain of 7 over the pre-recalibration 58 and 3 over the original
pre-fix 62 -- closes the Bug Tracker item with a clean answer: A7's regression wasn't an
inherent A5-vs-A7 asymmetry, it was one dial (`defense_stdev_mult`) whose old value had
never been tested against a live PASS baseline before.

**Side effect on A9 HBT 2-Ply**: A9 reuses A7's `hbt_advantage()`/`predicted_damage()`/
`predicted_block()` and reads `HBTParams` directly, so the new defaults move it too --
spot-checked at 1270/2000 = 63.5% vs `borealis` (up from its own shipped 59), and the
`test_moves.c` fixture
`test_hbt2ply_ply_changes_score_when_reply_possible()` needed updating: its single-champion
15.5-expected-attack scenario no longer reliably crosses the "worth blocking" threshold
under the new weights (lower `weight_energy_advantage` and `defense_stdev_mult` both
shrink the perceived benefit of blocking one card's worth of damage), so it now attacks
with two non-combo champions (~30 combined expected attack) to keep the assertion's
actual intent -- "the ply is alive when blocking is plausible" -- robust. A9's own rating
was not formally re-measured/re-shipped in this session; that remains open if wanted.
Also renamed `hbt_best_defense_move()`'s `undamaged_energy` local to `damaged_energy` (it
has held the damaged value since the 2026-08-27 fix; the old name was left over from
before that fix and was actively misleading) -- `ai_strat_heuristic.c`'s
`best_defense_move()` has the identical misnomer, left untouched here since A5's own code
is out of scope for this task.

**Housekeeping note**: the pre-recalibration "58" figure being compared against was
itself a single `n=4000` sample (2301/4000 = 57.5%). A much larger re-measurement of that
same, unrecalibrated code (60.2% over 4000 games via the direct binary; 59.95% over
32,000 games via the calibration harness) landed closer to 60% than 58 -- both figures
are genuine, just different sample sizes of the same distribution; the gap is ordinary
sampling variance, not a discrepancy in the code. Recorded here for anyone who re-derives
the "before" number and gets ~60 instead of 58.

`test_combo` `test_recall` `test_cash_exchange` `test_rating` `test_hbt2ply_reply`
`test_moves` `test_ismcts` all green (202 assertions) after the fixture fix above;
`./bin/oracle -a -p` stayed byte-identical to `bin/expectedresults.txt` (default matchup
is Random-vs-Random, which never touches A7).

## 2026-08-28 — A9 HBT 2-Ply re-attempted against the new A7: no improvement found, and the original diagnosis was wrong

Follow-on to the A7 recalibration above. A9's own shortfall against A7 (measured 47.19%
head-to-head, below its own >55% design target) had been diagnosed 2026-08-26 as caused
by A7's broken PASS-dominant defense giving the second ply "nothing real to correct
for" -- with fixing that defense believed to be "a genuine prerequisite for a future
re-attempt." That belief did not hold up.

Re-ran `aicalibsrc/hbt2ply/calibrate_hbt2ply.py optimize --opponent hbt` against the new,
fixed-and-recalibrated A7 (rating 65). Result: converged to `reply_trust = 0.013`,
validated 47.40% [46.91%, 47.89%] (40,000 games) -- statistically indistinguishable from
the shipped defaults' own 47.11% [46.34%, 47.89%] against the same new A7. In other
words, the search essentially rediscovered "turn the ply off";
`check_personality_flags()` correctly flagged it ("the second ply has been calibrated
into irrelevance"). **Not shipped** -- a candidate that is both no-better and
identity-destroying fails on both counts.

A follow-up univariate sweep of `reply_trust` alone (holding `surrogate_pessimism` at its
shipped default) against the new `hbt` made the mechanism explicit and rules out a search
artifact: win rate declines **monotonically** as trust increases --

| reply_trust | 0.00 | 0.25 | 0.50 | 0.75 | 1.00 |
|---|---|---|---|---|---|
| win rate vs new `hbt` | 47.64% | 43.40% | 39.35% | 37.29% | 31.20% |

(16,000 games/point). `reply_trust = 0` -- proven to recover A7's own decision
bit-for-bit (`test_hbt2ply_reply_trust_zero_matches_a7`) -- is the actual optimum on the
whole curve. **Conclusion: A7's old broken defense was never the reason this agent fell
short of its design target.** The two-ply mechanism itself doesn't have room to improve
on A7 against an opponent this well-calibrated, at least via the two dials this driver
searches (`reply_trust`/`surrogate_pessimism`) -- the surrogate-hand-based reply
simulation apparently becomes actively misleading, not just unhelpful, once matched
against a genuinely strong, well-tuned opponent rather than one whose real defense never
fires. `HBT2PLY_DEFAULTS` are unchanged as a result.

This agent's own Borealis-relative rating moved anyway, with zero parameter changes: A9's
`.base` field is populated by a live call to `hbt_get_default_params()`
(`ai_strat_hbt2ply.c:48`), not a frozen snapshot, so it automatically inherited A7's
2026-08-28 gain. Re-measured **61.62% [60.87%, 62.38%] vs `borealis` (32,000 games)** via
the calibration harness, cross-checked at 61.65% (4,000 games) via the direct binary --
**rating 59 -> 62**. `player_config.c`'s `AI_STRATEGY_RATINGS[AI_STRATEGY_HBT_2PLY]`
updated accordingly.

`ai_strat_hbt2ply.c`'s calibration-provenance comment, `aicalibsrc/hbt2ply/README.md`,
and `ideas/G1 AI agent general info/oracle_ai_agent_names.md`'s roster table all updated
to record this attempt and its negative result, so a future session doesn't re-derive
the same "fix A7's defense first" assumption. All seven test suites (202 assertions)
stayed green; `./bin/oracle -a -p` stayed byte-identical to `bin/expectedresults.txt`.

## 2026-08-27 — A10 IS-MCTS ("The Omniscient") implemented and calibrated: the roster ceiling, after two diagnosed and fixed problems

Single-Observer Information Set Monte Carlo Tree Search (Cowling, Powley & Whitehouse) per
`ideas/A10 .../about.md`: one UCT tree over information sets, a fresh determinization
every iteration, availability counts in the exploration denominator, progressive widening
against Oracle's ~93-typical branching factor. No partially-hidden moves in this game
(champions/draw/recall/cash are all played face-up), so SO-ISMCTS applies cleanly and the
tree needs no hand-written opponent model -- it searches the opponent's replies itself.

- **New files**: `src/ai_strat/ai_strat_ismcts1.{c,h}` (agent shell, `ISMCTSParams`,
  entry points -- replaces the 153-line design-comment stub), `ai_strat_ismcts_tree.{c,h}`
  (node arena -- `uint32_t` parent/first_child/next_sibling indices rather than per-node
  `malloc`, ~40 bytes/node -- UCT scoring, backprop), `ai_strat_ismcts_search.{c,h}` (the
  SELECT/EXPAND/SIMULATE/BACKPROP iteration loop, root move = most-visited child),
  `ai_strat_ismcts_flat.{c,h}` (mulligan/discard-to-7: unfiltered 0/1/2-card subset
  enumeration, each candidate scored by averaging determinized rollouts).
- **New shared infra** (`ai_strat_playout.c/.h`, alongside `A8`'s existing
  `mc_fork_context()`/`mc_determinize()`/`mc_playout()`): `mc_playout_from()` (playout with
  no leading move, for the tree's own newly-expanded leaves), `mc_advance_to_decision()`
  (applies one move and runs the engine forward to the next real decision point --
  the tree's `DoMove()`), `mc_playout_from_turn_boundary()` (playout from a turn boundary
  with no phase pending at all -- mulligan/discard-to-7's own primitive), and
  `mc_outcome_for()` (promoted from `static` so the search loop can read a terminal
  outcome when a decision ends the game mid-descent).
- **Two deliberate deviations from the design docs**: dice rolls stay engine-accurate
  sampled values in rollouts (`about.md`'s "closed-form dice stats" recommendation belongs
  to an expectimax formulation, not UCT, and closed-form `E[damage]` is itself biased by
  the `max(attack-defense,0)` truncation); nodes store no gamestate (re-determinized and
  replayed from the root every iteration), superseding the original stub's "store
  gamestate in child node," which predates the decision to re-determinize per iteration.
- **Reshuffle-aware determinization deferred, not dropped** (Phase 0, measured before
  building anything): 0 reshuffles across 8000 real games played by any of 8 real
  strategies (turn counts up to 42); only pure `rand` vs `rand` reshuffles meaningfully
  (5.1%, avg 34.1 turns) -- the correctness gap this refinement would close essentially
  never fires in real play, so it stays out of v1. See `about.md` for the full writeup,
  including a nuance: a single real `simplemc` game logs hundreds of reshuffle events
  *inside* its internal MC rollouts (random-ish rollout policy simply playing long enough
  to empty a simulated deck), which is expected and NOT the same gap -- the engine's own
  `shuffle_discard_and_form_deck()` handles that correctly inside the fork, no sampling
  step is involved once a rollout is already running on concrete simulated state.

### Finding 1: a uniformly-random rollout policy plateaus below the anchor, the same signature `A8`'s diagnosis found

A budget→rating scaling curve (1k/4k/16k/64k iterations, ~2000 games/level vs `Borealis`,
`AI_STRATEGY_RANDOM` rollout policy on both seats, per the original design intent) rose
from 1k (41.9%) to 4k-16k (~46-48%), then flattened through 64k (46.2%) -- never clearing
the anchor, and unmoved by a 16x further budget increase past the early rise. A controlled
A/B test isolating just the rollout policy (16k iterations, n≈2000, everything else
identical) confirmed the cause: swapping to `AI_STRATEGY_HEURISTIC` (`A5`, with the PASS-
dominance fix below applied) took the same measurement from 47.6% to 63.0% -- a ~15-point
jump, dwarfing every other effect measured on this agent. Shipped as
`ai_strat_ismcts1.c`'s `heuristic_rollout_strategy_set()` (and the matching change in
`ai_strat_ismcts_flat.c`, whose mulligan/discard-to-7 scoring is subject to the identical
bias). Consistent with the ISMCTS literature's own reported experience that a purely
random rollout policy's adequacy is domain-dependent (Powley/Cowling/Whitehouse's Spades
player was strong without heuristic knowledge; Oracle evidently is not -- random self-play
games run ~34 turns on average vs ~7-20 for any real strategy, and are the only matchup
that ever reshuffles a deck, so random play here is qualitatively unlike skilled play).

### Finding 2: with the fixed rollout policy, more search past a point actively hurts

Re-running the scaling curve with the heuristic rollout policy surfaced a different shape:
not a plateau, a peak followed by a real decline. 1k-16k iterations is a noisy plateau
(63-69%, no interior maximum distinguishable from noise at n≈800-2000/level), but 64k
(58.5%) and 100k (55.2%, quick n≈192 sample) drop well below it -- several standard errors,
not noise. Working hypothesis: `A5`'s rollout policy is deterministic given a position
(not randomly varied), so more search lets the tree converge more confidently on lines
that exploit that specific simulated opponent rather than generalizing to a real one --
not yet root-caused beyond this hypothesis. **Shipped `limit_iterations = 4000`**, the
most rigorously sampled point in the plateau (n=1992) -- a large, deliberate departure
from Phase 3's original ~100000-iteration/~1s-per-decision calibration, which was
calibrated against the since-replaced random rollout policy's own timing.

### A genuine bug found along the way, not a design issue

Diagnosing finding 2 via valgrind on the shipped agent surfaced a real uninitialized-read:
`stda_auto.c`'s `play_stda_auto_game()` called `apply_mulligan()` before initializing
`gstate.turn` (only `begin_of_turn()`, called from the main turn loop, did) -- invisible to
every prior agent's mulligan hook, since none of them simulate forward from the mulligan
point, but a genuine bug once this agent's flat-rollout mulligan scoring did (its rollouts
call `begin_of_turn()` internally, which increments `gstate->turn` rather than just
overwriting it). Fixed by moving `gstate.turn = 0`/`turn_phase`/`player_to_move`
initialization before `apply_mulligan()`, matching the pattern `stda_cli.c`/`stda_tui.c`
already used. Caught by a fresh valgrind pass on the shipped agent, not by any test.

### Calibration

Deliberately lean (user decision, given Phase 5's role here was a sanity check before
Phase 6's real measurement, not the main deliverable): a minimal harness
(`aicalibsrc/ismcts/calib_ismcts_efficiency.c`, CLI-overridable on the four EFFICIENCY
dials) rather than the full scipy-differential-evolution pipeline other agents have. A
sweep over `search_exploration_constant`/`threshold_widening_k`/`_alpha`/
`search_expand_threshold` found no distinguishable effect at n=200-800/candidate once
checked across multiple seeds (an apparent `threshold_widening_k=4.0` win at one seed
collapsed to a loss on two others) -- shipped the principled UCT/progressive-widening
defaults unchanged (`search_exploration_constant=sqrt(2)`, `threshold_widening_k=2.0`,
`threshold_widening_alpha=0.5`, `search_expand_threshold=3`). The real calibration axis
for this agent turned out to be `limit_iterations` (finding 2, above), found via direct
measurement, not a parameter search.

**Final shipped values**: `limit_iterations=4000`, `limit_max_nodes=200000`,
`limit_playout_steps=200`, rollout policy = `AI_STRATEGY_HEURISTIC` (`A5`) on both seats
for attack/defense and mulligan/discard-to-7 alike, everything else at the Phase 2
principled defaults.

- **Measured rating: 69** vs `Borealis` (68.55%, n=10008, 95% CI 67.6-69.5%) -- the new
  roster ceiling, above `A7`'s 58 (post-fix) and `A5`'s 64.
- **63.2% head-to-head vs `A7`** (n=10008) -- a direct, decisive win over the previous
  ceiling agent, not just a transitive one via the shared `Borealis` anchor.
- Design-intent estimate was 92 -- another overshoot on this roster's now-established
  pattern (A4 62->36, A6 74->52, A8 82->35, A9 85->59), but this agent's actual, honestly
  measured 69 is still the highest number on the board.

**Disposition**: shipped as `AI_STRATEGY_ISMCTS`'s live registry entry (attack, defense,
mulligan, discard-to-7 all wired), `player_config.c`'s `AI_STRATEGY_RATINGS` updated to
`{ 69, true }`.

**Verification**: `make clean && make` (no new warnings), `./bin/oracle -a -p` byte-
identical to `bin/expectedresults.txt` at every stage, all 7 suites green (`test_combo`
20/20, `test_recall` 10/10, `test_cash_exchange` 6/6, `test_rating` 41/41,
`test_hbt2ply_reply` 6/6, `test_moves` 79/79, `test_ismcts` 36/36 -- the last two new/
extended for this agent's primitives, tree/UCT unit tests, and flat-rollout scoring),
valgrind clean (0 errors, 0 leaks) on the shipped configuration after the `stda_auto.c`
fix, `make format` applied, one interactive sanity-check game via `stda.cli` (a clean
6-turn win including a 3-champion combo for +9 bonus damage), menu now shows the measured
69 rather than the `~92` estimate.

## 2026-08-27 — A5/A7 defense PASS-dominance fix shipped: A5 improved, A7 didn't, both kept

Fixed the PASS-dominance defect `A9`'s build found in `A5`/`A7`'s shared defense formula
(see the 2026-08-26 entry below) in place, in both `best_defense_move()`
(`ai_strat_heuristic.c`) and `hbt_best_defense_move()` (`ai_strat_hbt_enum.c`): the
decline/PASS baseline now scores `max(own_energy - incoming, 0)` instead of the raw,
undamaged `own_energy`, matching what `A9`'s own local `hbt2ply_reply_defense_move()`
already did. Triggered by an `A10` IS-MCTS diagnostic (see that agent's own entry below)
rather than pursued standalone -- swapping `A10`'s rollout policy from
`AI_STRATEGY_RANDOM` to `A5`'s (then-unfixed) heuristic jumped its win rate vs
`Borealis` from ~46-48% to ~65-66%, which made fixing the underlying defect worth
doing now rather than deferring further.

Measured both seats, n=4000 games each, vs `Borealis`:

- **`A5` Heuristic: 60 → 64** (2567/4000 = 64.2%) -- clears the user's
  improve-or-don't-ship bar from the original deferral decision. Shipped outright.
- **`A7` Hybrid HBT: 62 → 58** (2301/4000 = 57.5%) -- fails that bar (worse, not
  better), despite sharing what was believed to be an "identical" formula with `A5`.
  **Shipped anyway**, a deliberate user decision to keep focus on `A10` right now,
  with two things preserved for the follow-up: a documented revert path (see the
  comment at each fix site, and `player_config.c`'s `AI_STRATEGY_RATINGS` comment), and
  an open task -- *why does the identical fix help `A5` but hurt `A7`* -- with
  re-optimizing `A7`'s `HBTParams` with the fix already in place (rather than
  reverting) as the leading hypothesis, since `A7`'s aggression/lethal-combo-hold
  layers on top of `A5`'s shared shape are the most likely interaction site.

`src/ui/shared/player_config.c`'s `AI_STRATEGY_RATINGS[]` updated to `{ 64, true }` /
`{ 58, true }` for `AI_STRATEGY_HEURISTIC`/`AI_STRATEGY_HYBRID_HBT`. No test hardcoded
the old buggy behavior, so nothing needed updating there; `./bin/oracle -a -p` stayed
byte-identical (the default matchup is `Random`-vs-`Random`, which never calls either
fixed function) and all seven suites (`test_combo` `test_recall` `test_cash_exchange`
`test_rating` `test_hbt2ply_reply` `test_moves` `test_ismcts`, 198 assertions) stayed
green.

## 2026-08-26 — A9 HBT 2-Ply ("Grandmaster II") implemented and calibrated: the next rung on the ladder, below its own design target, with a precisely isolated root cause

`A7` Hybrid HBT plus one opponent-response ply, per `ideas/A9 .../about.md`: for every
candidate champion subset, clone the position, commit the subset, simulate the
opponent's best reply against a deterministic public-information surrogate hand, and
blend that net-of-reply score against `A7`'s own undefended score.

- **New files**: `src/ai_strat/ai_strat_hbt2ply.{c,h}` (orchestration, params,
  calibration hooks -- same shape as `ai_strat_hbt.c`) and
  `src/ai_strat/ai_strat_hbt2ply_reply.{c,h}` (surrogate-hand construction and the
  two-ply scoring/enumeration loop, split out the same way `A7`'s own
  `ai_strat_hbt`/`ai_strat_hbt_enum` are).
- **The surrogate hand** (`build_surrogate_hand()`): sized to the opponent's real
  (public) hand size, composed from the public unseen pool (`fullDeck` minus the
  attacker's own hand/discards/combat zones -- the identical pool definition as `A8`'s
  `mc_determinize()`, duplicated locally rather than shared, since this agent
  deliberately takes no dependency on `A8`'s playout infrastructure and stays fully
  deterministic, no sampling) in the same champion/non-champion proportion as that
  pool. `surrogate_pessimism` windows which champions: 1.0 selects the strongest
  available blockers, 0.0 a hand of roughly median blocking strength. Found and fixed
  an aliasing hazard during unit testing: the function read the target hand size
  *after* zeroing the output hand, which is a real bug when a caller (as this agent's
  own scoring does) builds the surrogate directly into a gamestate clone's own
  `hand[opponent]` slot -- fixed by reading the size first.
- **Reused, not reimplemented, from `A7`**: `hbt_advantage()`, `predicted_damage()`,
  `predicted_block()`, and `is_held_combo()` were un-`static`'d in
  `ai_strat_hbt_enum.c`/`.h` specifically so this agent shares `A7`'s own scoring rather
  than drifting from it. `hbt_advantage()`'s own header comment now flags that it is
  NOT sign-symmetric (the resource-shortfall penalty is one-sided and `HBTState` must be
  derived for whichever player is being scored) -- the main correctness trap this
  agent's design had to route around.

### Finding 1: A7's own defense formula has a PASS-dominance property

Building this agent's reply oracle around `A7`'s own `hbt_best_defense_move()`
(the original design intent -- "the ranking layer `A7` already has, applied from the
other seat") surfaced a pre-existing property of that shipped, calibrated, measured
function (and `A5`'s identical `best_defense_move()`, which `A7`'s shape was copied
from): its PASS/decline baseline scores the *undamaged* `own_energy`, never charging
the incoming attack against declining. Worked through `hbt_advantage()`'s algebra: for
any `gamma_eff > 0` (always true under the shipped calibration, ~1.8-2.1), blocking
costs at least one hand card (weighted by `gamma_eff`) while the energy saved is
weighted only `eps_eff = 0.349` -- even a *perfect* block only ties PASS on the energy
term, then loses on hand/cash. PASS strictly dominates every blocking option,
unconditionally, at the shipped defaults. Confirmed two ways:
- **Proof**: the inequality above holds for any incoming attack, given the shipped
  weights.
- **Empirically**: `HBT`-vs-`HBT` and `Heuristic`-vs-`Heuristic` mirror matches both
  average **6 turns** to burn 99 energy (~16.5 dmg/turn), vs `Random`-vs-`Random`'s
  **26 turns** -- consistent with combat landing essentially undefended. A direct trace
  confirmed `hbt_best_defense_move()` chose PASS even against a 15.5-damage hit with
  cash and blockers available.

**Decision (user, staged)**: fix this agent's own reply-oracle only, leave `A7`'s (and
`A5`'s) actual shipped, measured code untouched -- fixing it in place would move both
agents' own measured ratings and is deferred to its own future task, conditioned on
`A5`/`A7` measuring *at least as strong* against `Borealis` afterward, not merely "more
realistic" (memory: `project_a5_a7_defense_pass_dominance`). This agent's reply oracle
is `hbt2ply_reply_defense_move()` (`ai_strat_hbt2ply_reply.c`): identical to `A7`'s
function in every other respect -- same enumeration, same `variance_aware_incoming()`,
same `evaluate_defense_subset()`, both reused verbatim -- except the PASS baseline
correctly charges `own_energy - incoming`.

### Finding 2: the two-ply scoring formula itself over-credited resource-trading

With the corrected reply oracle in place, calibration (`optimize` against `borealis`,
then validated head-to-head against `hbt`) produced a candidate that still *lost* to
`A7` (46.84%). Re-running `optimize` directly against `hbt` -- the actual success
criterion -- plateaued at the same ~47% regardless, with a genuine, non-degenerate
`reply_trust`. A controlled test (both sides given the corrected, actually-blocking
defense, only the attack-side logic varied) showed *why*: `A9`'s own attack lost 43.13%
to 56.87% against `A7`'s naive "assume no block" attack, even when its block prediction
was accurate.

Root cause: the two-ply score credited *two* things when a block was predicted --
reduced damage (`eps_eff ≈ 0.35`) and the opponent spending cards/cash to block
(`gamma_eff`/`delta_eff ≈ 1.8-2.1`, roughly 5x higher). The "made them spend a
resource" credit dominated the "actually reduced their energy" term, biasing the
two-ply ranking toward attacks that provoke a block over attacks that maximize raw
damage -- a resource-trading bias that loses to a straightforward damage race in this
game, even under ideal (accurate block prediction) conditions.

**Fix**: `hbt2ply_score_attack_subset()`'s `two_ply` no longer subtracts the reply's
`count`/cost from `opp_hand`/`opp_cash` -- those two arguments are now identical between
`one_ply` and `two_ply`; only the damage term differs. This makes `two_ply <= one_ply`
a **mathematical guarantee** (a block only ever reduces net damage, never turns
negative) rather than an accidentally-violated design intention. Re-validated in the
same controlled test: **49.55% vs 50.45%** -- from a clear loss to near-exact parity.
Re-optimizing against `hbt` after the fix still plateaued at 47.19% [46.70%, 47.68%]
(40,000 games) -- statistically indistinguishable from before the fix, confirming the
real ceiling: this agent's mechanism is sound (parity, under ideal conditions) but has
nothing real to correct for against `A7`'s actual (non-blocking) defense.

### Calibration

Staged per the plan: the 34 `HBTParams` fields inherited from `A7` are **hard-pinned**
(`PINNED_PARAM_NAMES` in `calibrate_hbt2ply.py`, no `--identity-safe` escape hatch --
this agent's framing is "`A7` plus one ply," not a fresh fit, so there is nothing of
`A7`'s own tuning to erode). Of the 4 new fields, only `reply_trust` and
`surrogate_pessimism` are real behaviour dials (`optimize`'s default free set);
`ply_energy_ceiling`/`ply_beam_width` are pure compute-budget dials, shipped at their
"no restriction" defaults (99/0) since this agent's own cost (~3200 games/sec measured
via the calibration harness) is close to `A7`'s own closed-form cost, nowhere near
`A8`'s ~100x rollout overhead -- no budget pressure to restrict them.

Shipped: `reply_trust = 0.10358593`, `surrogate_pessimism = 0.32276109` (both from the
post-fix `optimize --opponent hbt` run).

- **Measured: 59** vs `borealis` (20,000 games, both seats, direct pairwise -- same
  methodology as `A8`'s precedent, chosen for consistency: unlike `A8`, this agent's own
  cost is cheap, but a roster-wide `--stda.rating` fit was skipped anyway since `A8`/
  `A12` already make any full round-robin featuring them impractical, regardless of
  what else is added to it). A genuinely respectable Borealis-scale rating -- above the
  anchor, above `A6`'s 52 -- despite failing its own specific design target.
- **Measured: 47.2%** [46.7%, 47.7%] head-to-head vs `A7` (40,000 games) -- below the
  `>55%` target set for this agent, honestly reported (see Findings 1-2 above for why).
- `AI_STRATEGY_RATINGS[AI_STRATEGY_HBT_2PLY]` (`player_config.c`) set to `{ 59, true }`.

**Shipped as-is, not chased further this session.** Matches the `A4`/`A8`/`A12`
precedent of reporting a below-target result honestly. Fixing `A7`'s (and `A5`'s
identical) defense formula is believed a genuine prerequisite for a future re-attempt
at this agent's design target -- not optional cleanup -- deferred to its own task.

- **New calibration tooling**: `aicalibsrc/hbt2ply/` (`calib_hbt2ply.c`,
  `calibrate_hbt2ply.py`, `README.md`) following `aicalibsrc/hbt/`'s pattern, scoped
  down to this agent's actual 4-field search space.
- **Verification**: `make` clean (no new warnings) across the main binary and all six
  test suites (146 tests total); `-a -p` byte-identical to `bin/expectedresults.txt`
  throughout every stage of this work; valgrind-clean (0 errors, no leaks) over 5 real
  games via a smoke-test harness; `reply_trust = 0` proven to recover `A7`'s decision
  bit-for-bit (`testsrc/test_moves.c`'s `test_hbt2ply_reply_trust_zero_matches_a7`, the
  central regression guard); strategy menu confirmed showing
  `HBT 2-ply [Grandmaster II] (available) [59]`.

## 2026-08-25 — A12 Clairvoyant ("The Clairvoyant") implemented: A8's sibling, a cheap opponent-rollout heuristic instead of pure random

Not part of the `A1`-`A11` ladder's authoritative order (`A9` HBT 2-Ply remains next) --
a side exploration of A8's own diagnosed ceiling: `mc_playout()`'s rollouts model *both*
seats as `AI_STRATEGY_RANDOM` regardless of the real opponent, so its win-probability
estimates are calibrated against `rand` specifically (see this file's 2026-08-25 A8
entry). The question was whether a cheap, non-tree fix to just the *opponent's*
simulated moves could raise the rating without drifting toward `A9`/`A10`/`A11`'s own
territory (a tree, learned policy, or determinized multi-sample reasoning). Appended
as a new roster slot (`AI_STRATEGY_CLAIRVOYANT`, enum ordinal after `AI_STRATEGY_ISMCTS_NN`,
shorthand `clairvoy`) rather than inserted into the numbered ladder, so `A9`-`A11`'s own
ordinals didn't need to shift for what is explicitly a secondary experiment, not the
next rung.

- **Shared search infrastructure generalized** (`src/ai_strat/ai_strat_playout.{c,h}`,
  `ai_strat_simplemc_search.{c,h}`): `mc_playout()` and `mc_search_best_move()` now take
  the rollout `StrategySet` as a parameter instead of building a hardcoded
  random/random one internally. A8's own call site (`ai_strat_simplemc1.c`) is
  unaffected -- it just builds and passes the same random/random set explicitly now,
  verified behavior-identical (`-a -p` byte-identical to `bin/expectedresults.txt`
  throughout). `SIMPLEMC_DEFAULTS` moved from `ai_strat_simplemc1.c` into
  `ai_strat_simplemc1.h` so both agents' `.c` files can use it -- `SimpleMcParams`
  itself is reused as-is by `A12`, unmodified; the budget/pruning shape has no reason
  to differ between the two agents.
- **`src/ai_strat/ai_strat_clairvoyant1.{c,h}`** (new). Same progressive-pruning search
  as `A8`, same `SimpleMcParams`. The only difference: rollouts keep this agent's own
  future moves uniformly random (still "a simple MC approach, no tree") but give the
  *opponent's* simulated replies a cheap heuristic instead -- no move enumeration, no
  per-subset search, no call into `A5`'s or any other agent's real mechanism. A single
  fixed candidate (the top up to 3 affordable champions by `power`, a cheap O(n)
  partial selection) scored by one closed-form formula and committed only if it clears
  a threshold, else pass/decline.
- **Two real defects found by playtracing, not by the aggregate number alone** -- the
  first smoke test (500 games vs `borealis`, both seats) measured **26.6%**, clearly
  *below* A8's own 35.4%, the opposite of the intended direction. Playtracing (turn/
  energy-enriched trace lines added to the rollout policy itself, `CVRollout ATTACK`/
  `CVRollout DEFENSE`) found why:
  - **Attack score never weighed cost.** `expected_attack` is always positive, so
    `score > 0` was a near-tautology: 176,875/176,875 (100.0%) of sampled attack
    decisions committed. The simulated opponent wasn't deciding anything -- it threw
    every affordable champion into the fight on every turn, modeling a strawman
    always-all-in opponent rather than a sharper one. Fixed by subtracting
    `cost_weight * total_cost` from the score, using Borealis's own calibrated
    `luna_value`/lambda (4.5846, "damage-units per luna") as a first estimate, since
    it's exactly the same quantity. This alone did *not* move the aggregate number
    (still 26.6%, coincidentally identical to the pre-fix figure), but it did fix a
    genuine defect (post-fix commit rate ~76.7%, varying sensibly with cost) and
    shifted the seat balance a lot: 71/250 -> 87/250 as seat A, but 62/250 -> 46/250
    as seat B -- the two shifts happened to net to the same total.
  - **Defense didn't weigh cost at all either** (`defense_score >= incoming_attack`,
    no cost term). Applying the identical fix -- mirroring Borealis's own defense
    evaluation exactly (`ai_strat_borealis_enum.h`: "value(S) is capped at the incoming
    threat"), i.e. `net = min(raw_defense, incoming) - cost_weight * total_cost`, commit
    iff `net > 0` -- raised the rating to 30.6% [26.7, 34.8] and, as a side effect,
    resolved the seat asymmetry the attack fix alone hadn't: spread went from 16.4pp
    (34.8%/18.4%) down to 6.0pp (33.6%/27.6%), back in this project's normal range (A2's
    own seat gap is 6.3pp). No separate asymmetry investigation was needed -- it was
    downstream of the same missing-cost-term defect.
- **`cost_weight` sweep** (`clairvoyant_set_cost_weight()`/`_get_cost_weight()`, a
  runtime override rather than a compile-time constant specifically so it could be
  swept without rebuilding): 14 values from 0.5 to 10.0, n=700 games/point (both seats)
  vs `borealis`. A quadratic fit was genuinely unimodal (R^2=0.73), implying an optimum
  near 3.86; Borealis's own lambda (4.5846) measured near the *low* edge of a fairly
  flat 1-7 plateau (28.0%) rather than its center (~30-32%), and both extremes measured
  clearly worse (0.5: 26.3%; 10.0: 19.4%, non-overlapping with the plateau -- too
  conservative, declines far too often). Shipped at **3.0** (empirical peak, close to
  the fitted optimum), a real but modest gain over the borrowed value.
- **Final measured rating: 31** (n=1500, both seats, vs `borealis`, at `cost_weight=3.0`
  -- 31.47%, seat spread 4.5pp). Consistently a few points below A8's own 35 across
  every measurement in this investigation (26.6% -> 30.6% -> 31.3% (sweep's own n=700
  point) -> 31.47% (final n=1500)) -- close, but not a clear win. A sweep of the
  smaller `ROLLOUT_ENERGY_WEIGHT` term was considered and deliberately not run: it
  contributes at most ~5-20% of the magnitude of the terms that actually decide most
  commits (a single champion's `expected_attack` is 2.5-10; even one cheap champion's
  cost term at `cost_weight=3.0` is -3.0; the energy term tops out around 0.3-0.5 in
  observed traces), so a same-scale sweep was assessed at roughly 15-20% odds of a
  >2-3pp gain -- not worth chasing given `A9`-`A11` are expected to be materially
  stronger agents by design.
- **Shipped as-is, not chased further.** `AI_STRATEGY_RATINGS[AI_STRATEGY_CLAIRVOYANT]`
  (`player_config.c`) set to `{ 31, true }`. This agent stays in the roster, selectable
  from every mode, as a legitimate but deliberately modest sibling of `A8` -- explicitly
  accepted as a "fun, easy to beat" agent rather than one worth further investment.
- **Verification**: `make` clean (no new warnings) across the main binary and all four
  test suites; `-a -p` byte-identical to `bin/expectedresults.txt` at every stage
  (the shared-infrastructure generalization, both rollout-policy fixes, and the final
  `cost_weight` change); valgrind-clean on live `clairvoy` play; strategy menu confirmed
  showing `Clairvoyant [The Clairvoyant] (available) [31]`.

---

## 2026-08-25 — A8 Simple Monte Carlo ("The Soothsayer") implemented, calibrated, and its ceiling diagnosed

Implemented per `ideas/A8 ai agent simple monte carlo (the soothsayer)/about.md` and
the design-comment stub that used to occupy `ai_strat_simplemc1.c`. The first agent on
the ladder that actually simulates rather than scoring closed-form -- `A1`-`A7` all
avoided `clone_gamestate()`/`apply_move()` specifically because cloning-and-replaying a
turn draws from the shared `GameContext` RNG stream and perturbs every downstream game
(`ai_strat_hbt.h`/`ai_strat_heuristic.h`'s own stated reason). Building this agent
therefore required new shared engine infrastructure first, reusable by `A9`-`A11`:

- **`src/actions/game_move.{c,h}` -> `move_gen.{c,h}`, `move_apply.{c,h}`** (new).
  `GameMove`/`MoveType` (pass, 0-3 champion subsets, draw, recall, cash), and
  `get_available_moves()`: attack phase enumerates the full space (recall and cash
  capped via `MoveGenLimits` -- recall's `C(discard, choose_num)` is the real
  worst-case branching-factor culprit, per `ideas/A10 .../mcts_depth_strategy.md`);
  defense phase enumerates 0-3 champion subsets only, matching `defense_phase()`'s own
  contract. `apply_move()` dispatches to the existing `play_champion()`/
  `play_draw_card()`/`play_cash_card_interactive()` plus a new `play_recall_card()`
  (`card_actions.c`) -- the headless engine counterpart to the interactive-only recall
  path, which was previously reachable only through `UiIO`-coupled code.
- **`src/ai_strat/ai_strat_playout.{c,h}`** (new). `mc_fork_context()` gives a
  simulation its own `MTRand` stream (`GameContext` is `{MTRand rng; config_t* config;}`,
  the RNG stored by value, so forking is two lines and the live `ctx->rng` is provably
  untouched -- this is what actually resolves the A5/A7-documented objection).
  `mc_determinize()` re-deals the information hidden from an observer (opponent's hand
  and *both* players' remaining deck composition -- a player's own deck is genuinely
  unknown to them too, not just the opponent's, per `setup_game()`'s shared-shuffle
  design) while leaving the observer's own hand and both discards/combat zones (public)
  untouched; deliberately not reshuffle-aware (`A10`'s job). `mc_playout()` resumes
  mid-turn from an arbitrary root, applies the given move, mirrors
  `attack_phase()`/`defense_phase()`'s own transitions, then plays to a terminal
  win/loss/draw via uniformly-random play on both seats.
- **`src/ai_strat/ai_strat_simplemc_search.{c,h}`, `ai_strat_simplemc1.{c,h}`** (new).
  Progressive-pruning search: a 7-rollout seed round drops every 0-win candidate
  outright, then repeated rounds add 25 more rollouts per surviving candidate and drop
  any whose confidence-interval upper bound falls below the current leader's lower
  bound (normal approximation to the binomial, `z=1.96` -- the stub's own second idea),
  with the stub's *other* idea (fixed `N^(3/4)/N^(1/2)/N^(1/4)`, capped 30/10/4,
  100/300/700-simulation schedule) enforced as hard survivor ceilings layered on top,
  so the shipped agent matches `about.md`'s stated identity even where CI pruning alone
  would keep more candidates alive longer. Stops at one survivor, 1500 cumulative
  sims/candidate, or 25000 total rollouts, whichever comes first.
- **Determinization is IN scope**, resolving a contradiction in the original design
  material: `about.md`'s "deliberately out of scope" list named determinization as
  `A10`'s job, but the design-comment stub itself explicitly called for
  `clone_and_randomize_gamestate()` per simulation. The stub's own spec won; `about.md`
  is corrected. `A10`'s distinguishing contribution narrows to the tree itself plus
  *reshuffle-aware* determinization (`ideas/A11 .../local_training_plan.md`'s
  narrowing note).
- **`src/actions/move_gen.c`/`move_apply.c` folded into `ENGINE_SRCS`,
  `ai_strat_playout.c`/`ai_strat_simplemc*.c` into `AGENT_SRCS`** (`makefile`) --
  hoisted out of the seven-times-duplicated `CALIB_*_SRCS` lists first (570 -> 428
  lines), specifically so this agent's five new files became a one-line addition to
  each list instead of a seven-list edit, same motivation as `ai_strategy.c`'s registry
  itself.

**Calibration** (`aicalibsrc/simplemc/`, new: `calib_simplemc.c` + `calibrate_simplemc.py`
+ `README.md`). This agent's twenty parameters are a different shape than every prior
agent's: most control how much the agent *searches*, and more searching is basically
always at least as strong, just slower -- there is no interior optimum to tune for those
fields, only a cost/strength curve. `calibrate_simplemc.py` splits them into
`BUDGET_PARAMS` (7 fields, swept not optimized), `EFFICIENCY_PARAMS` (11 fields --
pruning aggressiveness and enumeration caps, the ones `optimize` actually searches by
default, since these trade real risk for real speed at a fixed budget), and two fields
neither (`rollout_determinize`, a binary A/B question; `rollout_max_turns`, inert).

**Playtracing three losses vs `borealis`** (seeds 100/106/111, turn/energy added to the
decision trace) found no implementation bug -- decisions are internally coherent, and
the agent's own win-probability estimate tracks the real game reasonably (e.g. correctly
turning pessimistic before a big incoming hit rather than after). What it found instead:
seed 111 shows the agent repeatedly choosing CASH/DRAW/RECALL over committing champions
early against a strategic opponent that kept attacking, going from 99 to 7 energy while
`borealis` barely moved off 99 -- a resource-building line that would pay off against a
patient/random continuation and gets punished by a real one. This traces to a design
fact, not a bug: `mc_playout()` sets *both* seats to `AI_STRATEGY_RANDOM` for every
rollout regardless of who the real opponent is (per the design stub's own spec, "randomly
... make moves 2+"), so every candidate move's win-probability estimate is "if the
opponent now plays randomly forever" -- accurate against `rand`, systematically biased
against a real strategic opponent that converts tempo into damage more reliably than a
random continuation predicts.

**Budget-vs-rating sweep** (both seats, 1500 games/point, vs `borealis`) confirms the
above is a bias problem, not a variance one: scaling every `BUDGET_PARAMS` field by
1.0x/1.75x/2.3x (shipped defaults through ~1.8x the rollout budget, measured at
~5.6ms/~8ms/~10ms average decision time under the project's actual `-Og` build) produced
statistically indistinguishable ratings -- 35.4% [33.0, 37.9], 38.2% [35.8, 40.7], 35.9%
[33.5, 38.3]. If the ceiling were a search-depth problem, more rollouts should reduce
noise and raise the estimate; a flat curve under 2.3x the budget is the expected
signature of an estimator with a systematic bias that more sampling can't correct, not
one that's merely under-searched.

**Shipped as-is, honestly rated at 35** -- this agent was never intended to be
competitive (the original design intent, restated directly when this result was
reviewed: a tool for probing the value of every legal action from curated positions, to
help mine new heuristics for future agents, not a strong opponent in its own right).
Measured rating (both seats, `borealis`, 1500 games at shipped defaults, `rollout_seed_
simulations=7`, `rollout_round_simulations=25`, ~5.6ms/decision): **35** -- below the
Borealis anchor and in the same tier as `A2`/`A4`, and *not* raised by extra rollout
budget per the sweep above. `AI_STRATEGY_RATINGS[AI_STRATEGY_SIMPLE_MC]`
(`player_config.c`) updated from `{ 82, false }` (design-intent estimate) to
`{ 35, true }`. A cheap (non-tree) heuristic rollout policy is the lever that could
plausibly raise this -- deliberately not pursued here, kept as a live design question
for whether/how to pick it up without drifting into `A9`/`A10`/`A11`'s territory.

**Verification**: `make` clean (no new warnings) across the main binary, all four test
suites, and all eight calibration harnesses; `-a -p` byte-identical to
`bin/expectedresults.txt`; `test_combo` (20/20), `test_recall` (10/10),
`test_cash_exchange` (6/6), `test_rating` (41/41), new `test_moves` (57/57, covering
`move_gen`/`move_apply`/`ai_strat_playout` -- card-conservation and no-card-duplication
invariants for `mc_determinize()`, and byte-identical parent-`ctx->rng` invariants for
both `mc_fork_context()` alone and a full `mc_playout()`) all pass; valgrind-clean on
`test_moves` and on live `simplemc` play; strategy menu confirmed showing
`Simple Monte Carlo [The Soothsayer] (available) [35]`.

---

## 2026-08-25 — A7 Hybrid HBT ("The Grandmaster") implemented and calibrated

Implemented per `ideas/A7 ai agent hybrid hbt (the grandmaster)/about.md` and
`hbt_design_notes.md`/`strat_hbt_sketch.h` (a full LLM-drafted design sketch,
not a hand-written stub). A fixed three-layer synthesis of the three agents
below it on the ladder -- `A4` Balanced Rules, `A6` Tactical, `A5` Heuristic --
plus `A3` Borealis's lethal-combo hold, added deliberately: the requesting brief
for this agent explicitly called for "combo awareness to a good extent" beyond
what `A4`/`A5`/`A6` alone provide, on the grounds that a hybrid of three
combo-blind-or-implicit agents would "feel dumb" next to Borealis in human play.
`about.md`'s own "any mechanism not already present in A4/A5/A6 is out of scope"
line is updated to note this one deliberate exception.

- **`src/ai_strat/ai_strat_hbt.{c,h}`, `ai_strat_hbt_enum.{c,h}`,
  `ai_strat_hbt_cards.c`** (new). Move generation and the advantage function are
  `A5`'s verbatim mechanism (closed-form 1-move lookahead, argmax over
  {pass, every affordable 1-3 champion subset, every affordable draw card, every
  affordable cash card} -- no `clone_gamestate()`, the same RNG-perturbation
  constraint `A5`'s own changelog entry established). Two deliberate deviations
  from the design docs' literal text, both recorded in `ai_strat_hbt.h`'s header
  comment:
  - **`A4` enters as a soft penalty on the advantage function, not a hard
    filter**, despite `about.md`'s own wording ("`A4` Balanced Rules filters
    viable moves by resource constraints"). `A4`'s own `BALANCED_DEFAULTS`
    comment documents why a hard filter is risky: a traced game under the
    original spec-derived slope showed 4 of 5 early turns passing outright
    because the filter left no legal move, and `A4` is the weakest of the three
    source agents (rating 36 vs `A5`'s 60 and `A6`'s 52). A penalty
    (`penalty_cash_weight`/`penalty_cards_weight` times the shortfall below
    `A4`'s own resource targets) can shape the ranking without ever deleting a
    move the ranking would otherwise pick.
  - **Corrected sign versus `ideas/G1 .../balanced_tactical_hbt_comparison.md`'s
    sketch**, which scales `target_cash` by `(1 + (aggression - 0.5) * 0.4)` --
    higher aggression means a *larger* cash reserve target, i.e. spending *less*
    while aggressive. That contradicts `A4`'s own `late_game_aggro` (divides
    targets *down*, spending *more*, as a kill nears) and the same design doc's
    own prose two paragraphs later ("delta decreases as opponent energy drops --
    spend cash to finish them"). Shipped with the opposite sign; a correction
    note was added to that file.
  - `A6`'s phase/aggression formula is ported verbatim and modulates `A5`'s
    weights instead of gating an attacker count (`A5`'s enumerator already
    considers every subset size, so there is no count decision to gate):
    `eps_eff = eps*(1 + aggr_energy_gain*2*(a-0.5))`,
    `gamma_eff/delta_eff = gamma/delta*(1 - aggr_resource_fade*2*(a-0.5))`, with
    an extra `critical_epsilon_mult` multiplier when the opponent is
    `PHASE_CRITICAL` (`hbt_design_notes.md`'s own "epsilon increases 50% in
    critical phases," generalised from a step on `opp_energy` to a step on the
    richer aggression scalar).
  - `A3`'s `is_held_combo()` is ported verbatim (attack only) to exclude a
    combo-scoring subset from consideration when it's not yet lethal and the
    opponent isn't near-death -- `A5`'s own enumeration already scores the
    *realized* combo bonus for every subset, so this is the one piece of
    combo-awareness `A5` cannot supply on its own (recognising a combo worth
    holding, not just scoring one already committed to).
  - `hbt_mulligan()`/`hbt_discard_to_7()` are a **local port** of `A3`'s
    combo-protecting mulligan/discard shape, not a call into `A3`'s own
    functions -- calling `A3`'s directly would read Borealis's own
    calibratable `g_params[player]`, coupling this agent's discard behaviour to
    Borealis's tuning and breaking under `calib_hbt --opponent borealis`.
  - Defense unifies `A4`'s deflating `E[Attack] - beta*sigma` cap and `A6`'s
    inflating `+mult*sigma` estimate into one signed `defense_stdev_mult` dial
    (negative reproduces `A4`, positive reproduces `A6`, zero recovers `A5`'s
    plain expected value), scored by `A5`'s full 0-3 subset enumeration rather
    than `A6`'s cheaper prefix walk.

- **Calibration** (`aicalibsrc/hbt/`, new: `calib_hbt.c` + `calibrate_hbt.py` +
  `README.md`, `sweep`/`optimize`/`selfplay`/`validate` parity with
  `aicalibsrc/tactical/`). Thirty-four parameters -- roughly double `A6`'s
  sixteen (the previous largest space) -- so calibration was staged rather than
  run as one unconstrained search, and `weight_cash_advantage` (`A5`'s own
  scale-invariance pin) and `hold_lethal_combos` (a fixed design decision, not a
  dial to search away) are pinned in every stage.
  - **Stage 1** froze every `A3`/`A4`/`A5`/`A6`-sourced field at its *source
    agent's own shipped default* and freed only the eight parameters new to this
    agent (the T->H and B->H coupling terms, the two target-aggression scales,
    `defense_stdev_mult`) against `borealis`. `check_personality_flags()` (a
    three-part check -- T's aggression range, B's penalty weights and target
    slopes not eroding toward `BOUNDS`' own non-zero floors, H's epsilon/delta
    ratio and taper -- since `about.md`'s framing is "the personality *is* the
    fixed three-layer synthesis," all three must still be doing work) reported
    **no flags**. Validated: **60.96%** [60.48%, 61.43%] vs `borealis` (40,000
    games) -- already above `A5`'s own 58.99%, using none of `A5`'s tuned
    weights, only its shape.
  - **Stage 2** additionally freed `A5`'s own four non-pinned weights (twelve
    free total), re-deriving them jointly. Result was statistically
    indistinguishable (61.36% [60.88%, 61.83%], 40,000 games; a direct
    stage-1-vs-stage-2 head-to-head measured 49.11% [48.56%, 49.66%], a tie
    within noise) at the cost of `weight_cards_advantage` drifting to 10.72
    (`A5`'s own shipped value is 1.96) and `opp_card_discount` to 2.75 (near its
    3.0 search ceiling) -- no measurable gain for abandoning "H ranks with
    `A5`'s own tuned advantage function," so **stage 1 shipped**. Stage 3
    (jointly freeing `A6`'s twelve aggression/phase fields too) was skipped:
    stage 1 already clears all three source agents without touching their own
    tuned values, and `about.md`'s framing is a synthesis of three
    *already-calibrated* agents, not a fourth from-scratch fit.
  - **Shipped**: every H/T/B/combo-hold field at its source agent's own value;
    new fields `aggr_energy_gain=0.16112`, `aggr_resource_fade=0.07171`,
    `critical_epsilon_mult=2.46429`, `target_aggr_cash_scale=0.09639`,
    `target_aggr_cards_scale=0.20157`, `penalty_cash_weight=0.74932`,
    `penalty_cards_weight=0.86105`, `defense_stdev_mult=0.71117`. Measured
    (both seats): vs `borealis` 60.96% (validated, 40,000 games); vs `balanced`
    77.6% (18,000 games); vs `tactical` 60.9% (18,000 games); vs `heuristic`
    **26.0%** (40,000 games) -- a clear loss to the very agent whose ranking
    layer this agent reuses. Turn-count histograms (`-sa -p`, avg 7.3 turns,
    max 14) rule out a stall/hoarding bug: both sides finish games fast and
    decisively. Read as: `B`'s penalty and `T`'s weight modulation perturb
    `A5`'s advantage function away from `A5`'s own unperturbed optimum, and
    that perturbation costs more against an opponent using the exact
    unperturbed mechanism than it gains elsewhere -- not tuned away, per this
    project's honesty policy on measured results (`A4`'s 36, `A6`'s 39.30% vs
    `heuristic` are the precedents for reporting a real weakness rather than
    hiding it).

- **Rating**: despite the pairwise loss to `heuristic` above,
  `./bin/oracle -r -p --rating.games=2000`'s roster-wide Bradley-Terry fit
  places `The Grandmaster` at **rating 62** -- higher than all three of its
  ingredients in this same fit (`Eps-Gam-Del` 61, `Pressure Cooker` 53,
  `Bean Counter` 34) and above the Borealis anchor (50), the same way `A6`'s own
  pairwise loss to `heuristic` (39.30%) didn't stop it from rating above `A4`.
  This satisfies the design goal stated for this agent: a better Borealis
  rating than any of the three agents it combines.
  `AI_STRATEGY_RATINGS[AI_STRATEGY_HYBRID_HBT]` (`player_config.c`) updated
  from `{ 78, false }` (design-intent estimate) to `{ 62, true }`.

- **Verification**: `make` clean (no new warnings); `-a -p` byte-identical to
  `bin/expectedresults.txt`; `test_combo` (20/20), `test_recall` (10/10),
  `test_cash_exchange` (6/6), `test_rating` (41/41) all pass; valgrind-clean
  (`--leak-check=full --track-origins=yes`, `hbt` vs `hbt` and `hbt` vs
  `borealis`, 20 games each); strategy menu confirmed showing
  `Hybrid (HBT) [The Grandmaster] (available) [62]`.

---

## 2026-08-25 — A6 Tactical ("Pressure Cooker") implemented and calibrated

Implemented per `ideas/A6 ai agent tactical (pressure cooker)/about.md` and
`tactical_design_notes.md` (a full but partially-unfinished code sketch -- unlike
`A5`, no comment-only stub source file existed beforehand). Classifies the game into
a phase (early/mid/late/critical, by energy thresholds) and derives a single
0.0-1.0 aggression factor from energy difference, hand power, and cash surplus,
then scales how many champions to commit by that factor -- "turns up the heat as
the position sharpens." The first agent whose decision rule is a phase-classified,
continuously-adaptive weighting rather than a fixed advantage function (`A5`),
resource-target formulas (`A4`), or subset-value maximisation (`A3`).

- **`src/ai_strat/ai_strat_tactical.c/.h`** (new). Two design-sketch gaps filled
  in rather than left ambiguous: (1) the sketch's `GamePhase` thresholds (75/40/15)
  and its aggression "smell blood" cutoffs (independently 20/40) were two separate
  step functions over the same energy axis -- unified onto one tunable 3-threshold
  set shared by both, so "classify into a phase, then read the position" is one
  coherent mechanism; (2) the sketch calls `decide_num_attackers()` but never
  implements it -- filled in as aggression-scaled champion count (see the bug
  below for the exact mechanism). Card selection is greedy combo-aware ranking
  (A4's `attack_selection_score()`/`select_attack_champions()` shape), not `A3`'s
  exhaustive enumeration -- `about.md` doesn't forbid enumeration for `A6`, but
  this keeps the complexity budget on the phase/aggression mechanism, this
  agent's actual identity. Unlike `A4`, combo-awareness is unconditionally on (no
  `combo_weight` field), matching the sketch's "always evaluate 2-3 card
  combinations." No resource-target formula (`A4`'s explicit exclusion for this
  agent) -- cash/hand-size decisions flow from the phase-and-aggression-derived
  attack count and a reused `try_play_draw_card()` trigger. Defense is a
  standalone EV comparison independent of `aggression_factor` (the sketch's own
  defense function never references it), walking the greedy defense-efficiency
  ranking's prefixes of length 0-3 and keeping whichever maximises
  `value = -(damage*defense_damage_weight + cost*defense_cash_weight)` against a
  *conservative* (inflated, not capped) attack estimate -- the opposite sign from
  `A4`'s `E[Attack] - beta*sigma` cap. Two helpers (`champion_variance()`,
  `effective_hand_and_cash()`) were promoted from `ai_strat_balanced_rules.c` into
  `ai_strat_common.{c,h}` for reuse, following the project's established
  "promote on second use" convention (`try_play_cash_fallback`'s history) --
  verified behavior-preserving (`-a -p` byte-identical before and after).

- **A real bug found by playtracing, not just poor calibration** (the same
  discovery method that found `A4`'s cash-ladder trap): the original
  `decide_num_attackers()` fill-in, `num_attackers = round(aggression_factor *
  min(3, affordable_champion_count))`, put `max_playable=1`'s *sole* decision
  boundary exactly on `aggression_factor`'s neutral baseline (0.5). Since routine
  negative signals (e.g. the hand-power penalty) push aggression just below 0.5
  often, the agent was passing on its only affordable champion far more often
  than intended -- measured **losing to Random** (41.7%/39.15% both seats), the
  only implemented agent ever to do so. No calibration of the *weights* could
  have fixed this: it's structural to `round()` landing exactly on the formula's
  resting point, not a magnitude problem. Fixed by switching to four fixed
  aggression bands (>=0.75 -> 3, >=0.5 -> 2, >=0.25 -> 1, else -> 0, capped at
  `max_playable`) -- the neutral baseline now lands in the ">=0.25" tier instead
  of exactly on a boundary. Confirmed the fix: 76.2%/77.65% vs `rand` (both
  seats), in line with every other implemented agent.

- **Calibration** (`aicalibsrc/tactical/`, new: `calib_tactical.c` +
  `calibrate_tactical.py` + `README.md`, `sweep`/`optimize`/`selfplay`/`validate`
  parity with `aicalibsrc/heuristic/`, including its `--print-defaults`/`**tags`
  discipline). Sixteen free parameters -- the largest search space of any agent
  calibrated so far (`A4`'s ten was the previous max) -- targeted at `borealis`
  (the rating-50 anchor). A univariate sweep at spec defaults first showed every
  individual parameter's effect was small and mostly flat (win rate stuck around
  12-15% vs `borealis`, a floor effect); the real gains came from the joint
  search, which finished in ~20 seconds despite the dimensionality.
  - This agent's identity check is not a per-parameter ratio test like `A4`'s/
    `A5`'s: `about.md` names the exact failure mode ("a static version of this
    agent is a worse Heuristic, not a Tactical agent"), so
    `check_personality_flags()` computes `aggression_factor` across a battery of
    synthetic positions spanning the input space and flags if its range
    collapses (i.e. the position stops mattering), plus an inverted
    phase-threshold ordering check.
  - The first unconstrained `optimize` run (all sixteen free, `--opponent
    borealis`) came back with **no personality flags** -- phase ordering stayed
    intact (18 < 41 < 67) and the aggression range stayed well above the
    collapse threshold -- so, unlike `A4` (two free runs eroded its resource
    slopes) and `A5` (one free run needed playtracing to confirm a large weight
    was legitimate, not degenerate), no `--identity-safe` run was needed here;
    the first result shipped directly.
  - `defense_damage_weight` landed very small (0.042) relative to
    `defense_cash_weight` (1.623) -- declining a block became comparatively
    cheap. Playtracing (turn-by-turn energy deltas, `tactical` vs `borealis`)
    confirmed a real risk-tolerant defensive posture, not a "never defend"
    degenerate pattern -- both sides traded damage in a visibly competitive
    exchange, consistent with the near-50/50 measured result.
  - **Shipped** (`TACTICAL_DEFAULTS`): `phase_mid_threshold=67`,
    `phase_late_threshold=41`, `phase_critical_threshold=18`,
    `aggression_energy_diff_weight=0.0008`, `aggression_opp_late_bonus=0.126`,
    `aggression_opp_critical_bonus=0.282`, `aggression_self_late_penalty=0.053`,
    `aggression_self_critical_penalty=0.148`, `aggression_hand_power_bonus=0.248`,
    `aggression_hand_power_penalty=0.154`, `aggression_cash_surplus_threshold=10`,
    `aggression_cash_surplus_bonus=0.230`, `defense_damage_weight=0.042`,
    `defense_cash_weight=1.623`, `defense_conservative_stdev_mult=1.233`,
    `draw_min_hand_size=5`. Measured (validated, both seats): vs `borealis` ->
    **53.56%** [53.07%, 54.05%] (40,000 games); vs `rand` -> 98.68% [98.50%,
    98.83%] (18,000 games); vs `value` -> 77.81% [77.20%, 78.41%] (18,000
    games); vs `combo` -> 70.19% [69.52%, 70.85%] (18,000 games); vs `balanced`
    -> 70.43% [69.76%, 71.10%] (18,000 games); vs `heuristic` -> 39.30% [38.59%,
    40.02%] (18,000 games) -- the one agent this measures below, consistent
    with `heuristic` being the highest-rated agent so far.

- **Rating**: the design-intent estimate of 74 (`ideas/G1 .../
  oracle_ai_agent_names.md`) is exceeded -- `./bin/oracle -r -p
  --rating.games=2000`'s roster-wide Bradley-Terry fit places `Pressure Cooker`
  at **rating 52**, above the Borealis anchor (50) -- the second agent, after
  `A5` (61 in this same run), to measure above the anchor.
  `AI_STRATEGY_RATINGS[AI_STRATEGY_TACTICAL]` (`player_config.c`) updated from
  `{ 74, false }` to `{ 52, true }`.

- **Verification**: `make` clean (no new warnings); `-a -p` byte-identical to
  `bin/expectedresults.txt`; `test_combo` (20/20), `test_recall` (10/10),
  `test_cash_exchange` (6/6), `test_rating` (41/41) all pass; valgrind-clean
  (`--leak-check=full --track-origins=yes`, `tactical` vs `tactical`, 20 games);
  strategy menu confirmed showing `Tactical [Pressure Cooker] (available) [52]`.

---

## 2026-08-25 — A5 Heuristic ("Eps-Gam-Del") implemented and calibrated

Implemented per `ideas/A5 ai agent heuristic (eps-gam-del)/about.md` and
`src/ai_strat/ai_strat_heuristic1.c`'s original design-comment stub (now deleted,
its prose preserved in `ai_strat_heuristic.h`'s header comment and in `about.md`):
reduces the whole position to one weighted advantage function,
`Advantage = epsilon*EnergyAdv + taper*gamma*CardsAdv + taper*delta*CashAdv`, and
picks the legal move (1-move lookahead, no opponent-response simulation) that
maximises it after being applied. The first agent in the roster whose decision rule
is a tunable weighted sum over whole-position features rather than per-card scoring
(`A1`), threshold gates (`A2`), subset-value maximisation (`A3`), or resource-target
formulas (`A4`) -- every sibling agent's `about.md` explicitly cedes this mechanism
to `A5`.

- **`src/ai_strat/ai_strat_heuristic.c/.h`** (new; replaces the 26-line comment-only
  stub `ai_strat_heuristic1.c`, deleted). Move evaluation is closed-form, not
  clone-and-apply: the design docs' `clone_gamestate()`/`apply_move()`/
  `simulate_expected_defense_response()` sketch (`ideas/G1 .../
  balanced_tactical_hbt_comparison.md`) was not built, since applying a move to a
  clone would pull `draw_1_card()` from the shared `GameContext` RNG stream and
  perturb every downstream game. Cash and hand-size deltas are computed exactly
  from `card_actions.c`'s actual play functions; only the opponent's post-move
  energy is a prediction, `min(opp_energy, Sigma(expected_attack)+combo_bonus)` --
  deliberately no opponent-block term (a constant-fraction block model is a
  positive rescaling of the attack term, already absorbed into epsilon, so it
  would be a redundant, unidentifiable parameter). Reuses
  `build_affordable_champions()`, `expected_incoming_attack()`,
  `combo_bonus_for_selection()` from `ai_strat_common.h`; enumeration is the same
  3-nested-loop 0-3 champion subset pattern as `A3`'s `collect_candidates()`, with
  pass/draw-card/cash-card candidates scored by the same advantage function. No
  mulligan/discard override -- both `StrategySet` hooks stay `NULL` (shared
  default), same as `A1`/`A2`/`A4`.

- **Two `about.md`-vs-design-docs tensions resolved during implementation, not left
  ambiguous:**
  1. `about.md` excludes "dynamic/adaptive weights" as `A6` Tactical's territory,
     but the stub, the G1 sketch, and `ideas/G2 .../ai_params_guide.md` all call
     for gamma/delta to taper with opponent energy. Read as: `about.md`'s exclusion
     targets `A6`'s game-phase *state machine*, not a smooth function of one public
     scalar -- shipped as a single `weight_taper_exponent` dial (0.0 recovers
     strictly fixed weights; 1.0 reproduces the G1 sketch's linear
     `opp_energy/99`).
  2. `about.md` lists "subset enumeration ... as primary logic" out of scope, but
     that targets `A3`'s *decision rule* (maximise raw subset value); here
     enumeration is only the move generator the stub itself demands ("among all
     the possible moves") -- the decision rule is the weighted sum.
  The stub's further proposal of a hand-power / probability-weighted combo-potential
  term was deferred entirely (tracked in `doc/oracle_todo.md` as a follow-up), since
  `about.md` calls it an open question and out of scope as primary logic.

- **Calibration** (`aicalibsrc/heuristic/`, new: `calib_heuristic.c` +
  `calibrate_heuristic.py` + `README.md`, `sweep`/`optimize`/`selfplay`/`validate`
  parity with `aicalibsrc/balanced/`, including its `--print-defaults`/`**tags`
  discipline so this driver's `DEFAULTS` cannot drift from the shipped C constants).
  `weight_cash_advantage` (delta) is pinned throughout: the argmax of a three-term
  weighted sum is invariant to a positive rescaling of all three weights, so one is
  redundant (`ideas/G2 .../calibration_example.txt` reaches the same conclusion).
  `doc/oracle_todo.md`'s original "calibrate against `A4` Balanced Rules" note was
  superseded before running any search -- `A4` itself measured rating 36, below the
  anchor, so `borealis` (the rating-50 anchor) was used instead, with
  `balanced`/`combo`/`value`/`rand` as cross-checks.
  - A manual univariate sweep of `weight_cards_advantage` (gamma) at spec defaults
    otherwise found a far larger useful range than the spec's own 0.15: win rate vs
    `borealis` climbed from 26.8% at gamma=1 to a peak of ~48.7% around gamma=6-8
    before collapsing to 19.1% at gamma=12 (clear unimodal shape) -- `BOUNDS` was
    widened to 15.0 before searching, the same lesson `A3`'s `luna_value` bound and
    `A4`'s `target_cash_slope` bound each record.
  - An unconstrained `optimize` run (all four free params, `--opponent borealis`)
    found `weight_cards_advantage=9.815` -- more than 60x the spec's illustrative
    0.15 -- at 59.67% [59.18%, 60.14%] validated vs `borealis` (40,000 games),
    flagged by `check_personality_flags()` the same "measured stronger by eroding
    character is not automatically a win" protocol as `A2`'s rejected
    `aggression_level=2.21` and `A4`'s rejected free-search slopes. Unlike those
    precedents, playtracing this candidate (turn-count histograms via `-sa -p`, not
    just the aggregate rate) ruled out the failure mode the flag exists to catch:
    it still finishes nearly every game in under 20 turns and wins 99.8% vs `rand`
    -- a fast, decisive strategy, not a "hoard forever" stall. A large gamma
    changes which moves this agent's one weighted-sum mechanism prefers; unlike
    `A2`/`A4`'s rejected extremes, it does not disable an explicit rule the
    agent's identity depends on -- `about.md`'s own statement of this agent's
    identity is "its entire identity is its three weights," so this is a
    legitimate calibration finding, not erosion.
  - `optimize --identity-safe` (gamma capped at 2.0, epsilon kept away from 0,
    taper capped at 2.0) was still run as the character-preserving comparison per
    protocol: it converged to `weight_cards_advantage=1.962` (pinned against its
    own ceiling) at a statistically indistinguishable 58.99% [58.51%, 59.47%] vs
    `borealis`. A 3-way self-play round-robin (defaults vs both candidates, 48,000
    games/pairing) confirmed the two are inseparable (Bradley-Terry strength
    1.1294 vs 1.1098) and both far ahead of the spec defaults (0.0) -- the
    identity-safe candidate shipped: statistically the same measured strength,
    every weight closer to the stub's own illustrative numbers.
  - **Shipped** (`HEURISTIC_DEFAULTS`): `weight_energy_advantage=0.34929208`,
    `weight_cards_advantage=1.96227051`, `weight_cash_advantage=1.0` (pinned),
    `weight_taper_exponent=0.10115113`, `opp_card_discount=0.98660043`. Measured
    (validated, both seats): vs `borealis` 0% (uncalibrated design-comment
    illustration) -> **58.99%** [58.51%, 59.47%] (40,000 games); vs `rand` ->
    99.85% [99.78%, 99.90%] (18,000 games); vs `combo` -> 77.09% [76.47%, 77.70%]
    (18,000 games); vs `value` -> 81.81% [81.24%, 82.37%] (18,000 games); vs
    `balanced` -> 74.24% [73.59%, 74.87%] (18,000 games).

- **Rating**: the design-intent estimate of 70 (`ideas/G1 .../
  oracle_ai_agent_names.md`, `~70` in the menu) is exceeded -- `./bin/oracle -r -p
  --rating.games=2000`'s roster-wide Bradley-Terry fit places `Eps-Gam-Del` at
  **rating 60**, above the Borealis anchor (50) and above every other implemented
  agent (`Bean Counter` 36, `The Showboat` 30, `The Apprentice` 25, `The Gambler`
  2) -- the first agent in the roster to measure stronger than the anchor,
  consistent with the direct pairwise vs-`borealis` measurement above. This was not
  assumed going in -- `A4` measured well below its own design-intent estimate the
  session before, and per this project's stated calibration policy the job is to
  measure honestly, not to force a particular outcome either direction.
  `AI_STRATEGY_RATINGS[AI_STRATEGY_HEURISTIC]` (`player_config.c`) updated from
  `{ 70, false }` to `{ 60, true }`.

- **Verification**: `make` clean (no new warnings); `-a -p` byte-identical to
  `bin/expectedresults.txt`; `test_combo` (20/20), `test_recall` (10/10),
  `test_cash_exchange` (6/6), `test_rating` (41/41) all pass; valgrind-clean
  (`--leak-check=full --track-origins=yes`, `heuristic` vs `heuristic`, 20 games);
  strategy menu confirmed showing `Heuristic [Eps-Gam-Del] (available) [60]`.

---

## 2026-08-24 — A4 Balanced Rules ("Bean Counter") implemented and calibrated

Implemented per `ideas/A4 ai agent balanced rules (bean counter)/about.md` and its
only written spec, `src/ai_strat/ai_strat_balancedrules1.c`'s design-comment stub
(now deleted -- its prose lives on in `ai_strat_balanced_rules.h`'s header comment
and in `about.md`): a closed-form, no-search agent that derives a target cash
reserve and target effective hand size from the opponent's current energy (linear
formulas), spends/holds to hit those targets, and defends by a variance-aware rule,
`E[Total Def] <= E[Total Attack] - beta*sigma`, that can rationally decline a block
outright rather than always countering. Deliberately combo-blind on card selection
(`combo_weight` ships at `0.0`) -- combo scoring as a primary signal belongs to
`A2`/`A3`, per `about.md`'s explicit scope boundary.

- **`src/ai_strat/ai_strat_balanced_rules.c/.h`** (new; replaces the 120-line
  comment-only stub `ai_strat_balancedrules1.c`, deleted). Reuses
  `build_affordable_champions()`, `expected_incoming_attack()`,
  `combo_bonus_for_selection()` from `ai_strat_common.h`. `try_play_cash_fallback()`
  (previously `A3`-only, `static` in `ai_strat_borealis.c`) was promoted into
  `ai_strat_common.{c,h}` as a small preparatory refactor (verified
  behavior-preserving: `-a -p` byte-identical to `bin/expectedresults.txt` both
  before and after, since neither Random-vs-Random path touches either agent) --
  `A4` needed the identical "no champion affordable but a cash card is" fallback,
  and duplicating it a third time wasn't worth it. No mulligan/discard-to-7
  override: unlike `A3`, the shared `strat_lib_discard_to_7()` only ever discards
  *champions* by lowest `power`, which doesn't conflict with this agent's
  draw-card-based effective-hand-size accounting, so both `StrategySet` hooks stay
  `NULL` (shared default).

- **Two corrections to the design docs, found while implementing, not just while
  calibrating:**
  1. `ideas/G1 .../balanced_tactical_hbt_comparison.md` and
     `ideas/G2 .../ai_params_guide.md` both state
     `target_cash = (opp_energy-8)*19/91 + 8` and
     `target_cards = (opp_energy-8)*5/91 + 3`, but the stub's own numeric tables
     (cash 19->0, cards 5->0 as energy goes 99->8) fit `slope*(E-8)` with
     **intercept 0** exactly at every tabulated point -- the `+8`/`+3` were a
     misreading of the stub's inverse form
     (`Enemy Energy = 91/19 * cash + 8`). Shipped with intercept `0.0` (now a
     tunable field, since calibration below moved it off zero anyway).
  2. The `19`-luna cash-ladder ceiling is itself a fossil of an obsolete rule
     set -- starting cash is `INITIAL_CASH_DEFAULT = 30` today
     (`game_constants.h`), not 19. The initial implementation re-anchored the
     slope to `INITIAL_CASH_DEFAULT/91 (~0.3297)`, preserving the original
     *shape* (hold the full starting stack while the opponent is healthy, spend
     down to zero near a kill) at today's actual starting cash. This turned out
     to be a genuine bug in the shape, not just an untuned guess -- see below.

- **A real bug found by playtracing, not just poor calibration**: at
  `target_cash_slope = INITIAL_CASH_DEFAULT/91`, the cash surplus
  (`effective_cash - target_cash`) is `~0` by construction at full opponent
  energy (both start at exactly 30), and since `target_cash` tracks the
  *opponent's* energy (which barely falls while this agent is too cash-starved to
  attack with), the agent gets stuck unable to spend for many consecutive turns. A
  single traced game (`balanced` vs `value`, fixed seed) showed the agent passing
  outright on 4 of its first 5 turns despite holding playable champions in hand at
  every one of them -- not a selection-logic defect (confirmed by direct
  instrumentation: `build_affordable_champions()`/`select_attack_champions()`
  correctly returned zero *candidates*, not zero despite candidates existing), but
  the resource-target formula itself producing a self-reinforcing "cannot
  bootstrap" trap. Measured at the initial re-anchored default: `balanced` lost to
  `value` (a *weaker* rung, measured rating 24-26) ~14-20% of the time in both
  seats -- confirmed via a `target_cash_slope` sweep vs `borealis`
  (0.0 -> 25.1%, 0.05 -> 26.1%, 0.10 -> 25.75%, 0.20 -> 21.6%, 0.33 (the
  re-anchored default) -> 5.3%, 0.45 -> 0.06%, 0.6 -> 0.0%, all at 1600
  games/value) that shows the cliff directly.

- **Calibration** (`aicalibsrc/balanced/`, new: `calib_balanced.c` +
  `calibrate_balanced.py` + `README.md`, `sweep`/`optimize`/`selfplay`/`validate`
  parity with `aicalibsrc/borealis/`). `calib_balanced.c` adds a
  `--print-defaults` mode (dumps the compiled `BALANCED_DEFAULTS` as JSON) that
  the three earlier harnesses lack; `calibrate_balanced.py`'s own `DEFAULTS` dict
  is read from it at import time rather than hand-copied, so it structurally
  cannot drift from the shipped C constants the way the other three drivers'
  copies already have (`doc/oracle_todo.md`).
  - Two free `optimize` runs (all ten parameters, vs `combo`) independently
    drove `target_cash_slope`/`target_cards_slope` toward `0` (spend everything,
    ignore opponent energy entirely) and `defense_beta` past `2.0` (rarely
    defend) -- each measuring *stronger* (up to 70.7% vs `combo`) but eroding
    exactly the traits that make this agent "Bean Counter" rather than a worse
    `A2`/`A3`. Flagged by a new `check_personality_flags()` (slope-degeneracy and
    `defense_beta`-band checks, plus a `combo_weight > 0.5` check -- this
    agent's combo-blindness is a scope boundary from `about.md`, not a
    negotiable personality trait), the same protocol as `A2`'s rejected
    `aggression_level = 2.21` (2026-08-22, above): a result that measures
    stronger by eroding the agent's designed character is not shipped as-is.
  - Rather than hand-pick a compromise from limited sweep data, added
    `optimize --identity-safe`, which re-runs the search inside a narrower
    `BOUNDS_IDENTITY_SAFE` that keeps both resource-target slopes non-degenerate
    and `defense_beta` in `[0.25, 2.0]` by construction (and always fixes
    `combo_weight = 0.0` regardless of `--params`) -- the best this agent can do
    while still being this agent. Two such runs, targeting `combo` and then
    `borealis` separately from different starting points, converged to
    statistically indistinguishable ~34-35% win rates vs `borealis` -- a stable
    result, not an artifact of one search. A 3-way self-play round-robin
    (defaults vs both `--identity-safe` candidates, 24,000 games/pairing) picked
    the `borealis`-targeted one (72.1% vs 71.4% Bradley-Terry-fit win rate
    against the other two).
  - **Shipped** (`BALANCED_DEFAULTS`): `target_cash_slope=0.0810`,
    `target_cash_intercept=-2.728`, `target_cards_slope=0.0357`,
    `target_cards_intercept=-0.991`, `defense_beta=1.935`,
    `late_game_aggro=2.091`, `combo_weight=0.0` (unchanged, blind by design),
    `lethal_horizon=9`, `draw2_hand_threshold=6`, `draw3_hand_threshold=6`.
    Measured (validated, both seats): vs `borealis` 5.8% -> **34.3%**
    [33.8%, 34.7%] (40,000 games); vs `combo` -> ~59.7% (9,000 games); vs
    `value` -> ~58.1% (6,000 games); vs `rand` -> ~98.5% (6,000 games,
    ceiling-effected like `A1`/`A2`/`A3`).

- **Rating**: the design-intent estimate of 62 (`ideas/G1 .../
  oracle_ai_agent_names.md`, `~62` in the menu) does not survive measurement --
  `./bin/oracle -r -p --rating.games=2000`'s roster-wide Bradley-Terry fit places
  `Bean Counter` at **rating 36** (below the Borealis anchor at 50, above `The
  Showboat`/`A2` at 29 and `The Apprentice`/`A1` at 25), consistent with the
  direct pairwise vs-`borealis` measurement above. This is a legitimate,
  informative result, not a defect: a closed-form agent with no subset search is
  not guaranteed to beat a lambda-tuned exhaustive 0-3-champion enumerator, and
  measurement says it doesn't, at least not within the parameter region that
  keeps it recognizably "Bean Counter." `AI_STRATEGY_RATINGS[AI_STRATEGY_BALANCED]`
  (`player_config.c`) updated from `{ 62, false }` to `{ 36, true }`.

- **Verification**: `make` clean (no new warnings); `-a -p` byte-identical to
  `bin/expectedresults.txt`; `test_combo` (20/20), `test_recall` (10/10),
  `test_cash_exchange` (6/6), `test_rating` (41/41) all pass; vs `rand` 98.6-99.3%
  both seats (well above the `doc/oracle_todo.md` 70% bar, itself a stale,
  near-useless discriminator at this point in the roster -- every implemented
  agent clears it comfortably); vs `value`/`combo` ~58-60% both seats;
  valgrind-clean (`--leak-check=full --track-origins=yes`,
  `balanced` vs `balanced`, 20 games).

---

## 2026-08-24 — AI strategy menu shows Borealis rating, flavour name, and `-A` shorthand

The "Available AI Strategies" menu (`display_ai_strategy_menu()`,
`src/ui/shared/player_config.c`, shared by `stda.cli` and `stda.tui` -- both
call `get_ai_strategies()`) previously printed only a technical label and
availability, e.g. `Combo Threshold [The Showboat] (available)`, with the
flavour name hardcoded into just that one agent's label literal. Now every
row reads e.g. `Random [The Gambler] (available) [2] (rand)`:

- **Flavour name** for all 12 agents, not just Combo Threshold: the previously
  hardcoded `[The Showboat]` text was removed from `strategy_menu_label()`'s
  `AI_STRATEGY_COMBO_THRESHOLD` case; a new `format_menu_name()` helper appends
  `get_strategy_display_name()` in brackets for every agent, and omits the
  brackets when the two strings are identical (Borealis only).
- **Rating** in a second bracket: a new `AI_STRATEGY_RATINGS[]` table
  (`player_config.c`, indexed by `AIStrategyType`) holds the Borealis-scale
  rating and a `measured` flag per agent. `rand`/`value`/`combo`/`borealis`
  carry the 2026-08-23 measured values (2/24/30/50, see that entry below);
  the eight unimplemented agents carry the `ideas/G1 AI agent general info/
  oracle_ai_agent_names.md` design-intent estimates, printed with a `~`
  prefix (`format_menu_rating()`) so they read as a target, not a result.
  A `get_ai_strategy_rating()` accessor exposes the same table for reuse.
- **`-A`/`--ai` shorthand** in a trailing parenthesis, via the existing
  `get_ai_strategy_shorthand()` -- doubles as a mnemonic for the CLI flag.
- No new localized strings: the rating/shorthand fields are language-neutral;
  verified against `-u fr`/`-u es` (accented flavour names render correctly,
  no row overflow/mangling).
- Regression: `-a -p` byte-identical to `bin/expectedresults.txt` (this menu
  is never rendered in `stda.auto`); `test_combo` (20/20), `test_recall`
  (10/10), `test_cash_exchange` (6/6), `test_rating` (41/41) all pass;
  valgrind-clean on a `testsrc/cli_scripts/` run.

## 2026-08-23 — Bradley-Terry rating system implemented

Ports the math and design of `ideas/5 rating system/v2 Bradley-Terry (BT) Rating
System/` (a self-contained ~2300-line prototype/guide), not the file itself --
the prototype predates `A1`-`A3` and this session's own two-solver decision, and
carries a dozen-plus defects fixed on port (see below). Every entrant (AI agent
or human) gets a rating on a 1-99 scale that *is* their percent win probability
against Borealis (`A3`, `AI_STRATEGY_BOREALIS`), the anchor fixed at strength
1.0 / rating 50 by definition -- this is precisely the role `A3` was built for
(see its entry below).

- **`src/rating/rating.h`** (new) -- the single public header:
  `RatingEntry`/`RatingSystem`/`MatchResult`/`RatingConfig`/opaque
  `RatingBatchData`. Deliberately depends only on `game_types.h` (for
  `AIStrategyType`) and libc -- no `src/ui/`, no `src/ai_strat/` -- so the
  module is unit-testable standalone and mirrors why `AIStrategyType` itself
  lives in `game_types.h` rather than `ui/shared/`. `MatchResult`'s win counts
  are `uint32_t`, not the spec's `uint8_t` (which overflows past 255 games --
  this system's benchmark mode runs thousands per pairing).
- **`src/rating/rating_core.c`** (new) -- registration, lookup, and the pure
  strength<->rating math (`rating = 100*s/(s+1)`, clamped [1,99]), adaptive-A
  formula (`A(g) = a_min + (a_max-a_min)*e^(-g/tau)`), win probability,
  rebalance-to-Borealis, a proper Wilson-interval confidence estimate
  (replacing the spec's under-10-games/Wald hybrid), and `rating_find_opponent()`
  (nearest-rating matchmaking, excluding self and the anchor).
- **`src/rating/rating_update.c`** (new) -- the incremental `A^delta` path for
  real-time play. Two path-dependence bugs fixed vs. the spec: `games_played`
  now advances *per game* (so adaptive A actually decays within a match, not
  only between matches -- the spec's ordering left a 10,000-game batch running
  the entire way at `a_max`), and wins/losses/draws are interleaved
  proportionally (a deterministic largest-remainder merge, no RNG) rather than
  applied as three separate all-or-nothing blocks, which was a systematic bias
  given the update is inherently path-dependent.
- **`src/rating/rating_batch.c`** (new) -- order-independent MLE fit over
  accumulated match results, dispatching on two solvers:
  - `RATING_BATCH_MM` (default) -- the standard Bradley-Terry fixed point
    (Minorization-Maximization, a.k.a. the Zermelo/Newman/Hunter algorithm):
    `s_i <- W_i / sum_j N_ij/(s_i+s_j)`, renormalized to the anchor every
    iteration. Parameter-free and provably monotone in the log-likelihood --
    the standard method in the rating literature, not the spec's gradient
    ascent.
  - `RATING_BATCH_GRADIENT` -- the spec's gradient ascent, kept for
    cross-checking against MM, with three defects fixed: the hardcoded
    `learning_rate = 0.01` moved into `RatingConfig`; the convergence check
    moved to run *after* renormalization (the spec compared an unnormalized
    new value against a normalized old one, so it could never actually
    converge); and the gradient normalized by total game count -- **found
    diverging in testing**: the spec's raw gradient scales with dataset size,
    so a learning rate stable on a handful of games (as in the spec's own
    demo) diverges outright at the round-robin benchmark's thousands of games
    per pairing (one entrant's strength was observed running away to
    `5.3 x 10^12` before this fix). Also fixed: draws now contribute 0.5 wins
    to each side in the win/games matrices (the spec inflated games without
    ever crediting a win for a draw, biasing both players' fit downward), and
    an optional `prior_games` separation guard (fictitious win+loss vs the
    anchor) for entrants that are undefeated or winless against the field.
- **`src/rating/rating_csv.c`** (new) -- `rating_export_csv()`/`_import_csv()`.
  The first file I/O anywhere in `src/` (the only prior precedent is
  `main.c`'s `-o/--output` stdout redirection and `prng_seed.c`'s
  `/dev/urandom` read) -- deliberately independent of `-o`, since that flag
  redirects stdout and would otherwise swallow the CSV. Names containing a
  comma are rejected at registration (the format doesn't quote fields).
- **`testsrc/test_rating.c`** (new, `make test_rating`, 41 assertions) --
  unlike the spec's own `oracle_rating_test.c` (prints tables for a human to
  eyeball, `srand(time(NULL))`, almost no real assertions), every check here
  is deterministic pass/fail: scale round-trip, the Borealis anchor property,
  probability symmetry, adaptive-A monotonicity, MM recovering known synthetic
  strengths to within 5%, MM/gradient agreement, CSV round-trip (including a
  human entrant), the `uint32_t` win-count regression, empty/one-entrant
  leaderboard safety (the spec's `rating_print_leaderboard()` underflows an
  unsigned loop bound on zero entrants -- fixed in `rating_report.c` below),
  roster-full handling, and an all-draws match leaving equal-strength
  entrants unchanged.
- **`src/ui/shared/rating_report.c/h`** (new) -- localized leaderboard/detail
  rendering (`LOCALIZED_STRING_L`, per `CLAUDE.md`'s trilingual-UI rule),
  kept in `src/ui/` rather than `src/rating/` so the library itself stays
  UI-free. Sorted by rating descending via a selection sort that starts from
  an explicit `num_entrants == 0` check, not the spec's unsigned-underflow-prone
  loop bound.
- **`src/roles/stda/stda_rating.c/h`** (new) -- `MODE_STDA_RATING`
  (`-r`/`-sr`/`--stda.rating`), the round-robin benchmark and this project's
  first real, reproducible rating table. Registers every agent
  `ai_strategy_is_implemented()` returns true for (filtering matters: only 4
  of 12 `AIStrategyType` slots are implemented, and
  `set_player_strategy_by_type()` silently falls back to Random for the
  rest, so an unfiltered round-robin would rate eight silent aliases of
  Random), plays every unordered pair both seats-swapped (canceling
  first-player advantage, the same pattern
  `aicalibsrc/*/build_selfplay_jobs()` uses), reuses `run_simulation()`
  (`stda_auto.c`, the same in-process pattern `aicalibsrc/*/calib_*.c` use)
  per orientation, and fits via `rating_batch_compute()`. New options:
  `--rating.games=N` (games per orientation, default 2000), `--rating.file=PATH`
  (CSV output), `--rating.method=mm|gradient`.
- **`src/roles/stda/stda_rating_track.c/h`** (new) -- optional
  (`--rating.track`, off by default) human rating tracking wired into both
  `stda.cli` and `stda.tui`. A no-op for anything other than a single
  human-vs-AI game, so normal play is completely unaffected when the flag is
  unset. Loads/creates a caller-owned `RatingSystem` local (no global rating
  state -- see the spec's file-scope-global convention conflict, avoided
  here the same way `A1`-`A3`'s calibration params avoid it), prints an
  optional matchmaking suggestion (`rating_find_opponent()`, never an
  override of the agent actually configured), applies the incremental update
  after the game, and persists back to `--rating.file`. **Bug found and
  fixed during testing**: a player quitting mid-game (the CLI/TUI's
  `EXIT_SIGNAL` path) leaves `gstate->game_state == ACTIVE` rather than a
  real outcome; the first version of `stda_rating_track_finish()` read that
  as "not a win, not a draw" and silently recorded a fake loss -- fixed with
  an explicit `game_state == ACTIVE` guard that skips tracking entirely
  rather than guessing. Verified end-to-end (win/loss/draw, and reload
  across separate process invocations via the CSV) with a throwaway harness
  driving `stda_rating_track_start()`/`_finish()` directly, since scripting a
  full interactive game to natural completion through `testsrc/cli_scripts/`
  was impractical; all four existing canned scripts still run unchanged.
- **`src/core/game_types.h`**: `MODE_STDA_RATING` added to `game_mode_t`;
  `config_t` gained `rating_games`/`rating_file`/`rating_method_gradient`
  (a plain `bool`, not a `RatingBatchMethod`, to avoid `game_types.h`
  depending on `src/rating/rating.h` -- same reasoning as the existing
  `player_config` `void*`)/`rating_track`.
- **`src/main/cmdline.c`**: `-r`/`-sr`/`--stda.rating` mode switch;
  `--rating.games`/`--rating.file`/`--rating.method`/`--rating.track` (no
  short forms for the last one, per the plan). `--ai.a`/`--ai.b` (also
  `required_argument`) already don't get special shell-completion value
  suggestions, so `--rating.method`'s two values weren't special-cased either
  -- consistent with that existing precedent rather than inventing a new one.
- **`src/ui/shared/player_config.c/h`**: added `get_ai_strategy_shorthand()`,
  the inverse of the existing `parse_ai_strategy_shorthand()` -- needed so
  `stda_rating.c`/`stda_rating_track.c` can name a rating entrant after the
  agent's CLI shorthand rather than duplicating the table.
- **`makefile`**: `TEST_RATING_*` block copying the `TEST_RECALL_*` pattern
  (the `patsubst`/`filter` idiom, not `test_combo`'s hand-listed objects);
  since `src/rating/` is dependency-free the test needs no engine objects
  beyond the rating module itself.
- **Measured** (`./bin/oracle -r -p --rating.games=2000`, MM solver, both
  seats-swapped orientations, deterministic under the fixed seed): `borealis`
  50 (anchor), `combo` (`A2`) **30**, `value` (`A1`) **24**, `rand` **2**.
  Cross-validated against `A3`'s own entry below, whose independently
  *measured* Borealis-vs-X win rates (69.13% vs `combo`, 74.19% vs `value`,
  99.33% vs `rand`) imply ratings of ~31/~26/~1 respectively -- close
  agreement despite the two measurements coming from unrelated runs (a
  direct pairwise win rate vs. a transitive multi-pairing MLE fit).
  `RATING_BATCH_GRADIENT` reproduces the same table. This also supersedes
  the placeholder design-intent estimates in
  `ideas/G1 AI agent general info/oracle_ai_agent_names.md` (`rand` 5,
  `value` 15, `combo` 37) -- `A1`'s estimate of 15 was the largest miss
  against the measured ~24-26.
- Regression: `-a -p` output unchanged (byte-identical vs
  `bin/expectedresults.txt`); `test_combo` (20/20), `test_recall` (10/10),
  `test_cash_exchange` (6/6), `test_rating` (41/41) all pass; valgrind-clean
  on `--stda.rating` and on a `--rating.track` CLI session; deterministic
  (`--stda.rating` under a fixed seed, and the incremental path via the
  standalone harness above); all four `testsrc/cli_scripts/` scripts still
  run unchanged with `--rating.track` unset.

## 2026-08-23 — A3 Borealis (the Bradley-Terry benchmark) implemented and calibrated

Implemented per `ideas/A3 ai agent greedy power (borealis)/
greedy_power_borealis_handout.md`: a one-ply agent that exhaustively enumerates
every legal 0-3 champion subset (no pruning of any kind -- the explicit contrast
with `A2` Combo Threshold's threshold-gated search), scores each by
`Σ contribution + combo_bonus − λ·Σ cost` (defense caps the bracketed part at
`expected_incoming_attack()`, handout §4), and plays the epsilon tie-break
winner. This is the Bradley-Terry rating scale's anchor (rating 50 by
definition) precisely because λ (`luna_value`) is a single, monotone strength
dial -- win rate is unimodal in it -- rather than a personality trait like
`A2`'s `aggression_level`.

- **`src/ai_strat/ai_strat_borealis.c/.h`** (new, attack/defense orchestration,
  parameter management, discard/mulligan overrides) **+
  `ai_strat_borealis_enum.c/.h`** (new, candidate enumeration/scoring, split
  out per the handout's §10 file-length guidance once the combined size
  approached 400 lines). Reuses `build_affordable_champions()`,
  `expected_incoming_attack()`, `combo_bonus_for_selection()`,
  `try_play_draw_card()` from `ai_strat_common.h` -- all four were written for
  or already used by `A1`/`A2` with exactly this reuse in mind. The handout's
  `borealis_set_params(const BorealisParams*)` single global setter is
  superseded by a per-player `borealis_set_params(PlayerID, ...)`, matching
  `value_based_set_params()`/`combo_threshold_set_params()`, per the handout's
  own 2026-08-21 note that self-play calibration needs each seat running a
  different parameter set in one game.

- **Per-agent mulligan/discard-to-7 hooks** (`doc/oracle_todo.md`'s
  "do this before or alongside `A3`" item, landed alongside it as planned):
  `discard_to_7_cards()` (`card_actions.c`) and `apply_mulligan()`
  (`stda_auto.c`) are now thin dispatchers through new
  `StrategySet.discard_strategy[]`/`mulligan_strategy[]` function-pointer
  slots, defaulting to the shared power-based heuristic -- extracted verbatim,
  unchanged, into new `src/ai_strat/ai_strat_lib_heuristics.c/.h` -- when an
  agent's `STRATEGY_REGISTRY` entry (`ai_strategy.c`) leaves them `NULL`.
  Random, `A1`, and `A2` all leave them unset and are therefore byte-identical
  to before (`bin/expectedresults.txt`, which is Random-on-both-seats, was
  re-verified unchanged). `A3` is the first agent to override them:
  `borealis_discard_to_7()`/`borealis_mulligan()` protect the best 2- or
  3-card champion combo in hand (by the same `lethal_combo_bonus` threshold
  attack's holding logic uses) from being thrown away, discarding/mulliganing
  by Borealis's own `expected_attack − λ·cost` valuation instead of the
  shared `power` ratio, and only ever touching a protected card once nothing
  unprotected remains (never stalls). `play_cash_card_ai()`'s analogous
  `select_champion_for_cash_exchange()` was **not** given the same optional-
  hook treatment: unlike discard/mulligan, it's called directly from inside
  each agent's own `*_attack_strategy()` (`ai_strat_random.c`,
  `ai_strat_combo_threshold.c`), which only receive `(gstate, ctx)` -- adding
  a hook there would mean widening `AttackStrategyFunc`'s signature itself
  for every existing agent, for a feature nothing currently needs.

- **A bug found and fixed in the calibration *driver*, not the game engine**:
  `aicalibsrc/{value,combo}/calibrate_*.py`'s `sweep`/`selfplay` commands (and
  the new `calibrate_borealis.py`, copied from the `combo` template) tag each
  parallel match job with external bookkeeping (which parameter value, which
  grid cell) in a same-order Python list, then reattach it to the results
  `DataFrame` by *list position* after `run_many()` returns. But
  `run_many()` collects results via `ProcessPoolExecutor` + `as_completed()`,
  which yields futures in **completion order, not submission order** --
  under real parallelism (`--workers` > 1, the default) a job's win/loss
  counts can and did end up reattached to the *wrong* parameter value.
  Confirmed directly: a `luna_value` sweep vs `rand` showed a wildly erratic,
  non-monotonic curve (e.g. 88% -> 50% -> 32% -> 51% at nearby lambda values)
  under default parallel workers, and a smooth, sane curve (87-91%,
  monotonically increasing) with `--workers 1` forcing sequential completion
  -- and matched manual single-shot calls to `bin/calib_borealis` exactly.
  Root cause confirmed by inspection: A1's `sweep` is *not* affected (its
  `run_match()` keeps the echoed parameter values as real `DataFrame`
  columns, so `summarize_sweep()` recovers "which value was this row" from
  the row's own data), but A1's `selfplay`, A2's `sweep`/`selfplay`, and this
  file's `sweep`/`selfplay` all use the vulnerable pattern.
  **`calibrate_borealis.py`'s fix**: `run_match()` now accepts `**tags` that
  get merged directly into the result dict it returns, so bookkeeping travels
  *with* each result through the pool regardless of completion order --
  `cmd_sweep()`/`cmd_selfplay()` no longer need (or do) any pop-and-reattach
  step at all. **`aicalibsrc/value/` and `aicalibsrc/combo/` were not
  patched** (out of scope for this session; see `doc/oracle_todo.md`), but
  this casts real doubt on their previously-shipped results: A1's
  `VB_COST_FLOOR`/`VB_DEFEND_THRESHOLD` were chosen via `selfplay` (the
  vulnerable path) -- notably, that calibration run's own write-up
  (2026-08-21, above) already flagged an unexpectedly low quadratic-fit R²
  (~0.25-0.49) as noisier than the handout predicted, which this bug is a
  plausible (unconfirmed) contributor to. A2's shipped `CT_DEFAULTS` were
  chosen via `optimize` (differential evolution against `combo_win_rate()`,
  which reads `agent_a`/`agent_b`/`wins_a`/`wins_b` directly off each
  self-describing result row rather than external list-position metadata) and
  are **not** affected by this bug.

- **Calibration** (`aicalibsrc/borealis/`, new: `calib_borealis.c` +
  `calibrate_borealis.py` + `README.md`, full `sweep`/`optimize`/`selfplay`/
  `validate` parity with `aicalibsrc/combo/`). Because Borealis's only
  identity-defining property is λ's *shape* (unimodal), not a fixed
  acceptable range for any parameter, `optimize` replaces A2's static
  personality-band check with a post-search re-sweep of λ (the other five
  parameters held at the found values) fit to a quadratic -- concave-down
  means still usable as an anchor; the same technique already used to
  characterize `A1`'s `VB_COST_FLOOR` search.
  - A manual `luna_value` sweep vs `rand` was monotonically increasing
    through the handout's own §13 grid (0.0-2.0, 87.6% -> 96.0%) with no
    peak in range -- expected, since Random is weak enough that added
    caution keeps helping. Widened vs `combo` instead (real headroom: at
    the handout's untuned defaults, 0.5, Borealis actually *lost* to `combo`,
    43.6%) found the true peak far higher than the handout's guess: win rate
    climbs from 36.5% at λ=0 to ~61-62% around λ=4.0-4.5 (quadratic fit
    R²=0.976, vertex 4.098) before declining again by λ=6.0 (40.9%) --
    confirming §8's unimodal-in-λ claim, just at a very different λ than
    guessed. `BOUNDS["luna_value"]` in the driver was widened from an initial
    (0.0, 3.0) to (0.0, 6.0) once this was found, so `optimize` wouldn't
    silently cap short of the real peak.
  - `optimize --opponent combo` (differential evolution, all six parameters
    free) found `luna_value=4.5846, tiebreak_epsilon=0.3444,
    hold_lethal_combos=true, lethal_combo_bonus=24, lethal_hold_ceiling=38,
    min_hand_size_target=6` -- λ landing inside the manually-found peak, and
    the post-search unimodality re-check confirmed concave-down (R²=0.908,
    implied optimum 4.066, consistent with the manual sweep). `
    lethal_combo_bonus`/`min_hand_size_target` both landed at their search
    bounds (24, 6); unlike `A2`'s `aggression_level`/`combo_bonus_threshold`
    these aren't identity-defining traits (handout §9 calls the draw-card
    heuristic a placeholder outright), so this wasn't treated as a rejection
    -- a follow-up pass with wider bounds on those two may find further
    gains. An A/B (`hold_lethal_combos` true vs false, otherwise identical)
    measured an exact 50.00% self-play tie -- `lethal_combo_bonus=24` is
    high enough that holding rarely triggers either way, so it was left at
    its optimizer-found `true` rather than forced off per handout §7's
    fallback.
  - **Shipped** (`BOREALIS_DEFAULTS`, `ai_strat_borealis.c`): the `optimize`
    winner above, verbatim. Measured (validated, both seats): vs `combo`
    43.63% -> **69.13%** [68.67%, 69.58%] (40,000 games); vs `value` 49.63%
    -> **74.19%** [73.51%, 74.87%] (16,000 games); vs `rand` 93.13% ->
    **99.33%** [99.19%, 99.45%] (16,000 games) -- confirmed against the
    actual `bin/oracle` binary too (not just the calibration harness), e.g.
    vs `combo` at `-n 5000`: 70.8% as Player A, 68.0% as Player B. Mirror
    match (`borealis` vs `borealis`, shipped params both seats): ~55-57% for
    Player A across two seeds, attributable to first-player advantage per
    handout §13.

Full details of this and future calibration runs will continue to accumulate
in this file the way the `A1`/`A2` entries above do.

## 2026-08-22 — A2 Combo Threshold ("The Showboat") implemented and calibrated

(This entry was missing its own heading until the rating-system pass below
noticed the A2 content below reading as an A3 subsection -- fixed here, no
content changed.)

Implemented per `ideas/A2 ai agent combo threshold (the showboat)/
combo_threshold_handout.md`: a threshold-gated combo chaser that prunes attack
candidates to single champions plus pairs/triples whose combo bonus clears a
tunable threshold, and defends probabilistically -- declining a fraction of the
blocks it should take (handout §6.5 step 3). Both traits are the agent's
designed character (handout §3, §8), not defects.

- **`src/ai_strat/ai_strat_combo_threshold.c/h`** (new): `ComboThresholdParams`
  ships nine fields, not the handout's seven -- `combo_weight` (the `w` in
  §6.4 step 3's scoring formula) and `min_play_score_floor` (the cash-card
  fallback's unnamed floor, §6.4 step 5) were referenced in the handout's
  prose but never declared as struct fields; §9.1 flagged `combo_weight`
  explicitly as an open question. Attack decomposes into
  `eval_single_champions()`/`eval_two_card_combos()`/`eval_three_card_combos()`/
  `eval_cash_fallback()` per handout §5.3 (the original draft's single
  ~95-line function is exactly what that section says not to reproduce).
  Defense's probabilistic decline uses `genRand(&ctx->rng)` directly, matching
  `ai_strat_random.c`'s pattern -- no `rand()`, no static RNG state, per
  handout §6.6. Two design facts drove the implementation: `combo_bonus.c`'s
  species<->(color,order) mapping is a bijection (verified against all 102
  champion rows in `game_constants.c`), so the three colors are exact stat
  clones and combo choice is stat-neutral; and `discard_to_7_cards()`
  discards the lowest `power` (an efficiency ratio, not strength), which
  actively fights `save_big_combos_for_lethal`'s intent to hold expensive
  combo pieces -- logged as a cross-cutting future item (see below), not
  fixed here.
- **`src/ai_strat/ai_strat_common.c/h`**: added `combo_bonus_for_selection()`
  (a hand-subset variant of the existing `CombatZone`-only combo helper,
  refactored to share one `build_combat_cards()` construction site) -- meant
  for `A3` Borealis to reuse as-is, same as `build_affordable_champions()`/
  `expected_incoming_attack()`/`try_play_draw_card()` already are.
- **Strategy registry**: one line added to `ai_strategy.c`'s
  `STRATEGY_REGISTRY[]` -- `AI_STRATEGY_COMBO_THRESHOLD` now dispatches to
  `combo_threshold_attack_strategy`/`_defense_strategy`. Everything else
  (enum slot, display names "The Showboat"/"Le Frimeur"/"El Fanfarron",
  shorthand `combo`, CLI/TUI menu wiring) was already in place from the `A1`
  folder-sort/registry work (2026-08-21) -- see `ai_strategy_is_implemented()`
  in `src/ai_strat/ai_strategy.c`.
- **Handout corrections found stale during implementation** (predated
  `894409b`, the `A1` commit that moved `AIStrategyType` to `game_types.h` and
  collapsed shorthands to one per agent): §2/§7.4's "`showboat` primary,
  `combo` alias" is backwards -- `combo` is the sole canonical shorthand,
  already shipped that way; §7.1's enum location/name (`AI_STRATEGY_COMBO_AWARE`
  in `ui/shared/player_config.h`) is stale, it's `AI_STRATEGY_COMBO_THRESHOLD`
  in `core/game_types.h`; §6.2's `combo_threshold_set_params()` needed to be
  per-player (`PlayerID` argument), not a single global setter, for the same
  reason `A1`'s `value_based_set_params()` needed it -- self-play calibration
  requires each seat to run a different parameter set in one game.
- **Regression**: `-a -p` output unchanged (Combo Threshold isn't in the
  default matchup); `test_combo` (20/20), `test_recall` (10/10),
  `test_cash_exchange` (6/6) all still pass; valgrind-clean on
  `-a -p -n 50 --ai.a=combo --ai.b=combo`; deterministic (identical output
  across repeated runs at a fixed seed).
- **Measured strength, uncalibrated defaults** (n=10000, both seats): vs
  `rand` 84.85%/91.11% (ceiling-effected, like `A1` was); vs `value` 44.98%/
  55.35% (~50% average -- essentially at parity, unlike `A1`'s ~90%+ vs
  Random, because vs-`value` doesn't saturate the way vs-Random does, making
  it the more useful calibration target -- see below).
- Also noted (not fixed here, tracked as a separate future item, `doc/
  oracle_todo.md`): a consistent second-seat win-rate advantage visible
  across `A1` and `A2` matchups regardless of which agent is stronger
  (e.g. `combo` vs `rand` 84.85% as Player A vs 91.11% as Player B).

### Calibration: `aicalibsrc/combo/` tooling; shipped parameters

Followed the `aicalibsrc/value/` pattern (one self-contained subfolder per
agent): `calib_combo_threshold.c` links the engine directly and takes
`<numsim> <seed> <agent_a> <agent_b>` plus the nine `ComboThresholdParams`
fields per seat; `calibrate_combo_threshold.py` adds a fourth subcommand
beyond `A1`'s `sweep`/`selfplay`/`validate` -- `optimize`, a
`scipy.optimize.differential_evolution` black-box search, because a full
Cartesian grid (feasible for `A1`'s 2 parameters) is combinatorially
infeasible over 9; `selfplay` here round-robins a small set of *named*
JSON candidates instead of a full grid. Two bugs caught and fixed during
tooling verification (cross-checked against `bin/oracle` at matching
seeds): an off-by-one in `calib_combo_threshold.c`'s argv parsing (params
were read one slot too early -- caught because round-tripped CSV output
didn't match the CLI input); and `calibrate_combo_threshold.py`'s
`summarize_selfplay()` computed `overall_win_rate` via a matrix-transpose
trick that summed the *opponent's* wins for the "seated as defender" half
instead of the candidate's own (the Bradley-Terry fit itself was
unaffected -- it works directly off the win/game matrices -- only the
display column was wrong).

`optimize --opponent value` (differential evolution, all nine parameters
free, 80,000-game validation) found `aggression_level=2.21,
save_big_combos_for_lethal=false` at 77.3% vs `value`, but that result was
**not shipped**: `aggression_level` divides both combo thresholds (handout
§5.4), and at 2.21 that collapses `combo_bonus_threshold`'s effective value
to ~5.0 -- low enough to admit every 2-card combo bonus including a plain
color pair, erasing the "chases only the spectacular" selectivity that's
this agent's stated identity (handout §1, §8; `about.md`'s "hoards big
combos for a finishing blow", which `save_big_combos_for_lethal=false`
removed outright). Both passed the tooling's automated personality-band
check (`defend_probability_base`/`defend_damage_threshold`, the only two
checked automatically) -- the drift was only visible by hand-computing the
*effective* threshold, the same kind of interaction the handout's §3 named
as why this design was disqualified as the benchmark. Hand-patched instead:
`aggression_level` set to 1.3 (not the found 2.21) and
`save_big_combos_for_lethal` restored to `true`, keeping the optimizer's
other seven values. `11/1.3 ~= 8.46` and the untuned default's `10/1.0 = 10`
both admit only the bonus-10 species-pair tier, so 2-card selectivity is
unchanged from the handout's original design intent.
**Shipped** (`CT_DEFAULTS`, `ai_strat_combo_threshold.c`):
`aggression_level=1.3` (was 1.0), `combo_bonus_threshold=11` (was 10),
`combo3_bonus_threshold=16` (was 14), `combo_weight=2.3626` (was 1.0),
`min_play_score_floor=2.1184` (was 4.0), `defend_probability_base=0.4085`
(was 0.55, still within the handout's 0.40-0.70 band),
`defend_damage_threshold=8` (unchanged), `min_hand_size_target=5` (was 4),
`save_big_combos_for_lethal=true` (unchanged). Measured win rate (80,000
games, both seats): vs `value` **49.94% -> 58.77%** (+8.83pp, CI
[58.29%, 59.26%]); vs `rand` **88.11% -> 92.78%** (+4.67pp, CI
[92.52%, 93.03%]).
- Regression re-checked against the shipped defaults: `-a -p` unchanged,
  `test_combo`/`test_recall`/`test_cash_exchange` all still pass,
  valgrind-clean.

## 2026-08-21 — A1 Value Based parameter calibration; `aicalibsrc/` tooling

Calibrated `VB_COST_FLOOR`/`VB_DEFEND_THRESHOLD` (`ai_strat_valuebased.c`) using
new calibration infrastructure, built specifically because the vs-Random
comparison used to validate A1 (see the entry below) turned out to be
**ceiling-effected**: at ~90% vs Random, nearby parameter values are hard to
tell apart. Self-play (same agent, two different parameter sets, head-to-head)
doesn't have that ceiling, and was the actual mechanism used to find and
compare candidates:

- **`src/ai_strat/ai_strat_valuebased.c/h`**: added a calibration-only
  per-player parameter override hook -- `value_based_set_params(PlayerID,
  cost_floor, defend_threshold)` / `value_based_reset_params()` -- backed by
  file-static per-player arrays instead of the `#define`s directly. This is
  what makes "Player A runs Value Based with parameter set 1, Player B runs
  Value Based with parameter set 2, in the same game" possible; previously
  the parameters were compile-time constants shared by both seats, which the
  engine had no way to differentiate per player. Deliberately scoped to this
  one file rather than threaded through `AttackStrategyFunc`/`GameContext`/
  `StrategySet` -- no other agent needs runtime-tunable parameters yet, so a
  general mechanism isn't justified before a second instance exists. When A2
  Combo Threshold or A3 Borealis need the same self-play calibration
  capability (both likely, per design discussion), each gets the same
  lightweight per-agent pattern rather than a shared framework built ahead of
  need. Note for A3 specifically: its handout's `borealis_set_params(const
  BorealisParams*)` is currently a single global setter, not per-player --
  will need the same per-player adaptation `value_based_set_params()` used,
  flagged in that handout as `## Note (2026-08-21)`.
- **`aicalibsrc/value/calib_valuebased.c`** (new, `make calib_valuebased` ->
  `bin/calib_valuebased`): links the engine directly (same pattern as
  `testsrc/test_recall.c` etc.), takes `<numsim> <seed> <agent_a> <agent_b>
  <cost_floor_a> <defend_threshold_a> <cost_floor_b> <defend_threshold_b>`,
  runs `run_simulation()` in-process, prints one CSV result line. No
  subprocess-spawn or text-parsing overhead per parameter combination --
  ~27ms for 2500 games.
- **`aicalibsrc/value/calibrate_valuebased.py`** (new): Python driver on top of that
  binary. `sweep` subcommand: the handout's own univariate vs-Random
  diagnostic (Sec 11), with Wilson-interval confidence bounds and an optional
  plot. `selfplay` subcommand: round-robin over a parameter grid, both seat
  orders, fits a Bradley-Terry model (`scipy.optimize`) to get a relative
  strength per combo instead of trusting individual pairwise results (which
  can be intransitive), then automatically validates the winner against
  Random vs the shipped defaults. `validate` subcommand: the same
  winner-vs-default-vs-Random comparison, standalone. Needs `numpy`, `pandas`,
  `scipy`, `matplotlib`, `scikit-optimize` (system packages via apt on this
  Debian/Ubuntu box -- `pip install` is blocked by PEP 668 without a venv,
  and building a venv itself needs `python3.14-venv` from apt anyway).
- **Calibration run and result**: with `VB_DEFEND_THRESHOLD` searched freely,
  self-play favored increasingly high values (5, then 10, with no peak found
  before stopping) -- i.e. "defend only against near-maximal attacks,
  otherwise never." That measured stronger (~94% vs Random) but changes the
  agent's designed character from "moderate, threshold-gated defender" to
  "attacks almost exclusively," eroding the intended contrast with later
  agents' defensive styles. Decision: hold `VB_DEFEND_THRESHOLD` at a
  human-chosen, deliberately moderate `0.8` (up from `0.5`) instead of the
  self-play-optimal value, and re-optimize only `VB_COST_FLOOR` with that
  fixed. That search was noisier than expected -- a quadratic fit through the
  cost_floor/strength data only reached R^2 ~=0.25-0.49 even at 7,040,000
  games per candidate -- confirming the handout's own prediction that this
  parameter's effect "should be mild": real, but smaller than what's cleanly
  resolvable. 1.2-1.7 formed a consistently-better cluster than the extremes
  tested; `1.3` was picked as a defensible value inside that cluster, not a
  precise optimum.
  **Shipped**: `VB_COST_FLOOR = 1.3` (was `1.0`), `VB_DEFEND_THRESHOLD = 0.8`
  (was `0.5`). Measured win rate vs Random: **91.0% -> 92.4%** (both seats,
  64,000 games, 95% CI [92.2%, 92.6%], non-overlapping with the old default's
  [90.8%, 91.2%] -- a real, not noise, improvement). Verified against the
  actual `bin/oracle` binary (not just the calibration harness) at `-n 10000`:
  90.1% as Player A, 94.5% as Player B, averaging to the same ~92.3%.
- Regression: `-a -p` output unchanged (Value Based isn't in the default
  matchup, so this doesn't touch `bin/expectedresults.txt`); `test_combo`
  (20/20), `test_recall` (10/10), `test_cash_exchange` (6/6) all still pass;
  valgrind-clean on `-a -p -n 50 --ai.a=value --ai.b=value`.

## 2026-08-21 — A1 Value Based ("The Apprentice") implemented; strategy registry; per-player agent CLI options

- **A1 Value Based** (`src/ai_strat/ai_strat_valuebased.c/h`) is implemented per
  `ideas/A1 ai agent value based (the apprentice)/value_based_handout.md`: a
  one-ply, combo-blind agent that ranks affordable champions by an efficiency
  ratio (`contribution / (cost + VB_COST_FLOOR)`) and greedily plays up to
  `VB_MAX_ATTACK_CARDS` (2) on attack, or exactly one champion on defense when
  `expected_incoming_attack() >= VB_DEFEND_THRESHOLD * candidate's expected
  defense`. No pass option in the attack phase (shipped deliberately per the
  handout's §12 decision), no combo awareness (the omission is the point). Two
  handout assumptions turned out stale and were corrected in the implementation:
  `fullDeck[]` *does* already carry precomputed `expected_attack`/
  `expected_defense`/`*_efficiency` fields, but the precomputed `*_efficiency`
  fields use a fixed 0.25 cost-0 divisor rather than a tunable floor, so the
  agent derives its own ratio from `expected_attack`/`expected_defense` instead
  of using them directly; and `HDCLL_toArray()` (assumed to need `free()`
  discipline) no longer exists -- collections are fixed-size structs now.
- **`src/ai_strat/ai_strat_common.c/h`** (new): `build_affordable_champions()`,
  `expected_incoming_attack()`, `try_play_draw_card()` -- turn-strategy helpers
  meant to be reused as-is by later agents (A2 Combo Threshold, A3 Borealis)
  rather than re-derived, so draw-card behavior stays comparable across
  head-to-head runs. Distinct from the still-unbuilt `strat_lib` (scoped to
  non-turn heuristics: mulligan, discard-to-7, cash-exchange selection).
- **Strategy registry** (`src/ai_strat/ai_strategy.c/h`): `AIStrategyType` moved
  from `ui/shared/player_config.h` to `core/game_types.h` (so `src/ai_strat/`
  doesn't depend on `src/ui/`), and a table-driven
  `AIStrategyType -> {AttackStrategyFunc, DefenseStrategyFunc}` registry
  replaces the three call sites that used to hardcode
  `random_attack_strategy`/`random_defense_strategy`
  (`stda_auto.c`, `cli_game.c` -- shared by CLI and TUI).
  `ai_strategy_is_implemented()`/`set_player_strategy_by_type()` are the single
  dispatch point every mode now goes through. The interactive strategy menu
  (`player_config.c`'s `display_ai_strategy_menu()`/`get_ai_strategy_choice()`)
  now derives its "available"/"not yet implemented" labels and its fallback
  logic from that same registry instead of a hardcoded `choice > 1` check --
  the CLI and TUI menus already asked both players for an agent, but any
  answer besides Random was silently discarded before this change.
- **One CLI shorthand per agent**: dropped the `showboat` (flavour name) and
  `greedy` (retired pre-rename tech name) aliases for
  `AI_STRATEGY_COMBO_THRESHOLD`/`AI_STRATEGY_BOREALIS`. Every agent now has
  exactly one canonical shorthand (`combo`, `borealis`, ...). Breaking change
  for any script using either alias, but neither had an implemented agent
  behind it yet.
- **Per-player agent selection for `--stda.auto`**: new `-Aa`/`-ai.a`/`--ai.a`
  and `-Ab`/`-ai.b`/`--ai.b` options (`cmdline.c`), distinct from the existing
  `-A`/`--ai` (still the unimplemented AI-agent client-mode selector, left
  untouched). Unknown or not-yet-implemented shorthands are rejected up front
  with an error and the agent list, rather than silently falling back to
  Random. `run_mode_stda_auto()` now builds a minimal `PlayerConfig` from
  `cfg->agent[]`/`agent_set[]` instead of hardcoding Random for both players;
  `present_results()` prints a "Matchup: Player A = ..., Player B = ..." line,
  but only when either agent differs from Random, so a plain `-a` run's output
  stays byte-identical to `bin/expectedresults.txt` (verified: still matches).
  These options only apply to `--stda.auto`; using them with `--stda.cli`/
  `--stda.tui` prints a one-line warning and is ignored, since those modes
  already have a (now-working) interactive per-player menu.
- **`MAX_NUMBER_OF_SIM`** raised 1000 -> 10000 (`game_constants.h`) so the
  handout's suggested `n = 10000` sample size (SE ~ 0.5pp) is reachable via
  `-n`. The **default** stays 1000 -- introduced a separate
  `DEFAULT_NUMBER_OF_SIM` constant for `run_mode_stda_auto()`'s `numsim <= 0`
  fallback, rather than letting it inherit the raised cap.
- **Measured strength** (both seat orders, n=10000 each, `-p` seed): Value
  Based beat Random ~89.4% as Player A and ~92.0% as Player B -- well above
  the handout's speculative "~60-70%" estimate. Investigated for a bug and
  found none: Random's attack strategy plays at most **one** card per turn
  (champion, draw, or cash, chosen uniformly, no preference for champions),
  and its defense strategy declines to defend 53% of the time even when it
  could. Value Based's structural advantage (up to 2 champions per turn,
  gated but consistent defense) compounds against that weak a baseline. The
  handout's win-rate table should be treated as superseded by this measurement.
  Mirror match (`value` vs `value`) came in at ~47.8% for Player A in one
  10000-game run, consistent with ordinary first-player/mulligan variance.
- Regression: `-a -p` output unchanged (verified against
  `bin/expectedresults.txt`); `test_combo` (20/20), `test_recall` (10/10),
  `test_cash_exchange` (6/6) all still pass; valgrind-clean on a
  `-a -p -n 50 --ai.a=value --ai.b=rand` run.

## 2026-08-20 — Ideas 2 and 3 cleanup (engine refactoring notes, TUI prototype)

- **Idea 2** (`ideas/2 engine and action system design/`): pared
  down to only mode-agnostic core-engine material. Deleted outright, as superseded
  before ever being built: a full CLI-specific reference implementation
  (`reference_implementation_with_callbacks.c`, `cli_refactor_summary.md`,
  `Refactoring of stda_cli in 4 modules...md`, ~2000 lines) describing an
  engine/`Action*`/`UICallbacks` redesign the real CLI split (2026-07-14) never
  adopted — it took a simpler path (`cli_display`/`cli_action_display`/`cli_input`/
  `cli_io`/`cli_game` around the `UiIO` seam). GUI-flavored sketches
  (`gamegui_main_loop.txt`, `unified_gui_interface.txt`, `stda_game_gui.txt`,
  `stda_game_impl.txt`, plus the GUI example from `mode_usage_examples.txt`) were
  consolidated into new `ideas/9 gui/game_loop_engine_integration_notes.md` — genuinely
  new content, since the existing SDL3 GUI plan there covers rendering/assets/input but
  not engine integration. Network-flavored sketches (`More notes on client-server
  preparation.md`, `client_game_gui.txt`, plus the Server example) were checked against
  `ideas/8 client server/`'s two existing ~5100-line docs — mostly redundant/inferior
  and dropped, but an opaque-handle server/client API sketch, a `CardVisibility` enum,
  and a simpler single-threaded poll-based client loop were confirmed (by grep) *not*
  already present there, so those moved into new
  `ideas/8 client server/game_loop_and_client_api_notes.md`, which also flags that
  ideas/8's own "Strategy Interface" section still shows the stale `void`-mutating
  strategy signature rather than the `Action`-returning one a networked AI client
  actually needs (load-bearing for AI-agent-as-network-client scenarios). Verified
  before any of this that the AI-as-network-client use case itself stays fully covered
  (ideas/8's own "AI Integration" section, untouched). Added a `README.md` to folder 2
  summarizing what remains and why.
- **Idea 3**: nine early flat-file TUI prototype files (`oracle_tui_impl.txt`/
  `_header.txt`, `oracle_cmdline.txt`/`_h.txt`, `oracle_main_updated.txt`,
  `oracle_makefile_tui.txt`, `oracle_version_h.txt`, `oracle_tui_readme.md`,
  `oracle_integration_guide.md` — pre-reorg includes, the old `HDCLL` linked-list
  types, a single `tui.c`, missing most of today's `cmdline.c` options) were confirmed
  superseded by the real `src/ui/tui/` + `src/roles/stda/stda_tui*.c` implementation
  and deleted outright. One file — `ascii art fonts for logo in tui and cli modes.txt`
  — stayed, since it describes a logo feature confirmed (by grep) not implemented
  anywhere. Since nothing TUI-specific remained, the folder was renamed
  `ideas/3 tui/` → `ideas/3 misc ui ideas/`; all repo cross-references to the old path
  updated (`CLAUDE.md`, `doc/oracle_roadmap.md`, `ideas/2 …/target_folder_structure_v4.md`,
  `src/ui/tui/tui display input and callbacks.txt`) except one intentionally-preserved
  historical mention in the already-archived `doc/done/1 …/pragmatic_cleanup_
  implementation_plan.md`.
- No game-logic or build changes; this is a documentation/`ideas/`-only pass.

## 2026-08-20 — Idea 1 (source folder structure) closed out; doc cleanup pass

- **Idea 1 second pass**: `doc/done/1 improve source code folder structure/`'s pragmatic
  cleanup (done 2026-07-14) left a small remainder, now closed. The ten `src/`
  placeholder `.txt` files had cross-references to a pre-renumbering `ideas/` layout
  (`ideas/9`, `11`, `12.1`, `15`, `16`, `18`, `1 tui`) — repointed at the current folder
  numbers. `src/ui/cli/cli display input and callbacks.txt`, which its own text marked
  "safe to remove" once superseded by real code, was deleted (confirmed nothing
  references it first). The three test targets (`test_combo`, `test_recall`,
  `test_cash_exchange`) compiled objects straight into `src/**`/`testsrc/` via the
  Makefile's default `%.o: %.c` rule, leaving twelve stray git-ignored `.o` files that
  `make clean` never removed; `makefile` now routes them through `$(BUILDDIR)` (a new
  `$(BUILDDIR)/testsrc/%.o` pattern rule mirrors the existing source rule), and `clean`
  removes the test binaries plus any leftover in-tree `.o` files defensively.
  `make test_stda_auto` invoked `./bin/oracle.exe` (the MSYS2 name) unconditionally, so
  it could never pass on the primary Linux target; fixed to use `$(TARGET)` and marked
  `.PHONY`, with `help` listing all four test targets. The folder itself moved to
  `doc/done/1 improve source code folder structure/`; its still-relevant target
  architecture doc (`revised_folder_structure.md`) moved instead to
  `ideas/2 engine and action system design/
  target_folder_structure_v4.md`, trimmed of content duplicated elsewhere in that
  folder, since that's where the work it describes actually happens.
- **Doc cleanup**: `doc/oracle_design.md` (dated December 2025, describing a
  pre-reorg flat `src/*.c` tree, missing recall/TUI/fixed-arrays, an ~800-line
  `stda_cli.c` awaiting a split, and Geany/Arch Linux) was rewritten top to bottom
  against the current codebase and restructured around what's actually true today,
  including new UI-architecture (`UiIO` seam) and modes/command-line sections.
  `doc/oracle_roadmap.md` and `doc/oracle_todo.md`, which had drifted into duplicating
  and sometimes disagreeing with each other, were split by role — roadmap owns
  long-horizon phases/ordering/vision, todo owns actionable near-term checkboxes — with
  stale paths, the `<30` vs `35`-line contradiction, and completed-work narratives
  (now just linked to this changelog) removed. `README.md` synced similarly (TUI is
  working, not planned; AI list matches the real `A1`–`A11` agent scheme).
- No game-logic changes; `-a -p` output still matches `bin/expectedresults.txt`.
  `make test_combo` (20/20), `test_recall` (10/10), `test_cash_exchange` (6/6), and
  `test_stda_auto` all pass; `make clean` now leaves the tree free of `.o` files.

## 2026-07-26 — Hidden `--oracle-complete` option + bash tab-completion script

- Added a hidden `--oracle-complete[=WHAT]` option to `parse_options()`
  (`src/main/cmdline.c`) that dumps completion candidates one per line and exits
  cleanly (return `-1`, same as `-h`/`-V`) — deliberately absent from `print_usage()`,
  not part of the public CLI. Bare `--oracle-complete` lists every option spelling
  (all 3 forms where they exist, `-`/`--` dashed per `print_usage()`'s convention,
  trailing `=` when the option takes an argument); `=agents` lists the `-A`/`--ai`
  shorthand codes (via new `print_ai_agent_shorthand_codes()` in
  `src/ui/shared/player_config.c`/`.h`, a bare-codes counterpart to the existing
  localized `print_ai_agent_shorthand_list()`); `=langs` lists the `-u`/`--ui.lang`
  codes (`en`/`fr`/`es`). `long_options[]` moved from a `parse_options()`-local
  `static` array to file scope so `print_completion_list()` can also read it.
- Added `tools/oracle-completion.bash`, a bash completion script (source it manually,
  e.g. from `~/.bashrc`) that calls the binary for every candidate list instead of
  hardcoding any of them, so adding a new option/agent/language needs no script
  change. Handles bash's default `COMP_WORDBREAKS` splitting `--ai=rand`-style
  arguments at `=`, the attached-only `-A<agent>` form, and file completion for
  `-i`/`-o`.
- No game-logic changes; `-a -p` output still matches `bin/expectedresults.txt`.

## 2026-07-24 — Removed `oldsrc/`; `make format` now excludes `ideas/`

- **Removed `oldsrc/`** (pre-refactor implementation) and the `oldcode`/`make oldcode`
  Makefile target, per the `oracle_todo.md` "Code Cleanup" item -- everything in it is
  fully preserved in git history, so it had no reason to keep living in the working
  tree. `OLDSRCDIR`/`OLDBUILDDIR` variables and the `oldobj/%.o` build rule removed
  along with it. (The file deletions themselves ended up bundled into the prior
  commit alongside the TUI polish pass -- a staging mix-up, not intentional -- but
  the removal is recorded here where it belongs.)
- **`make format`** now passes `--exclude=ideas` to `astyle` so the recursive
  `--project` sweep no longer reformats `ideas/`'s design-exploration/prototype code
  (which deliberately doesn't follow the real codebase's conventions -- see
  `CLAUDE.md`'s note on that folder).

## 2026-07-24 — TUI polish pass: layout, colors, card formatting, playability fixes

Hands-on play of the Milestone 2 human-vs-AI `stda.tui` surfaced ~17 UI/layout/
playability rough edges; all fixed in one pass, grouped low-risk to higher-risk. Two
items were deliberately deferred (see "Left for a future pass" below).

- **Gameplay fixes**:
  - Removed the forced "press any key" pause after combat resolves
    (`tui_play_turn_with_humans()`, `stda_tui_interactive.c`) — safe now that combat/
    damage/energy persist in the Game Messages box instead of scrolling past in the
    Console.
  - Fixed the Active/Waiting status-bar label to key off `gstate->player_to_move`
    (the player whose decision is actually pending) instead of `current_player` (this
    turn's fixed attacker) — previously a human *defender* showed as "Waiting" while
    the AI attacker showed "Active". Found and fixed a follow-on bug during tmux
    playtesting: `attack_phase()`/its human mirror always point `player_to_move` at
    the defender, even when no champions were played or after combat already
    resolved, so the attacker's own end-of-turn housekeeping (luna collection,
    discard-to-7) was showing the *defender* as wrongly Active — fixed by resetting
    `player_to_move` back to the attacker right after the attack/defense/combat block.
- **Info-column text**: `q=quit` added to the Shortcuts box (was missing); `P=pass`
  lowercased to `p=pass` display-wide (parsing already accepted both cases);
  `Enter=play` now wraps onto its own line (`tui_print_wrapped()` honors an explicit
  `\n`); the game-start Console line is now just "Game started." for human games
  (full hints live in the always-visible Shortcuts box).
- **Bottom-row layout restructure**: the command line now shares the bottom row with
  Player A's status bar (table portion for the bar, info-column portion for the
  command line) instead of its own full-width row — reclaims a body row. The
  PLAY-mode status line shrank to just the staged-index list (`PLAY [1,2]`) to fit
  the narrower command window; full key hints live in the Shortcuts box.
- **Play-area labels/positioning**: hand labels dropped the redundant player name
  (`Hand (6)` / `Hand (5) [Hidden]` — the status bars already say whose hand it is);
  `Deck`/`Discard` counts moved from one combined centered label to separate
  corner labels, each tucked right where that pile's actual content starts/grows
  (Discard top-left / Deck top-right for Player B, mirrored for Player A); the
  `Combat zone PLAYER x (n):` labels were removed (table side + the `-- combat zone --`
  divider already make ownership obvious) and champions now render as a vertical
  stack tucked directly against the divider, growing toward the owning player's hand
  as more are added.
- **Game Messages box now does something**: added a second message ring buffer
  (`tui_add_game_message()`/`_colored()`, mirroring the existing Console
  `tui_add_message()` pair) and a shared `tui_draw_message_pane()` renderer for both
  boxes. Routing: all narrative (turn summaries, combat resolution/damage/energy,
  action outcomes like "Played N champion(s)") now goes to Game Messages; Console is
  reserved for interaction (prompts, input echo, validation errors, recall/cash
  candidate lists). The box itself was previously decorative — drawn but never
  populated. Also made it (and the Shortcuts box) genuinely tall: both now split the
  info column's remaining height (after Shortcuts) evenly, instead of Console getting
  whatever was left over.
- **Card formatting — hybrid, localized, colored**: `tui_format_card()` (compact
  board form, used in the discard grid) no longer hardcodes French `Pig`/`Rap`/
  `Echange` labels regardless of UI language — draw/recall/cash labels now go
  through `LOCALIZED_STRING` like everywhere else. New detailed CLI-style form
  (`tui_format_card_detailed()`/`tui_draw_card_detailed()`, mirroring
  `cli_display.c`'s `display_player_hand()`) used for the hand and combat zone —
  roomier areas, so full species name + `(D+, L)` breakdown instead of the compact
  abbreviated form; both the compact and detailed forms now color the luna cost cyan
  and the card's own name/label in its type color (champion color, green for draw,
  dim for cash) — previously the whole compact string was a single flat color.
  Hand-card index prefixes (`[1] ...`) gained a space after the bracket.
- **Player colors borrowed from the CLI**: top/bottom status bars switched from
  plain red/green to the CLI's bold cyan (Player A) / bold yellow (Player B)
  (`cli_display.h`'s `COLOR_P1`/`COLOR_P2`); player-attributed Game Messages lines
  (attacker/defender headers, energy-change lines) now use the same two colors via a
  new `tui_player_msg_color()` helper.
- **Left for a future pass** (deliberately out of scope this time, see the approved
  plan): moving the pre-ncurses player-setup questions (mode/name/AI-strategy prompts,
  currently plain stdio before `initscr()`) into the Console box — larger scope,
  touches CLI-shared setup code, planned as its own milestone; rendering deck-card
  contents (only meaningful once a card-visibility model exists for "discard shuffled
  back into deck" — not yet modeled).
- Verified: `-a -p` regression identical throughout every incremental step; multiple
  `tmux`-scripted human-vs-AI sessions (including escape-sequence capture to confirm
  actual ANSI colors, not just layout) in both English and French confirmed every fix
  end-to-end — corner labels, combat-zone stacking/divider-tucking, Game-Messages
  vs. Console routing, no more forced pause, correct Active/Waiting through a full
  attack→defense→discard-to-7 cycle, and the localized card labels in both languages.

## 2026-07-23 — TUI Milestone 2, Passes 2 & 3: playable human-vs-AI TUI

Builds on Pass 1's `UiIO` seam to deliver a fully playable human-vs-AI `stda.tui`:
attack/defense/recall/cash-exchange/mulligan/discard-to-7, both a `TAB`-toggled
COMMAND-mode line editor and a PLAY-mode digit-staging flow for champion selection,
live combat-result display, and context-sensitive shortcuts text.

- **New `src/ui/tui/tui_input.c/h`**: the TUI's `UiIO` backend. `message` maps
  `UiMsgKind` to color-tagged console lines (new `tui_add_message_colored()` /
  `TUI_MSG_COLOR_*` in `tui_render.h`); `read_line` is a `getch()`-based line editor
  drawn into the command window (`tui_draw_command_line()`); `show_card_list` formats
  recall/cash-exchange candidates via the newly-exported `tui_format_card()`.
- **`src/roles/stda/stda_tui.c`**: now runs the same pre-ncurses player-configuration
  menu as `stda_cli.c` (`display_player_selection_menu`/`get_player_names`/
  `get_ai_strategies`/`get_player_assignment`, all before `tui_screen_create()`),
  then a human-vs-AI-aware game loop. **New `src/roles/stda/stda_tui_interactive.c/h`**
  holds the human-turn handlers: `tui_handle_interactive_attack`/`_defense` (PLAY-mode
  digit-staging -- 1-9 toggles a hand card, Enter plays the staged set via
  `validate_and_play_champions()`, Esc clears, `P` passes, `TAB` drops into full
  COMMAND-mode line editing for draw/cash/recall/exit), `tui_handle_interactive_mulligan`/
  `_discard_to_7` (COMMAND-mode only), and `tui_play_turn_with_humans()` (phase-by-phase
  orchestrator mirroring `cli_game.c`'s `execute_game_turn()`, mixing human handlers and
  plain AI strategy calls per phase per player type). AI-vs-AI games still take the
  original M1 `play_turn()` fast path unchanged.
- **New `src/ui/tui/tui_render_playarea.c` and `tui_render_io.c`**: `tui_render.c` was
  split three ways (mirroring the `cli_display.c`/`cli_action_display.c` precedent) to
  stay under the file-size guideline after this milestone's additions -- board/hand/
  discard/combat-zone drawing moved to `_playarea.c`; the message log, input predicates,
  command-line drawing, and combat-details rendering (`tui_show_combat_details()`,
  TUI's equivalent of `display_combat_details_cli()`) moved to `_io.c`.
- **Shared mulligan/discard-to-7 grammar**: `game_process_mulligan_command()` /
  `game_process_discard_command()` added to `ui/interactive/game_commands.c`
  (mirroring the attack/defense split from Pass 1); `cli_game.c`'s
  `process_mulligan_command`/`process_discard_command` are now thin wrappers
  (CLI-only `help` interception, then delegate) -- same pattern as `cli_input.c`.
- **Bug found and fixed, both in `tui_render.c`**:
  1. **Blank-screen hang**: with the player-config menu now running its own
     `printf`/`fgets` prompts before `tui_screen_create()`, the very first
     `wrefresh()` on any of the independent `newwin()`-created panels became a
     silent no-op (returned OK, wrote nothing) because `stdscr` itself is never
     drawn to or refreshed and ncurses' physical/virtual screen sync was never
     seeded. Fixed with `fflush(stdout)` + one plain `refresh()` right after
     `initscr()`/color setup, before any panel is ever refreshed. (M1 never hit
     this because it called `initscr()` immediately, with no prior stdio output.)
  2. **Attacker/Defender status-bar labels inverted mid-turn**: `tui_role_label()`
     keyed off `gstate->turn_phase`, which `attack_phase()` (AI path) flips to
     `DEFENSE` partway through -- fine for M1 (only ever redrew once, after a full
     `play_turn()`, when `turn_phase` was always stale-`DEFENSE` in a way that
     happened to cancel out) but wrong once a human is actually watching mid-turn.
     Fixed by keying the label purely off `current_player` (always this turn's
     attacker until `end_of_turn()`), which needs no `turn_phase` reference at
     all. Also made `tui_handle_interactive_attack()` set
     `turn_phase = DEFENSE`/`player_to_move` itself (mirroring what
     `attack_phase()` does for AI), so the shortcuts-panel hint text stays
     correct in a Human-vs-Human game too, not just Human-vs-AI.
- Verified: `-a -p` regression identical; `test_recall`/`test_cash_exchange`/
  `test_combo` still 10/10, 6/6, 20/20; a full `tmux`-scripted human-vs-AI game
  (attack via PLAY-mode staging, AI auto-defense, combat display, AI attack,
  human defense via PLAY-mode staging, second combat display, COMMAND-mode
  `draw` command, graceful `q` quit) played correctly end-to-end; `tmux`-scripted
  valgrind pass on the same flow: 0 errors, 0 definitely/indirectly-lost bytes
  (same ncurses/terminfo "still reachable" pattern as M1's prior valgrind checks).
- Not yet done from the M2 handout (left for a future pass): visual highlighting of
  staged cards directly in the hand display (currently shown as a `[n,m]` list in the
  command-line row instead); a help overlay; TUI↔SIM mode switching (low priority,
  `stda.sim` doesn't exist yet either).

## 2026-07-23 — TUI Milestone 2, Pass 1: shared interactive command seam (`UiIO`)

Behavior-preserving refactor, no user-visible change yet -- lays the groundwork so
Milestone 2's human-vs-AI TUI can reuse the CLI's interactive rules instead of
duplicating them (see the "TUI Mode" section of `doc/oracle_todo.md`).

- **New `src/ui/shared/ui_io.h`**: a small `UiIO` function-pointer struct
  (`message`/`read_line`/`show_card_list`) that decouples the interactive command
  grammar from stdio. Board/state rendering is explicitly NOT part of this seam --
  each UI keeps rendering its own way (`cli_display.c` vs `tui_render.c`); only the
  three points where the shared rules used to touch stdio directly (feedback
  messages, blocking line reads, "show this titled card list") go through it.
- **New `src/ui/interactive/game_commands.c` + `game_commands_cards.c`**: the
  UI-agnostic command grammar and rules moved out of `ui/cli/cli_input.c` --
  attack/defense dispatch (`cham`/`draw`/`cash`/`pass`/`exit`), champion-play
  validation, and (in the `_cards.c` split, mirroring the `cli_display.c`/
  `cli_action_display.c` precedent) recall (draw/recall cards, exact-count) and
  cash exchange. Each function now takes a `UiIO*` instead of calling
  `printf`/`fgets` directly.
- **New `src/ui/cli/cli_io.c/h`**: the CLI's `UiIO` backend -- `message` maps to
  the existing ANSI color scheme, `read_line` to `fgets`, `show_card_list` to
  `display_card_with_power()` (reusing `select_champion_for_cash_exchange()` for
  the cash-exchange "suggested" marker instead of re-deriving it).
  `src/ui/cli/cli_input.c` is now a thin wrapper: it intercepts the CLI-only
  diagnostic commands (`gmst`/`shod`/`help`, which dump the full board/discard/help
  text and have no TUI equivalent yet) and delegates everything else to the shared
  grammar.
- **Relocated `ui/cli/cli_constants.h` to `src/ui/shared/ui_constants.h`**: it was
  already reached into from `ui/shared/player_config.c`/`player_selection.c`
  (a pre-existing sign it was misplaced), and the new shared `game_commands.c`
  needed it too.
- Verified: `-a -p` regression identical to `bin/expectedresults.txt`;
  `test_recall`/`test_cash_exchange`/`test_combo` still 10/10, 6/6, 20/20; all four
  `testsrc/cli_scripts/` canned scripts (recall, cash exchange, combat, discard)
  replayed with unchanged output; valgrind clean (0 leaks/errors) on the recall path.
- Next: Pass 2 wires a TUI `UiIO` backend (`ui/tui/tui_input.c`, `read_line` as a
  `getch()` line editor in the command window) and a human-turn branch in
  `stda_tui.c` for a command-mode-only playable human-vs-AI game.

## 2026-07-14 — TUI layout: shortcuts hint moved, vertical hand, discard corners

Further Milestone 1 polish.

- **Moved the "TAB to toggle play/command modes" hint into the Shortcuts panel**
  (merged with the existing "(context sensitive - M2)" note, wrapped via new
  `tui_print_wrapped()`); `win_command` is now just a bare `> ` prompt.
- **Player A's hand is now a vertical stack** (`tui_draw_hand_vertical()`, one card
  per row, matching the target PDF) instead of the horizontal wrapping row used
  elsewhere. All entries share one x position (centered on the widest entry) so the
  stack reads as a clean column instead of each line being independently (and
  raggedly) centered.
- **Each player's full discard pile now renders as a compact card grid**, growing
  from one corner of the table toward the vertical middle: Player B's grows down
  from the top-left (respecting the blank separator below its status bar), adding
  a new column to the right once a column fills; Player A's mirrors this exactly
  from the bottom-right corner, growing up, adding columns to the left. New shared
  `tui_draw_discard_column()` handles both directions via signed row/column steps,
  with a safety clamp so columns stop before crossing into the centered hand/deck
  /combat-zone content in the middle. Verified up to a 17-card / 2-column pile (B)
  and 14-card / 2-column pile (A) via `tmux`, both totals matching exactly.
- **Corrected an oversized assumption**: hand-related buffers/loops assumed up to
  10-12 cards; the game rule (`discard_to_7_cards()`, called every `end_of_turn()`)
  actually caps hand at 7, and M1 only ever renders after a full turn completes
  (never mid-turn) -- so 7 is a real, not defensive, bound. Tightened
  `tui_draw_hand()`/`tui_draw_hand_vertical()`'s arrays and loop caps from
  12 to 7 accordingly.
- Verified: `-a -p` regression identical; `test_recall`/`test_cash_exchange`/
  `test_combo` still 10/10, 6/6, 20/20; `tmux`-driven valgrind pass (0
  definitely-lost, same ncurses/terminfo pattern as before).
- **Known gap, discussed but not yet addressed**: `stda_tui.c` calls `play_turn()`
  in full per keypress, and `resolve_combat()` clears both combat zones before that
  call returns -- so `gstate->combat_zone` is always empty at draw time, meaning
  `tui_draw_combat_zone()`'s card-rendering path (as opposed to its "(0):" empty
  case) is not exercised by normal AI-vs-AI play under M1. Real coverage needs
  either a one-off synthetic/manual check or Milestone 2's finer-grained
  per-phase advancement (which would naturally pause after `attack_phase()`
  /`defense_phase()` while combat zones are populated).

## 2026-07-14 — TUI layout: mirrored status bars, combat-zone clustering, console wrap

Further Milestone 1 polish, still before starting Milestone 2.

- **Status bars now mirror across the screen's horizontal center line.** Player
  name is centered within the play-area ("table") width (previously the whole
  status line was left-jammed against column 0 of the full-width window, ignoring
  the info column alongside it); lunas/energy sit on the left edge of the table for
  both bars, status/role on the right edge for both bars (Player B's top bar
  previously had status/role on the left and lunas/energy on the right -- the
  opposite of the bottom bar). New shared helpers `tui_print_centered()` /
  `tui_print_3segment()` (moved into a new "Layout helpers" section, used by both
  the status bars and the play-area code) compute position from `pane_width`
  (`getmaxx(win_play)`), not the status window's own full-terminal width.
- **Both players' combat zones now cluster near the vertical middle**, next to the
  `-- combat zone --` divider, with hand/deck/discard pushed to the outer edges
  (near each player's own status bar) -- previously Player B's combat zone sat
  right under its hand/deck near the top, leaving a large blank gap before the
  divider, while Player A's deck/discard/hand sat right under its combat zone near
  the middle, leaving a large blank gap before the bottom status bar (backwards
  from what was intended).
- **One blank separator row** now sits between each status bar and the block next
  to it (top: below Player B's bar; bottom: above Player A's bar). Caught and fixed
  a bug during verification: the first pass put Player A's blank row in the middle
  of the reserved bottom block instead of as the very last row adjacent to the
  status bar -- a scripted `tmux` comparison against Player B's (correctly
  positioned) separator caught the asymmetry.
- **Console messages now wrap instead of truncating.** New
  `tui_build_console_segments()` wraps the most recent messages (bounded lookback)
  into fixed-width segments in chronological order; the display then takes just the
  tail segments that fit the console's height, same "recent window" logic as
  before but at wrapped-line granularity instead of raw-message granularity.
- Verified via `tmux` at 140x45 and the user's actual 281x65 Konsole size, plus the
  full regression/test/valgrind pass (identical `-a -p`, 10/10, 6/6, 20/20, 0
  definitely-lost).
- **Watch item**: `tui_render.c` is now 602 lines, over the 500-line soft limit (not
  the 1000-line firm one). Deferred splitting it while the layout is still being
  actively iterated on (per `cli_display.c`'s precedent, split once feature work in
  this file settles rather than mid-iteration).

## 2026-07-14 — `-h` usage text: added an Examples section

`print_usage()` (`src/main/cmdline.c`) now ends with 3 real-world usage examples (the
most common invocations so far): `-a -p` (automated AI-vs-AI, fixed seed), `-l -u=fr`
(interactive CLI, French UI), `-t -u=fr` (TUI, French UI). Along the way, confirmed
`-u=fr` (short option with `=`) actually works: `getopt_long_only` matches single-letter
names against the long-options table too (`"u"` is registered there), so it splits on
`=` the same way `--ui.lang=fr` does — not just a short-option-attached quirk.

## 2026-07-14 — `-A`/`--ai` now lists AI-agent shorthands instead of erroring

`./oracle -A` previously required an argument via getopt and just threw an unhelpful
getopt error if omitted. Now:

- `-A`/`--ai` takes an **optional** argument. Bare `-A` (or `--ai`) prints the list of
  11 agent shorthands (same roster as the CLI's `display_ai_strategy_menu()`) and exits
  cleanly (exit 0); an unrecognized value (`-Afoo`) prints an error plus the same list
  and exits with failure (exit 1); a valid shorthand proceeds to `MODE_CLIENT_AI` as
  before (still an unimplemented stub).
- New shorthand table (lowercase, letters/digits, <=10 chars each), case-insensitive
  matching: `rand`, `value`, `greedy`, `combo`/`borealis` (two aliases for the same
  agent, A4), `balanced`, `heuristic`, `hbt`, `hbt2ply`, `simplemc`, `ismcts`,
  `ismctsnn`. Implemented as `parse_ai_strategy_shorthand()` /
  `print_ai_agent_shorthand_list()` in `src/ui/shared/player_config.c/h` (reusing the
  existing `AIStrategyType` enum and `get_strategy_display_name()` rather than
  duplicating a second list), called from `src/main/cmdline.c`'s `case 'A':`.
- `print_usage()`'s `-A` entry now matches the `=[VALUE]` convention already used by
  `-u`/`-p` (optional args) and `-i`/`-o` (required args), plus an explicit note that the
  argument must be attached (`-Afoo`/`--ai=foo`), not space-separated — same
  getopt-driven limitation `-u`/`-p` already have, just undocumented there.
- Verified: all four combinations (bare, valid, invalid, both short/long forms) behave
  as designed; `-a -p` regression identical; `test_recall`/`test_cash_exchange`/
  `test_combo` still 10/10, 6/6, 20/20.

## 2026-07-14 — TUI layout: centered play-area content ("table" feel)

Follow-up polish on Milestone 1 before starting Milestone 2. The play area previously
left-justified every label and card row at column 1 of `win_play`, so on any terminal
wider than the bare minimum the whole right side of the play area was empty space —
didn't read as a card table the way the target PDF/xlsx layout does.

- `src/ui/tui/tui_render.c`: added `tui_print_centered()` (single-line labels) and
  `tui_draw_card_row()` (a shared, wrapping, per-row-centered layout for both
  `tui_draw_hand()`'s and `tui_draw_combat_zone()`'s card lists, via a small `TuiCardCell`
  struct so both call sites build pre-formatted cells and hand them to one layout
  routine instead of duplicating the wrap/measure logic). Hand/combat-zone headers,
  deck/discard counts, and the `-- combat zone --` divider are now all horizontally
  centered in the play window; card rows are centered as a block per row too.
- Verified visually via `tmux` (now installed) at several sizes, including the practical
  minimum (100x30) and the user's actual full-screen Konsole size (281x65) — confirmed
  the "please enlarge" fallback and the 100x30 minimum both work correctly (an earlier
  live-resize report of needing 143x43 turned out to be Konsole not having reached
  100x30 yet mid-drag, not a bug).
- Re-verified: `-a -p` regression identical, `test_recall`/`test_cash_exchange`/
  `test_combo` still 10/10, 6/6, 20/20, and a `tmux`-driven valgrind pass (0
  definitely-lost, same ncurses/terminfo "possibly lost" pattern as Milestone 1).

## 2026-07-14 — TUI mode Milestone 1 (ncurses display skeleton)

`stda.tui` (`-t`/`--stda.tui`) is real: an ncurses text UI matching the target layout in
`Template TUI Game Interface.pdf`/`Gabarit Interface de Jeu Version Texte.xlsx` (2/3 play
area + 1/3 info column, mirrored top/bottom status bars, scrolling console). Milestone 1
is **display-only** — AI-vs-AI, one turn advances per keypress, no human interaction yet
(that's Milestone 2, see `doc/oracle_todo.md`).

- New `src/ui/tui/tui_render.c/h`: all ncurses window layout + drawing, fully responsive
  (`tui_layout()` recomputes every window from the live terminal size, handles
  `KEY_RESIZE`, shows a "please enlarge terminal" fallback below `TUI_MIN_COLS`x
  `TUI_MIN_ROWS` (100x30) and recovers cleanly once resized back up).
- New `src/roles/stda/stda_tui.c/h`: the real `run_mode_stda_tui()`, reusing
  `initialize_cli_game()`/`cleanup_cli_game()`/`apply_mulligan()` and driving `play_turn()`
  once per keypress; `q`/`Q` quits.
- `makefile`: added `-lncursesw` to `LIBS`.
- `game_constants.c/h`: added `CHAMPION_SPECIES_ABBR[]` (3-letter card labels), matching
  `CHAMPION_SPECIES_NAMES`'s existing English-only convention (species names aren't
  localized elsewhere in the codebase either).
- **Two real bugs found via testing, both fixed**: (1) the top status bar duplicated the
  PDF mockup's literal "Actif / En attente" header text instead of resolving it to a
  single computed label per player (copy-paste artifact caught by a scripted PTY
  walkthrough); (2) `setup_game()` never initializes `gstate->turn_phase`/
  `player_to_move` (only `begin_of_turn()` does) — the CLI never notices because it always
  runs `begin_of_turn()` before displaying anything, but the TUI draws once before the
  first `play_turn()` call, so `stda_tui.c`'s setup now sets both fields explicitly
  (caught by valgrind as an uninitialized-value read).
- **ncurses/`ChampionColor` naming collision** (`COLOR_RED` is both an ncurses macro and
  this codebase's own enum constant): `tui_render.h` never includes `<ncurses.h>` (forward
  -declares `WINDOW` as opaque); `tui_render.c` includes `game_types.h` first, then
  `<ncurses.h>`, then `#undef COLOR_RED`, using its own `NC_RED`/etc. constants for
  `init_pair()`. See `CLAUDE.md`'s "Known architectural gaps" for the durable note.
- Also fixed a related loop bug: pressing a key while the terminal was below the minimum
  size used to silently advance a game turn with nothing visible to show for it; now
  ignored until resized back up.

Verified via `make clean && make` (no new warnings), `./bin/oracle -a -p` regression
(identical), `make test_recall`/`test_cash_exchange`/`test_combo` (10/10, 6/6, 20/20), a
scripted PTY walkthrough (`python3` + the `pty` module) driving `stda.tui` through several
turns, a resize up/down/below-minimum/recovery cycle, and FR localization, plus a
valgrind pass (0 definitely-lost bytes; the only "possibly lost" blocks trace entirely
into `libncursesw`/`libtinfo` terminfo internals, a well-known false-positive pattern, not
Oracle's own code).

## 2026-07-14 — Source folder structure cleanup (pragmatic pass)

Pragmatic pass only (not the full v4 engine rewrite) — see
`doc/done/1 improve source code folder structure/pragmatic_cleanup_implementation_plan.md`
for full detail.

- Split `cli_display.c` (576 lines) into `cli_display.c` (233 lines, core status/turn
  display) + new `cli_action_display.c` (357 lines, action-flow/card-selection display)
  — both now under the 500-line soft limit.
- Fixed `make test_combo`: stale include paths, stale Makefile paths, removed a test of
  the since-removed `get_order_from_species()`, and fixed a latent test bug where
  `CombatCard` literals left the (later-added) `.order` field zero, causing spurious
  order-match bonuses — now 20/20 passing.
- Doc sync: `CLAUDE.md` module layout / file-size / test-status notes updated.

Verified via `make clean && make` (no new warnings), `./bin/oracle -a -p` regression
(identical to `bin/expectedresults.txt`), `make test_recall`/`test_cash_exchange`/
`test_combo` (10/10, 6/6, 20/20), `testsrc/cli_scripts/` re-run, and a full valgrind pass
(0 errors/0 leaks, auto + interactive).

## 2026-07-14 — CLI AI-strategy menu synced with planned agent roster

`display_ai_strategy_menu()`/`get_ai_strategy_choice()`/`get_strategy_display_name()` in
`src/ui/shared/player_config.c` and the `AIStrategyType` enum now list all 11 planned
agents (`A1`-`A11`, skipping `A2` since parameter storing/optimization is calibration
tooling, not an agent) as "not yet implemented" stub menu entries, in `ideas/A#` order,
each with a comment cross-referencing its `ideas/A#` folder. `A4`'s menu entry is
explicitly labeled "Combo Aware [Borealis benchmark]". The former "Hybrid" entry is
confirmed to be `A7` (tactical+HBT: Heuristics+Balanced+Tactical) and is now labeled
"Hybrid (HBT)".

## 2026-07-14 — `ideas/` folder renumbering

Folders were renumbered twice in one session: first to flatten decimal numbers (`12.1`,
`14.3`, etc.) into plain integers, then to pull all AI-agent folders into their own
`A1`-`A11` namespace (kept in their existing relative order) so adding new AI ideas
doesn't require renumbering everything else. `ismcts_nn_overview.md` became its own
folder (`A11`) since it also covers the NN+MCTS extension, distinct from plain IS-MCTS
(`A10`). See `git log` if an old number (e.g. `ideas/8/`, `ideas/14.3/`) shows up in an
older doc or commit message.

## 2026-07-13 — Turn Logic Module: recall, cash exchange, combat display, discard display

Complete Turn Logic Module: full game loop working end-to-end in interactive mode with
all the rules.

- **Display Discard Pile in CLI Mode** — `gmst` (summary) and `shod` (detailed,
  power-sorted) commands; see `doc/done/4 ...`.
- **Recall Card functionality in stda.cli mode** — recall is **exact and mandatory** (a
  "recall 1 / draw 2" card recalls exactly 1 champion, "recall 2 / draw 3" recalls
  exactly 2; recall is only offered when discard holds enough champions). The Random AI
  engine still only ever draws (never recalls), which is fine given it's not meant to be
  strong. See `doc/done/2 ...`, `doc/game_rules_doc.md` (recall section corrected to
  match), and `testsrc/test_recall.c`. Implementation: `validate_and_recall_champions()`
  + `handle_recall_choice()` in `cli_input.c`, UI via `display_recallable_champions()`.
- **Combat results display in stda.cli mode** — per-champion rolls/base/combo/damage
  breakdown, shown whenever a human is involved; `stda.auto` unaffected. See
  `doc/done/3 ...`. Implementation: `display_combat_details_cli()` in
  `ui/cli/cli_display.c` (now `cli_action_display.c` after the 2026-07-14 split).
- **Cash card champion selection in interactive mode** — ask user to select the champion
  card to exchange instead of the AI power-heuristic auto-pick; interactive path
  (`play_cash_card_interactive`) lets the human pick freely. Along the way, fixed a real
  bug in the AI heuristic (`select_champion_for_cash_exchange` conflated "not found"
  with card index 0, a valid champion, using it as a sentinel — now uses `UINT8_MAX`).
  See `doc/done/5 ...` and `testsrc/test_cash_exchange.c`.

**Note**: fixing the index-0 sentinel bug changed `stda_auto`'s RNG-dependent play
sequence (different AI hand state whenever that bug used to fire), so
`bin/expectedresults.txt` was regenerated (2026-07-13) to reflect the corrected behavior
— this was a deliberate re-baseline, not a regression.

All four verified via `make test_recall` / `make test_cash_exchange`, the
`testsrc/cli_scripts/` manual scripts, a full valgrind pass (auto + interactive), and the
`./bin/oracle -a -p` regression check against the regenerated `bin/expectedresults.txt`.
