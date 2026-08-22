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
pass) are done. Random and `A1` Value Based ("The Apprentice") AI strategies are
implemented. **Active work**: `A2` Combo Threshold — see "Next Up" below.

### Recently Completed

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

- Only Random and `A1` Value Based implemented — everything from `A2` onward on the AI
  ladder below is open.
- Automated simulation mode (`stda_auto.c`) needs a refactor (see `doc/oracle_todo.md`).
- No save/load, no config file system, no CSV export, no rating system, no network, no
  GUI, no `stda.sim`.

---

## Next Up (single authoritative order)

1. **`A2` Combo Threshold** (`ideas/A2 ai agent combo threshold (the showboat)/`), then
   `A3` in order. `A1` Value Based ("The Apprentice") is done (2026-08-21). This isn't
   just easiest-first: the rating system (`ideas/5 rating system/`) needs the Borealis
   benchmark agent (`A3`), which itself needs `A1`–`A2` implemented for comparison.
   Support material that isn't itself an agent — general info and calibration tooling —
   was moved off the A-line into `ideas/G1 AI agent general info/` and `ideas/G2 ai
   agent parameters storing and optimization/` (2026-08-21 folder-sort pass), so it no
   longer occupies an `AIStrategyType` enum slot or interrupts the A-line numbering.

   The CLI's `display_ai_strategy_menu()`/`get_ai_strategy_choice()`/
   `get_strategy_display_name()` (`src/ui/shared/player_config.c`) list all twelve
   planned agents, with availability now driven by a single registry
   (`ai_strategy_is_implemented()` in `src/ai_strat/ai_strategy.c`, added alongside
   `A1`) rather than hardcoded per strategy; remaining work per agent is implementing
   its attack/defense functions in `src/ai_strat/` and adding one line to that
   registry. See `doc/oracle_todo.md`'s "Checklist: Adding a New AI Strategy" for the
   mechanical steps.

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

### Phase: AI Development — `A1` done, `A2` next

Ladder: `A1` value-based (done, 2026-08-21) → `A2` combo threshold (The Showboat) → `A3`
greedy power (Borealis benchmark) → `A4` balanced rules → `A5` heuristic → `A6` tactical
→ `A7` hybrid (HBT) → `A8` simple MC → `A9` HBT 2-ply → `A10` IS-MCTS → `A11` IS-MCTS +
neural network. One `ideas/A#` folder per agent, `A#` matching that agent's
`AIStrategyType` enum ordinal (`src/core/game_types.h` as of `A1`; it previously lived in
`src/ui/shared/player_config.h`). See "Next Up" above for why `A2→A3` comes next, and
`ideas/G1 AI agent general info/oracle_ai_agent_names.md` for the canonical roster,
flavour names, and ratings.

### Phase: Simulation & Analysis Tools — spec complete, implementation pending

CSV export (`ideas/4 match results export/`); interactive simulation UI, `stda.sim`
(no dedicated `ideas/` folder yet, see `ideas/2 …/target_folder_structure_v4.md` for
scoping notes); configuration file system (`ideas/7 config file/`, back-burnered).

### Phase: Rating System — spec complete, ready for implementation

Bradley-Terry core calculations, adaptive learning rate, keeper benchmark (rating 50 =
the `A4` Borealis agent), incremental + batch updates, CSV persistence, matchmaking.
`ideas/5 rating system/` (v2 spec).

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
- [ ] Rating system accurately ranks AI strength
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
