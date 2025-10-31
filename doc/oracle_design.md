# Oracle: Technical Design Document

**Les Champions d'Arcadie / The Arcadian Champions of Light**

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Current Implementation Status](#current-implementation-status)
3. [Core Data Structures](#core-data-structures)
4. [GameContext Pattern](#gamecontext-pattern)
5. [Game Logic Flow](#game-logic-flow)
6. [Strategy Framework](#strategy-framework)
7. [Combat System](#combat-system)
8. [Command-Line Interface](#command-line-interface)
9. [File Organization](#file-organization)
10. [Future Architecture](#future-architecture)

---

## 1. Architecture Overview

### Design Principles

- **Maximum 30 lines per function** (excluding comments/whitespace)
- **Maximum 500 lines per source file** (ideally ≤400)
- **Separation of concerns**: Game logic, UI, AI, and modes are independent
- **Testability**: GameContext pattern allows dependency injection
- **Extensibility**: Strategy framework enables pluggable AI

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
- Turn-based flow (with some gaps)
- Card playing (champions, draw, cash)

**AI Framework**:
- Strategy function pointer system
- Random strategy (functional baseline)

**User Interface**:
- Command-line argument parsing
- CLI mode (human vs AI)
- ANSI color output with UTF-8 symbols
- Basic game display and input

**Infrastructure**:
- GameContext pattern for RNG and config
- Debug macro system (compile-time)
- Mersenne Twister RNG
- Data structures (deck stack, circular linked list)

### What's Missing ⚠️

**Game Logic Gaps**:
- Mulligan system (Player B, 2 cards)
- Discard to 7 cards (end of turn)
- Recall mechanic (draw/recall cards)
- Phase transition validation
- Win/draw condition detection

**AI Strategies**:
- Balanced rules AI
- Heuristic AI
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
    struct HDCLList hand[2];              // Cards in hand
    struct HDCLList discard[2];           // Discard piles
    struct HDCLList combat_zone[2];       // Cards in combat (max 3)
    
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

**Hand/Discard/Combat Circular Linked List** (`hdcll.c`):
- Dynamic linked list
- Insert at beginning (O(1))
- Remove by value or index
- Convert to array for iteration
- Size tracked in list structure

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
GameContext* ctx = create_game_context(SEED, &cfg);

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
- ⚠️ Some mode code still uses global config
- 📋 Future: Add network context, UI callbacks

---

## 5. Game Logic Flow

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
    if (DeckStk_isEmpty(&gstate->deck[player])) {
        shuffle_discard_and_form_deck(&gstate->discard[player], 
                                      &gstate->deck[player], ctx);
    }
    
    // Draw top card from deck
    uint8_t card_idx = DeckStk_pop(&gstate->deck[player]);
    HDCLL_insertNodeAtBeginning(&gstate->hand[player], card_idx);
}
```

**Edge Cases**:
- What if discard is also empty? (Game design question - should never happen?)
- First player on turn 1 doesn't draw

---

## 6. Strategy Framework

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

1. Create `src/ai/strategy_name.c`
2. Implement attack and defense functions with signature:
   ```c
   void strategyname_attack_strategy(struct gamestate* gstate, GameContext* ctx);
   void strategyname_defense_strategy(struct gamestate* gstate, GameContext* ctx);
   ```
3. Functions modify `gstate` directly (play cards, update combat zone)
4. Register strategy in mode initialization

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

## 7. Combat System

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

See `combo_bonus.c` for full decision tree.

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

## 8. Command-Line Interface

### Implementation

**Entry Point**: `main.c` → `run_mode_stda_cli()`  
**Implementation**: `stda_cli.c` (550 lines, needs refactoring)

### Features

**Display**:
- ANSI color codes (player names, card types, energy, lunas)
- UTF-8 symbols (❤ energy, ☾ luna, ⚔ attack, 🛡 defense)
- Card display with species, dice, cost
- Game status (energy, lunas, hand size, deck size)

**Commands**:
- `cham 1 2 3` - Play champions at hand indices 1, 2, 3
- `draw 2` - Play draw/recall card at index 2
- `cash 1` - Play cash exchange card at index 1
- `pass` - Pass turn (attack phase) or decline defense
- `gmst` - Show game status
- `help` - Show commands
- `exit` - Quit game

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
    if (current_player == HUMAN) {
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

---

## 9. File Organization

### Current Structure

```
oracle/
├── src/
│   ├── card_actions.c/h      # Play cards, draw, shuffle
│   ├── cmdline.c/h           # Command-line parsing
│   ├── combat.c/h            # Combat resolution
│   ├── combo_bonus.c/h       # Combo calculations
│   ├── debug.h               # Debug macros
│   ├── deckstack.c/h         # Deck data structure
│   ├── game_constants.c/h    # Full deck, enums, strings
│   ├── game_context.c/h      # GameContext pattern
│   ├── game_state.c/h        # Game initialization
│   ├── game_types.h          # All type definitions
│   ├── hdcll.c/h             # Circular linked list
│   ├── main.c                # Main entry point
│   ├── main.h                # Mode function declarations
│   ├── mtwister.c/h          # Mersenne Twister RNG
│   ├── rnd.c/h               # RNG wrapper functions
│   ├── stda_auto.c/h         # Automated simulation mode
│   ├── stda_cli.c/h          # CLI interactive mode
│   ├── strat_random.c/h      # Random AI strategy
│   ├── strategy.c/h          # Strategy framework
│   ├── turn_logic.c/h        # Turn flow management
│   └── version.h             # Version info
│
├── ideas/                    # Design explorations
│   ├── config file/
│   ├── gui/
│   ├── rating system/
│   ├── sim_export/
│   └── tui/
│
├── docs/                     # Documentation
│   ├── ROADMAP.md
│   ├── TODO.md
│   ├── DESIGN.md (this file)
│   └── (game rule documents)
│
├── Makefile
└── README.md
```

### File Size Guidelines

**Target**: ≤500 lines actual code per file

**Current Status**:
- ✅ Most core files: <300 lines
- ⚠️ stda_cli.c: 550 lines (needs split)
- ⚠️ game_constants.c: 350 lines (mostly data, acceptable)

**Refactoring Plan**:
```
stda_cli.c (550 lines) → split into:
├── cli_display.c (~250 lines)  # Display functions
└── cli_input.c (~250 lines)    # Input parsing/validation
```

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

**Asset Pipeline**:
```
assets/
├── cards/artwork/       # 102 champion portraits
├── icons/species/       # 15 species icons
├── icons/orders/        # 5 order symbols
└── fonts/               # TTF fonts
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
   - Consider function size limits (30 lines)

2. **Implementation Phase**:
   - Create `.c` and `.h` files
   - Implement one function at a time
   - Keep functions short and focused

3. **Integration Phase**:
   - Update `Makefile`
   - Add to appropriate mode(s)
   - Update `DESIGN.md`

4. **Testing Phase**:
   - Write unit tests
   - Test integration with existing code
   - Run with valgrind (check leaks)

### Code Quality Checklist

Before committing:
- [ ] All functions ≤30 lines
- [ ] All files ≤500 lines
- [ ] No compiler warnings (-Wall -Wextra)
- [ ] Doxygen comments on public functions
- [ ] No memory leaks (valgrind clean)
- [ ] Consistent naming (snake_case)
- [ ] Updated TODO.md checkboxes
- [ ] Updated DESIGN.md if architecture changed

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
- Use memory pools for linked list nodes
- Profile with gprof to find actual bottlenecks

---

## Testing Strategy

### Unit Tests

Test individual functions in isolation:
- `test_combo_bonus.c`: All combo scenarios
- `test_combat.c`: Dice distribution, damage calculation
- `test_turn_logic.c`: Phase transitions, turn counter

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

- **Game Rules**: See attached documents (Diagramme déroulement du jeu.pdf, General information about the game)
- **GitHub**: https://github.com/JonathanFerron/oracle/tree/main
- **MCTS Resources**: (to be added as studied)
- **Bradley-Terry Model**: See `ideas/rating system/` for full spec and papers

---

*Last Updated: November 2024*  
*Next Review: After Phase 2 completion*
