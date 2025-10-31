# Oracle Development Roadmap

**Project**: Les Champions d'Arcadie / The Arcadian Champions of Light  
**Type**: Open source hobby/research project  
**Focus**: Card game AI research, C programming patterns, game architecture

---

## Current Status

**Active Work**: Phase 2 - Turn Logic & Game Loop Completion  
**Recent Completion**: Phase 1 - Core Foundation (100%)

### What's Working Now
- ✅ Full 120-card deck with all champion attributes
- ✅ Combo bonus calculator (random/monochrome/custom decks)
- ✅ Combat resolution with dice rolling
- ✅ Basic game state management
- ✅ Random AI strategy (functional baseline)
- ✅ CLI mode with color output and UTF-8 symbols
- ✅ Command-line argument parsing framework
- ✅ GameContext pattern for testability

### What Needs Work
- ⚠️ Turn logic incomplete (phase transitions, mulligan)
- ⚠️ Automated simulation mode needs refactoring
- ⚠️ No save/load functionality
- ⚠️ Limited AI strategies (only random implemented)

---

## Long-Term Vision

### Research Goals
1. **AI Development**: Progress from random → rule-based → Monte Carlo → MCTS
2. **Rating System**: Implement Bradley-Terry model to measure AI strength objectively
3. **Architecture**: Clean client/server separation for future multiplayer
4. **Simulation**: Export framework for statistical analysis of strategies
5. **Cross-Platform**: Terminal (ncurses), desktop (SDL3), mobile (future)

### Learning Objectives
- Advanced AI techniques (MCTS, information sets)
- Network programming patterns
- Statistical modeling (rating systems)
- GUI programming (SDL3)
- Build systems and cross-platform development

---

## Development Phases

### Phase 1: Core Foundation ✅ COMPLETE

**Status**: All critical components implemented and working

#### 1.1 Data Structures ✅
- [x] Card structure with all 120 cards
- [x] Game state structure
- [x] Deck management (stack-based)
- [x] Hand/discard/combat zone (circular linked lists)
- [x] GameContext pattern for RNG and config

#### 1.2 Combat System ✅
- [x] Combo bonus calculator (all deck types)
- [x] Combat resolver with dice rolling
- [x] Damage application
- [x] Combat zone management

#### 1.3 Basic Infrastructure ✅
- [x] Command-line parsing (cmdline.c)
- [x] Debug macro system (debug.h)
- [x] Mersenne Twister RNG
- [x] Color terminal output (CLI)

---

### Phase 2: Complete Game Loop ⚠️ IN PROGRESS

**Status**: Core logic exists, needs refinement and testing

#### 2.1 Turn Logic
- [x] Basic turn structure (begin/attack/defense/end)
- [x] Card drawing mechanics
- [x] Luna collection
- [ ] **Phase transition validation** [CURRENT FOCUS]
- [ ] **Mulligan system** (Player B, 2 cards max)
- [ ] **Discard to 7 cards** (end of turn)
- [ ] Win condition detection

#### 2.2 Deck Management
- [x] Shuffle and deal
- [x] Draw from deck
- [x] Reshuffle discard when deck empty
- [ ] Edge case testing (empty deck, empty hand)

#### 2.3 Card Actions
- [x] Play champion cards
- [x] Play draw/recall cards
- [x] Play cash exchange cards
- [ ] Recall mechanic (draw/recall cards)
- [ ] Proper cost validation

**Blockers**: None  
**Next**: Implement mulligan system, then validate full game loop with tests

---

### Phase 3: Standalone Modes

**Status**: Partial implementation, needs completion

#### 3.1 Automated Simulation Mode (stda.auto) ⚠️
- [x] Basic simulation loop
- [x] Random vs Random testing
- [x] Win statistics
- [x] Histogram generation
- [ ] **Refactor simulation engine** (extract from stda_auto.c)
- [ ] Better statistics (confidence intervals, effect size)
- [ ] Export to CSV (see sim_export_spec.md)
- [ ] Support for multiple deck types

#### 3.2 Interactive CLI Mode (stda.cli) ⚠️
- [x] Human vs AI gameplay
- [x] Command parsing (cham, draw, cash, pass)
- [x] Color output with symbols
- [ ] **Mulligan UI** for Player B
- [ ] **Discard UI** at end of turn
- [ ] Better error messages
- [ ] Game state display improvements
- [ ] Save/load game state

