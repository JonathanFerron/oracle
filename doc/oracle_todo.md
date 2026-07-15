# Oracle Development TODO

**Quick Status**: Turn-logic interactive-mode commands (recall, combat results details, discard pile inspection, cash card functionality), the source folder structure cleanup (pragmatic pass), and TUI Milestone 1 (ncurses display skeleton) are complete -- see `doc/changelog.md`. TUI Milestone 2 (human interaction) is next (see "Next Up" below).

---

## Next Up (preferred order)

**Note on `ideas/` numbering (2026-07-14)**: folders were renumbered twice this session.
Non-AI planning folders now use plain integers 1-10 reflecting rough priority order; AI
agent folders were pulled into their own `A1`-`A11` namespace (kept together, in their
existing relative order) so adding new AI ideas doesn't require renumbering everything
else. See `git log` / folder contents if an old number (e.g. `ideas/8/`, `ideas/14.3/`)
shows up in an older doc or commit message.

Note: source code folder structure cleanup is now DONE -- see `doc/changelog.md`. Future
directories not created yet, to be added only when their first real file lands, in the
folder noted:
- `deck_formats/` -- when a draft/deck-format feature is implemented (`ideas/10 Draft Format and Game Depth Addition Ideas/`)
- `game_rules/` -- when the game-engine refactor (below) needs a home for rules data separate from `core/`
- `interactive/` -- when TUI/GUI interactive-mode code needs a shared home distinct from `ui/cli/`
- `network/` -- client/server (`ideas/8 client server/`)
- `persistence/` -- save/load game state (`ideas/6 save and load gamestate/`)
- `config/` -- configuration file system (`ideas/7 config file/`)
- `platform/` -- if/when platform-specific code (beyond the current `#ifdef _WIN32` blocks) grows enough to warrant its own directory

1. **TUI mode Milestone 2** (human interaction) -- see the "TUI Mode (stda_tui.c)" checklist
   below for the concrete breakdown. Milestone 1 (display skeleton) is done
   (`doc/changelog.md`, 2026-07-14) without needing any of `ideas/2 game engine
   refactoring for GUI and network support/`'s engine rewrite; Milestone 2 may need a
   *minimal* slice of it (a display/input callback seam), scoped down from the full
   Action-struct/`VisibleGameState`/client-server rewrite, to let CLI and TUI share the
   interactive turn orchestration instead of duplicating it.
2. **First "non-dumb" AI strategy** (`ideas/A1 ai agent value based/`) -- implement agents in `A1 -> A2 -> A3 -> A4` order first: the rating system (`ideas/5/`) needs the Borealis benchmark agent (`ideas/A4 ai agent combo aware`), which itself needs A1-A3 to exist for comparison, so this order isn't just "easiest first" -- it's a real dependency.

   **CLI menu is now stub-synced (2026-07-14)**: `display_ai_strategy_menu()`/`get_ai_strategy_choice()`/`get_strategy_display_name()` in `src/ui/shared/player_config.c` and the `AIStrategyType` enum now list all 11 planned agents (`A1`-`A11`, skipping `A2` since parameter storing/optimization is calibration tooling, not an agent) as "not yet implemented" stub menu entries, in `ideas/A#` order, each with a comment cross-referencing its `ideas/A#` folder. `A4`'s menu entry is explicitly labeled "Combo Aware [Borealis benchmark]" so it's identifiable in the CLI. The former "Hybrid" entry is confirmed to be `A7` (tactical+HBT: **H**euristics+**B**alanced+**T**actical) and is now labeled "Hybrid (HBT)". **Remaining work per agent**: as each strategy is actually implemented (attack/defense functions in `src/ai_strat/`), wire its menu choice through to the real strategy functions instead of falling back to Random.

**Back burner (explicitly deferred for now)**:

- Save/load game state (`ideas/6 save and load gamestate/`)
- Configuration file system (`ideas/7 config file/`)

---

# Oracle Card Game: Code Quality & Architecture Review

## Critical Issues

### Config Structure Scattered

**Problem:** Configuration handling split between multiple files

- `cmdline.c` - parsing
- `main.c` - cleanup
- `stda_auto.c`, `stda_cli.c` - usage

**Recommendation:** Centralize in new `config.h` / `config.c`

---

## Architecture Recommendations

### Prepare for Client-Server Split

Current code is monolithic. Start preparing for separation. See notes in `ideas/2 game engine refactoring for GUI and network support/`

---

## Code Quality Issues

### Magic Numbers

