# Oracle Development TODO

**Quick Status**: In progress: completing miscellaneous actions / commands in interactive mode (recall, combat results details, discard pile inspection, cash card functionality)

---

## Current Focus

**Complete Turn Logic Module** - Get full game loop working end-to-end in interactive mode with all the rules

Tasks:

- Display Discard Pile in CLI Mode

- Get Recall Card functionality to work in at least stda.cli mode (it's fine to just use the 'draw n cards' option for the Random AI engine given that this engine is not meant to be strong) 

- Enhance display of combat results in stda.cli mode

- When playing cash card in interactive mode, ask user to select the champion card that they want to discard in exchange of 5 lunas instead of letting the AI decide automatically based on power heuristic

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

Current code is monolithic. Start preparing for separation. See notes in 'ideas/game engine refactoring for GUI and network support'

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

#### Card Actions ⚠️ IN PROGRESS

- [ ] **Recall mechanic** (draw/recall cards)
  - [ ] recall_champion() function
  - [ ] UI for choosing which champion to recall
  - [ ] Validation (champion must be in discard)
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

- [ ] Show combat results clearly
- [ ] Save/load game state

#### TUI Mode (stda_tui.c) 📋

See `ideas/tui/` for full implementation plan

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

- Draw/recall cards allow choice
- Can recall up to N champions from discard
- Must be champions (not draw/cash cards)
- Champions go to hand
- Integrate with CLI/TUI input

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

*Last Updated: December 2025*
