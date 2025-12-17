# Oracle: Technical Design Document

**Les Champions d'Arcadie / The Arcadian Champions of Light**

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Current Implementation Status](#current-implementation-status)
3. [Core Data Structures](#core-data-structures)
4. [GameContext Pattern](#gamecontext-pattern)
5. [Module Organization](#module-organization)
6. [Game Logic Flow](#game-logic-flow)
7. [Strategy Framework](#strategy-framework)
8. [Combat System](#combat-system)
9. [Command-Line Interface](#command-line-interface)
10. [Future Architecture](#future-architecture)

---

## 1. Architecture Overview

### Design Principles

- **Maximum 35 lines per function** (excluding comments/whitespace, firm limit at 100 lines)
- **Maximum 500 lines per source file** (ideally ≤400, firm limit at 1000 lines)
- **Separation of concerns**: Game logic, UI, AI, and modes are independent
- **Testability**: GameContext pattern enables dependency injection
- **Extensibility**: Strategy framework enables pluggable AI
- **Manual implementation preferred**: Code duplication acceptable for readability over macro magic

### Current Architecture

```
┌─────────────────────────────────────────────┐
│           Main Entry Point                   │
│              (main.c)                        │
└──────────────────┬──────────────────────────┘
                   │
       ┌───────────┴──────────┐
       │  Command-Line Parser  │
       │     (cmdline.c)       │
       └───────────┬───────────┘
                   │
    ┌──────────────┴─────────────────┐
    │   Mode Selection               │
    │   - stda.auto (automated sim)  │
    │   - stda.cli (interactive CLI) │
    │   - stda.tui (ncurses - future)│
    └──────────────┬─────────────────┘
                   │
    ┌──────────────┴─────────────────┐
    │     Game Loop                  │
    │   (turn_logic.c)               │
    │   - Begin turn                 │
    │   - Attack phase               │
    │   - Defense phase              │
    │   - Combat resolution          │
    │   - End turn                   │
    └──────────────┬─────────────────┘
                   │
    ┌──────────────┴─────────────────┐
    │  Core Game Logic               │
    │  - Game state (game_state.c)   │
    │  - Combat (combat.c)           │
    │  - Card actions (card_actions.c)│
    │  - Combo bonus (combo_bonus.c) │
    └────────────────────────────────┘
```

---

## 2. Current Implementation Status

### What's Working ✅

**Core Game Engine**:

- 120-card deck with full attributes
- Game state management (energy, cash, hands, decks)
- Combat resolution with dice rolling
- Combo bonus calculations (all 3 deck types)
- Turn-based flow
- Card playing (champions, draw, cash)

**AI Framework**:

- Strategy function pointer system
- Random strategy (functional baseline)

**User Interface**:

- Command-line argument parsing
- CLI mode (human vs AI, human vs human, AI vs AI)
- ANSI color output with UTF-8 symbols
- Player configuration and assignment
- Localization support (English, French, Spanish)

**Infrastructure**:

- GameContext pattern for RNG and config
- Debug macro system (compile-time)
- Mersenne Twister RNG
- Data structures (deck stack, circular linked list)
- PRNG seed management (random or specified)

### What's Missing ⚠️

**Game Logic Gaps**:

- Recall mechanic (draw/recall cards)

**AI Strategies**:

- Balanced rules AI (in design phase)
- Heuristic AI (in design phase)
- All Monte Carlo variants
- MCTS implementations

**Features**:

- Save/load game state
- Configuration file system
- CSV export for simulations
- Rating system
- Network multiplayer
- TUI/GUI modes

---

## 3. Core Data Structures

### Card Structure

```c
// In game_types.h
struct card {
    CardType card_type;
    uint8_t cost;

    // Champion fields
    uint8_t champion_id;
    uint8_t defense_dice;        // d4, d6, d8, d12, d20
    uint8_t attack_base;         // 0-5
    ChampionColor color;         // RED, INDIGO, ORANGE
    ChampionSpecies species;     // 15 species total
    ChampionOrder order;         // ORDER_A through ORDER_E

    // Draw card fields
    uint8_t draw_num;            // 2 or 3
    uint8_t choose_num;          // For recall (future)

    // Calculated efficiency values
    float expected_attack;
    float expected_defense;
    float attack_efficiency;
    float defense_efficiency;
    float power;                 // Overall card value

    // Cash card fields
    uint8_t exchange_cash;       // 5 lunas
};
```

**Full Deck**: 120 cards total

- 102 champions (34 per color, 5 orders, 15 species)
- 9 draw-2 cards (cost 1)
- 6 draw-3 cards (cost 2)
- 3 cash exchange cards (cost 0, give 5 lunas)

**Deck in Memory**: `fullDeck[120]` in `game_constants.c`

### Game State Structure

```c
// In game_types.h
struct gamestate {
    PlayerID current_player;              // PLAYER_A or PLAYER_B
    uint16_t current_cash_balance[2];     // Luna for each player
    uint8_t current_energy[2];            // Health for each player
    bool someone_has_zero_energy;         // Game end flag

    struct deck_stack deck[2];            // Draw piles (40 cards each)
    Hand hand[2];
    Discard discard[2];
    CombatZone combat_zone[2];

    uint16_t turn;                        // Turn counter (1-based)
    GameStateEnum game_state;             // PLAYER_A_WINS, PLAYER_B_WINS, DRAW, ACTIVE
    TurnPhase turn_phase;                 // ATTACK or DEFENSE
    PlayerID player_to_move;              // Who makes next decision
};
```

**Key Design Decisions**:

- Separate deck/hand/discard/combat for each player
- Stack for deck (LIFO), circular linked list for others
- Turn counter includes both players (turn 1 = A attacks, turn 2 = B attacks)
- Energy starts at 99, first to 0 loses

### Data Structure Implementations

**Deck Stack** (`deckstack.c`):

- Fixed-size array (40 cards max)
- LIFO operations (push/pop)
- Used for draw pile only



**Fixed-size arrays**

- Hand: max 15 cards
- CombatZone: max 3 cards
- Discard: max 40 cards
  
  

---

## 4. GameContext Pattern

### Purpose

Eliminate global state and enable testability by passing all "context" (RNG, config, future network state) through a single pointer.

### Structure

```c
// In game_context.h
typedef struct {
    MTRand rng;              // Mersenne Twister state
    config_t* config;        // Runtime configuration
    // Future: network_context, ui_context, etc.
} GameContext;
```

### Usage Pattern

```c
// Initialize once at program start
GameContext* ctx = create_game_context(cfg);

// Pass to all game functions
setup_game(initial_cash, &gstate, ctx);
play_turn(&gstats, &gstate, strategies, ctx);
draw_1_card(&gstate, player, ctx);

// Cleanup at program end
destroy_game_context(ctx);
```

### Benefits

1. **Testability**: Can inject mock RNG for deterministic tests
2. **Thread Safety**: Each thread can have its own context
3. **Extensibility**: Add new context fields without changing function signatures
4. **No Globals**: Eliminates hidden dependencies

### Migration Status

- ✅ Core game functions use GameContext
- ✅ RNG functions take GameContext parameter
- ✅ PRNG seed managed through config
- 📋 Future: Add network context, UI callbacks

---

## 5. Module Organization

### File Structure

```
oracle/
├── src/
│   ├── Core Game Logic
│   │   ├── game_types.h          # All enums and struct definitions
│   │   ├── game_constants.c/h    # Full deck, enums, strings
│   │   ├── game_state.c/h        # Game initialization
│   │   ├── card_actions.c/h      # Card playing, drawing
│   │   ├── combat.c/h            # Combat resolution
│   │   ├── combo_bonus.c/h       # Combo calculations
│   │   ├── turn_logic.c/h        # Turn flow management
│   │
│   ├── Strategy Framework
│   │   ├── strategy.c/h          # Function pointer framework
│   │   ├── strat_random.c/h      # Random AI
│   │   ├── strat_balancedrules1.c # Balanced AI (design)
│   │   ├── strat_heuristic1.c    # Heuristic AI (design)
│   │   ├── strat_simplemc1.c     # Simple MC (design)
│   │   └── strat_ismcts1.c       # IS-MCTS (design)
│   │
│   ├── Game Modes
│   │   ├── stda_auto.c/h         # Automated simulation
│   │   └── stda_cli.c/h          # CLI interactive mode
│   │
│   ├── User Interface
│   │   ├── player_selection.c/h  # Player type selection
│   │   ├── player_config.c/h     # Names, strategies, assignment
│   │   └── localization.h        # I18n support
│   │
│   ├── Utilities
│   │   ├── rnd.c/h               # RNG wrapper functions
│   │   ├── mtwister.c/h          # Mersenne Twister PRNG
│   │   ├── prng_seed.c/h         # Seed management
│   │   ├── game_context.c/h      # GameContext pattern
│   │   └── debug.h               # Debug macros
│   │
│   ├── Data Structures
│   │   ├── deckstack.c/h         # Fixed-size stack
│   │
│   ├── Build System
│   │   ├── cmdline.c/h           # Command-line parsing
│   │   ├── version.h             # Version info
│   │   └── main.c/h              # Entry point
│   │
├── doc/                          # Documentation
├── ideas/                        # Design explorations
├── Makefile
└── README.md
```

### Module Dependencies

```
main.c
├─ cmdline.c → game_types.h, prng_seed.h
├─ game_state.h → game_types.h, strategy.h
├─ stda_auto.h → game_types.h, strategy.h
├─ stda_cli.h → game_types.h, localization.h, player_config.h
└─ game_context.h → mtwister.h, game_types.h

game_state.c
├─ game_constants.h → game_types.h
├─ turn_logic.h → game_types.h, strategy.h
├─ card_actions.h → game_types.h
└─ rnd.h → game_context.h

turn_logic.c
├─ card_actions.h
├─ combat.h → game_types.h
└─ strategy.h

combat.c
├─ combo_bonus.h → game_types.h
├─ game_constants.h
└─ rnd.h

card_actions.c
├─ game_constants.h
└─ rnd.h

combo_bonus.c
└─ game_constants.h

strategy.c
└─ game_types.h

strat_random.c
├─ card_actions.h
├─ game_constants.h
└─ rnd.h

stda_auto.c
├─ game_types.h
├─ strategy.h
├─ game_state.h
├─ turn_logic.h
└─ card_actions.h

stda_cli.c
├─ game_types.h
├─ strategy.h
├─ game_state.h
├─ turn_logic.h
├─ card_actions.h
├─ localization.h
├─ player_selection.h
└─ player_config.h

player_config.c
├─ localization.h
└─ rnd.h

rnd.c
└─ mtwister.h
```

### Module Responsibilities

- **Data Layer**: `game_types.h`, `game_constants.h/c` - Core data structures and game data
- **Data Structures**: `deckstack.h/c` - Specialized collections
- **Game Logic**: `card_actions.h/c`, `combat.h/c`, `combo_bonus.h/c` - Core game mechanics
- **Flow Control**: `turn_logic.h/c`, `game_state.h/c` - Game loop and lifecycle
- **AI Framework**: `strategy.h/c` - Strategy abstraction
- **AI Implementations**: `strat_*.c` - Specific AI strategies
- **User Interface**: `player_selection.h/c`, `player_config.h/c`, `localization.h` - Player setup
- **Game Modes**: `stda_auto.c/h`, `stda_cli.c/h` - Mode implementations
- **Utilities**: `rnd.h/c`, `mtwister.h/c`, `prng_seed.h/c`, `game_context.h/c` - Infrastructure
- **Entry Point**: `main.c` - Program initialization and mode selection

### Code Quality Metrics

- **Function Length**: Target ≤35 lines of actual code (firm limit 100 lines)
- **File Length**: Target ≤500 lines (ideally ≤400, firm limit 1000 lines)
- **Modularity**: Clean separation between game logic, AI, and utilities
- **Memory Management**: Explicit allocation/deallocation with helper functions
- **Debug Support**: Global `debug_enabled` flag with detailed logging
- **Platform Support**: MSYS2 (Windows) and Arch Linux
- **Compiler**: GCC with C23 standard

---

## 6. Game Logic Flow

### Turn Structure

```
Turn N (Active Player = Current Player)
├── 1. begin_of_turn()
│   ├── Increment turn counter
│   ├── Set phase = ATTACK
│   ├── Draw 1 card (except first player, turn 1)
│   └── Set player_to_move = current_player
│
├── 2. attack_phase()
│   ├── Call attack_strategy(current_player)
│   │   └── Play 0-3 champions, OR 1 draw/cash card, OR pass
│   └── Set phase = DEFENSE, player_to_move = opponent
│
├── 3. defense_phase() [if combat]
│   ├── Call defense_strategy(opponent)
│   │   └── Play 0-3 champions OR decline
│   └── resolve_combat()
│       ├── Calculate total attack (base + dice + combo)
│       ├── Calculate total defense (dice + combo)
│       ├── Apply damage (max(attack - defense, 0))
│       └── Clear combat zones
│
├── 4. end_of_turn()
│   ├── Collect 1 luna (current_player)
│   ├── Discard to 7 cards (if hand > 7)
│   └── Switch current_player
│
└── Check win condition
    └── If energy[defender] == 0, set game_state, end game
```

### Phase Transitions

```
ATTACK Phase:
- player_to_move = current_player (attacker)
- Can play champions, draw/recall cards, cash cards, or pass
- After action, if champions played → DEFENSE phase
- After action, if no champions → skip to end_of_turn()

DEFENSE Phase:
- player_to_move = opponent (defender)
- Can play 0-3 champions or decline
- After action → resolve_combat()
- After combat → end_of_turn()
```

### Card Drawing and Deck Management

```c
void draw_1_card(struct gamestate* gstate, PlayerID player, GameContext* ctx) {
    // If deck empty, shuffle discard to form new deck
   
    // Draw top card from deck
   
}
```

**Edge Cases**:

- First player on turn 1 doesn't draw
- Empty discard reshuffles to form new deck

---

## 7. Strategy Framework

### Function Pointer System

```c
// In strategy.h
typedef void (*AttackStrategyFunc)(struct gamestate* gstate, GameContext* ctx);
typedef void (*DefenseStrategyFunc)(struct gamestate* gstate, GameContext* ctx);

typedef struct {
    AttackStrategyFunc attack_strategy[2];   // One per player
    DefenseStrategyFunc defense_strategy[2]; // One per player
} StrategySet;
```

### Usage

```c
// Setup
StrategySet* strategies = create_strategy_set();
set_player_strategy(strategies, PLAYER_A,
                   random_attack_strategy, random_defense_strategy);
set_player_strategy(strategies, PLAYER_B,
                   balanced_attack_strategy, balanced_defense_strategy);

// During game
strategies->attack_strategy[attacker](gstate, ctx);
strategies->defense_strategy[defender](gstate, ctx);

// Cleanup
free_strategy_set(strategies);
```

### Adding New Strategy

1. Create `src/strat_name.c/h`

2. Implement attack and defense functions:
   
   ```c
   void stratname_attack_strategy(struct gamestate* gstate, GameContext* ctx);void stratname_defense_strategy(struct gamestate* gstate, GameContext* ctx);
   ```

3. Functions modify `gstate` directly (play cards, update combat zone)

4. Register strategy in mode initialization

5. Update `player_config.c` strategy menu and mapping

### Random Strategy Example

```c
// In strat_random.c
void random_attack_strategy(struct gamestate* gstate, GameContext* ctx) {
    PlayerID attacker = gstate->current_player;

    // Build list of affordable cards
    uint8_t affordable[hand_size];
    uint8_t count = 0;

    for each card in hand {
        if (card.cost <= cash_balance) {
            affordable[count++] = card_idx;
        }
    }

    // Play random affordable card
    if (count > 0) {
        uint8_t chosen = RND_randn(count, ctx);
        play_card(gstate, attacker, affordable[chosen], ctx);
    }
}
```

---

## 8. Combat System

### Combo Bonus Calculation

**Input**: Array of 2-3 `CombatCard` structs (species, color, order)  
**Output**: Bonus integer (0-16 depending on deck type and matches)

```c
// In combo_bonus.h
typedef struct {
    ChampionSpecies species;
    ChampionColor color;
    ChampionOrder order;
} CombatCard;

int calculate_combo_bonus(CombatCard *cards, int num_cards, DeckType deck_type);
```

**Algorithm Priority** (Random Deck):

1. Species matches (2+ same species)
2. Order matches (2+ same order, no species match)
3. Color matches (2+ same color, no species/order match)

**Example**:

- 3 Humans (same species): +16
- 2 Humans + 1 Elf (same order A): +14
- 2 Humans + 1 Hobbit (same color): +13
- 2 Orange cards (no species/order match): +5

### Combat Resolution

```c
// In combat.c
void resolve_combat(struct gamestate* gstate, GameContext* ctx) {
    int16_t total_attack = calculate_total_attack(gstate, attacker, ctx);
    int16_t total_defense = calculate_total_defense(gstate, defender, ctx);

    int16_t damage = max(total_attack - total_defense, 0);
    gstate->current_energy[defender] -= damage;

    if (gstate->current_energy[defender] == 0) {
        gstate->someone_has_zero_energy = true;
        gstate->game_state = (attacker == PLAYER_A) ? 
                             PLAYER_A_WINS : PLAYER_B_WINS;
    }

    clear_combat_zones(gstate, ctx);
}
```

**Attack Calculation**:

```
Total Attack = Σ(attack_base + RND_dn(defense_dice)) + combo_bonus
```

**Defense Calculation**:

```
Total Defense = Σ(RND_dn(defense_dice)) + combo_bonus
```

**Note**: Defense doesn't add `attack_base` (champions only defend with dice).

---

## 9. Command-Line Interface

### Implementation

**Entry Point**: `main.c` → `run_mode_stda_cli()`  
**Implementation**: `stda_cli.c` (~800 lines after refactoring)

### Features

**Display**:

- ANSI color codes (player names, card types, energy, lunas)
- Card display with species, dice, cost
- Game status (energy, lunas, hand size, deck size)
- Localization support (English, French, Spanish)

**Commands**:

- `cham 1 2 3` - Play champions at hand indices 1, 2, 3
- `draw 2` - Play draw/recall card at index 2
- `cash 1` - Play cash exchange card at index 1
- `pass` - Pass turn (attack phase) or decline defense
- `gmst` - Show game status
- `help` - Show commands
- `exit` - Quit game

**Player Configuration**:

- Player type selection (Human vs AI, Human vs Human, AI vs AI)
- Player names
- AI strategy selection (currently only Random available)
- Player assignment (direct, inverted, random)

**Input Handling**:

- Parse command string
- Validate card indices (1-based for user, convert to 0-based)
- Check affordability (cost ≤ cash)
- Check card type (champion for combat, draw for drawing, etc.)
- Display errors for invalid commands

### Architecture

```c
// Main game loop
while (!game_over) {
    if (cfg->player_types[current_player] == INTERACTIVE_PLAYER) {
        display_prompt_and_hand();
        get_command_from_user();
        parse_and_validate_command();
        if (valid) execute_action();
    } else {
        // AI plays
        strategies->attack_strategy[AI](gstate, ctx);
    }

    // ... defense phase, combat, etc.
}
```

### Platform-Specific Code

```c
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);  // Enable UTF-8 output
    SetConsoleCP(CP_UTF8);         // Enable UTF-8 input
#endif
```

### Known Issues

**⚠️ File Size**: `stda_cli.c` at ~800 lines exceeds recommended 500-line limit

- **Planned Split**:
  - `cli_display.c` (~350 lines) - Display functions
  - `cli_input.c` (~350 lines) - Input parsing/validation
  - `cli_game.c` (~100 lines) - Game loop and initialization

---

## 10. Future Architecture

### Network Architecture (Phase 9)

**Goal**: Separate client from server, enable multiplayer

```
                 Network
Client A  ←────────────────→  Server  ←────────────────→  Client B
(Visible State)           (Full State)             (Visible State)
```

**Key Concepts**:

- **Server**: Maintains full `GameState`, validates all actions
- **Client**: Maintains `VisibleGameState`, sends actions, receives updates
- **Protocol**: Action messages (text and binary), state sync messages

**File Organization**:

```
src/
├── shared/          # Types used by both client and server
├── server/          # Server-only code
├── client/          # Client-only code
└── protocol/        # Message serialization
```

### GUI Architecture (Phase 10)

**Technology**: SDL3 (cross-platform)

```
SDL3 Event Loop
└─→ Input Manager → Game Logic → Renderer
                        ↓
                   GameState
                        ↓
                   Card Renderer, UI Elements
```

**Modular Design**:

```
src/gui/
├── gui_main.c           # SDL initialization, event loop
├── card_renderer.c      # Card compositing (artwork + stats)
├── font_manager.c       # Font loading and caching
├── texture_cache.c      # Image caching
├── layout.c             # Responsive layout system
└── input.c              # Input abstraction (mouse/touch)
```

### Rating System Architecture (Phase 6)

**Bradley-Terry Model**:

```
Each player has:
- Rating (1-99, displayed)
- Strength (internal, 0.01-99.0)
- Games played
- Adaptive A value (1.08-1.30, decreases with experience)

Keeper AI = benchmark at rating 50 (strength 1.0)
```

**Integration**:

```c
// After each match
MatchResult result = {player1_id, player2_id, wins1, wins2, draws};
rating_update_match(&rating_system, &result);

// Query
double win_prob = rating_win_probability(&rating_system, p1, p2);
```

**Files**:

```
src/rating/
├── rating.c/h           # Core Bradley-Terry logic
├── calibration.c/h      # Parameter optimization
└── persistence.c/h      # Save/load ratings (CSV)
```

---

## Implementation Guidelines

### Adding a New Module

1. **Design Phase**:
   
   - Write design notes in `ideas/`
   - Define public API in header file
   - Consider function size limits (35 lines)

2. **Implementation Phase**:
   
   - Create `.c` and `.h` files
   - Implement one function at a time
   - Keep functions short and focused

3. **Integration Phase**:
   
   - Update `Makefile`
   - Add to appropriate mode(s)
   - Update `oracle_design.md`

4. **Testing Phase**:
   
   - Write unit tests
   - Test integration with existing code
   - Run with valgrind (check leaks)

### Code Quality Checklist

Before committing:

- [ ] All functions ≤35 lines (firm limit 100)
- [ ] All files ≤500 lines (firm limit 1000)
- [ ] No compiler warnings (-Wall -Wextra)
- [ ] Comments on public functions
- [ ] No memory leaks (valgrind clean)
- [ ] Consistent naming (snake_case)
- [ ] Updated `oracle_todo.md` checkboxes
- [ ] Updated `oracle_design.md` if architecture changed

### Common Patterns

**Error Handling**:

```c
// Return bool for success/failure
bool do_thing(Args* args) {
    if (!validate(args)) return false;
    // Do the thing
    return true;
}

// Use in calling code
if (!do_thing(&args)) {
    fprintf(stderr, "Error: couldn't do thing\n");
    return EXIT_FAILURE;
}
```

**Memory Management**:

```c
// Functions that allocate must have matching free function
Thing* create_thing(void);
void destroy_thing(Thing* t);

// Or use explicit malloc/free at call site
Thing* t = malloc(sizeof(Thing));
// ... use t ...
free(t);
```

**RNG Usage**:

```c
// Always use GameContext, never global RNG
uint8_t roll = RND_dn(6, ctx);  // Roll d6
uint8_t choice = RND_randn(n, ctx);  // Choose 0 to n-1
```

---

## Key Design Decisions

### Why GameContext?

**Problem**: Global `MTwister_rand_struct` made testing difficult  
**Solution**: Pass context pointer to all functions  
**Benefit**: Can inject test RNG, thread-safe, extensible

### Why Function Pointers for AI?

**Problem**: Need pluggable AI strategies without switch statements  
**Solution**: Store function pointers in `StrategySet`  
**Benefit**: Easy to add new AIs, clean separation

### Why Separate Deck Types?

**Problem**: Different combo rules for random vs monochrome vs custom  
**Solution**: Pass `DeckType` to combo calculator  
**Benefit**: Single implementation handles all cases

### Why Stack for Deck, List for Hand?

**Problem**: Different access patterns  
**Solution**: Stack (LIFO) for deck, circular linked list for hand  
**Benefit**: Optimal operations for each use case  
**Future**: Migrate to fixed arrays for better performance

### Why PRNG Seed in Config?

**Problem**: Seed is a configuration setting controlling program behavior  
**Solution**: Store in `config_t`, pass to `create_game_context()`  
**Benefit**: One source of truth, cleaner API, easier testing

---

## Performance Considerations

### Hot Paths

1. **Combat Resolution**: Called every turn with combat
2. **Combo Bonus Calculation**: Called twice per combat
3. **Card Drawing**: Called every turn (except first)
4. **Strategy Decision**: Called twice per turn

### Optimization Strategies

**Don't Optimize Yet**: Current performance is acceptable for:

- 10,000 game simulations in <5 minutes
- Interactive CLI with instant response

**Future Optimizations** (if needed):

- Cache combo bonus for card combinations
- Use memory pools for linked list nodes (or migrate to fixed arrays)
- Profile with gprof to find actual bottlenecks

---

## Testing Strategy

### Unit Tests

Test individual functions in isolation:

- `test_combo_bonus.c`: All combo scenarios
- `test_combat.c`: Dice distribution, damage calculation
- Future: `test_protocol.c`, `test_rating.c`

### Integration Tests

Test modules working together:

- Full game Random vs Random (should work without crashes)
- Statistics (10,000 games should give ~50% win rate)
- CLI mode (manual testing for now)

### System Tests

Test complete workflows:

- Automated simulation produces reasonable results
- Interactive CLI allows full gameplay
- Save/load preserves game state correctly (future)

---

## References

- **Game Rules**: See `doc/game_rules_doc.md`

- **Roadmap**: See `doc/oracle_roadmap.md`

- **TODO**: See `doc/oracle_todo.md`

- **GitHub**: https://github.com/JonathanFerron/oracle/tree/main

- **MCTS Resources**: (to be added as studied)

- **Bradley-Terry Model**: See `ideas/rating system/` for full spec and papers

---

## Architectural Tensions and Design Trade-offs

### Known Technical Debt

**1. File Size Violations**

Current violations of the 500-line guideline:

- `stda_cli.c`: ~800 lines (needs split into display/input/game modules)
- **Impact**: Medium - Makes code harder to navigate and maintain
- **Resolution**: Planned refactoring into `cli_display.c`, `cli_input.c`, `cli_game.c`

**2. Mixed Responsibilities**

Some functions have responsibilities that could be better separated:

- `select_champion_for_cash_exchange()` in `card_actions.c` uses strategy logic
  - **Comment in code**: "this code could be moved to the strategy"
  - **Impact**: Low - Works fine but violates separation of concerns
  - **Resolution**: Move to strategy modules when implementing smarter AIs



### Architectural Boundaries

**What Goes Where**:

| Concern             | Current Location | Future Location   | Rationale                                               |
| ------------------- | ---------------- | ----------------- | ------------------------------------------------------- |
| Player types        | `config_t`       | `PlayerConfig`    | Player-specific data belongs together                   |
| PRNG seed           | `config_t`       | ✅ Stays in config | It's a configuration setting                           |
| Cash card selection | `card_actions.c` | Strategy modules  | Strategy decision, not game rule                        |

### Design Patterns in Use

**1. Strategy Pattern**

- Used for AI strategies
- Function pointers enable runtime polymorphism
- Clean separation between game engine and AI logic

**2. Dependency Injection**

- `GameContext` passed to all functions
- Enables testing with mock RNG
- Future: Can inject UI callbacks, network connections

**3. Builder Pattern**

- `PlayerConfig` built incrementally through UI
- `StrategySet` configured per player
- Clean separation of construction from use

**4. Template Method Pattern** (implicit)

- `play_turn()` defines the algorithm structure
- Strategy functions fill in the variable parts
- Ensures consistent turn flow regardless of AI

---

## Development Environment

### Platform Support

**Primary Platforms**:

- MSYS2 (Windows) - GCC with C23
- Arch Linux - GCC with C23

**Editor**: Geany on both platforms

**Build System**: GNU Make with automatic source discovery

**Code Formatting**: Artistic Style (astyle)

- Run-in braces
- 2-space indentation
- Type-aligned pointers (`int* ptr`)

### Compiler Flags

**Default Build**:

```makefile
CFLAGS := -g -Og -Wall -std=c23
```

**Debug Build**:

```makefile
CFLAGS := -g -O0 -Wall -std=c23 -DDEBUG -DDEBUG_ENABLED=1
```

**Future**:

- Consider `-Wextra` for more warnings
- Consider `-Werror` to enforce warning-free builds
- Profile-guided optimization for MCTS

### Makefile Features

```makefile
# Automatic source discovery
SOURCES := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))

# Targets
all          # Build the project
clean        # Remove build artifacts
debug        # Build with debug symbols and -O0
test_combo   # Build combo bonus tests
oldcode      # Build old code for regression testing
format       # Format code with astyle
help         # Show help message
```

---

## Module Details

### Entry Point (main.c)

**Responsibilities**:

- Command-line argument parsing delegation
- Mode dispatcher
- Global cleanup

**Key Functions**:

- `main()` - Entry point (~40 lines)
- `run_mode_*()` - Mode entry points (stubs for unimplemented modes)
- `cleanup_config()` - Free allocated resources

### Command-Line Parsing (cmdline.c)

**Responsibilities**:

- Parse all command-line options
- Initialize configuration with defaults
- Generate random or use specified PRNG seed

**Key Functions**:

- `parse_options()` - Main parsing function (~150 lines)
- `print_usage()` - Display help
- `print_version()` - Display version info
- `parse_language()` - Language code parsing

**Options Supported**:

- `-h, --help` - Show help
- `-v, --verbose` - Enable verbose output
- `-V, --version` - Show version
- `-n, --numsim=N` - Set simulation count
- `-u, --ui.lang=[LANG]` - Set language (en/fr/es)
- `-p, --prng.seed=[SEED]` - Set PRNG seed (random if omitted)
- Game mode flags: `-a, -l, -t, -g, -S, -C, -L, -T, -G, -A`

### PRNG Seed Management (prng_seed.c)

**Responsibilities**:

- Generate cryptographically secure random seeds
- Parse seed arguments (decimal or hex)
- Validate seed values
- Robust error handling

**Key Functions**:

- `generate_random_seed()` - Platform-specific secure random
  - Windows: `CryptGenRandom`
  - Linux: `/dev/urandom`
  - Fallback: Time + clock + stack address mixing
- `parse_seed_arg()` - Parse user input with extensive validation
- `validate_seed()` - Ensure seed is valid for MT19937

**Design Philosophy**: Fail-safe approach - invalid input → default seed + warning

### Player Configuration (player_config.c)

**Responsibilities**:

- Manage player names, types, and strategies
- Player assignment (direct, inverted, random)
- AI strategy selection menu
- Localization support

**Key Structures**:

```c
typedef struct {
    char player_names[2][MAX_PLAYER_NAME_LEN];
    AIStrategyType ai_strategies[2];
    PlayerAssignmentMode assignment_mode;
} PlayerConfig;
```

**Key Functions**:

- `init_player_config()` - Initialize with defaults
- `get_player_names()` - Interactive name entry
- `get_ai_strategies()` - AI strategy selection
- `get_player_assignment()` - Assignment mode selection
- `apply_player_assignment()` - Apply with optional random swap
- `get_strategy_display_name()` - Localized strategy names

### Localization (localization.h)

**Approach**: Macro-based with language enum

```c
#define LOCALIZED_STRING(en, fr, es) \
    ((const char*[]){en, fr, es}[cfg->language])
```

**Supported Languages**:

- English (default)
- French
- Spanish

**Usage**:

```c
printf("%s\n", LOCALIZED_STRING("Game Over", "Fin du jeu", "Juego terminado"));
```

### Game Constants (game_constants.c)

**Responsibilities**:

- Full deck definition (120 cards)
- String name arrays for all enums
- Game-wide constants

**Key Data**:

- `fullDeck[120]` - Complete card database
  - All champion attributes
  - Calculated efficiency values
  - Power ratings
- Enum name arrays for debugging/display
- Species-to-Order mapping (via Order field in card struct)

**Constants**:

```c
#define FULL_DECK_SIZE 120
#define MAX_NUMBER_OF_TURNS 500
#define INITIAL_CASH_DEFAULT 30
#define INITIAL_ENERGY_DEFAULT 99
#define INITAL_HAND_SIZE_DEFAULT 6
#define AVERAGE_POWER_FOR_MULLIGAN 4.98
#define M_TWISTER_SEED 1337UL
```

### Card Actions (card_actions.c)

**Responsibilities**:

- Card playing logic
- Draw mechanics
- Deck shuffling
- Discard-to-7 (currently AI logic only)

**Key Functions**:

- `play_card()` - Dispatcher based on card type
- `play_champion()` - Move to combat zone, pay cost
- `play_draw_card()` - Draw N cards
- `play_cash_card()` - Exchange champion for lunas
- `draw_1_card()` - Draw with deck refresh if needed
- `shuffle_discard_and_form_deck()` - Reshuffle mechanic
- `discard_to_7_cards()` - Power-based discard (AI logic)

**TODO Items**:

- Add recall mechanic (currently only draws)
- Move strategy-specific logic to strategy modules
- Add interactive player choice for discard-to-7

### Combat (combat.c)

**Responsibilities**:

- Combat resolution
- Attack/defense calculation with combo bonuses
- Damage application
- Combat zone clearing

**Key Functions**:

- `resolve_combat()` - Main orchestrator (~20 lines)
- `calculate_total_attack()` - Base + dice + combo
- `calculate_total_defense()` - Dice + combo (no base)
- `apply_combat_damage()` - Apply damage, check win condition
- `clear_combat_zones()` - Move cards to discard

**Formula**:

```c
attack = Σ(base + roll_dice(die)) + combo_bonus
defense = Σ(roll_dice(die)) + combo_bonus
damage = max(attack - defense, 0)
```

### Combo Bonus (combo_bonus.c)

**Responsibilities**:

- Calculate combo bonuses for 2-3 champions
- Support 3 deck types (random, monochrome, custom)
- Complex matching logic

**Key Functions**:

- `calculate_combo_bonus()` - Router by deck type
- `calc_random_bonus()` - Full combo system (species/order/color)
- `calc_prebuilt_bonus()` - Simplified for monochrome/custom
- Helper functions: counting, matching

**Priority Order** (Random Deck):

1. Species matches (highest)
2. Order matches (if no species match)
3. Color matches (if no species/order match)

### Turn Logic (turn_logic.c)

**Responsibilities**:

- Turn sequence orchestration
- Phase management
- Turn counter

**Key Functions**:

- `play_turn()` - Main turn loop (~20 lines)
- `begin_of_turn()` - Increment turn, draw card
- `attack_phase()` - Call attack strategy
- `defense_phase()` - Call defense strategy
- `end_of_turn()` - Collect luna, discard, switch player

**TODO**: Phase transition timing needs review for MCTS integration

### Game State (game_state.c)

**Responsibilities**:

- Game initialization
- Luna collection
- Player switching

**Key Functions**:

- `setup_game()` - Initialize gamestate, distribute cards (~40 lines)
- `collect_1_luna()` - Increment cash
- `change_current_player()` - Switch active player

**Initialization Steps**:

1. Set starting energy (99) and cash (30)
2. Shuffle full deck
3. Deal 40 cards to each player
4. Initialize all collections (hand, discard, combat zone)
5. Draw 6 cards for each player

### Strategy Framework (strategy.c)

**Responsibilities**:

- Manage function pointer sets
- Strategy registration

**Key Functions**:

- `create_strategy_set()` - Allocate (~5 lines)
- `set_player_strategy()` - Register functions (~5 lines)
- `free_strategy_set()` - Cleanup (~5 lines)

**Design**: Minimal framework, strategies are self-contained

### Random Strategy (strat_random.c)

**Responsibilities**:

- Baseline AI implementation
- Random card selection

**Key Functions**:

- `random_attack_strategy()` - Play random affordable card (~35 lines)
- `random_defense_strategy()` - 47% chance to defend with random champion (~30 lines)

**Behavior**:

- Attack: Play any affordable card (champions, draw, cash)
- Special case: Skip cash cards if no champions in hand
- Defense: 47% chance to play random affordable champion

**Performance**: ~50% win rate in mirror matches (as expected)

### Automated Simulation (stda_auto.c)

**Responsibilities**:

- Batch game simulation
- Statistics collection
- Result presentation
- Histogram generation

**Key Functions**:

- `run_mode_stda_auto()` - Entry point (~30 lines)
- `run_simulation()` - Run N games (~10 lines)
- `play_stda_auto_game()` - Single game (~40 lines)
- `apply_mulligan()` - AI mulligan logic (~40 lines)
- `record_final_stats()` - Update statistics (~20 lines)
- `present_results()` - Display results with histogram (~60 lines)

**Statistics**:

- Win/loss/draw counts
- Turn length statistics (min/max/avg)
- Histogram with underflow/overflow bins

### CLI Interactive Mode (stda_cli.c)

**Responsibilities**:

- Human vs AI gameplay
- Command parsing
- Display formatting
- Player configuration UI

**Key Components**:

- Display functions (~250 lines)
  - `display_player_prompt()`, `display_player_hand()`
  - `display_attack_state()`, `display_game_status()`
  - `display_cli_help()`
- Input parsing (~150 lines)
  - `parse_champion_indices()`, `validate_and_play_champions()`
- Command handlers (~200 lines)
  - `handle_draw_command()`, `handle_cash_command()`
  - `process_attack_command()`, `process_defense_command()`
- Game phases (~150 lines)
  - `handle_interactive_attack()`, `handle_interactive_defense()`
  - `execute_game_turn()`
- Initialization/cleanup (~50 lines)

**Features**:

- ANSI color support
- UTF-8 symbols (❤ ☾ ⚔ 🛡)
- Localized messages
- Player configuration flow
- Game summary at end

---

## Future Considerations

### Callback Architecture for UI

**Problem**: Game logic needs to communicate events to UI without tight coupling

**Proposed Solution**:

```c
typedef struct {
    void (*on_card_drawn)(PlayerID player, uint8_t card_id, void* ui_ctx);
    void (*on_combat_resolved)(int16_t damage, void* ui_ctx);
    void (*on_champion_played)(PlayerID player, uint8_t card_id, void* ui_ctx);
} UICallbacks;

// Pass to game functions
void draw_1_card(struct gamestate* gstate, PlayerID player, 
                GameContext* ctx, UICallbacks* uicb);
```

**Benefits**:

- Game logic remains UI-agnostic
- Easy to add new UI implementations
- Can disable callbacks for automated simulations

### AI Development Path

**Phase 1**: Balanced Rules (next)

- Target metrics based on opponent energy
- Resource management heuristics
- Card selection by efficiency

**Phase 2**: Heuristic Evaluation

- Advantage function (energy + cards + cash)
- 1-move lookahead
- Parameter calibration

**Phase 3**: Simple Monte Carlo

- Progressive pruning (93 → 30 → 10 → 4 moves)
- Staged simulations (100 → 200 → 400 → 800)
- Can serve as interactive AI assistant

**Phase 4**: IS-MCTS

- Tree-based search
- UCT node selection
- Information set handling
- Transposition tables

### Configuration File System

**Planned**: INI-style configuration

```ini
[game]
initial_cash = 30
initial_energy = 99

[ui]
language = en
color_enabled = true

[simulation]
default_numsim = 1000
```

**Benefits**:

- Persistent preferences
- Easy to edit
- Override with command-line

### CSV Export System

**Purpose**: Export simulation data for analysis

**Files**:

- Detail CSV: Per-game data (all stats)
- Summary CSV: Aggregate statistics

**Fields**:

- Game ID, winner, turns, final energy/cash
- Strategy names, deck types
- Simparam string for grouping

---

## Appendix: Code Metrics

### Function Length Compliance

**Target**: ≤35 lines  
**Firm Limit**: 100 lines

**Current Status**: Most functions comply, with exceptions for:

- Display functions (formatting-heavy, acceptable)
- Data initialization (mostly assignments, acceptable)

---

*Last Updated: December 2025* 
