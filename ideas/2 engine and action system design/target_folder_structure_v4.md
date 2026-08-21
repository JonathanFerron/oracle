# Target Source Folder Structure (v4)

**Provenance**: this was `revised_folder_structure.md` in `ideas/1 improve source code
folder structure/`. That folder's pragmatic cleanup pass is done and archived
(`ideas/done/1 improve source code folder structure/`), so the still-useful part — the
*target directory layout* this refactor is aiming at — was moved here, where the engine /
role / UI restructure it depends on actually lives.

**Trimmed on the move (2026-08-20)**: the original also carried the state-machine API,
action-struct design, callback interface, per-mode loop examples, CLI 3-file breakdown and
GUI event-loop sketches. All of that is already covered, in more depth, by this folder's
sibling files — see the pointer table at the bottom rather than reintroducing a second
copy. What remains below is only the layout view: the tree, who owns which subtree, and
the mode matrix.

**Status**: aspirational target, not current reality. Directories are created only when
their first real file lands (see "Ownership" below for which `ideas/` folder triggers each).

---

## Conceptual Model

Modes are defined as **(role, ui)** tuples:

- **Roles**: `stda` (standalone), `client`, `server`
- **UIs**: `cli`, `tui`, `gui`, `simauto`, `simtui`, `servercli`

---

## Complete Directory Structure

