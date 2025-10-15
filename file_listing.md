# Complete File Listing for Refactored Oracle Game

### 1. game_types.h
**Purpose:** All enum and struct definitions

### 2. game_constants.h
**Purpose:** Constants and external declarations

### 3. game_constants.c
**Purpose:** Full deck definition and lookup tables

### 4. combo_bonus.h
**Purpose:** Combo bonus calculator interface

### 5. combo_bonus.c
**Purpose:** Combo bonus calculator implementation

### 6. card_actions.h
**Purpose:** Card playing functions interface

### 7. card_actions.c
**Purpose:** Card playing and game actions implementation

### 8. combat.h
**Purpose:** Combat resolution interface

### 9. combat.c
**Purpose:** Combat resolution implementation with combo bonuses

### 10. strategy.h
**Purpose:** Strategy function pointer framework

### 11. strategy.c
**Purpose:** Strategy management implementation

### 12. strat_random.h
**Purpose:** Random strategy interface

### 13. strat_random.c
**Purpose:** Random strategy implementation

### 14. turn_logic.h
**Purpose:** Turn flow interface

### 15. turn_logic.c
**Purpose:** Turn flow implementation

### 16. game_state.h
**Purpose:** Game initialization and simulation interface

### 17. game_state.c
**Purpose:** Game initialization and simulation implementation

### 18. main.c
**Purpose:** Program entry point


### Header Files (`.h`)

1. **game_types.h** 
   - All enums: PlayerID, GameStateEnum, TurnPhase, CardType, ChampionColor, ChampionSpecies, ChampionOrder, DeckType
   - All structs: card, gamestate, gamestats
 
2. **game_constants.h**
   - Constants definitions
   - External declarations for fullDeck array and SPECIES_TO_ORDER
   - String name array declarations

3. **combo_bonus.h** 
   - CombatCard struct definition
   - Combo bonus calculation function prototypes

4. **card_actions.h** 
   - Card playing functions
   - Helper functions for card management
   - Game action functions

5. **combat.h** 
   - Combat resolution function prototypes

6. **strategy.h**
   - Strategy function pointer types
   - StrategySet struct
   - Strategy management functions

7. **strat_random.h**
   - Random strategy function prototypes

8. **turn_logic.h** 
   - Turn flow function prototypes

9. **game_state.h** 
   - Game initialization and management
   - Simulation functions
   - Stats recording and presentation

10. **deckstack.h** 
11. **hdcll.h** 
12. **rnd.h** 
13. **mtwister.h** 

### Implementation Files (`.c`)

1. **main.c**
   - Program entry point
   - Strategy setup
   - Simulation execution
   - Minimal, clean, focused

2. **game_constants.c**
   - fullDeck array definition (102 champions + 18 special cards)
   - SPECIES_TO_ORDER mapping array
   - String name arrays
   - get_order_from_species() implementation

3. **combo_bonus.c** 
   - Complete combo bonus calculator
   - Implements all bonus rules for RANDOM, MONOCHROME, CUSTOM modes
   - Uses the new "Order" terminology

4. **card_actions.c**
   - play_card(), play_champion(), play_draw_card(), play_cash_card()
   - has_champion_in_hand(), select_champion_for_cash_exchange()
   - draw_1_card(), shuffle_discard_and_form_deck()
   - collect_1_luna(), discard_to_7_cards(), change_current_player()

5. **combat.c** 
   - resolve_combat()
   - calculate_total_attack(), calculate_total_defense()
   - apply_combat_damage(), clear_combat_zones()
   - Integrates combo bonus calculations

6. **strategy.c**
   - create_strategy_set(), set_player_strategy(), free_strategy_set()
   - Function pointer framework

7. **strat_random.c** 
   - random_attack_strategy()
   - random_defense_strategy()
   - Implements random card selection logic

8. **turn_logic.c** 
   - play_turn()
   - begin_of_turn(), end_of_turn()
   - attack_phase(), defense_phase()

9. **game_state.c**
   - setup_game(), apply_mulligan()
   - play_game(), run_simulation()
   - record_final_stats(), present_results()

10. **deckstack.c** 
11. **hdcll.c** 
12. **rnd.c** 
13. **mtwister.c** 
