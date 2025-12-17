# Oracle Development Roadmap

**Project**: Les Champions d'Arcadie / The Arcadian Champions of Light  
**Type**: Open source hobby/research project  
**Focus**: Card game AI research, C programming patterns, game architecture

---

## Current Status

**Active Work**: Turn Logic & Game Loop Completion  

### What Needs Work

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

## File Size Targets Violated

**Contradiction:**

- Design guideline: "Maximum 500 lines per source file (ideally ≤400)"
- **Violations:**
  - `stda_cli.c`: 550 lines

**Impact:** Low-Medium - Code organization debt

---

## Development Phases

### Complete Game Loop ⚠️ IN PROGRESS

**Status**: Core logic exists, needs refinement and testing

#### Card Actions

- [ ] Play draw/recall cards
- [ ] Play cash exchange cards
- [ ] Recall mechanic (draw/recall cards)

---

## Recall Mechanic

**Contradiction:**

- Full deck includes 9 "Draw 2/Recall 1" and 6 "Draw 3/Recall 2" cards
- Game rules document extensively describes recall mechanics
- `card_actions.c` TODO: "TODO: must give the option to the interactive player to choose between draw and recall"
- **Reality:** `play_draw_card()` ONLY draws cards, never recalls
- `struct card` has `choose_num` field (for recall) but it's never used

**Impact:** High - Major game feature completely missing

---

## Standalone Modes

**Status**: Partial implementation, needs completion

#### Automated Simulation Mode (stda.auto) ⚠️

- [ ] **Refactor simulation engine** (extract from stda_auto.c)
- [ ] Better statistics (confidence intervals, effect size)
- [ ] Export to CSV (see sim_export_spec.md)
- [ ] Support for multiple deck types

#### Interactive CLI Mode (stda.cli) ⚠️

- [ ] Save/load game state

#### Text UI Mode (stda.tui) 📋 PLANNED

- [ ] ncurses-based full-screen UI
- [ ] Real-time game board display
- [ ] Scrolling message log
- [ ] Command palette
- [ ] Keyboard shortcuts
- [ ] See `ideas/tui/` for detailed plan

---

### Basic AI Development

**Status**: Foundation ready, implementation pending

Cash Card Selection**: `select_champion_for_cash_exchange()` is in `card_actions.c` with TODO "this code could be moved to the strategy" - architectural boundary violation

#### Balanced Rules AI 📋

- [ ] **Attack heuristics** (when to play champions vs draw)
- [ ] **Defense heuristics** (when to defend vs decline)
- [ ] **Card selection** (which cards to play)
- [ ] **Resource management** (luna/energy trade-offs)
- [ ] Parameter tuning against Random AI

**Reference**: See `src/strat_balancedrules1.c` for design notes

Notes on adding AI strategy to player_config.c and stda_cli.c:

```c
// In player_config.c - get_ai_strategies()
// When new strategy implemented, remove the warning:
if(choice == 2) // Balanced strategy now available
{
    return AI_STRATEGY_BALANCED;
}
```

```c
// In stda_cli.c - initialize_cli_game()
// Map AIStrategyType to actual function pointers:
PlayerConfig* pconfig = (PlayerConfig*)cfg->player_config;

for(int i = 0; i < 2; i++)
{
    if(cfg->player_types[i] == AI_PLAYER)
    {
        switch(pconfig->ai_strategies[i])
        {
            case AI_STRATEGY_RANDOM:
                set_player_strategy(strategies, i,
                    random_attack_strategy, random_defense_strategy);
                break;
            case AI_STRATEGY_BALANCED:
                set_player_strategy(strategies, i,
                    balanced_attack_strategy, balanced_defense_strategy);
                break;
            // Add other strategies as implemented
        }
    }
}
```

#### Heuristic AI 📋

- [ ] Power heuristic for cards (offensive/defensive value)
- [ ] Advantage function (energy + cards + cash)
- [ ] 1-move lookahead evaluation
- [ ] Parameter calibration (epsilon, gamma)
- [ ] Compare performance vs Balanced AI

**Reference**: See `src/strat_heuristic1.c` for approach

#### Hybrid AI 📋

- [ ] Combine Balanced + Heuristic
- [ ] Situational decision logic (early/mid/late game)
- [ ] Leading vs trailing tactics
- [ ] Resource-based strategy switching

---

### Simulation & Analysis Tools

**Status**: Specification complete, implementation pending

#### CSV Export System 📋

- [ ] Per-game detail export
- [ ] Summary statistics export
- [ ] Simparam string generation (deck_stratA_stratB_params)
- [ ] Filename conventions
- [ ] Integration with stda.auto mode

**Specification**: See `ideas/sim_export_spec.md`

#### Interactive Simulation UI (stda.sim) 📋

- [ ] ncurses-based results display
- [ ] Live progress updates
- [ ] Parameter adjustment UI
- [ ] Win rate graphs (ASCII art)
- [ ] Export commands
- [ ] Mode switching (sim ↔ tui)

