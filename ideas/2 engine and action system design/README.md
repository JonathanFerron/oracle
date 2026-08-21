# Game Engine Refactoring — Scope

**Purpose**: design notes for the pollable game-engine state machine
(`core/game_engine.c`) that would let the same core engine drive blocking modes
(CLI/TUI/server) and event-driven modes (GUI) alike, plus the `Action*` object system
and strategy-signature change it depends on. Not started/implemented — see
`doc/oracle_design.md` §11 "Planned Architecture" for how this fits the rest of the
roadmap.

**Cleaned up 2026-08-20**: this folder used to also hold a full CLI-specific reference
implementation of an engine/`Action*`/`UICallbacks` redesign. The real CLI split
(2026-07-14) took a simpler, different path — five files
(`cli_display`/`cli_action_display`/`cli_input`/`cli_io`/`cli_game`) built around the
`UiIO` seam, no engine/action/callback layer — so that CLI-specific material was
superseded before it was ever built and was deleted outright, not archived (nothing in
it was still true or still planned). GUI-flavored and network-flavored sketches that
were genuinely still useful moved to `ideas/9 gui/` and `ideas/8 client server/`
respectively, next to (and clearly marked secondary to) the larger, more authoritative
design docs already in those folders. See `target_folder_structure_v4.md`'s "Where the
rest of the v4 design went" table for the full file-by-file trail.

## What's here now

- `unified_state_machine.txt` — `core/game_engine.h`: the `GameEngine`/`GamePhase`
  state machine API (`engine_step()`, `engine_run_until_input()`, etc.).
- `game_engine_impl.txt` — `core/game_engine.c`: a worked implementation of that API.
- `game engine refactoring approach to prepare for client server separation of duties
  using clean state machine approach.md` — the design rationale: separating
  decision-making from action application, the `Action` struct, changing
  `AttackStrategyFunc`/`DefenseStrategyFunc` to return `Action` instead of mutating
  `gstate` directly (load-bearing for any networked — including AI — client), the
  action-list generator, and a table of which component types (server, human client,
  simple AI client, MCTS AI client, standalone) use `apply_action()` and how.
- `mode_usage_examples.txt` — CLI and auto-simulation usage examples of the unified
  engine, plus the `advance_to_resolve_phase()` helper (its former GUI and Server
  examples moved to folders 9 and 8, see the table above).
- `stda_auto_split_plan.md` — unrelated topic (splitting `stda_auto.c` for CSV export),
  kept here because it's already cross-referenced by name from `CLAUDE.md`,
  `doc/oracle_todo.md`, and `doc/oracle_roadmap.md`.
- `target_folder_structure_v4.md` — the target `src/` directory layout this refactor
  (and the GUI/network work in folders 8–9) is aiming at, with an ownership table
  showing which `ideas/` folder is responsible for which planned subtree.