```c
// stda_auto.c - BAD
if(genRand(&MTwister_rand_struct) > 0.47) return;

// GOOD
#define DEFENSE_PROBABILITY 0.47  // Tunable strategy parameter
if(genRand(&MTwister_rand_struct) > DEFENSE_PROBABILITY) return;
```

### Error Handling Inconsistency

**Recommendation:** Consistent error enum:

```c
typedef enum {
    GAME_OK = 0,
    GAME_ERR_EMPTY_DECK,
    GAME_ERR_INSUFFICIENT_CASH,
    GAME_ERR_INVALID_CARD,
    GAME_ERR_ILLEGAL_MOVE
} GameError;

GameError DeckStk_pop_safe(struct deck_stack* deck, uint8_t* out);
```

---

## By Module Status

### Core Game Logic (src/core/)

#### Card Actions

Recall mechanic (draw/recall cards, interactive CLI only) is complete -- see `doc/changelog.md`.

- [ ] Better error handling

---

### AI Strategies (src/ai/)

#### Balanced Rules Strategy

- [ ] In stda.cli mode, when AI against AI play is selected, use 'AI strategy name + (A or B)' as the player name instead of asking for player 1's name and not asking for player 2
- [ ] Design decision framework (see strat_balancedrules1.c notes)
- [ ] Attack heuristics:
  - [ ] When to play 0-cost champions
  - [ ] When to play draw/recall vs champions
  - [ ] Target hand size based on opponent energy
  - [ ] Target cash balance based on opponent energy
- [ ] Defense heuristics:
  - [ ] When to defend vs decline
  - [ ] Which defenders to play
  - [ ] E[Total Def] ≤ E[Total Attack] - β·σ rule
  - [ ] Prioritize low attack efficiency cards for defense
- [ ] Card selection:
  - [ ] Best attacker selection
  - [ ] Best defender selection
  - [ ] Power/efficiency calculations
- [ ] Parameter tuning:
  - [ ] Calibrate target cash/hand formulas
  - [ ] Optimize defend probability
  - [ ] Test vs Random AI (should win >70%)

#### Heuristic Strategy 📋

- [ ] Advantage function (see strat_heuristic1.c notes)
  - [ ] Energy advantage (own - opponent)
  - [ ] Cards advantage (effective deck size)
  - [ ] Cash advantage
- [ ] 1-move lookahead
- [ ] Action evaluation
- [ ] Parameters:
  - [ ] ε (epsilon) for energy weight
  - [ ] γ (gamma) for cards weight
- [ ] Calibration against Balanced AI

---

### Game Modes (src/)

#### Automated Simulation (stda_auto.c) ⚠️

- [ ] **Refactor**: Extract simulation.c module (part of the 'improve source code folder structure' folder under 'ideas')
- [ ] Support multiple deck types (currently hardcoded random)
- [ ] Better statistics:
  - [ ] Confidence intervals
  - [ ] Effect size calculations
  - [ ] Win rate standard error
- [ ] CSV export integration
- [ ] Progress display during long runs

#### CLI Mode (stda_cli.c) ⚠️

Combat results display is complete -- see `doc/changelog.md`.

- [ ] Save/load game state

#### TUI Mode (stda_tui.c)

**Milestone 1 (ncurses display skeleton) is done** -- see `doc/changelog.md`
(2026-07-14). `src/ui/tui/tui_render.c/h` + `src/roles/stda/stda_tui.c/h`; AI-vs-AI only,
one turn advances per keypress, layout matches `Template TUI Game Interface.pdf` /
`Gabarit Interface de Jeu Version Texte.xlsx` and is fully responsive (any terminal size
>= 100x30, `KEY_RESIZE`-aware). See `ideas/3 tui/` for the original design intent (its
code prototype is superseded).

- [x] ncurses initialization
- [x] Window layout: play area (left), info column split into shortcuts/message-box
      /console panels (right), top+bottom status bars, command line (bottom, inert)
- [x] Display functions: player info (energy, lunas), hand display (with colors, hidden
      for opponent), combat zone, deck/discard counts
- [x] Message log system (scrolling console)

**Milestone 2 (human interaction) -- not started. This is the next step.**

### Milestone 2 handout (read this first in a fresh session)

