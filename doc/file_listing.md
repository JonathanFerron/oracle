# Complete File Listing for Oracle: The Champions of Arcadia

## Project Structure Overview

The Oracle game is organized into a modular C codebase with clear separation of concerns. Each module handles a specific aspect of the game logic, from core data types to AI strategies. Header files (.h) and their corresponding implementation files (.c) are documented together for easy reference.

---

## Core Entry Point

### main.c
- **Purpose:** Program entry point and mode selection
- **Key Functionality:**
  - Command-line argument parsing (placeholder)
  - Mode dispatcher: `run_chosen_mode()`
  - Standalone automated simulation: `runmode_standalone_automatedsim()`
  - RNG initialization with `M_TWISTER_SEED`
  - Global `debug_enabled` flag
- **Lines of Code:** ~60 (excluding comments/whitespace)

---

## Core Type and Constant Modules

### 1. game_types.h
- **Purpose:** Foundation header containing all enum and struct definitions
- **Key Contents:**
  - Enums: `PlayerID`, `GameStateEnum`, `TurnPhase`, `CardType`, `ChampionColor`, `ChampionSpecies`, `ChampionOrder`, `DeckType`
  - Structs: `card`, `gamestate`, `gamestats`
  - Includes dependencies: `deckstack.h`, `hdcll.h`
- **Note:** Header-only module (no corresponding .c file)

### 2. game_constants.h / game_constants.c

**game_constants.h**
- **Purpose:** Game-wide constants and external declarations
- **Key Contents:**
  - Compile-time constants: `FULL_DECK_SIZE`, `MAX_NUMBER_OF_TURNS`, `MAX_NUMBER_OF_SIM`, `DEBUG_NUMBER_OF_SIM`, `AVERAGE_POWER_FOR_MULLIGAN`
  - External array declarations: `fullDeck[]`, `SPECIES_TO_ORDER[]`
  - String name array declarations for all enums

**game_constants.c**
- **Purpose:** Full game data definitions and lookup tables
- **Key Functionality:**
  - `fullDeck[120]` - Complete card database (102 champions + 18 special cards)
  - String name arrays for all enums
  - Champion data includes: cost, dice, attack base, color, species, order, expected values, efficiencies, power rating
- **Lines of Code:** ~250 (data definitions)

---

## Game Logic Modules

### 3. combo_bonus.h / combo_bonus.c

**combo_bonus.h**
- **Purpose:** Combo bonus calculation system interface
- **Key Contents:**
  - `CombatCard` struct for simplified combat tracking
  - Main calculation function: `calculate_combo_bonus()`
  - Mode-specific calculators: `calc_random_bonus()`, `calc_prebuilt_bonus()`
  - Helper functions for counting and matching logic

**combo_bonus.c**
- **Purpose:** Implements complex combo bonus calculation system
- **Key Functionality:**
  - Main router: `calculate_combo_bonus()` - Dispatches to mode-specific calculators
  - `calc_random_bonus()` - Full combo system for random decks (species/order/color matching)
  - `calc_prebuilt_bonus()` - Simplified combo system for monochrome/custom decks
  - Helper functions: `count_by_species()`, `count_by_order()`, `count_by_color()`, `get_max_count()`
  - Advanced matching: `third_matches_order_of_species_pair()`, `third_matches_color_of_species_pair()`, `third_matches_color_of_order_pair()`
- **Lines of Code:** ~175

### 4. card_actions.h / card_actions.c

**card_actions.h**
- **Purpose:** Card playing and game action function interfaces
- **Key Contents:**
  - Card playing: `play_card()`, `play_champion()`, `play_draw_card()`, `play_cash_card()`
  - Card management: `has_champion_in_hand()`, `select_champion_for_cash_exchange()`
  - Game actions: `draw_1_card()`, `shuffle_discard_and_form_deck()`, `collect_1_luna()`, `discard_to_7_cards()`, `change_current_player()`

