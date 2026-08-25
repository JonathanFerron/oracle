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
("The Showboat"), `A3` Borealis (the Bradley-Terry benchmark), and `A4` Balanced Rules
("Bean Counter") AI strategies are all implemented and calibrated, and the
Bradley-Terry rating system itself is now built on top of them. **Active work**: `A5`
Heuristic — see "Next Up" below.

### Recently Completed

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

- Random, `A1` Value Based, `A2` Combo Threshold, `A3` Borealis, and `A4` Balanced
  Rules implemented, and the Bradley-Terry rating system now built on top of them —
  everything from `A5` onward on the AI ladder below is open, and is what the rating
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

1. **`A5` Heuristic** ("ε-γ-δ", `ideas/A5 ai agent heuristic (eps-gam-del)/`). The next
   rung on the AI ladder — `A1`-`A4` are all implemented and calibrated, and the
   Bradley-Terry rating system (2026-08-23, `src/rating/`) can now measure it against
   Borealis via `--stda.rating` as soon as it exists.

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

### Phase: AI Development — `A1`–`A4` done, `A5` next

Ladder: `A1` value-based (done, 2026-08-21) → `A2` combo threshold (The Showboat, done,
2026-08-22) → `A3` greedy power (Borealis benchmark, done, 2026-08-23) → `A4` balanced
rules (Bean Counter, done, 2026-08-24) → `A5` heuristic → `A6` tactical → `A7` hybrid
(HBT) → `A8` simple MC → `A9` HBT 2-ply → `A10` IS-MCTS → `A11` IS-MCTS + neural
network. One `ideas/A#` folder per agent,
`A#` matching that agent's `AIStrategyType` enum ordinal (`src/core/game_types.h` as of
`A1`; it previously lived in `src/ui/shared/player_config.h`). See
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