#### 3.3 Text UI Mode (stda.tui) 📋 PLANNED
- [ ] ncurses-based full-screen UI
- [ ] Real-time game board display
- [ ] Scrolling message log
- [ ] Command palette
- [ ] Keyboard shortcuts
- [ ] See `ideas/tui/` for detailed plan

---

### Phase 4: Basic AI Development

**Status**: Foundation ready, implementation pending

#### 4.1 Balanced Rules AI 📋
- [x] Strategy framework (function pointers)
- [ ] **Attack heuristics** (when to play champions vs draw)
- [ ] **Defense heuristics** (when to defend vs decline)
- [ ] **Card selection** (which cards to play)
- [ ] **Resource management** (luna/energy trade-offs)
- [ ] Parameter tuning against Random AI

**Reference**: See `src/strat_balancedrules1.c` for design notes

#### 4.2 Heuristic AI 📋
- [ ] Power heuristic for cards (offensive/defensive value)
- [ ] Advantage function (energy + cards + cash)
- [ ] 1-move lookahead evaluation
- [ ] Parameter calibration (epsilon, gamma)
- [ ] Compare performance vs Balanced AI

**Reference**: See `src/strat_heuristic1.c` for approach

#### 4.3 Hybrid AI 📋
- [ ] Combine Balanced + Heuristic
- [ ] Situational decision logic (early/mid/late game)
- [ ] Leading vs trailing tactics
- [ ] Resource-based strategy switching

---

### Phase 5: Simulation & Analysis Tools

**Status**: Specification complete, implementation pending

#### 5.1 CSV Export System 📋
- [ ] Per-game detail export
- [ ] Summary statistics export
- [ ] Simparam string generation (deck_stratA_stratB_params)
- [ ] Filename conventions
- [ ] Integration with stda.auto mode

**Specification**: See `ideas/sim_export_spec.md`

#### 5.2 Interactive Simulation UI (stda.sim) 📋
- [ ] ncurses-based results display
- [ ] Live progress updates
- [ ] Parameter adjustment UI
- [ ] Win rate graphs (ASCII art)
- [ ] Export commands
- [ ] Mode switching (sim ↔ tui)

#### 5.3 Configuration System 📋
- [ ] INI-style config file parser
- [ ] Default configuration
- [ ] Per-user config (~/.oraclerc)
- [ ] Command-line override
- [ ] Save current settings

**Reference**: See `ideas/config file/` for implementation

---

### Phase 6: Rating System

**Status**: Complete specification, ready for implementation

#### 6.1 Bradley-Terry Implementation 📋
- [ ] Core rating calculations (rating.c)
- [ ] Adaptive learning rate (A function)
- [ ] Keeper benchmark (rating = 50)
- [ ] Incremental updates
- [ ] Batch gradient ascent
- [ ] CSV persistence

**Specification**: See `ideas/rating system/rating system BT v2/`

#### 6.2 Rating Integration 📋
- [ ] Per-player rating tracking
- [ ] Automatic updates after matches
- [ ] Leaderboard display
- [ ] Rating-based matchmaking
- [ ] Historical rating graphs
- [ ] Confidence intervals

#### 6.3 Calibration Tools 📋
- [ ] Heuristic parameter optimization
- [ ] Non-champion card power values
- [ ] Strategy strength measurement
- [ ] Python analysis scripts

---

### Phase 7: Advanced AI (Monte Carlo)

**Status**: Design notes exist, major research component

#### 7.1 Simple Monte Carlo 📋
- [ ] Action enumeration (get all legal moves)
- [ ] Random rollout to game end
- [ ] Win rate per action
- [ ] Best action selection
- [ ] Performance optimization

**Reference**: See `src/strat_simplemc1.c`

#### 7.2 Progressive Pruning MC 📋
- [ ] Multi-stage rollouts (100/200/400/800)
- [ ] Confidence-based pruning
- [ ] Top-N retention
- [ ] Early stopping criteria

#### 7.3 UCB1 / PUCB1 📋
- [ ] Upper confidence bound for exploration
- [ ] Prior probability estimation
- [ ] Exploration-exploitation balance

---

### Phase 8: Information Set MCTS

**Status**: Advanced research goal, longest-term objective

#### 8.1 MCTS Core 📋
- [ ] Tree node structure
- [ ] Selection (UCT)
- [ ] Expansion
- [ ] Simulation (rollout)
- [ ] Backpropagation

**Reference**: See `src/strat_ismcts1.c` for design notes

