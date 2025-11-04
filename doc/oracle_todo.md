# Oracle Development TODO

**Quick Status**: Phase 2 in progress - completing turn logic and game loop

---

## Current Sprint Focus

### This Week's Priority

**Complete Turn Logic Module** - Get full game loop working end-to-end

Tasks:

- [ ] Implement mulligan system (Player B, 2 cards max)
- [ ] Implement discard-to-7 system (end of turn)
- [ ] Add phase transition validation
- [ ] Test complete game with Random vs Random
- [ ] Write unit tests for turn_logic.c

---

## By Module Status

### Core Game Logic (src/core/)

#### Card System ✅

- [x] Full 120-card deck definition (game_constants.c)
- [x] Card structure with all attributes
- [x] Champion species/orders
- [x] Draw cards (pige 2/3)
- [x] Cash exchange cards

#### Game State ✅

- [x] GameState structure (game_types.h)
- [x] PlayerState tracking
- [x] Deck/hand/discard/combat zones
- [x] Energy and luna management
- [x] Game initialization (game_state.c)

#### Combat System ✅

- [x] Combo bonus calculator (combo_bonus.c)
  - [x] Random deck combos
  - [x] Monochrome deck combos
  - [x] Custom deck combos
- [x] Combat resolver (combat.c)
- [x] Dice rolling (RND_dn)
- [x] Damage calculation
- [x] Combat zone clearing

#### Turn Logic ⚠️ IN PROGRESS

- [x] Basic turn structure (turn_logic.c)
- [x] begin_of_turn()
- [x] attack_phase()
- [x] defense_phase()
- [x] end_of_turn()
- [x] Card drawing (skip first player, first turn)
- [x] Luna collection
- [ ] **Mulligan system** [NEXT]
  - [ ] UI for Player B (2 cards max)
  - [ ] Power-based selection heuristic
  - [ ] Integration with CLI/TUI modes
- [ ] **Discard to 7 cards** [NEXT]
  - [ ] UI for attacker at end of turn
  - [ ] Power-based selection heuristic
  - [ ] Edge case: exactly 7 cards (do nothing)
- [ ] Phase transition validation
- [x] Win condition detection (energy = 0)
- [x] Draw condition (max turns exceeded)

#### Card Actions ⚠️

- [x] play_card() dispatcher
- [x] play_champion()
- [x] play_draw_card()
- [x] play_cash_card()
- [x] draw_1_card()
- [x] shuffle_discard_and_form_deck()
- [ ] **Recall mechanic** (draw/recall cards)
  - [ ] recall_champion() function
  - [ ] UI for choosing which champion to recall
  - [ ] Validation (champion must be in discard)
- [ ] Improve cost validation
- [ ] Better error handling

---

### AI Strategies (src/ai/)

#### Strategy Framework ✅

- [x] Function pointer system (strategy.h)
- [x] StrategySet structure
- [x] create_strategy_set()
- [x] set_player_strategy()
- [x] AttackStrategyFunc / DefenseStrategyFunc types

#### Random Strategy ✅

- [x] random_attack_strategy() (strat_random.c)
- [x] random_defense_strategy()
- [x] Affordable card filtering
- [x] 47% defend rate (tunable parameter)

#### Balanced Rules Strategy 📋 NEXT MAJOR FEATURE

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

- [x] Basic simulation loop
- [x] Random vs Random testing
- [x] Win statistics
- [x] Histogram generation
- [x] GameContext integration
- [ ] **Refactor**: Extract simulation.c module
- [ ] Support multiple deck types (currently hardcoded random)
- [ ] Better statistics:
  - [ ] Confidence intervals
  - [ ] Effect size calculations
  - [ ] Win rate standard error
- [ ] CSV export integration
- [ ] Progress display during long runs

#### CLI Mode (stda_cli.c) ⚠️

- [x] Basic game loop
- [x] Human vs AI gameplay
- [x] Command parsing (cham, draw, cash, pass, gmst, help, exit)
- [x] ANSI color output
- [x] UTF-8 symbols (❤ ☾ ⚔ 🛡)
- [x] Attack phase UI
- [x] Defense phase UI
- [x] Error messages
- [ ] **Mulligan UI** (Player B)
  - [ ] Display hand with power values
  - [ ] Prompt for cards to discard
  - [ ] Validation (max 2 cards)
- [ ] **Discard UI** (end of turn if hand > 7)
  - [ ] Display hand
  - [ ] Prompt for cards to discard
  - [ ] Validation (discard down to exactly 7)
- [ ] Better game state display
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

- [x] cmdline.c/h implementation
- [x] Long/short option support
- [x] Mode selection (--stda.auto, --stda.cli, etc.)
- [x] --numsim, --verbose, --output
- [x] Help and version display
- [ ] Add --config option
- [ ] Add --deck option (random/mono/custom/the 3 drafting formats)

#### Game Context ✅

- [x] GameContext structure (game_context.h)
- [x] RNG encapsulation
- [x] Config pointer
- [x] create_game_context()
- [x] destroy_game_context()
- [ ] Document usage patterns in DESIGN.md

#### Debug System ✅

- [x] debug.h with compile-time flags
- [x] DEBUG_PRINT macro
- [x] DEBUG_ONLY macro
- [x] -DDEBUG_ENABLED=1 flag support
- [ ] Add debug levels (INFO, WARN, ERROR)
- [ ] Add file/line number to debug output

#### Random Number Generation ✅

- [x] Mersenne Twister implementation (mtwister.c)
- [x] RND wrapper functions (rnd.c)
- [x] RND_dn() for dice rolls
- [x] RND_randn() for ranges
- [x] RND_partial_shuffle()
- [x] Add seed management (for reproducible games)

