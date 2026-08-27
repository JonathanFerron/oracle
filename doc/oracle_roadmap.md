# Oracle Development Roadmap

**Project**: Les Champions d'Arcadie / The Arcadian Champions of Light
**Type**: Open source hobby/research project
**Focus**: Strategic dueling card game AI research, C programming patterns, game
architecture

**Scope of this document**: long-horizon phases, ordering, and status-at-a-glance. For
actionable near-term checkboxes see `doc/oracle_todo.md`. For a dated history of finished
work see `doc/changelog.md`. For architecture/design rationale see `doc/oracle_design.md`.

---

## Current Status

Core game engine, CLI interactive mode, and TUI mode (Milestones 1 & 2 plus a polish
pass) are done. Random, `A1` Value Based ("The Apprentice"), `A2` Combo Threshold
("The Showboat"), `A3` Borealis (the Bradley-Terry benchmark), `A4` Balanced Rules
("Bean Counter"), `A5` Heuristic ("Eps-Gam-Del"), `A6` Tactical ("Pressure
Cooker"), `A7` Hybrid HBT ("The Grandmaster"), and `A8` Simple Monte Carlo ("The
Soothsayer") AI strategies are all implemented and calibrated, and the Bradley-Terry
rating system itself is now built on top of them. `A5` (rating 61 in the current
roster fit), `A6` (rating 53), and `A7` (rating 62, the highest measured so far) all
measure above the Borealis anchor; `A8` measures 35 — legitimately below it and not
raised by extra rollout budget, per its own diagnosis (`doc/changelog.md`) rather
than a defect. `A12` Clairvoyant ("The Clairvoyant") — `A8`'s sibling, not part of the
`A1`-`A11` ladder's authoritative order — was also implemented as a side exploration,
measuring 31; kept in the roster as a deliberately modest agent, not pursued further.
**Active work**: `A10` IS-MCTS — see "Next Up" below.

### Recently Completed

- **2026-08-26** — `A9` HBT 2-Ply ("Grandmaster II") implemented and calibrated: `A7`
  Hybrid HBT plus one opponent-response ply -- for each candidate champion subset, clone
  the position, commit it, and score against a simulated best reply from a deterministic
  public-information surrogate hand (`ai_strat_hbt2ply_reply.c`), blended with `A7`'s own
  undefended score via a `reply_trust` dial (0 provably recovers `A7` exactly, verified in
  `testsrc/test_moves.c`). Two real findings surfaced while building it, both documented
  in depth further down this file's history and in `ai_strat_hbt2ply.h`/
  `ai_strat_hbt_enum.h`: (1) `A7`'s own shipped defense formula
  (`hbt_best_defense_move()`) has a PASS-dominance property -- its decline baseline never
  charges the incoming attack, making declining mathematically dominate every blocking
  option under `A7`'s weights -- so `A9` uses its own local, corrected reply oracle
  (`hbt2ply_reply_defense_move()`) rather than `A7`'s real one, leaving `A7` itself
  untouched; (2) `A9`'s own first scoring formula over-credited "forced the opponent to
  spend a resource blocking" (~5x the weight of the actual damage-reduction term),
  making the ply actively harmful even when its block prediction was accurate -- fixed to
  credit damage reduction only, which is now a mathematical guarantee
  (`two_ply <= one_ply` always) rather than a design intention. Calibration (staged: the
  34 inherited `HBTParams` fields hard-pinned, only `reply_trust`/`surrogate_pessimism`
  searched) converged to the same ~47% ceiling against `A7` whether optimized against
  `borealis` or directly against `hbt`, both with tight confidence intervals over 40,000
  games -- not a tuning gap. A controlled test isolating the attack-side logic (both
  sides given the corrected defense) showed the ply reaching near-parity with `A7` there
  (49.6% vs 50.5%), confirming the mechanism works in principle but has nothing real to
  correct for against `A7`'s actual (non-blocking) defense. Measured: **59** vs
  `borealis` (20,000 games, both seats, direct pairwise -- same methodology as `A8`,
  chosen for consistency rather than necessity: unlike `A8`, `A9`'s own cost is cheap,
  ~3200 games/sec, but a roster-wide fit was skipped anyway since `A8`/`A12` already make
  any full round-robin impractical regardless of what else is added to it); measured
  **47.2%** [46.7%, 47.7%] head-to-head vs `A7` specifically -- below this agent's own
  design target, honestly reported. Shipped as a playable, calibrated roster member
  (`A4`/`A8`/`A12` precedent); fixing `A7`'s (and `A5`'s identical) defense formula is
  believed a genuine prerequisite for a future re-attempt at the design target, deferred
  to its own task and conditioned on `A5`/`A7` measuring at least as strong against
  `Borealis` afterward, not merely "more realistic" (user's stated condition). Full
  details: `ideas/A9 .../about.md`, this session's diagnosis.
- **2026-08-25** — `A12` Clairvoyant ("The Clairvoyant") implemented: a side exploration
  of whether a cheap (non-tree, no enumeration) fix to `A8`'s diagnosed rollout-policy
  bias could raise its rating without drifting toward `A9`/`A10`/`A11`'s own territory.
  Reuses `A8`'s identical progressive-pruning search verbatim (required generalizing
  `mc_playout()`/`mc_search_best_move()` to take the rollout `StrategySet` as a
  parameter -- `A8`'s own behavior confirmed unchanged throughout); the only
  difference is that rollouts give the *opponent's* simulated moves a cheap
  closed-form heuristic instead of `A8`'s uniformly-random policy. Playtracing found
  and fixed two real defects in that heuristic (an attack score that never weighed
  cost, committing 100% of the time; a defense formula with the same gap, once fixed
  over-correcting until cost was weighed the same way on both sides) before a
  `cost_weight` sweep (14 values, vs `borealis`) replaced a borrowed Borealis-lambda
  starting estimate with an empirically better one. Measured rating: **31** — close to
  but consistently a few points below `A8`'s own 35, not a clear improvement; shipped
  anyway as a deliberately modest, "fun and easy to beat" roster member rather than
  chased further, since `A9`-`A11` are expected to be materially stronger agents by
  design. Full details: `doc/changelog.md`.
- **2026-08-25** — `A8` Simple Monte Carlo ("The Soothsayer") implemented and
  calibrated: the first agent on the ladder that actually simulates (progressive-pruning
  rollout search) rather than scoring closed-form, which required new shared engine
  infrastructure first (`src/actions/` move enumeration/application,
  `ai_strat_playout.c`'s forked-RNG-stream `mc_determinize()`/`mc_playout()`, reusable by
  `A9`-`A11`). Playtracing three losses vs `borealis` plus a budget-vs-rating sweep
  (1.0x/1.75x/2.3x the rollout budget, statistically indistinguishable ratings)
  diagnosed the ceiling precisely: `mc_playout()`'s rollouts model *both* seats as
  `AI_STRATEGY_RANDOM` regardless of the real opponent, so its win-probability estimates
  are well-calibrated against `rand` and systematically biased against a real strategic
  opponent — a bias more sampling can't correct, confirmed by the flat budget sweep.
  Shipped as-is: this agent was never intended to be competitive (the original design
  intent is a tool for probing per-action values from curated positions to help mine new
  heuristics, not a strong opponent). Measured rating: **35** — below the Borealis
  anchor, honestly reported per this project's convention (`A4`'s 36 is the precedent).
  Full details: `doc/changelog.md`.
