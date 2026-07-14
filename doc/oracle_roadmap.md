# Oracle Development Roadmap

**Project**: Les Champions d'Arcadie / The Arcadian Champions of Light  
**Type**: Open source hobby/research project  
**Focus**: Card game AI research, C programming patterns, game architecture

---

## Current Status

**Active Work**: Deciding next feature area (see "Next Up" below); Turn Logic & Game Loop's interactive-mode command set is now complete.

### Recently Completed (2026-07-13)

- ✅ Recall mechanic (interactive CLI, exact/mandatory count)
- ✅ Interactive cash-card champion selection
- ✅ Detailed combat results display (interactive CLI)
- ✅ Discard pile display (`gmst` summary, `shod` detail)

### What Needs Work

- ⚠️ Automated simulation mode needs refactoring
- ⚠️ No save/load functionality
- ⚠️ Limited AI strategies (only random implemented)

### Next Up (preferred order)

1. Improve source code folder structure (`ideas/8/`)
2. TUI mode (`ideas/13/`) -- may need part of the game-engine refactoring for GUI/network support (`ideas/9/`, clean state-machine/UI-callback groundwork) first
3. First "non-dumb" AI strategy (`ideas/14.3/`, tactical + HBT)

Back burner (explicitly deferred): save/load game state (`ideas/6/`), configuration file system (`ideas/7/`).

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

- [x] Play draw/recall cards -- draw path was already working; recall path added (interactive CLI only, see below)
- [x] Play cash exchange cards -- AI auto-select path was already working; interactive champion choice added
- [x] Recall mechanic (draw/recall cards) -- see "Recall Mechanic" below

---

## Recall Mechanic -- RESOLVED (2026-07-13)

Was a major gap (`play_draw_card()` only ever drew; `struct card.choose_num` was unused).
Now implemented for the interactive CLI: `handle_recall_choice()`/`validate_and_recall_champions()`
in `ui/cli/cli_input.c`, using `choose_num` as an **exact, mandatory** count (not "up to") --
recall is only offered when discard holds enough champions. `game_rules_doc.md`'s recall
wording was corrected to match. The Random AI strategy still only ever draws (acceptable,
per its "not meant to be strong" design intent); a smarter AI recall strategy is future work.
See `ideas/done/2 Recall Card functionality in cli mode/` and `testsrc/test_recall.c`.

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

**Cash Card Selection**: `select_champion_for_cash_exchange()` (AI-only heuristic) is in `card_actions.c` with TODO "this code could be moved to the strategy" -- architectural boundary violation. It also had an index-0 sentinel bug (fixed 2026-07-13: now uses `UINT8_MAX` instead of `0` to mean "not found," since 0 is a valid champion index) -- fixing it changed `stda.auto`'s RNG-dependent output, so `bin/expectedresults.txt` was deliberately regenerated at the same time. The interactive CLI path no longer goes through this function at all -- `play_cash_card_interactive()` lets the human choose freely (see `ideas/done/5 cash card functionality in cli mode/` and `testsrc/test_cash_exchange.c`).

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
- GitHub repo: https://github.com/JonathanFerron/oracle/
- Design notes: See `ideas/` directory
- Similar projects: (add as you discover them)
- Academic papers: (add MCTS/rating system papers as you study them)

---

*Last Updated: July 2026*
