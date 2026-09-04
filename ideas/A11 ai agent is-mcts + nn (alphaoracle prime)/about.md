# A11 — IS-MCTS + NN · "AlphaOracle Prime" (identical in EN / FR / ES)

| | |
|---|---|
| Enum | `AI_STRATEGY_ISMCTS_NN` |
| Shorthand | `ismctsnn` |
| Est. Borealis rating | 97 — original design-intent estimate; **measured 74** (2026-09-02), the new roster ceiling |
| Source files | `src/ai_strat/ai_strat_ismctsnn.{c,h}`, `ai_strat_ismctsnn_net.{c,h}`, `ai_strat_ismctsnn_state.{c,h}` |
| Status | **done and registered** (2026-09-03) — both ship gates PASS, weights shipped at `assets/ismctsnn/prime_657k_weights.bin`, loaded by default at startup |

## The one thing this agent does

Replaces `A10`'s random rollouts and UCT-only selection with a trained policy/value
network: the policy head focuses tree search on promising moves, the value head replaces
rollout-to-terminal simulation with a direct position estimate, trained via AlphaZero-style
self-play. "Search plus learned intuition" (names file).

## Deliberately out of scope

- Hand-written heuristics anywhere in the hot search path — if a signal matters, it should
  be learned by the network from self-play data, not encoded by hand (that's the entire
  distinction from `A4`–`A9`).
- Skipping `A10`. This agent is explicitly staged as "IS-MCTS Foundation" first,
  "Neural Network Integration" second — see design sources.
- Any part of the tree/determinization machinery diverging from `A10`'s — this agent
  changes *what guides* the tree, not the tree/determinization structure itself.

## Design sources

- `nn_mcts_overview.md` (this folder) — network architecture (policy/value heads),
  self-play training loop, four-stage implementation path, and the IS-MCTS vs NN+MCTS
  comparison table. Its "Expected Training Requirements" section assumes GPU-scale
  (RTX 4090/KataGo) training — superseded for this project's actual hardware by
  `local_training_plan.md` (this folder).
- `local_training_plan.md` (this folder) — hardware-grounded training plan for the
  actual local machine (no GPU): deployed-model sizing, why disk/corpus size isn't
  the real constraint, the single-pass-distillation-from-A10 recipe, and the
  reshuffle-aware information-set encoding note (composition becomes known, order
  stays hidden).
- `../A10 ai agent is-mcts (the omniscient)/ismcts_overview.md` — the prerequisite
  foundation this agent builds on.
- `../A13 ai agent cartographer (the cartographer)/about.md` — shelved, but its
  calibration record is the direct precedent for the ship-gate/trust-dial/staging
  decisions below (see "Confirmed plan").

## Confirmed plan (2026-09-01, scoping discussion before implementation starts)

Decided against Cartographer's record specifically — A13 (a closed-form agent, not
NN-based) hit a repeated "trust dial" failure signature (`A9`'s `reply_trust`, `A13`'s
`hplus_trust`: win rate declining *monotonically* as trust in a new mechanism rises,
with trust=0 the actual optimum) and taught that a roster-wide Bradley-Terry rating
alone doesn't establish a win against the specific agent a design is built on. Both
lessons apply directly to an NN blended into `A10`'s tree.