**Where M1 left off**: `stda.tui` (`-t`/`--stda.tui`) renders a full, responsive,
mirrored-table ncurses layout for a live game, but it's **AI-vs-AI only** --
`src/roles/stda/stda_tui.c`'s loop calls `play_turn()` (the same whole-turn function
`stda_auto.c` uses) once per keypress, so there is no human player, no card
selection, and the `> ` command line at the bottom is a dead placeholder. All of
this is done and verified (`doc/changelog.md` has 4 dated 2026-07-14 entries
covering it) -- don't re-litigate the layout, colors, centering, discard-corner
grid, or console wrapping; those are settled. M2 is purely about wiring up
**interaction**.

**The goal**: let a human play a real game against an AI opponent inside the TUI,
using the same rules/features the interactive CLI (`stda.cli`) already supports.

**Why this is harder than it sounds**: the CLI's interactive logic
(`src/ui/cli/cli_game.c`, `cli_input.c`) is tightly coupled to blocking
`fgets`/line-based `printf` -- e.g. `handle_interactive_attack()`,
`handle_recall_choice()`, `prompt_champion_exchange()` all read a whole line from
stdin and print prompts directly. None of that can be dropped into an ncurses
window as-is (ncurses wants single-keystroke input via `getch()`, not blocking
`fgets`). So M2 isn't "just add input handling to the TUI" -- it's "figure out how
much of the CLI's interactive orchestration logic can be *shared* with the TUI
instead of duplicated."

**Concrete features to port** (each already works in `stda.cli` -- use it as the
spec/reference, don't re-derive the rules):
1. **`TAB` toggle** between **PLAY mode** (single keystrokes: `0`-`9`/similar select
   a hand card, `P` pass, etc.) and **COMMAND mode** (the `> ` line, reusing the
   CLI's exact grammar: `cham <indices>`, `draw <index>`, `cash <index>`, `pass`).
   This hybrid was the user's explicit choice when M1 was scoped (see the PDF/xlsx
   template, which shows both a play zone and a command line).
2. **Card play** (attack phase) and **defense selection** -- `handle_interactive_attack`
   / `handle_interactive_defense` in `cli_game.c` are the reference.
3. **Recall**: playing a draw/recall card must offer "Draw N" vs "Recall **exactly**
   M champions from discard" (never "up to") -- `card_actions.c`'s `choose_num`,
   `cli_input.c`'s `handle_recall_choice()`/`validate_and_recall_champions()`.
4. **Cash exchange**: human picks which champion to exchange (AI auto-picks
   lowest-power) -- `cli_input.c`'s `prompt_champion_exchange()`.
5. **Mulligan** (Player B, game start) and **discard-to-7** (end of every turn) --
   `cli_game.c`'s `handle_interactive_mulligan()` / `handle_interactive_discard_to_7()`.
6. Once real interaction exists, the combat-zone card-rendering path can finally be
   exercised by normal play (see the M1 changelog's "known gap" note: right now
   `resolve_combat()` clears both zones before `play_turn()` returns, so
   `tui_draw_combat_zone()`'s non-empty path has never been hit in AI-vs-AI M1
   testing) -- this alone is a good early smoke test once phase-by-phase pausing
   exists.

**The one real architectural decision, make it early**: a minimal
**display/input callback seam** so `cli_game.c`'s orchestration functions can call
out to "show this prompt" / "read this input" through a small interface that both
`cli_input.c` (prints/reads via stdio) and a new `tui_input.c` (draws into ncurses
windows/reads via `getch()`) implement -- versus just duplicating the CLI's
prompt/validation logic wholesale into `stda_tui.c`/`tui_input.c`. This is the
*only* slice of `ideas/2 game engine refactoring for GUI and network support/`'s
proposal that's relevant here (its Action-struct/`get_list_of_possible_actions()`/
`apply_action()`/`VisibleGameState`/client-server split is NOT needed -- that
solves network multiplayer + MCTS tree search, a different problem). Decide this
before writing card-selection code, since it determines whether new files land in
`src/ui/tui/tui_input.c` calling shared logic, or whether `cli_input.c` itself gets
refactored to take a callback struct.