**card_actions.c**
- **Purpose:** Card playing and game action implementations
- **Key Functionality:**
  - Card type routing: `play_card()` dispatches to specific handlers
  - Champion playing: `play_champion()` - Moves to combat zone, removes from hand, pays cost
  - Draw card handling: `play_draw_card()` - Draws N cards, moves to discard
  - Cash card handling: `play_cash_card()` - Exchanges champion for lunas
  - Deck management: `draw_1_card()`, `shuffle_discard_and_form_deck()`
  - Turn actions: `collect_1_luna()`, `discard_to_7_cards()`, `change_current_player()`
  - Helper: `has_champion_in_hand()`, `select_champion_for_cash_exchange()`
- **Lines of Code:** ~185

### 5. combat.h / combat.c

**combat.h**
- **Purpose:** Combat resolution system interface
- **Key Contents:**
  - Main combat: `resolve_combat()`
  - Combat calculations: `calculate_total_attack()`, `calculate_total_defense()`
  - Combat effects: `apply_combat_damage()`, `clear_combat_zones()`

**combat.c**
- **Purpose:** Combat resolution with integrated combo bonuses
- **Key Functionality:**
  - Main resolver: `resolve_combat()` - Orchestrates full combat sequence
  - Attack calculation: `calculate_total_attack()` - Base attack + dice + combo bonus
  - Defense calculation: `calculate_total_defense()` - Dice roll + combo bonus (no base)
  - Damage application: `apply_combat_damage()` - Applies damage, checks win condition
  - Cleanup: `clear_combat_zones()` - Moves cards to discard
  - Integrates with `combo_bonus.c` for all bonus calculations
- **Lines of Code:** ~130

---

## Turn Flow and Game State Modules

### 6. turn_logic.h / turn_logic.c

**turn_logic.h**
- **Purpose:** Turn flow and phase management interface
- **Key Contents:**
  - Main turn: `play_turn()`
  - Turn phases: `begin_of_turn()`, `end_of_turn()`, `attack_phase()`, `defense_phase()`

**turn_logic.c**
- **Purpose:** Turn flow orchestration
- **Key Functionality:**
  - Main loop: `play_turn()` - Manages full turn sequence
  - Turn phases: `begin_of_turn()` (card draw), `attack_phase()` (strategy call), `defense_phase()` (strategy call), `end_of_turn()` (collect luna, discard, change player)
  - Strategy integration via function pointers
  - TODO comments about MCTS timing considerations
- **Lines of Code:** ~70

### 7. game_state.h / game_state.c

**game_state.h**
- **Purpose:** Game initialization, simulation, and statistics interface
- **Key Contents:**
  - Game setup: `setup_game()`, `apply_mulligan()`
  - Game execution: `play_game()`
  - Simulation: `run_simulation()`
  - Statistics: `record_final_stats()`, `present_results()`

**game_state.c**
- **Purpose:** Game lifecycle and statistics management
- **Key Functionality:**
  - Game initialization: `setup_game()` - Deck distribution, hand drawing, zone setup
  - Mulligan system: `apply_mulligan()` - Player B discards up to 2 weak cards
  - Game execution: `play_game()` - Main game loop until win/draw/max turns
  - Simulation framework: `run_simulation()` - Runs N games
  - Statistics: `record_final_stats()`, `present_results()`
  - Histogram generation: `createHistogram()` with underflow/overflow bins
- **Lines of Code:** ~215

---

## Strategy Framework and Implementations

### 8. strategy.h / strategy.c

**strategy.h**
- **Purpose:** Strategy function pointer framework
- **Key Contents:**
  - Function pointer types: `AttackStrategyFunc`, `DefenseStrategyFunc`
  - `StrategySet` struct for player strategies
  - Strategy management: `create_strategy_set()`, `set_player_strategy()`, `free_strategy_set()`

**strategy.c**
- **Purpose:** Strategy function pointer framework implementation
- **Key Functionality:**
  - `create_strategy_set()` - Allocates strategy container
  - `set_player_strategy()` - Assigns attack/defense functions per player
  - `free_strategy_set()` - Memory cleanup
- **Lines of Code:** ~20

### 9. strat_random.h / strat_random.c

**strat_random.h**
- **Purpose:** Random strategy implementation interface
- **Key Contents:**
  - `random_attack_strategy()`
  - `random_defense_strategy()`