**Staged scope — value net first, policy head gated on results.** Stage 1 ships (or
doesn't) before Stage 2 is attempted at all:

1. **Self-play data generation.** No action-encoding problem needed yet (value-only)
   — log `(info-set state, outcome)` pairs from `A10` (its shipped
   `limit_iterations=4000`) games, logged from `A10`'s own decision points only
   (never the opponent's), across a **curated opponent pool, not pure mirror
   self-play**: `A10` vs itself, vs `A7` (Grandmaster — the strongest deterministic
   synthesis, structurally nothing like tree search), and vs `A3` (Borealis — the
   fixed rating anchor, stable and well-understood positions). Deliberately excludes
   `A5` as an explicit opponent despite being a strong closed-form agent in its own
   right — it's already `A10`'s own internal rollout policy, so it adds little
   opponent diversity beyond what's already baked into every `A10` decision. This is
   the training-corpus analogue of `A10`'s own Phase 6 finding: rollouts that always
   modeled the opponent as `AI_STRATEGY_RANDOM` produced value estimates
   "well-calibrated against `rand` specifically," which was the dominant bottleneck
   until fixed — a value net trained only on `A10`-mirror games risks the same
   narrow-calibration failure against genuinely different playstyles. Self-play
   generation is embarrassingly parallel either way, so broadening the opponent pool
   costs little relative to the risk it heads off. State features build on
   `ai_strat_a13_belief.c/h` (kept specifically as reusable
   feature-extraction infrastructure for this) rather than re-deriving unseen-pool/
   hypergeometric/reshuffle-boundary reasoning from scratch. Value target reuses
   `mc_outcome_for()`'s existing 0.0/0.5/1.0 convention, not a second -1/0/+1 scale.
   Reshuffle-aware determinization stays deferred, same as `A10` — the 0/8000
   real-non-random-game finding applies equally to this self-play corpus.
2. **C inference + integration.** Training stays Python/PyTorch, offline (settled,
   see `local_training_plan.md`). Inference is a **hand-written plain-C forward pass**
   — weights exported as a raw float array, `rating_csv.c`-style persistence
   precedent, no new external runtime dependency, stays portable to MSYS2/Windows,
   matches the project's manual-code-over-macro-magic convention (the net is small
   enough, ~1M params, for this to be a modest amount of code). The NN value directly
   evaluates a newly-expanded leaf, **replacing** the to-terminal rollout; UCT
   selection is untouched — no PUCT yet. A `nn_value_trust` blend dial (0.0 = pure
   `A10` rollout result, bit-for-bit recoverable; 1.0 = pure NN value) gives the same
   superset-guarantee safety net `A7`→`A9`/`A13` used, and gets swept exactly like
   `reply_trust`/`hplus_trust` — watch for the same monotonic-decline signature.
3. **Measurement — two gates, not one** (direct `A13` precedent): (1) roster
   Bradley-Terry rating, context only; (2) **head-to-head vs `ismcts` (`A10`), both
   seats, large n, Wilson CI lower bound > 50%** — the real bar. Also re-run a
   budget-vs-iterations curve, since NN-eval-per-leaf has different cost than
   heuristic-rollout-to-terminal.
4. **Policy head + PUCT — gated, not automatic.** Only attempted if Stage 3 clears
   both gates. This is where the action-encoding problem (fixed-size logits over
   hand-card-slot "include in subset" plus pass/draw/recall/cash-target, masked to
   `get_available_moves()`'s legal set) actually needs solving — no point solving it
   for a value net that hasn't proven itself.

**Null-result policy (Jonathan's call, 2026-09-01, matching `A13`'s precedent
exactly):** if `nn_value_trust` calibrates to 0, or measures at parity with `A10`
head-to-head, **shelve rather than register** — a redundant clone of `A10` would
pollute the Bradley-Terry fit even without being actively harmful, the same reasoning
`A13` was shelved under. Source stays on disk as reference either way.

## Progress (2026-09-02): Stages 1-2 built and verified; Stage 3 sweep promising

**Stage 1** (`ai_strat_ismctsnn_state.h/.c`, `aicalibsrc/ismctsnn/gen_corpus.c` +
`run_selfplay.sh`, `train_value_net.py`) and **Stage 2** (`ai_strat_ismctsnn_net.h/.c`,
`ai_strat_ismctsnn.h/.c`, `export_weights.py`) are implemented and verified — 537-float
state encoder locked in, corpus generator with a headerless flat-float32 record format,
a small MLP (537→256→128→64→1, BatchNorm+dropout) trained on a 1-hour/12-worker pilot
corpus (657K records), hand-written C forward pass verified to `2.4e-7` max diff against
the live PyTorch model (BatchNorm fused into `Linear1` at export time), and the
superset guarantee (`nn_value_trust=0.0` bit-for-bit identical to `A10`) confirmed
empirically via byte-identical `stda.auto` output, not just by code inspection.

**A real methodological gotcha, worth remembering for any future harness in this
shape:** the first `calib_ismctsnn.c` copied `A13`'s "set params on both registries,
harmless if unread" pattern from `calib_a13.c`. That's unsafe here specifically —
`A10`/`A11` share one `ISMCTSParams` struct and one `ismcts_search_best_move()`
function (unlike `A13Params`/`HBTParams`, genuinely disjoint), so an "ismcts" seat
was silently inheriting whatever `nn_value_trust` sat in its parsed params block
(the sweep/validate baseline's `DEFAULTS` carries `nn_value_trust=1.0`). Every
measurement before the fix was invalid. Fixed by forcing `nn_value_trust=0.0f`
specifically on the copy passed to `ismcts_set_params()`. **Lesson: "harmless to set
params another agent won't read" only holds when the structs are actually disjoint —
verify, don't assume, whenever two agents share one struct/function.**

**Timing reality** (measured, not estimated): the hand-written C forward pass
(unoptimized `-Og`, no SIMD, ~179K params) costs **~0.504s mean per decision at
`nn_value_trust=1.0`**, called up to `limit_iterations=4000` times/decision — about
**16x** `A10`'s own plain-rollout cost (~0.031s/decision, confirmed to match `A10`'s
documented `calib_ismcts_timing` figure exactly at `trust=0.0`). The NN forward pass
itself dominates: `trust=1.0` (rollout skipped entirely) costs about the same as
`trust=0.5` (both rollout and NN paid), so the rollout being replaced is cheap by
comparison. In a real asymmetric match (`ismctsnn` vs `ismcts`, only one seat paying
the NN cost) under real 15-worker concurrent contention, a full 260-game match at
`trust=1.0` measured **~7.5s mean per game** (up from an isolated small-sample
estimate of ~6.3s/game — see the concurrent-worker-timing lesson: real multi-worker
throughput consistently runs behind naive small-sample extrapolation on this
machine). This is exactly the "NN-eval-per-leaf has different cost than
heuristic-rollout-to-terminal" question flagged above — a real budget-vs-iterations
re-tune (or a compiler-optimization pass on the hot loop, currently unoptimized) may
be worth revisiting once Stage 3's gates are settled.

**Stage 3 exploratory sweep** (`nn_value_trust` vs `ismcts`, both seats, n=1040
games/value, 95% Wilson CI — a cheap first look, not the final Gate 2 measurement):

| `nn_value_trust` | wins | n | win rate | 95% CI |
|---|---|---|---|---|
| 0.00 | 520 | 1040 | 0.5000 | [0.4697, 0.5303] |
| 0.10 | 515 | 1040 | 0.4952 | [0.4649, 0.5255] |
| 0.25 | 592 | 1040 | 0.5692 | [0.5389, 0.5990] |
| 0.50 | 595 | 1040 | 0.5721 | [0.5418, 0.6019] |
| 0.75 | 605 | 1040 | 0.5817 | [0.5515, 0.6114] |
| 0.90 | 607 | 1040 | 0.5837 | [0.5534, 0.6133] |
| 1.00 | 626 | 1040 | 0.6019 | [0.5718, 0.6312] |

Win rate **rises monotonically** with trust (aside from a flat/noise point at 0.1) —
the mirror image of `A9`'s `reply_trust`/`A13`'s `hplus_trust` decline signature, not
a repeat of it. From `trust=0.25` on, every point's CI lower bound already clears
50% even at this modest n. `trust=1.0` is both the strongest performer measured and
the cheapest to run (no rollout paid at all).

## Stage 3 result (2026-09-02): both gates PASS at nn_value_trust=1.0

Full-rigor `validate --trust 1.0 --numsim 137 --replicates 15` (30 evenly-divisible
jobs across 15 workers -- 2 full rounds, no idle capacity), n=4,110 games each,
target half-width 1.5pp:

- **Gate 2 (the real bar) — vs `ismcts` (`A10`), both seats**: baseline
  (`trust=0.0`) read exactly 0.5000 (superset guarantee reconfirmed at this n);
  candidate (`trust=1.0`) **58.44%**, 95% CI **[56.93%, 59.94%]** (half-width
  ≈1.5pp). **PASS** — Wilson CI lower bound clears 50%.
- **Gate 1 (context) — vs `borealis` (`A3`), same structure**: baseline
  (`trust=0.0`, i.e. plain `A10`) read **67.88%** [66.44%, 69.29%] — matches `A10`'s
  own documented measured rating (67.6-69.5) almost exactly, a free cross-check that
  the harness measures correctly, not just internally consistently. Candidate
  (`trust=1.0`) **74.04%**, 95% CI **[72.68%, 75.36%]** (half-width ≈1.34pp) —
  on this project's rating convention (rating = win% vs `Borealis`), that's an
  estimated **Borealis rating of ~74**, about +5 over `A10`'s 69.

This is **not** the null result the "Null-result policy" section above was written
for — `nn_value_trust` did not calibrate to 0, and the agent measures a real,
well-powered win over `A10`, not parity. `ismctsnn_get_default_params()`'s existing
default (`nn_value_trust=1.0`) is already the best-performing point tested, so no
further trust-value tuning is indicated before considering this ready to register
for real.

**Registered 2026-09-03** — see "Next session's work" below for the three items this
closed (all done: rating table, weight packaging, default load path) and
`doc/changelog.md`'s 2026-09-03 entry for the full record, including a related latent
bug the load-path wiring surfaced and fixed (`g_params[]` needed an explicit
promotion to this agent's own default on a successful load, or real play would have
stayed silently at plain `A10` even with weights loading correctly).

## Next session's work: promote AlphaOracle Prime to the roster (2026-09-02) — DONE 2026-09-03

Jonathan's stated intent: do items 1-3 below for real, promoting the agent trained
on the current 1-hour/657K-record pilot corpus (`checkpoints/pilot_c_weights.bin`,
the exact weights measured above — 58.44% vs `A10`, ~74 Borealis rating) to a
properly-registered roster agent, as it stands right now, without waiting for a
bigger corpus first. Item 4 is real future work but explicitly gated/not urgent.

**Naming decision (discussed 2026-09-02, resolving Jonathan's "not overly
technical" concern about KataGo/KataHex-style build-string names):** don't encode
corpus size or algorithm generation (UCT vs PUCT) directly in the player-facing
flavor name — that information belongs to training provenance, not to what a player
picks from a menu. Concretely:

- **Flavor name stays "AlphaOracle Prime"** for the whole UCT + value-net lineage
  (Stages 1-3), unchanged, indefinitely — this is what ships now.
- **If/when Stage 4 (PUCT + policy head) ever ships as a genuinely different search
  algorithm**, name it **"AlphaOracle Prime Plus I"** (revised 2026-09-04, Jonathan's
  call — a distinct "Plus" lineage rather than "Prime II", since this changes the
  search mechanism itself, not just one added lookahead ply the way `A7`→`A9`
  "Grandmaster"→"Grandmaster II" did; "Plus I" leaves room for a later "Plus II" if
  a further PUCT-generation iteration ever ships). A player doesn't need "UCT" vs
  "PUCT" spelled out; "Plus" already signals "meaningfully upgraded successor," which
  is the actual high-level nature that matters at the menu.
- **Corpus size / training provenance is documentation metadata, not UI text** — it
  lives here (`about.md`), in `doc/changelog.md`, and in the weights filename itself
  (rename target below), never in `strategy_menu_label()`/`get_strategy_display_name()`.
- If a *visible* training-generation hint is ever wanted later (e.g. once a second,
  bigger-corpus weights snapshot exists to actually distinguish from), the lightest
  fit is a qualifier appended inside the existing `<technical> [<flavor>]` menu
  bracket (`player_config.c`'s `format_menu_name()`) — e.g.
  `IS-MCTS + Neural Network [AlphaOracle Prime · Pilot]` — not a change to the
  flavor name itself. Not needed yet: only one corpus generation exists today.

**Item 1 — DONE 2026-09-03.** `src/ui/shared/player_config.c`'s
`AI_STRATEGY_RATINGS[AI_STRATEGY_ISMCTS_NN]` currently reads `{ 97, false }`
(pre-measurement design-intent estimate). Change to the real measured value —
`{ 74, true }` (the Gate 1 Borealis-rating estimate above; round/adjust if a final
larger-n confirmation run is preferred first). This is also where `A10` (`{ 69,
true }`) and every other shipped agent's real measured rating already lives, so
`AlphaOracle Prime` at 74 becomes the new roster ceiling once this lands.

**Item 2 — DONE 2026-09-03.** Path decided
(2026-09-02): a new top-level `assets/` directory, sibling to `src/`/`bin/`/`doc/`
— not inside `src/`, which is exclusively human-authored `.c`/`.h` source in this
project (see `CLAUDE.md`'s module layout); a binary blob there would break that
invariant for no benefit. Matches the project's existing pattern of giving each
file category its own top-level home (`aicalibsrc/`, `testsrc/`, `doc/`, ...).
**Not single-purpose**: `assets/` is deliberately category-scoped per subfolder
(`assets/<category>/...`) rather than a flat dump, since it will also host the
future SDL3 GUI's champion artwork (`assets/champions/` or similar, per
`doc/oracle_roadmap.md`'s "Next Up" item 6) — `assets/ismctsnn/` is this agent's
slice of that shared directory, not the whole thing. Concrete sub-steps:
1. Ship the *current* pilot-trained weights as-is (already measured, real,
   strong — 74 rating), not blocked on a bigger corpus first — Jonathan's
   explicit call.
2. Move/rename `aicalibsrc/ismctsnn/checkpoints/pilot_c_weights.bin` to
   **`assets/ismctsnn/prime_657k_weights.bin`** (agent-scoped subdirectory,
   mirroring `aicalibsrc/<agent>/`; filename carries the flavor name + a
   corpus-size hint at the asset/internal level, never the player-facing display
   name — see naming decision above). Add a sidecar,
   **`assets/ismctsnn/prime_657k_weights.json`** (or `.md`), recording corpus
   composition (mirror/vs_a7/vs_a3 split), training date, and the measured Gate
   1/2 numbers — a compact spec sheet a manual entry or future in-game "About
   this AI" panel can read directly, distinct from this file's full narrative.
3. Un-gitignore exactly that new path (`aicalibsrc/ismctsnn/checkpoints/` etc.
   stay excluded — see `.gitignore`).
4. Item 3's default load path is `assets/ismctsnn/prime_657k_weights.bin`
   relative to repo root — same relative-path assumption `bin/expectedresults.txt`
   already relies on.

(All four sub-steps done as planned: 715,780-byte file confirmed byte-for-byte
against `reg_strong_value_net.pt`'s export output, `.json` sidecar written, no
`.gitignore` change needed since `assets/` was never ignored.)

**Item 3 — DONE 2026-09-03.** `ismctsnn_load_weights()` used to require an explicit
call (only calibration harnesses did this); nothing in `stda_auto.c`/CLI/TUI startup
called it, so selecting `ismctsnn` from a menu silently fell back to safe `trust=0`
(plain `A10`) behavior rather than real `AlphaOracle Prime` play — see
`ai_strat_ismctsnn.h`'s own documented defense-in-depth design. Landed as: a call to
`ismctsnn_load_weights()` in `main.c` before mode dispatch (covers every mode in one
place), with a new `--ai.weights=PATH` override (`cmdline.c`) and
`ISMCTSNN_DEFAULT_WEIGHTS_PATH` otherwise. Failure behavior: one `stderr` warning
line, silent fallback to `trust=0` (the existing per-decision fallback in
`decide_and_apply()` already made that the safe default, so no new logic was needed
there). **A related latent bug surfaced and fixed while landing this**: `g_params[]`
started at plain `ISMCTS_DEFAULTS` (`nn_value_trust=0.0f`) and nothing outside
calibration harnesses ever called `ismctsnn_set_params()` to change that — so even
with weights loading correctly, real play would have stayed silently at plain `A10`
forever. Fixed by having `ismctsnn_load_weights()` promote `g_params[]` to
`ismctsnn_get_default_params()` (`nn_value_trust=1.0f`) on a successful load, and
changing `ismctsnn_reset_params()` to reset to this agent's own default rather than
`A10`'s — each agent's "reset" now means "back to my own shipped default,"
consistent with `ismcts_reset_params()`'s existing meaning for `A10`.

**"Bigger training corpus" follow-up — attempted and falsified, 2026-09-04.** The
roadmap's stated premise (heavy regularization needed + val MSE "not yet plateaued" =
data-starved net) was tested directly rather than assumed, on two independent axes, both
against the shipped net's own pinned validation shards:

- **Volume**: a learning curve on the existing pilot corpus (60K/120K/240K/360K/488K
  training records) flattens by ~240-360K — the full 488K pilot set is statistically
  indistinguishable from 360K. Confirmed this isn't a regularization artifact: relaxing
  `dropout`/`weight_decay` at the same 488K record count made val MSE *worse*
  (0.170512 → 0.175361 → 0.177736 as regularization was removed), so the shipped
  hyperparameters were already close to the right point.
- **Opponent diversity**: extended `gen_corpus.c` with two new curated opponents,
  `vs_a4` (Balanced Rules, rating 36) and `vs_a6` (Tactical, rating 52) — both clearing
  Jonathan's Borealis-rating≥35 floor for "a useful proxy for real play" — generated
  214,660 more records at the real `limit_iterations=4000` budget, and retrained on
  pilot+widen combined (702,976 training records, +44%). Val MSE: 0.170579 — no
  improvement over pilot-alone.

**Verdict: stop, no retrain, no new agent** — see `doc/changelog.md`'s 2026-09-04 entry
for the full record. The ceiling looks like genuine outcome variance in close self-play
(`mirror` games measure far less predictable than `vs_a3`), not a data gap. The tooling
additions (`gen_corpus.c`'s `vs_a4`/`vs_a6`, `run_selfplay.sh`'s matchup-list parameter,
`train_value_net.py`'s multi-label/`--val-seeds`/`--max-train-records` flags) stay as
reusable infrastructure — this data-size-curve check is worth re-running before any
future large generation commitment, not just for this agent. The shipped weights
(`assets/ismctsnn/prime_657k_weights.bin`, rating 74) are unchanged, and "AlphaOracle
Prime Plus I" stays reserved for Stage 4 below, exactly per the naming decision above.

**Item 4 — Stage 4 (PUCT + policy head) — gated, not urgent.** Now technically
unlocked (Stage 3 cleared both ship gates), but entirely undesigned: needs the
action-encoding problem solved (fixed-size logits over hand-card-slot subsets plus
pass/draw/recall/cash-target, masked to `get_available_moves()`'s legal set — see
the "Confirmed plan" section above), a policy-head architecture/training-data
change (visit-count targets, not just outcome), and PUCT selection replacing plain
UCT in `ai_strat_ismcts_search.c`. Not scheduled; revisit only after items 1-3 ship
and there's appetite for a genuinely new project phase.