#### Configuration System 📋

- [ ] INI-style config file parser
- [ ] Default configuration
- [ ] Per-user config (~/.oraclerc)
- [ ] Command-line override
- [ ] Save current settings

**Reference**: See `ideas/config file/` for implementation

---

### Rating System

**Status**: Complete specification, ready for implementation

#### Bradley-Terry Implementation 📋

- [ ] Core rating calculations (rating.c)
- [ ] Adaptive learning rate (A function)
- [ ] Keeper benchmark (rating = 50)
- [ ] Incremental updates
- [ ] Batch gradient ascent
- [ ] CSV persistence

**Specification**: See `ideas/rating system/rating system BT v2/`

#### Rating Integration 📋

- [ ] Per-player rating tracking
- [ ] Automatic updates after matches
- [ ] Leaderboard display
- [ ] Rating-based matchmaking
- [ ] Historical rating graphs
- [ ] Confidence intervals

#### Calibration Tools 📋

- [ ] Heuristic parameter optimization
- [ ] Non-champion card power values
- [ ] Strategy strength measurement
- [ ] Python analysis scripts

---

### Advanced AI (Monte Carlo)

**Status**: Design notes exist, major research component

#### Simple Monte Carlo 📋

- [ ] Action enumeration (get all legal moves)
- [ ] Random rollout to game end
- [ ] Win rate per action
- [ ] Best action selection
- [ ] Performance optimization

**Reference**: See `src/strat_simplemc1.c`

#### Progressive Pruning MC 📋

- [ ] Multi-stage rollouts (100/200/400/800)
- [ ] Confidence-based pruning
- [ ] Top-N retention
- [ ] Early stopping criteria

#### UCB1 / PUCB1 📋

- [ ] Upper confidence bound for exploration
- [ ] Prior probability estimation
- [ ] Exploration-exploitation balance

---

### Information Set MCTS

**Status**: Advanced research goal, longest-term objective

#### MCTS Core 📋

- [ ] Tree node structure
- [ ] Selection (UCT)
- [ ] Expansion
- [ ] Simulation (rollout)
- [ ] Backpropagation

**Reference**: See `src/strat_ismcts1.c` for design notes

#### Information Set Handling 📋

- [ ] Determinization (observer's view)
- [ ] Hidden information management
- [ ] Clone and randomize game state
- [ ] Belief state tracking

#### Optimizations 📋

- [ ] Tree reuse between turns
- [ ] Transposition tables
- [ ] RAVE (Rapid Action Value Estimation)
- [ ] Parallelization (multi-threaded)

#### Neural Network Enhancement (Long-term) 🔮

- [ ] Prior probability predictor
- [ ] Value network
- [ ] Policy network
- [ ] Training infrastructure

---

### Client/Server Architecture

**Status**: Design complete, major refactoring required

#### Protocol Design 📋

- [ ] Message types (action, gamestate, event)
- [ ] Binary serialization
- [ ] Text protocol (development/debugging)
- [ ] Action serialization
- [ ] State serialization (visible only)

**Reference**: See DESIGN DOC

#### Server Implementation 📋

- [ ] Socket server (TCP)
- [ ] Client connection management
- [ ] Game room system
- [ ] Full game state management
- [ ] Action validation
- [ ] Broadcast system

#### Client Implementation 📋

- [ ] Socket client
- [ ] Local visible state tracking
- [ ] Action submission
- [ ] State sync
- [ ] Reconnection handling

#### Code Separation 📋

- [ ] Extract shared types (sh_*.c/h)
- [ ] Server-only logic (sr_*.c/h)
- [ ] Client-only logic (cl_*.c/h)
- [ ] Protocol layer (pr_*.c/h)

---

### Cross-Platform GUI

**Status**: Detailed plan exists, major undertaking

#### SDL3 Desktop GUI 📋

- [ ] SDL3 setup (Windows/Linux)
- [ ] Card rendering system
- [ ] Font management
- [ ] Texture cache
- [ ] Layout system (normalized coords)
- [ ] Animation framework
- [ ] Input handling (mouse/keyboard)

**Specification**: See `ideas/gui/oracle_sdl3_gui_plan.md`

#### Asset Pipeline 📋

- [ ] Champion artwork (102 cards)
- [ ] Card frame templates
- [ ] Species icons (15)
- [ ] Order symbols (5)
- [ ] UI elements
- [ ] Font selection
- [ ] Asset generation tools (Python)

#### Mobile Platforms (Future) 🔮

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

- [ ] At least 3 different AI strategies working
- [ ] Rating system accurately ranks AI strength
- [ ] CSV export generates usable data for R/Python analysis
- [ ] TUI mode provides good user experience

### Longer-Term

- [ ] ISMCTS AI demonstrably stronger than rule-based
- [ ] Network multiplayer works reliably
- [ ] Cross-platform GUI runs on Windows/Linux/macOS
- [ ] Project serves as good portfolio/learning showcase

---

## Contributing

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

*Last Updated: December 2025*