**strat_random.c**
- **Purpose:** Random decision-making strategy
- **Key Functionality:**
  - `random_attack_strategy()` - Plays random affordable card (champions or draw/cash)
  - `random_defense_strategy()` - 47% chance to play random affordable champion
  - Affordability checking based on current cash balance
  - Special handling: skips cash cards if no champions available
- **Lines of Code:** ~70

### 10. Strategy Templates (Stub Files)

**strat_balancedrules1.c**
- **Purpose:** Planned balanced rules-based strategy
- **Status:** Template/design document only (no corresponding .h file)
- **Key Concepts:**
  - Effective hand size and cash calculations
  - Target metrics based on opponent energy
  - Attack/defense budget allocation
  - Situational awareness (early/mid/late game)

**strat_heuristic1.c**
- **Purpose:** Planned heuristic evaluation strategy
- **Status:** Template/design document only (no corresponding .h file)
- **Key Concepts:**
  - Advantage calculation: Energy + Cards + Cash
  - Epsilon and gamma calibration parameters
  - 1-move look-ahead optimization
  - Hand power and combo potential estimation

**strat_ismcts1.c**
- **Purpose:** Planned Information Set Monte Carlo Tree Search
- **Status:** Template/design document only (no corresponding .h file)
- **Key Concepts:**
  - Tree-based search with information hiding
  - Clone and randomize hidden information
  - UCT (Upper Confidence Bound for Trees) node selection
  - Detailed notes on tree structure and move representation

**strat_simplemc1.c**
- **Purpose:** Planned Monte Carlo single-stage analysis
- **Status:** Template/design document only (no corresponding .h file)
- **Key Concepts:**
  - Progressive pruning (93 → 30 → 10 → 4 moves)
  - Staged simulation (100 → 200 → 400 → 800 sims)
  - Rule extraction from best moves
  - Can serve as interactive AI assistant

---

## Utility Modules

### 11. deckstack.h / deckstack.c

**deckstack.h**
- **Purpose:** Fixed-size stack implementation for card decks
- **Key Contents:**
  - `deck_stack` struct (40-card capacity)
  - Stack operations: `push()`, `pop()`, `isEmpty()`, `emptyOut()`, `print()`

**deckstack.c**
- **Purpose:** Fixed-size stack for deck management
- **Key Functionality:**
  - Stack operations: `push()`, `pop()`, `isEmpty()`, `isFull()`
  - Deck operations: `emptyOut()`, `peek()`, `print()`
  - Fixed capacity of 40 cards (MAX_DECK_STACK_SIZE)
- **Lines of Code:** ~65

### 12. hdcll.h / hdcll.c

**hdcll.h**
- **Purpose:** Head-tracked doubly-circular linked list for dynamic collections
- **Key Contents:**
  - `LLNode` and `HDCLList` structs
  - List operations: `initialize()`, `insertNodeAtBeginning()`, `removeNodeFromBeginning()`, `removeNodeByValue()`, `removeNodeByIndex()`, `getNodeValueByIndex()`, `toArray()`, `emptyOut()`, `printLinkedList()`

**hdcll.c**
- **Purpose:** Head-tracked doubly-circular linked list implementation
- **Key Functionality:**
  - List management: `initialize()`, `emptyOut()`
  - Node insertion: `insertNodeAtBeginning()`
  - Node removal: `removeNodeFromBeginning()`, `removeNodeByValue()`, `removeNodeByIndex()`
  - Access: `getNodeValueByIndex()`
  - Conversion: `toArray()` - Creates heap-allocated array (caller must free)
  - Debug: `printLinkedList()`
- **Lines of Code:** ~170

### 13. rnd.h / rnd.c

**rnd.h**
- **Purpose:** Random number generation interface
- **Key Contents:**
  - `randn()`, `dn()` - Random number functions
  - `swap()`, `partial_shuffle()` - Array manipulation

**rnd.c**
- **Purpose:** Random number generation wrapper
- **Key Functionality:**
  - Dice rolling: `dn()` - Returns 1 to n
  - Random selection: `randn()` - Returns 0 to n-1
  - Array manipulation: `swap()`, `partial_shuffle()` (Fisher-Yates variant)
  - Global `MTwister_rand_struct` instance
- **Lines of Code:** ~40