#### 8.2 Information Set Handling 📋
- [ ] Determinization (observer's view)
- [ ] Hidden information management
- [ ] Clone and randomize game state
- [ ] Belief state tracking

#### 8.3 Optimizations 📋
- [ ] Tree reuse between turns
- [ ] Transposition tables
- [ ] RAVE (Rapid Action Value Estimation)
- [ ] Parallelization (multi-threaded)

#### 8.4 Neural Network Enhancement (Long-term) 🔮
- [ ] Prior probability predictor
- [ ] Value network
- [ ] Policy network
- [ ] Training infrastructure

---

### Phase 9: Client/Server Architecture

**Status**: Design complete, major refactoring required

#### 9.1 Protocol Design 📋
- [ ] Message types (action, gamestate, event)
- [ ] Binary serialization
- [ ] Text protocol (development/debugging)
- [ ] Action serialization
- [ ] State serialization (visible only)

**Reference**: See DESIGN DOC section 6

#### 9.2 Server Implementation 📋
- [ ] Socket server (TCP)
- [ ] Client connection management
- [ ] Game room system
- [ ] Full game state management
- [ ] Action validation
- [ ] Broadcast system

#### 9.3 Client Implementation 📋
- [ ] Socket client
- [ ] Local visible state tracking
- [ ] Action submission
- [ ] State sync
- [ ] Reconnection handling

#### 9.4 Code Separation 📋
- [ ] Extract shared types (sh_*.c/h)
- [ ] Server-only logic (sr_*.c/h)
- [ ] Client-only logic (cl_*.c/h)
- [ ] Protocol layer (pr_*.c/h)

---

### Phase 10: Cross-Platform GUI

**Status**: Detailed plan exists, major undertaking

#### 10.1 SDL3 Desktop GUI 📋
- [ ] SDL3 setup (Windows/Linux)
- [ ] Card rendering system
- [ ] Font management
- [ ] Texture cache
- [ ] Layout system (normalized coords)
- [ ] Animation framework
- [ ] Input handling (mouse/keyboard)

**Specification**: See `ideas/gui/oracle_sdl3_gui_plan.md`

#### 10.2 Asset Pipeline 📋
- [ ] Champion artwork (102 cards)
- [ ] Card frame templates
- [ ] Species icons (15)
- [ ] Order symbols (5)
- [ ] UI elements
- [ ] Font selection
- [ ] Asset generation tools (Python)

#### 10.3 Mobile Platforms (Future) 🔮
- [ ] iOS port (Xcode + SDL3)
- [ ] Android port (NDK + SDL3)
- [ ] Touch input
- [ ] Tablet UI layout
- [ ] Platform-specific builds

---

## Proposed File Structure Reorganization

### Current Issues
- Mixed concerns in `src/` (game logic, UI, strategies, modes)
- No clear client/server separation
- Growing file count hard to navigate

### Recommended Structure

```
oracle/
├── src/
│   ├── core/              # Pure game logic (platform-agnostic)
│   │   ├── card.c/h
│   │   ├── gamestate.c/h
│   │   ├── combat.c/h
│   │   ├── turn_logic.c/h
│   │   ├── deck.c/h
│   │   ├── combo_bonus.c/h
│   │   └── constants.c/h
│   │
│   ├── ai/                # AI strategies
│   │   ├── ai_interface.h
│   │   ├── random.c/h
│   │   ├── balanced.c/h
│   │   ├── heuristic.c/h
│   │   ├── hybrid.c/h
│   │   ├── mc_simple.c/h
│   │   └── ismcts.c/h
│   │
│   ├── rating/            # Bradley-Terry rating system
│   │   ├── rating.c/h
│   │   └── calibration.c/h
│   │
│   ├── sim/               # Simulation engine
│   │   ├── simulation.c/h
│   │   └── export.c/h
│   │
│   ├── ui/                # User interfaces
│   │   ├── cli/
│   │   │   ├── cli_main.c/h
│   │   │   ├── cli_display.c/h
│   │   │   └── cli_input.c/h
│   │   ├── tui/
│   │   │   ├── tui_main.c/h
│   │   │   ├── tui_render.c/h
│   │   │   └── tui_input.c/h
│   │   └── gui/           # SDL3 GUI (future)
│   │       ├── gui_main.c/h
│   │       ├── card_render.c/h
│   │       └── font_manager.c/h
│   │
│   ├── modes/             # Game mode entry points
│   │   ├── stda_auto.c/h  # Standalone automated
│   │   ├── stda_sim.c/h   # Standalone simulation UI
│   │   ├── stda_cli.c/h   # Standalone CLI
│   │   └── stda_tui.c/h   # Standalone TUI
│   │
│   ├── net/               # Network layer (future)
│   │   ├── protocol.c/h
│   │   ├── server.c/h
│   │   └── client.c/h
│   │
│   ├── util/              # Utilities
│   │   ├── rnd.c/h
│   │   ├── mtwister.c/h
│   │   ├── config.c/h
│   │   ├── debug.h
│   │   └── context.c/h
│   │
│   ├── data/              # Data structures
│   │   ├── deckstack.c/h
│   │   ├── hdcll.c/h
│   │   └── types.h
│   │
│   ├── cmdline.c/h        # Command-line parsing
│   ├── version.h
│   └── main.c             # Main entry point
│
├── assets/                # Game assets (GUI)
│   ├── cards/
│   ├── icons/
│   ├── fonts/
│   └── ui/
│
├── tools/                 # Python utility scripts
│   ├── generate_assets.py
│   ├── analyze_sims.py
│   ├── calibrate.py
│   └── test_protocol.py
│
├── tests/                 # Unit tests
│   ├── test_combo.c
│   ├── test_combat.c
│   └── test_protocol.c
│
├── docs/                  # Documentation
│   ├── ROADMAP.md         # This file
│   ├── TODO.md            # Task tracking
│   ├── DESIGN.md          # Technical design
│   ├── API.md             # API reference
│   ├── PROTOCOL.md        # Network protocol
│   └── STRATEGY_GUIDE.md  # AI strategy notes
│
├── ideas/                 # Design explorations
│   ├── gui/
│   ├── rating_system/
│   ├── config_file/
│   └── sim_export/
│
├── Makefile
└── README.md
```

### Migration Plan
1. **Phase 1**: Create new directory structure (empty)
2. **Phase 2**: Move files incrementally (one module at a time)
3. **Phase 3**: Update Makefile for new paths
4. **Phase 4**: Update #include statements
5. **Phase 5**: Test compilation after each module move
6. **Phase 6**: Update documentation to reflect new structure

**Note**: No need to rush this. Can be done gradually as features are added.

---

## Research Questions to Explore

### AI Development
- What's the minimum number of MCTS rollouts for good play?
- How much does combo bonus affect optimal strategy?
- Can rule-based AI approach MCTS performance?
- What's the skill ceiling with perfect information?

### Game Balance
- Are all three deck types (random/mono/custom) balanced?
- Do certain species/orders dominate?
- Is the mulligan rule fair?
- What's the optimal starting cash amount?

### System Design
- Best way to serialize game state for network play?
- How to handle reconnection in multiplayer?
- Efficient card representation for GUI rendering?
- Optimal strategy framework for pluggable AIs?

---

## Success Criteria

### Short-Term (Current Phase)
- [ ] Can simulate 10,000 games without crashes
- [ ] Human can play full game via CLI
- [ ] Random AI vs Random AI produces ~50% win rate
- [ ] All game rules correctly implemented

### Medium-Term (Phases 4-6)
- [ ] At least 3 different AI strategies working
- [ ] Rating system accurately ranks AI strength
- [ ] CSV export generates usable data for R/Python analysis
- [ ] TUI mode provides good user experience

### Long-Term (Phases 7-10)
- [ ] ISMCTS AI demonstrably stronger than rule-based
- [ ] Network multiplayer works reliably
- [ ] Cross-platform GUI runs on Windows/Linux/macOS
- [ ] Project serves as good portfolio/learning showcase

---

## Contributing (to yourself, future self!)

### Before Starting a Module
1. Read relevant DESIGN.md section
2. Check TODO.md for current status
3. Review any design notes in `ideas/`
4. Write test cases first (TDD approach)
5. Keep functions under 30 lines

### After Completing a Module
1. Update TODO.md checkboxes
2. Add entry to CHANGELOG (future)
3. Update DESIGN.md if architecture changed
4. Commit with descriptive message
5. Push to GitHub for backup

### When Stuck
1. Write design notes in `ideas/`
2. Implement simplest version that works
3. Refactor later (but not too much later)
4. Ask for help (GitHub discussions, forums)
5. Take a break, come back fresh

---

## References

- Game rules: See documents 1-2 (attached)
- GitHub repo: https://github.com/JonathanFerron/oracle/tree/main
- Design notes: See `ideas/` directory
- Similar projects: (add as you discover them)
- Academic papers: (add MCTS/rating system papers as you study them)

---

*Last Updated: November 2024*  
*Next Review: When Phase 2 completes*