---

### Data Structures (src/)

#### Deck Stack ✅

- [x] deckstack.c/h implementation
- [x] DeckStk_push/pop
- [x] DeckStk_isEmpty
- [x] DeckStk_emptyOut
- [ ] Add DeckStk_size() helper
- [ ] Add DeckStk_peek_at(index) for debugging

#### Circular Linked List ✅

- [x] hdcll.c/h implementation (hand/discard/combat/list)
- [x] HDCLL_initialize
- [x] HDCLL_insertNodeAtBeginning
- [x] HDCLL_removeNodeFromBeginning
- [x] HDCLL_removeNodeByValue
- [x] HDCLL_removeNodeByIndex
- [x] HDCLL_getNodeValueByIndex
- [x] HDCLL_toArray
- [x] HDCLL_emptyOut
- [ ] Add HDCLL_find(value) helper
- [ ] Add HDCLL_contains(value) helper

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

### Unit Tests 📋

- [x] test_combo_bonus.c (exists, needs expansion)
- [ ] test_combat.c
  - [ ] Test dice rolling distribution
  - [ ] Test damage calculation
  - [ ] Test combo bonuses in combat
- [ ] test_turn_logic.c
  - [ ] Test phase transitions
  - [ ] Test turn counter
  - [ ] Test player switching
- [ ] test_card_actions.c
  - [ ] Test all card types
  - [ ] Test cost validation
  - [ ] Test draw mechanics
- [ ] test_gamestate.c
  - [ ] Test initialization
  - [ ] Test deck shuffling
  - [ ] Test hand dealing
- [ ] test_protocol.c (for future network code)

### Integration Tests 📋

- [x] Full game Random vs Random (1000 games)
  - [x] ~50% win rate for each
  - [x] No crashes
  - [x] Reasonable turn counts
- [ ] Full game Human vs AI (manual testing)
  - [ ] All phases work correctly
  - [ ] UI is responsive
  - [ ] Error handling works
- [ ] Simulation batch tests
  - [ ] Statistics are correct
  - [ ] CSV export works
  - [x] Reproducible with same seed

### Performance Tests 📋

- [ ] 10,000 game simulation (should complete in <5 min)
- [ ] Memory leak detection (valgrind)
- [ ] Profile hot paths (gprof)
- [ ] Optimize combo_bonus if needed

### Code Quality 📋

- [ ] Run with -Wall -Wextra (fix all warnings)
- [ ] Check with cppcheck
- [ ] Run with valgrind (no leaks)
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
  - [x] Build instructions
  - [ ] Usage examples
  - [ ] Feature list
  - [ ] Screenshot (CLI mode)
- [ ] Write STRATEGY_GUIDE.md:
  - [x] Explain combo bonuses
  - [ ] AI strategy descriptions
  - [x] Tips for playing
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

### Potential Issues ⚠️

- [ ] Memory leak in HDCLL_toArray (must free() after use)

---

## Technical Debt

### Refactoring Needed 🔧

- [ ] stda_cli.c exceeds 500 line limit (split into cli_display.c + cli_input.c)
- [ ] stda_auto.c mixes simulation logic with presentation (extract stda_simulation.c)
- [ ] card_actions.c needs better error handling
- [ ] gamestate.c setup_game() is too long (split deck setup)
- [x] Remove global MTwister_rand_struct (use GameContext everywhere)

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

### Gameplay Variants 🎮

- [ ] Different starting energy (not just 99)
- [ ] Different starting cash (not just 30)
- [ ] Best-of-N match mode
- [ ] Tournament mode (bracket)
- [ ] Survival mode (AI gauntlet)

### Analysis Tools 📊

- [ ] Replay system (record actions, replay later)
- [ ] Position analysis (evaluate game state)
- [ ] Opening book (common early game patterns)
- [ ] Card usage statistics (which cards win most)
- [ ] Combo occurrence frequency

### AI Experiments 🤖

- [ ] Parameter sweep (find optimal values)
- [ ] Strategy tournaments (round-robin)
- [ ] Co-evolution (strategies compete and evolve)
- [ ] Neural network evaluation function
- [ ] Reinforcement learning agent

### Network Features 🌐

- [ ] Lobby system
- [ ] Chat
- [ ] Friend list
- [ ] Matchmaking by rating
- [ ] Spectator mode
- [ ] Game replay streaming

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

Current status (approximate):

- ✅ game_constants.c: ~350 lines (mostly data)
- ✅ combat.c: ~100 lines
- ✅ turn_logic.c: ~80 lines
- ✅ card_actions.c: ~150 lines
- ⚠️ stda_auto.c: ~280 lines (split needed)
- ⚠️ stda_cli.c: ~550 lines (OVER LIMIT - split needed)
- ✅ strat_random.c: ~60 lines
- ✅ combo_bonus.c: ~200 lines

---

## Notes for Future Self

### When Implementing Mulligan

- Only Player B can mulligan
- Max 2 cards
- Use power heuristic (< AVERAGE_POWER_FOR_MULLIGAN)
- Draw replacement cards immediately
- Integrate with CLI/TUI input

### When Implementing Discard to 7

- Only at end of attacker's turn
- Only if hand > 7 cards
- Use power heuristic (discard lowest)
- Must discard down to exactly 7
- Integrate with CLI/TUI input

### When Implementing Recall

- Draw/recall cards allow choice
- Can recall up to N champions from discard
- Must be champions (not draw/cash cards)
- Champions go to hand
- Integrate with CLI/TUI input

### When Starting Network Code

- Read DESIGN.md section 6 thoroughly
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

*Last Updated: November 2024*  
*Next Review: When Phase 2 completes*
