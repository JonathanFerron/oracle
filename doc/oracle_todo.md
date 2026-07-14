# Oracle Development TODO

**Quick Status**: Turn-logic interactive-mode commands (recall, combat results details, discard pile inspection, cash card functionality) are complete. Deciding next feature area (see "Next Up" below).

---

## Current Focus -- DONE (2026-07-13)

**Complete Turn Logic Module** - Get full game loop working end-to-end in interactive mode with all the rules

Tasks:

- [x] Display Discard Pile in CLI Mode -- `gmst` (summary) and `shod` (detailed, power-sorted) commands; see `ideas/done/4 ...`.

- [x] Get Recall Card functionality to work in stda.cli mode -- recall is **exact and mandatory** (a "recall 1 / draw 2" card recalls exactly 1 champion, "recall 2 / draw 3" recalls exactly 2; recall is only offered when discard holds enough champions). The Random AI engine still only ever draws (never recalls), which is fine given it's not meant to be strong. See `ideas/done/2 ...`, `doc/game_rules_doc.md` (recall section corrected to match), and `testsrc/test_recall.c`.

- [x] Enhance display of combat results in stda.cli mode -- per-champion rolls/base/combo/damage breakdown, shown whenever a human is involved; `stda.auto` unaffected. See `ideas/done/3 ...`.

- [x] When playing cash card in interactive mode, ask user to select the champion card to exchange instead of the AI power-heuristic auto-pick -- interactive path (`play_cash_card_interactive`) lets the human pick freely. Along the way, fixed a real bug in the AI heuristic (`select_champion_for_cash_exchange` conflated "not found" with card index 0, a valid champion, using it as a sentinel -- now uses `UINT8_MAX`). See `ideas/done/5 ...` and `testsrc/test_cash_exchange.c`.

**Note**: fixing the index-0 sentinel bug changed `stda_auto`'s RNG-dependent play sequence (different AI hand state whenever that bug used to fire), so `bin/expectedresults.txt` was regenerated (2026-07-13) to reflect the corrected behavior -- this was a deliberate re-baseline, not a regression.

All four verified via `make test_recall` / `make test_cash_exchange`, the `testsrc/cli_scripts/` manual scripts, a full valgrind pass (auto + interactive), and the `./bin/oracle -a -p` regression check against the regenerated `bin/expectedresults.txt`.

---

## Next Up (preferred order)

**Note on `ideas/` numbering (2026-07-14)**: folders were renumbered twice this session.
Non-AI planning folders now use plain integers 1-10 reflecting rough priority order; AI
agent folders were pulled into their own `A1`-`A11` namespace (kept together, in their
existing relative order) so adding new AI ideas doesn't require renumbering everything
else. See `git log` / folder contents if an old number (e.g. `ideas/8/`, `ideas/14.3/`)
shows up in an older doc or commit message.

1. **Improve source code folder structure** (`ideas/1 improve source code folder structure/`) -- revisit before adding more UI surface area, so new modes don't compound existing structural debt. See `ideas/1 improve source code folder structure/pragmatic_cleanup_implementation_plan.md` for the scoped-down plan (splitting `cli_display.c`, fixing `make test_combo`, doc sync). That plan deliberately does **not** create the following directories yet -- they get created only when their first real file lands, in the folder noted:
   - `deck_formats/` -- when a draft/deck-format feature is implemented (`ideas/10 Draft Format and Game Depth Addition Ideas/`)
   - `game_rules/` -- when the game-engine refactor (below) needs a home for rules data separate from `core/`
   - `interactive/` -- when TUI/GUI interactive-mode code needs a shared home distinct from `ui/cli/`
   - `network/` -- client/server (`ideas/8 client server/`)
   - `persistence/` -- save/load game state (`ideas/6 save and load gamestate/`)
   - `config/` -- configuration file system (`ideas/7 config file/`)
   - `platform/` -- if/when platform-specific code (beyond the current `#ifdef _WIN32` blocks) grows enough to warrant its own directory
2. **TUI mode** (`ideas/3 tui/`) -- may need to pull in part of **game engine refactoring for GUI/network support** (`ideas/2 game engine refactoring for GUI and network support/`) first, specifically the clean state-machine / UI-callback groundwork, so the TUI isn't built directly on top of the CLI-specific display/input functions.
3. **First "non-dumb" AI strategy** (`ideas/A1 ai agent value based/`) -- once the above structural work is settled. **Once any new AI strategy beyond Random is implemented**, update `display_ai_strategy_menu()`/`get_ai_strategy_choice()` in `src/ui/shared/player_config.c` (currently only lists Random/Balanced/Heuristic/Hybrid/Simple MC/IS-MCTS) and the `AIStrategyType` enum to match the fuller planned roster, now tracked as `ideas/A1`-`A11`: `A1` value-based, `A2` parameter storing/optimization, `A3` greedy power, `A4` combo-aware (Borealis benchmark), `A5` balanced, `A6` heuristics, `A7` tactical + HBT, `A8` HBT 2-ply, `A9` simple MC, `A10` IS-MCTS, `A11` IS-MCTS + neural network -- the CLI menu is missing several of these and should stay in sync as each is built.

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

- [x] **Recall mechanic** (draw/recall cards) -- interactive CLI only (see "Current Focus" above); `validate_and_recall_champions()` + `handle_recall_choice()` in `cli_input.c`
  - [x] recall function (`validate_and_recall_champions()`)
  - [x] UI for choosing which champion(s) to recall (`display_recallable_champions()`, exact-count prompt)
  - [x] Validation (champions must be in discard; exact count enforced; recall not offered below the required count)
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

- [x] Show combat results clearly -- `display_combat_details_cli()` in `ui/cli/cli_display.c`
- [ ] Save/load game state

#### TUI Mode (stda_tui.c) 📋

See `ideas/3 tui/` for full implementation plan

- [ ] ncurses initialization
- [ ] Window layout:
  - [ ] Game area (left)
  - [ ] Console/log (right)
  - [ ] Status bar (bottom)
  - [ ] Command input (bottom)
- [ ] Display functions:
  - [ ] Player info (energy, lunas, hand size)
  - [ ] Hand display (with colors)
  - [ ] Combat zone
  - [ ] Deck/discard counts
- [ ] Input handling:
  - [ ] Keyboard shortcuts
  - [ ] Command mode
  - [ ] Mouse support (maybe)
- [ ] Message log system
- [ ] Help overlay
- [ ] Mode switching (TUI ↔ SIM)

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