- **2026-08-25** — `A7` Hybrid HBT ("The Grandmaster") implemented and calibrated: a
  fixed three-layer synthesis of `A4` (resource targets, entering as a soft penalty
  rather than a hard filter -- see `doc/changelog.md` for why), `A6` (phase/aggression
  modulating the advantage weights), and `A5` (the closed-form enumerate-and-rank
  mechanism itself), plus `A3`'s lethal-combo hold and combo-protecting mulligan/
  discard, added deliberately so the hybrid wouldn't be combo-blind next to Borealis
  in human play. Calibration was staged (34 parameters, double `A6`'s previous max):
  stage 1 froze every inherited field at its source agent's own shipped value and
  freed only the eight parameters new to this agent, reaching 60.96% vs `borealis`
  (40,000 games) with no personality flags; stage 2's joint re-fit of `A5`'s own
  weights found no significant improvement, so stage 1 shipped. Measured rating
  (roster-wide `--stda.rating` fit): **62** -- above all three of its ingredients in
  the same fit (`A5` 61, `A6` 53, `A4` 34) despite a clear pairwise loss to `A5`
  specifically (26.0%), reported honestly rather than tuned away. See
  `doc/changelog.md` for the full entry.

- **2026-08-25** — `A6` Tactical ("Pressure Cooker") implemented and calibrated:
  classifies the game into a phase (early/mid/late/critical, by energy thresholds)
  and derives a single 0.0-1.0 aggression factor from energy difference, hand power,
  and cash surplus, scaling how many champions to commit by that factor. Playtracing
  found a real bug in the first `decide_num_attackers()` fill-in (proportional
  rounding put the single-affordable-champion case's decision boundary exactly on
  aggression's neutral baseline, causing the agent to measure **losing to Random** --
  the only implemented agent ever to do so); fixed with four fixed aggression bands
  instead. Calibration (`aicalibsrc/tactical/`) targeted `borealis`, sixteen free
  parameters -- the largest search space so far -- and the first unconstrained
  `optimize` run came back with no personality flags (unlike `A4`'s and `A5`'s free
  searches), so it shipped directly with no `--identity-safe` run needed. Measured
  rating (roster-wide `--stda.rating` fit): **52** -- above the Borealis anchor (50),
  the second agent after `A5` to clear it, against a `~74` design-intent estimate.
  Full details: `doc/changelog.md`.
- **2026-08-25** — `A5` Heuristic ("Eps-Gam-Del") implemented and calibrated: reduces
  the whole position to one weighted advantage function,
  `Advantage = epsilon*EnergyAdv + taper*gamma*CardsAdv + taper*delta*CashAdv`, and
  picks the 1-move-lookahead legal move that maximises it, closed-form (no
  clone-and-apply, to keep the shared RNG stream undisturbed). The first agent whose
  decision rule is a tunable weighted sum over whole-position features rather than
  per-card scoring/threshold gates/subset enumeration/resource-target formulas.
  Calibration (`aicalibsrc/heuristic/`) targeted `borealis` (not `A4` as the original
  todo item said — superseded once `A4` itself measured below the anchor). A manual
  sweep found `weight_cards_advantage`'s useful range far exceeds the spec's
  illustrative 0.15 (peak ~48.7% vs `borealis` around gamma=6-8); an unconstrained
  `optimize` run found gamma=9.815 at 59.67% validated, flagged by
  `check_personality_flags()`, but playtracing ruled out the "hoard forever" failure
  mode the flag exists to catch (still finishes <20 turns, 99.8% vs `rand`) — a large
  weight changing which moves the one mechanism prefers, not erosion of the mechanism.
  `optimize --identity-safe` converged to a statistically indistinguishable 58.99% at
  gamma=1.962, and shipped instead (same measured strength, weights closer to the
  spec). Measured rating (roster-wide `--stda.rating` fit): **60** — above the
  Borealis anchor (50), though below the `~70` design-intent estimate — unlike `A4`
  (which undershot both its own estimate and the anchor), `A5` undershot its estimate
  but still cleared the anchor, the first agent in the roster to do so. Full details:
  `doc/changelog.md`.
- **2026-08-24** — `A4` Balanced Rules ("Bean Counter") implemented and calibrated:
  closed-form resource accounting (target cash/hand-size scale linearly with opponent
  energy), variance-aware defense (`E[Def] <= E[Attack] - beta*sigma`, can decline a
  block outright). The re-anchored spec-derived cash-target slope
  (`INITIAL_CASH_DEFAULT/91`) turned out to be a genuine bug, not just untuned — it
  produces ~0 cash surplus at full opponent energy, trapping the agent unable to spend
  for many early turns (confirmed by playtrace and a parameter sweep). Free
  `differential_evolution` search eroded the agent's resource-conservation identity
  (slopes toward 0, defense_beta toward "never defend") while measuring stronger,
  mirroring `A2`'s rejected `aggression_level=2.21` — so calibration added
  `optimize --identity-safe` (`aicalibsrc/balanced/`), which searches a narrower,
  character-preserving bound instead. Measured rating (roster-wide `--stda.rating` fit):
  **36** — below the Borealis anchor (50) and below the `~62` design-intent estimate, a
  legitimate result, not a defect (see `doc/changelog.md`). Its calibration harness also
  fixed the `DEFAULTS`-drift item below for itself, via a new `--print-defaults` mode.
  Full details: `doc/changelog.md`.
- **2026-08-23** — Bradley-Terry rating system implemented (`src/rating/`): ports the
  math/design of `ideas/5 rating system/v2 Bradley-Terry (BT) Rating System/`, fixing a
  dozen-plus defects on the way (win-count overflow, leaderboard underflow, batch
  convergence-vs-normalisation ordering, draw handling, incremental-update
  path-dependence, a diverging unnormalised gradient step). Ships two batch solvers —
  MM (Minorization-Maximization, the standard method, default) and gradient ascent
  (kept for cross-checking) — plus the incremental `A^delta` path for live play, CSV
  persistence, and the new `--stda.rating` round-robin benchmark mode (every
  implemented agent, both seats, Borealis anchored at rating 50) and `--rating.track`
  opt-in human rating tracking in `stda.cli`/`stda.tui` with a matchmaking suggestion.
  Measured leaderboard: `borealis` 50 (anchor), `combo` 30, `value` 24, `rand` 2 —
  cross-validated against `A3`'s own independently-measured win rates below. Full
  details: `doc/changelog.md`.
- **2026-08-23** — `A3` Borealis (the Bradley-Terry benchmark) implemented and
  calibrated: exhaustive 0-3 champion subset enumeration (no pruning), one monotone
  strength dial (`luna_value`/lambda), epsilon tie-break randomisation, lethal-combo
  holding. Landed alongside it: per-agent `mulligan_strategy[]`/`discard_strategy[]`
  hooks in `StrategySet` (`src/ai_strat/ai_strategy.h`), so Borealis can protect held
  combo pieces from the shared power-based discard/mulligan heuristic without changing
  Random/`A1`/`A2`'s behaviour. `aicalibsrc/borealis/` calibration tooling added, full
  `sweep`/`optimize`/`selfplay`/`validate` parity with `aicalibsrc/combo/`; also fixed a
  parallel-execution result-misattribution bug in the driver pattern it was copied from
  (present, unpatched, in `aicalibsrc/value/`'s and `aicalibsrc/combo/`'s `sweep`/
  `selfplay` commands — see `doc/oracle_todo.md`). The handout's default lambda (0.5)
  turned out far from optimal: measured win rate vs `combo` climbed from 43.6% to 69.1%
  after calibration (lambda≈4.58, confirmed unimodal). Full details: `doc/changelog.md`.
- **2026-08-22** — `A2` Combo Threshold ("The Showboat") implemented and calibrated:
  threshold-gated combo chaser (pairs/triples must clear a tunable bonus threshold to
  be pursued), probabilistic defense decline -- both deliberate character, not defects
  (handout §3, §8). `aicalibsrc/combo/` calibration tooling added (`sweep`/`optimize`/
  `selfplay`/`validate`), mirroring `aicalibsrc/value/` but with a `differential_evolution`
  black-box search in place of a full grid (infeasible over 9 parameters vs `A1`'s 2).
  Measured win rate vs `value`: 49.94% -> 58.77% after calibration; vs `rand`: 88.11% ->
  92.78%. Full details: `doc/changelog.md`.
- **2026-08-21** — `A1` Value Based ("The Apprentice") implemented: efficiency-ratio
  card ranking, no combo awareness, no attack-phase pass option (by design). A single
  `AIStrategyType -> function pointer` registry (`src/ai_strat/ai_strategy.c`) now
  drives every strategy-set build site (`stda_auto.c`, `cli_game.c` shared by CLI/TUI)
  and the interactive strategy menu's availability labels, replacing three separate
  hardcoded Random assignments. Per-player agent selection for `--stda.auto`
  (`-Aa`/`-Ab`), `AIStrategyType` moved to `game_types.h`, one CLI shorthand per agent
  (dropped `showboat`/`greedy` aliases). Full details: `doc/changelog.md`.
- **2026-07-23/24** — TUI Milestone 2 (human-vs-AI play: `TAB`-toggled PLAY/COMMAND
  modes, recall, cash exchange, mulligan, discard-to-7, live combat-result display) and
  a follow-up UI/playability polish pass.
- **2026-07-14** — Recall mechanic, interactive cash-card champion selection, detailed
  combat results display, discard pile display, source folder structure cleanup
  (pragmatic pass), TUI Milestone 1 (ncurses display skeleton, AI-vs-AI).

Full details: `doc/changelog.md`.

### What Needs Work

- Random, `A1` Value Based, `A2` Combo Threshold, `A3` Borealis, `A4` Balanced Rules,
  `A5` Heuristic, `A6` Tactical, `A7` Hybrid HBT, and `A8` Simple Monte Carlo
  implemented, and the Bradley-Terry rating system now built on top of them —
  everything from `A9` onward on the AI ladder below is open, and is what the rating
  system will next measure.
- Automated simulation mode (`stda_auto.c`) needs a refactor (see `doc/oracle_todo.md`).
- No save/load, no config file system, no general match-result CSV export (the rating
  system's own CSV persistence is separate and done), no network, no GUI, no `stda.sim`.
- Two calibration-driver items flagged during `A3`'s calibration, not yet actioned for
  `aicalibsrc/value/`/`aicalibsrc/combo/` (both fixed for `A4`'s own driver from the
  start): a parallel-execution result-misattribution bug unpatched in `aicalibsrc/
  value/`'s and `aicalibsrc/combo/`'s `sweep`/`selfplay` (casts doubt on A1's shipped
  values specifically, chosen via the vulnerable `selfplay` path), and both drivers'
  `DEFAULTS` dicts having drifted from their shipped C constants (`aicalibsrc/
  borealis/`'s still does too). See `doc/oracle_todo.md`.

---

## Next Up (single authoritative order)

1. **`A10` IS-MCTS** ("The Omniscient", `ideas/A10 ai agent is-mcts (the omniscient)/`).
   The next rung on the AI ladder — `A1`-`A9` and `A12` are all implemented and
   calibrated. The question of whether to give `A8` Simple Monte Carlo a cheap,
   non-tree heuristic rollout policy (its uniformly-random rollouts are the diagnosed
   cause of its below-anchor rating, see `doc/changelog.md`'s 2026-08-25 entry) without
   drifting it toward `A9`/`A10`/`A11`'s own territory is now resolved: `A12` Clairvoyant
   (2026-08-25) is exactly that experiment, and it did not pay off (measured rating 31,
   a few points below `A8`'s own 35) -- see `doc/changelog.md`.

**Back burner** (explicitly deferred): save/load game state
(`ideas/6 save and load gamestate/`), configuration file system
(`ideas/7 config file/`).

---

## Long-Term Vision

### Research Goals

1. **AI Development**: progress from random → rule-based → heuristic → Monte Carlo →
   Information Set MCTS.
2. **Rating System**: Bradley-Terry model to measure AI strength objectively.
3. **Architecture**: clean client/server separation for future multiplayer.
4. **Simulation**: CSV export framework for statistical analysis of strategies.
5. **Cross-Platform**: terminal (ncurses, done), desktop (SDL3, future), mobile
   (long-term).

### Learning Objectives

Advanced AI techniques (MCTS, information sets); network programming patterns;
statistical modeling (rating systems); GUI programming (SDL3); build systems and
cross-platform development.

---

## Development Phases

Each phase below names its `ideas/` home; see `doc/oracle_todo.md` for the actionable
task breakdown within whichever phase is currently active.

### Phase: Complete Game Loop — mostly done

Core turn/combat/card-action logic and all interactive-mode features (recall, cash
exchange, mulligan, discard-to-7, combat/discard display) are implemented. Remaining:
error-handling polish (see `doc/oracle_todo.md`).

### Phase: Standalone Modes — partial

- `stda.auto` (automated simulation): working, needs a refactor + CSV export
  (`ideas/2 engine and action system design/stda_auto_split_plan.md`,
  `ideas/4 match results export/`).
- `stda.cli` (interactive CLI): done except save/load.
- `stda.tui` (ncurses TUI): Milestones 1–2 done and its design-exploration folder
  archived accordingly — see `doc/changelog.md`.
- `stda.sim` (simulation UI): not started.

### Phase: AI Development — `A1`–`A8` done, `A9` next

Ladder: `A1` value-based (done, 2026-08-21) → `A2` combo threshold (The Showboat, done,
2026-08-22) → `A3` greedy power (Borealis benchmark, done, 2026-08-23) → `A4` balanced
rules (Bean Counter, done, 2026-08-24) → `A5` heuristic (Eps-Gam-Del, done, 2026-08-25)
→ `A6` tactical (Pressure Cooker, done, 2026-08-25) → `A7` hybrid HBT (The Grandmaster,
done, 2026-08-25) → `A8` simple MC (The Soothsayer, done, 2026-08-25, rating 35 --
see `doc/changelog.md`)
→ `A9` HBT 2-ply → `A10` IS-MCTS → `A11` IS-MCTS + neural
network. One `ideas/A#` folder per agent,
`A#` matching that agent's `AIStrategyType` enum ordinal (`src/core/game_types.h` as of
`A1`; it previously lived in `src/ui/shared/player_config.h`). `A12` Clairvoyant (The
Clairvoyant, done, 2026-08-25, rating 31) is appended after `A11` in enum ordinal but
is **not** part of this authoritative ladder order -- it's `A8`'s sibling (same search,
a cheap opponent-rollout heuristic instead of pure random), built as a side exploration
rather than the next rung; see `doc/changelog.md`. See
`ideas/G1 AI agent general info/oracle_ai_agent_names.md` for the canonical roster,
flavour names, and ratings.

### Phase: Simulation & Analysis Tools — spec complete, implementation pending

CSV export (`ideas/4 match results export/`); interactive simulation UI, `stda.sim`
(no dedicated `ideas/` folder yet, see `ideas/2 …/target_folder_structure_v4.md` for
scoping notes); configuration file system (`ideas/7 config file/`, back-burnered).

### Phase: Rating System — done (2026-08-23)

Bradley-Terry core calculations (MM + gradient-ascent batch solvers), adaptive
learning rate, the Borealis (`A3`) benchmark anchor (rating 50 by definition),
incremental + batch updates, CSV persistence, matchmaking, and the `--stda.rating`
round-robin benchmark mode. `src/rating/`; ports `ideas/5 rating system/` (v2 spec)'s
design, not the file — see `doc/changelog.md` for the defects fixed on port.

### Phase: Client/Server Architecture — design complete, major refactor required

Protocol design, server (full state + validation + broadcast), client (visible state +
action submission), code separation (`sh_`/`sr_`/`cl_`/`pr_`-style modules). Depends on
the engine state-machine/action-system rework in
`ideas/2 engine and action system design/` landing first.
`ideas/8 client server/` for the client/server-specific design.

### Phase: Cross-Platform GUI — plan exists, major undertaking

SDL3 desktop GUI (`ideas/9 gui/oracle_sdl3_gui_plan.md`): card rendering, font/texture
management, responsive layout, input handling; asset pipeline (champion artwork, frames,
species/order icons); mobile ports (iOS/Android) as a long-term stretch goal.

---

## Research Questions to Explore

**AI Development**: minimum MCTS rollouts for good play? How much does combo bonus
affect optimal strategy? Can rule-based AI approach MCTS performance? What's the skill
ceiling with perfect information?

**Game Balance**: are random/mono/custom decks balanced? Do certain species/orders
dominate? Is the mulligan rule fair? What's the optimal starting cash amount?

**System Design**: best way to serialize game state for network play? How to handle
reconnection in multiplayer? Efficient card representation for GUI rendering? Optimal
strategy framework for pluggable AIs?

---

## Success Criteria

- [ ] At least 3 different AI strategies working
- [x] Rating system accurately ranks AI strength (2026-08-23, `src/rating/`) — the
      `--stda.rating` round-robin table currently orders `rand` < `value` < `combo` <
      `borealis` (50, anchor by definition), matching the intended ladder
- [ ] CSV export generates usable data for R/Python analysis
- [ ] TUI mode provides a good user experience *(largely met already — Milestones 1–2 +
      polish pass done; see "Left for a future pass" in `doc/oracle_todo.md`)*

### Longer-Term

- [ ] IS-MCTS AI demonstrably stronger than rule-based
- [ ] Network multiplayer works reliably
- [ ] Cross-platform GUI runs on Windows/Linux/macOS
- [ ] Project serves as a good portfolio/learning showcase

---

## References

- Game rules: `doc/game_rules_doc.md`
- Architecture: `doc/oracle_design.md`
- Actionable backlog: `doc/oracle_todo.md`
- History: `doc/changelog.md`
- Contributing workflow: `CLAUDE.md`, `doc/REFACTORING.md`
- GitHub repo: https://github.com/JonathanFerron/oracle/
- Design notes: `ideas/` directory

---

*Last Updated: August 2026*