```
src/
├── core/                      # Platform-agnostic game engine
│   ├── game_types.h          # All enums and struct definitions
│   ├── game_constants.c/h    # Full deck, static data, enums
│   ├── game_state.c/h        # Game initialization, setup
│   ├── game_engine.c/h       # Unified state machine (~300 lines)
│   ├── turn_logic.c/h        # Turn flow helpers (execute_begin_turn, etc.)
│   ├── card_actions.c/h      # Card playing mechanics
│   ├── combat.c/h            # Combat resolution
│   ├── combo_bonus.c/h       # Combo calculations
│   └── game_context.c/h      # Context pattern implementation
│
├── structures/                # Data structure implementations
│   ├── deckstack.c/h         # Fixed-size stack for decks
│   └── card_collection.c/h   # Fixed-size hand/discard/combat-zone collections
│
├── util/                      # Utility functions
│   ├── rnd.c/h               # RNG wrapper functions
│   ├── mtwister.c/h          # Mersenne Twister PRNG
│   ├── prng_seed.c/h         # Seed management
│   ├── debug.h               # Debug macros
│   └── logger.c/h            # Logging system (future)
│
├── ai_strat/                  # AI strategy implementations
│   ├── ai_strategy.c/h       # Strategy framework (function pointers)
│   ├── ai_strat_<name>.c/h   # One pair of attack/defense functions per agent
│   └── ai_params.c/h         # AI parameter management
│
├── deck_formats/              # Deck construction methods
│   ├── deck_random.c/h       # Random deck distribution
│   ├── deck_monochrome.c/h   # Monochrome deck builder
│   ├── deck_custom.c/h       # Custom deck builder
│   ├── deck_solomon.c/h      # Solomon 7×7 draft
│   ├── deck_draft12x8.c/h    # Draft 12×8
│   ├── deck_draft123.c/h     # Draft 1-2-3
│   └── deck_draft_common.c/h # Shared draft utilities
│
├── game_rules/                # Optional game depth additions
│   ├── abilities.c/h         # Champion abilities (Berserker, Vampire)
│   ├── momentum.c/h          # Momentum token system
│   └── order_powers.c/h      # Order ultimate powers
│
├── roles/                     # Role-specific orchestration
│   ├── stda/
│   │   ├── stda_main.c/h     # Standalone entry point dispatcher
│   │   └── stda_game.c/h     # Standalone game loops
│   │                         # stda_game_loop_cli(), _tui(), _gui(), _simauto(), _simtui()
│   │                         # engine_run_until_input() for blocking modes
│   │                         # engine_step() for event-driven GUI
│   │
│   ├── client/
│   │   ├── client_main.c/h   # Client entry point dispatcher
│   │   ├── client_game.c/h   # Client game loops (network → engine)
│   │   ├── client_state.c/h  # Visible state management
│   │   └── client_network.c/h # Network communication
│   │
│   └── server/
│       ├── server_main.c/h   # Server entry point
│       ├── server_game.c/h   # Server game loop (full engine)
│       ├── server_state.c/h  # Full game state management
│       ├── session_manager.c/h # Game session handling
│       └── matchmaking.c/h   # Matchmaking logic
│
├── ui/                        # UI implementations (role-agnostic)
│   ├── shared/
│   │   ├── ui_callbacks.h        # Callback interface definition
│   │   ├── ui_context.c/h        # Context pattern for callbacks
│   │   ├── ui_io.h               # Blocking input/output seam
│   │   ├── localization.h        # I18n support
│   │   ├── player_selection.c/h  # Player type selection
│   │   └── player_config.c/h     # Player name/strategy config
│   │
│   ├── interactive/           # UI-agnostic interactive command grammar
│   │   └── game_commands*.c/h
│   │
│   ├── cli/                   # Text-based interface (interactive)
│   │   ├── cli_display.c/h   # Output formatting
│   │   ├── cli_input.c/h     # Input parsing
│   │   └── cli_callbacks.c/h # Event handlers
│   │
│   ├── tui/                   # Terminal UI (ncurses)
│   │   ├── tui_display.c/h   # ncurses rendering
│   │   ├── tui_input.c/h     # ncurses input handling
│   │   ├── tui_callbacks.c/h # Event handlers
│   │   └── tui_windows.c/h   # Window management (helper)
│   │
│   ├── gui/                   # Game GUI (SDL3 for gameplay)
│   │   ├── gui_display.c/h    # SDL3 rendering coordinator
│   │   ├── gui_input.c/h      # SDL3 event handling
│   │   ├── gui_callbacks.c/h  # Event handlers
│   │   ├── card_renderer.c/h  # Card compositing (helper)
│   │   ├── font_manager.c/h   # Font loading (helper)
│   │   ├── texture_cache.c/h  # Image caching (helper)
│   │   └── layout.c/h         # Responsive layout (helper)
│   │
│   ├── simulation/            # Simulation and analysis
│   │   ├── sim_engine.c/h    # Core simulation engine
│   │   ├── sim_export.c/h    # CSV export
│   │   ├── sim_stats.c/h     # Statistics calculation
│   │   │
│   │   ├── simauto/          # Non-interactive CLI (automation)
│   │   │   ├── simauto_display.c/h   # Minimal output (progress, summary)
│   │   │   └── simauto_callbacks.c/h # Event handlers (logging only)
│   │   │
│   │   └── simtui/           # Simulation TUI (ncurses visualization)
│   │       ├── simtui_display.c/h    # Stats/graph rendering
│   │       ├── simtui_input.c/h      # Control input (pause/resume/speed)
│   │       ├── simtui_callbacks.c/h  # Event handlers
│   │       └── simtui_stats_visualizer.c/h # Real-time stats (helper)
│   │
│   └── servercli/             # Server admin CLI
│       ├── servercli_display.c/h  # Server status formatting
│       ├── servercli_input.c/h    # Admin command parsing
│       └── servercli_commands.c/h # Command handlers (list, show, stats, …)
│
├── actions/                   # Action system
│   ├── action.c/h            # Action structures and creation
│   ├── action_generator.c/h  # Generate legal moves
│   ├── action_validator.c/h  # Validate actions
│   └── action_processor.c/h  # Apply actions (server-side)
│
├── visibility/                # Information hiding (for network)
│   ├── visible_state.c/h     # VisibleGameState conversion
│   └── state_filter.c/h      # Filter hidden information
│
├── network/                   # Protocol definitions (shared)
│   ├── protocol.c/h          # Message format
│   ├── serialization.c/h     # State serialization
│   ├── socket_utils.c/h      # Socket wrappers
│   └── crypto.c/h            # Authentication/checksums
│
├── rating/                    # Bradley-Terry rating system
│   ├── rating.c/h            # Core BT calculations
│   ├── rating_batch.c/h      # Batch processing
│   ├── rating_export.c/h     # CSV persistence
│   └── calibration.c/h       # Parameter optimization
│
├── config/                    # Configuration management
│   ├── config.c/h            # INI file parser
│   ├── ai_config.c/h         # AI parameter loading
│   └── config_defaults.c/h   # Default values
│
├── persistence/               # Save/load functionality
│   ├── save_game.c/h         # Game state persistence
│   └── load_game.c/h         # Game state restoration
│
├── platform/                  # Platform-specific code
│   ├── platform_windows.c/h  # Windows-specific
│   ├── platform_linux.c/h    # Linux-specific
│   ├── platform_ios.m/h      # iOS-specific
│   └── platform_android.c/h  # Android-specific
│
└── main/                      # Entry point
    ├── main.c                 # Main dispatcher
    ├── cmdline.c/h            # Command-line parsing
    └── version.h              # Version information
```

---

## Ownership — which subtree is whose, and what exists today