### 14. mtwister.h / mtwister.c

**mtwister.h**
- **Purpose:** Mersenne Twister PRNG interface
- **Key Contents:**
  - `MTRand` struct
  - `seedRand()`, `genRandLong()`, `genRand()`

**mtwister.c**
- **Purpose:** Mersenne Twister PRNG implementation
- **Key Functionality:**
  - High-quality PRNG with 623-dimensional equidistribution
  - Seeding: `seedRand()`, `m_seedRand()`
  - Generation: `genRandLong()` - 32-bit unsigned
  - Generation: `genRand()` - Double in [0..1]
  - Based on MT19937 algorithm
- **Lines of Code:** ~65

---

## Build System

**makefile**
- **Purpose:** GNU Make build configuration
- **Key Features:**
  - Automatic source discovery in `src/` directory
  - Debug build target with -O0
  - Test target for `test_combo_bonus`
  - Old code regression testing support
  - Code formatting with astyle
  - Help documentation
- **Compiler:** gcc with C23 standard
- **Flags:** `-g -Og -Wall` (default), `-g -O0 -Wall -DDEBUG` (debug)

**.astylerc**
- **Purpose:** Code formatting configuration for Artistic Style
- **Style:** Run-in braces, 2-space indentation
- **Pointer Alignment:** Type-aligned pointers (`int* ptr`)

---

## Module Dependencies

### Dependency Graph

```
main.c
├─ game_state.h
│  ├─ game_types.h
│  │  ├─ deckstack.h
│  │  └─ hdcll.h
│  └─ strategy.h
├─ strategy.h
│  └─ game_types.h
├─ strat_random.h
│  └─ game_types.h
├─ game_constants.h
│  └─ game_types.h
└─ mtwister.h

game_state.c
├─ turn_logic.h
│  ├─ game_types.h
│  └─ strategy.h
├─ card_actions.h
│  └─ game_types.h
├─ game_constants.h
├─ rnd.h
└─ deckstack.h

turn_logic.c
├─ card_actions.h
├─ combat.h
│  └─ game_types.h
└─ strategy.h

combat.c
├─ combo_bonus.h
│  └─ game_types.h
├─ game_constants.h
└─ rnd.h

card_actions.c
├─ game_constants.h
└─ rnd.h

combo_bonus.c
└─ game_constants.h

rnd.c
└─ mtwister.h
```

### Module Responsibilities

- **Data Layer:** `game_types.h`, `game_constants.h/c` - Core data structures and game data
- **Data Structures:** `deckstack.h/c`, `hdcll.h/c` - Specialized collections
- **Game Logic:** `card_actions.h/c`, `combat.h/c`, `combo_bonus.h/c` - Core game mechanics
- **Flow Control:** `turn_logic.h/c`, `game_state.h/c` - Game loop and lifecycle
- **AI Framework:** `strategy.h/c` - Strategy abstraction
- **AI Implementations:** `strat_*.c` - Specific AI strategies
- **Utilities:** `rnd.h/c`, `mtwister.h/c` - Random number generation
- **Entry Point:** `main.c` - Program initialization and mode selection

---

## Code Quality Metrics

- **Function Length:** Target ≤30 lines of actual code (excluding comments/whitespace)
- **Modularity:** Clean separation between game logic, AI, and utilities
- **Memory Management:** Explicit allocation/deallocation with helper functions
- **Debug Support:** Global `debug_enabled` flag with detailed logging
- **Platform Support:** MSYS2 (Windows) and Arch Linux
- **Compiler:** GCC with C23 standard

---

## Future Development Notes

1. **Command-line Arguments:** Parsing framework in `main.c` (placeholder)
2. **MCTS Strategies:** Detailed design notes in stub files
3. **Client-Server Split:** Comments indicate future network play architecture
4. **Interactive Mode:** Human player interface and AI assistant mode
5. **Turn Phase Timing:** TODO comments about MCTS integration requirements

---

## File Count Summary

- **Header Files:** 13 (10 custom + 3 utility)
- **Implementation Files:** 18 (9 core + 4 stubs + 5 utility)
- **Build Files:** 2 (makefile, .astylerc)
- **Documentation:** This file

**Total Source Files:** 33