**Suggested sequencing**: (1) settle the callback-seam question with a short design
note or plan; (2) wire the `TAB` toggle + inert-to-live command line first (lowest
risk, mirrors M1's own staged approach); (3) card play/defense; (4) recall, cash
exchange, mulligan, discard-to-7 in whatever order feels natural; (5) context-sensitive
shortcut panel content + live combat-result formatting in the message box (currently
static placeholders); (6) help overlay; (7) mode switching (TUI <-> SIM), low priority.

**Key files**: `src/roles/stda/stda_tui.c/h` (current AI-vs-AI loop, will need a
human-branch), `src/ui/tui/tui_render.c/h` (rendering, done), `src/ui/cli/cli_game.c`
+ `cli_input.c` (the interactive logic to port/share), `src/core/card_actions.c`
(engine-level recall/cash/discard functions both UIs call into either way).

Two refinements to the above paragraphs:

**Refinement 1 — Seam granularity: plan for multi-step dialogues, not just prompt/read pairs.**
The handout describes the seam as "show this prompt / read this input," which fits single prompt-then-read interactions (attack, defense, pass). But recall and cash-exchange are multi-step sub-dialogues with their own validation loops — recall is "offer Draw N vs. Recall exactly M champions from discard, then have the user pick which M, re-prompting on invalid input," and cash-exchange is "pick which champion to exchange." A pure line-read seam forces that loop/validation logic to either branch on CLI-vs-TUI or be reimplemented in the TUI. So design a **two-level seam**: low-level primitives (`show_prompt`, `read_line`, `read_key`) plus a few **composite helpers** for the multi-step flows (e.g. `ui_choose_from_discard(...)`, `ui_confirm_count(...)`) that own the validation loop once and call the primitives internally. That way neither UI duplicates the recall/cash loops.

**Refinement 2 — Keep validation in `ui/shared/`, callback struct is I/O-only.**
Both the handout and this chat agree the CLI's rules/validation (`parse_card_indices_with_validation`, affordability checks, exact-count enforcement) should be shared. The handout leaves open *how* — new `tui_input.c` calling shared logic, vs. refactoring `cli_input.c` to take a callback struct. Take the first fork: pull the **pure validation** (no `printf`, no `fgets`) into `ui/shared/`, callable by both `cli_input.c` and `tui_input.c`, and let the callback struct carry **only I/O** (prompt/read primitives + the composite helpers from Refinement 1). This keeps the interface small and the validation UI-agnostic, avoiding the messier "thread a callback struct through cli_input.c" option.

Both belong in step (1) of the suggested sequencing — the design note that settles the callback-seam question — and should be decided before any card-selection code is written, since together they determine the seam's shape and where new files land.

#### Simulation UI (stda_sim.c) 📋

- [ ] ncurses-based results display
- [ ] Live progress bar
- [ ] Win rate display
- [ ] Strategy comparison table
- [ ] Parameter controls
- [ ] ASCII art graphs (histogram)
- [ ] Export commands
- [ ] Mode switching (SIM ↔ TUI)

---

### Utilities (src/)

#### Command-Line Parsing ✅

- [ ] Add --config option
- [ ] Add --deck option (random/mono/custom/the 3 drafting formats)

#### Game Context ✅

- [ ] Document usage patterns in DESIGN.md

#### Debug System ✅

- [ ] Add debug levels (INFO, WARN, ERROR)
- [ ] Add file/line number to debug output 

---

### Data Structures (src/)

#### Deck Stack ✅

- [ ] Add DeckStk_size() helper
- [ ] Add DeckStk_peek_at(index) for debugging

---

## New Features to Add

### Configuration System 📋

See `ideas/config file/` for implementation

- [ ] config.c/h implementation
- [ ] INI-style parser
- [ ] read_config_file()
- [ ] Default configuration
- [ ] User config (~/.oraclerc)
- [ ] Command-line override
- [ ] save_config()

### CSV Export System 📋

See `ideas/sim_export_spec.md` for full specification

- [ ] sim_export.c/h implementation
- [ ] SimExporter structure
- [ ] generate_simparam_string()
- [ ] export_game_result()
- [ ] export_summary()
- [ ] Detail CSV (per-game)
- [ ] Summary CSV (aggregate)
- [ ] Filename conventions
- [ ] Integration with stda_auto mode

### Rating System 📋

See `ideas/rating system/rating system BT v2/` for complete spec

- [ ] rating.c/h implementation
- [ ] RatingSystem structure
- [ ] Bradley-Terry calculations
- [ ] rating_init()
- [ ] rating_register_player()
- [ ] rating_update_match()
- [ ] rating_win_probability()
- [ ] Adaptive A function
- [ ] Keeper rebalancing
- [ ] CSV persistence
- [ ] Batch gradient ascent
- [ ] Calibration tools

---

## Testing & Quality

### Performance Tests 📋

- [ ] Memory leak detection (valgrind)
- [ ] Profile hot paths (gprof)

### Code Quality 📋

- [ ] Run with -Wall -Wextra (fix all warnings)
- [ ] Check with cppcheck
- [ ] Format with astyle (consistent style)
- [ ] Review all functions >35 lines (refactor)
- [ ] Review all files >500 lines (split if needed)

---

## Documentation Tasks

### Code Documentation 📋

- [ ] Add Doxygen comments to all public functions
- [ ] Document GameContext pattern in DESIGN.md
- [ ] Document CLI integration in DESIGN.md
- [ ] Document strategy framework in DESIGN.md
- [ ] Add examples to API.md
- [ ] Update file organization in DESIGN.md

### User Documentation 📋

- [ ] Update README.md:
  - [ ] Usage examples
  - [ ] - [ ] Feature list
  - [ ] Screenshot (CLI mode)
- [ ] Write STRATEGY_GUIDE.md:
  - [ ] AI strategy descriptions
- [ ] Write PROTOCOL.md (when network code exists)

### Design Documentation 📋

- [ ] Document actual vs planned architecture
- [ ] Add diagrams (flow charts, class diagrams)
- [ ] Document key design decisions
- [ ] Add examples for common operations

---

## Bug Tracker

### Known Bugs 🐛

- [ ] Describe bug here

---

## Action Items (preparation for client / server approach and MCTS)

- **Create get_available_moves()** function
- **Implement game state cloning** for MCTS
- **Add phase state machine** for cleaner turn flow

---

## Technical Debt

### Refactoring Needed 🔧

- [ ] stda_auto.c mixes simulation logic with presentation
- [ ] card_actions.c needs better error handling
- [ ] gamestate.c setup_game() is too long

### Architecture Improvements 🏗️

- [ ] Separate game logic from UI (already started with GameContext)
- [ ] Define clear API boundaries (core vs modes vs ui)
- [ ] Create action validation layer (before applying actions)
- [ ] Implement proper error codes (not just printf)
- [ ] Add logging system (not just DEBUG_PRINT)

### Code Cleanup 🧹

- [ ] Remove old commented-out code
- [ ] Consistent naming (some use camelCase, some snake_case)
- [ ] Consolidate constants (some in .h, some in .c)
- [ ] Remove unused functions/variables
- [ ] Update all file headers with consistent format

---

## Ideas / Future Exploration

### AI Experiments 🤖

- [ ] Parameter sweep (find optimal values)
- [ ] Strategy tournaments (round-robin)
- [ ] Co-evolution (strategies compete and evolve)
- [ ] Neural network evaluation function
- [ ] Reinforcement learning agent

### Network Features 🌐

- [ ] Matchmaking by rating

---

## Checklist: Adding a New AI Strategy

1. [ ] Create `src/ai/strategy_name.c/h`
2. [ ] Implement `strategyname_attack_strategy()`
3. [ ] Implement `strategyname_defense_strategy()`
4. [ ] Add strategy registration in main.c
5. [ ] Test against Random AI (10,000 games)
6. [ ] Measure win rate
7. [ ] Compare against other strategies
8. [ ] Document in STRATEGY_GUIDE.md
9. [ ] Add design notes to `ideas/`
10. [ ] Update ROADMAP.md status

---

## Checklist: Adding a New Game Mode

1. [ ] Create entry point `src/modes/mode_name.c/h`
2. [ ] Add mode to game_mode_t enum (game_types.h)
3. [ ] Add command-line option (cmdline.c)
4. [ ] Add run_mode_name() in main.c
5. [ ] Implement mode-specific UI
6. [ ] Handle mode initialization/cleanup
7. [ ] Test mode thoroughly
8. [ ] Document in README.md
9. [ ] Update help text (cmdline.c)
10. [ ] Update ROADMAP.md

---

## Quick Reference: File Line Counts

*Target: <500 lines per file, <30 lines per function*

---

## Notes for Future

### When Implementing Recall

**Done for CLI** (see "Current Focus" above) -- still applies when building TUI input:

- Draw/recall cards allow choice
- Must recall **exactly** N champions from discard (not "up to" -- mandatory, no partial/zero recall), and only when discard holds at least N champions
- Must be champions (not draw/cash cards)
- Champions go to hand
- Integrate with TUI input (CLI already done)

### When Starting Network Code

- Read DESIGN.md thoroughly
- Start with text protocol (easier to debug)
- Implement binary protocol later
- Test with localhost first
- Use separate processes (not threads initially)

### When Starting GUI

- Read `ideas/gui/oracle_sdl3_gui_plan.md`
- Start with card rendering only
- Test on one platform first (Linux easier)
- Don't worry about mobile yet
- Focus on functionality over polish

---

*Last Updated: July 2026*