| Subtree | State today | Owned by |
| --- | --- | --- |
| `core/`, `structures/`, `util/`, `main/` | **Real code**, matches the target | — (maintained in place) |
| `core/game_engine.c/h` | Not started | this folder (`ideas/2`) |
| `actions/`, `ui/shared/ui_callbacks.h`, `ui/*/\*_callbacks.c` | Placeholder only | this folder (`ideas/2`) |
| `ai_strat/` | Real code; `ai_strat_random` only, rest are design stubs. Names kept as `ai_strat_*` (no rename to `aistrat_*` — pure churn) | `ideas/A1`–`ideas/A12` |
| `roles/stda/` | Real code, different split than target (`stda_auto/_cli/_tui/_tui_interactive`) | this folder (`ideas/2`); `stda_auto.c` split specifically → `stda_auto_split_plan.md` |
| `roles/client/`, `roles/server/`, `network/`, `visibility/` | Not created / placeholder | `ideas/8 client server/` |
| `ui/cli/` | Real code, 5-file split (`cli_display`, `cli_action_display`, `cli_input`, `cli_io`, `cli_game`) | this folder for the callback rework |
| `ui/tui/` | Real code (`tui_render*`, `tui_input`), Milestones 1–2 done | — (complete; `ideas/3 misc ui ideas/` no longer holds TUI-specific content) |
| `ui/interactive/` | Real code — added with TUI M2, not in the original v4 tree | — |
| `ui/gui/` | Placeholder only | `ideas/9 gui/` |
| `ui/simulation/sim_export.c`, `simexport/` | Placeholder only | `ideas/4 match results export/` |
| `ui/simulation/simauto/`, `simtui/` | Placeholder only | this folder (callbacks) + `ideas/4` |
| `deck_formats/`, `game_rules/` | Not created | `ideas/10 Draft Format and Game Depth Addition Ideas/` |
| `rating/` | Placeholder only | `ideas/5 rating system/`, `ideas/A2 …` |
| `config/` | Not created | `ideas/7 config file/` (back burner) |
| `persistence/` | Not created | `ideas/6 save and load gamestate/` (back burner) |
| `platform/` | Not created — current `#ifdef _WIN32` blocks are small enough to stay inline | create only if platform code grows |

---

## Valid Mode Combinations

| UI        | stda | client | server | Loop Ownership |
| --------- | ---- | ------ | ------ | -------------- |
| cli       | ✓    | ✓      | ✗      | Game owns      |
| tui       | ✓    | ✓      | ✗      | Game owns      |
| gui       | ✓    | ✓      | ✗      | UI owns        |
| simauto   | ✓    | ✗      | ✗      | Game owns      |
| simtui    | ✓    | ✗      | ✗      | Game owns      |
| servercli | ✗    | ✗      | ✓      | Game owns      |

---

## Migration sequencing (layout view)

1. **Core engine** — `core/game_engine.c` pollable state machine, phase logic extracted
   from `turn_logic.c`.
2. **Action system** — `actions/` populated; validation moved out of the UI layer.
3. **Callback system** — `ui/shared/ui_callbacks.h` + per-UI `*_callbacks.c`.
4. **CLI rework** — existing `ui/cli/` files re-fitted onto engine + callbacks.
5. **TUI** — mostly done already; re-fit onto callbacks when step 3 lands.
6. **GUI foundation** — event-driven `engine_step()` path, `ui/gui/` populated.

Each step creates directories only as real files land in them.

---

## Where the rest of the v4 design went

**2026-08-20 cleanup**: folder 2 was pared down to only mode-agnostic core-engine
material — its CLI-specific reference implementation (an engine/`Action*`/callback
redesign the real CLI split never adopted; see `CLAUDE.md`'s `ui/cli/` bullet for what
was actually built) was deleted outright as superseded-before-built, and its
GUI-flavored and network-flavored sketches moved to where that work will actually
happen. See `README.md` in this folder for what remains and why.

| Topic | See |
| --- | --- |
| Pollable state machine API (`engine_step`, `engine_run_until_input`) | `unified_state_machine.txt`, `game_engine_impl.txt` (stayed) |
| Action structs, validation, `apply_action()`, strategy-returns-`Action` signature | `game engine refactoring approach … clean state machine approach.md` (stayed) |
| `VisibleGameState` conversion / state filtering | `ideas/8 client server/Client Server Architecture Ideas 2 (consolidated).md` ("State Filtering") — more complete than this folder's now-removed sketch |
| Callback interface + CLI worked example | deleted — CLI never adopted this design; see `CLAUDE.md`'s `ui/cli/` bullet for the actual 5-file split that was built instead |
| CLI 3/4-component breakdown proposal | deleted — superseded before being built |
| GUI main-loop / engine-integration patterns | `ideas/9 gui/game_loop_engine_integration_notes.md` |
| Client loop, opaque server/client API, `CardVisibility`, poll-based server loop | `ideas/8 client server/game_loop_and_client_api_notes.md` |
| Client/server separation rationale, action generator, who-uses-`apply_action()` | `game engine refactoring approach … clean state machine approach.md` (stayed) |
| `stda_auto.c` split | `stda_auto_split_plan.md` (stayed, unrelated topic) |
