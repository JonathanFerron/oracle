Here are 10 material contradictory or inconsistent issues, sorted by importance:

## 1: Mulligan Implementation Partially Complete

**Contradiction:**

- `stda_auto.c` has a working `apply_mulligan()` implementation (lines 109-149) with power-based heuristic
- **But:** This is hardcoded for automated simulation only
- `stda_cli.c` line 485: TODO comment "add here the logic to perform the mulligan for player B" but no implementation
- `doc/file_listing.md` describes mulligan as if it's fully integrated across all modes
- Game rules require interactive player choice in CLI/TUI modes

**Impact:** High - Feature works in one mode but missing in interactive modes where player agency is critical

---

## 2: Discard-to-7 Implementation Exists But Not Integrated in CLI

**Contradiction:**

- `card_actions.c` has `discard_to_7_cards()` fully implemented (lines 138-162) with power-based heuristic
- `turn_logic.c` line 73 calls it in `end_of_turn()`
- **But:** This is automated via power heuristic - no player choice
- `stda_cli.c` line 487: TODO "add here the functionality to discard to 7 cards"
- Game rules state player must choose which cards to discard

**Impact:** High - Automated implementation removes player agency in interactive mode

---

## 3. **Recall Mechanic Not Implemented Despite Card Definitions**

**Contradiction:**

- Full deck includes 9 "Draw 2/Recall 1" and 6 "Draw 3/Recall 2" cards
- Game rules document extensively describes recall mechanics
- `card_actions.c` TODO: "TODO: must give the option to the interactive player to choose between draw and recall"
- **Reality:** `play_draw_card()` ONLY draws cards, never recalls
- `struct card` has `choose_num` field (for recall) but it's never used

**Impact:** High - Major game feature completely missing

---

## 4. **Strategy Function Timing Comments Contradict Implementation**

**Contradiction:**

- `turn_logic.c` has extensive TODO comments about when to set `turn_phase` and `player_to_move`:
  - "TODO: may eventually need to look into whether or not this line of code should be moved inside the attack_strategy function"
  - Similar comment in `attack_phase()` about moving phase transition
- **But:** These phase transitions are critical RIGHT NOW for any MCTS implementation
- The comments suggest uncertainty about architecture that should be resolved before MCTS work begins

**Impact:** Medium - Will block MCTS implementation

---

## 5. **GameContext vs Global RNG Inconsistency**

**Contradiction:**

- Documentation states: "Eliminate global state" as a design goal
- `main.c` declares: `extern MTRand MTwister_rand_struct;` (global)
- `stda_auto.c` also declares: `extern MTRand MTwister_rand_struct;`
- **But:** `rnd.c` was refactored to use `GameContext* ctx` parameter
- TODO.md states "Remove global MTwister_rand_struct (use GameContext everywhere)" but externals still exist

**Impact:** Medium - Incomplete refactoring compromises testability

---

## 6. **File Size Targets Violated**

**Contradiction:**

- Design guideline: "Maximum 500 lines per source file (ideally ≤400)"
- **Violations:**
  - `stda_cli.c`: 550 lines (acknowledged in TODO.md as "OVER LIMIT")
  - TODO.md says "split needed" but provides no timeline
  - Makefile and doc/file_listing.md still reference it as single file

**Impact:** Low-Medium - Code organization debt

---

## 7. **Configuration Struct Location Confusion**

**Contradiction:**

- `game_types.h` line 109 TODO comment: "consider whether this struct should be moved to the game_context.h source file, or to cmdline.h"
- `config_t` is in `game_types.h` but only used by command-line and mode code
- `game_context.h` has `config_t* config;` pointer
- **Design issue:** Core game types file shouldn't contain CLI-specific configuration

**Impact:** Low-Medium - Architectural clarity

---

## 8. **Debug Macro Inconsistency**

**Contradiction:**

- `debug.h` defines `DEBUG_PRINT` and `DEBUG_ONLY` macros
- Some files use `DEBUG_PRINT`, others use raw `printf` in `DEBUG_ONLY` blocks
- Example in `combat.c` line 59: `DEBUG_ONLY(if(bonus > 0) printf(...))` instead of `DEBUG_PRINT`
- Inconsistent pattern makes debugging harder

**Impact:** Low - Cosmetic but affects maintainability

---

## 9. **Species-to-Order Mapping Declaration Without Definition**

**Contradiction:**

- `game_constants.h` line 26 declares: `extern const ChampionOrder SPECIES_TO_ORDER[];`
- **This array is NEVER defined in `game_constants.c`**
- `combo_bonus.c` used to need this (per old comments) but was refactored to use direct `order` field
- Dead declaration should be removed

**Impact:** Low - Dead code but confusing

---

## 10. **StrategySet Design vs Documentation**

**Contradiction:**

- `strategy.h` line 13: TODO comment "split this so that a Strategy Set contains the strategy of only one player"
- Current design: `StrategySet` has arrays `[2]` for both players
- **But:** Documentation and usage patterns treat it as managing both players
- Design doc doesn't mention this split as necessary
- Would require significant refactoring of all mode code

**Impact:** Low - Design philosophy inconsistency, not urgent

---

## 

11. Revised Issue #11: Turn Counter Initialization and Semantics
    
    **Contradiction:**
    
    - Game is initialized with `turn = 0` in `setup_game()` (not explicitly set, but implicit from memset or missing initialization)
    - `begin_of_turn()` immediately increments: `gstate->turn++` (line 36 of `turn_logic.c`)
    - This means first action happens at `turn = 1`, which is correct
    - **But:** Comments throughout code reference "turn 1 = Player A attacks, turn 2 = Player B attacks"
    - Debug output shows `(uint16_t)((gstate->turn-1) * 0.5)+1` for round calculation, suggesting confusion
    - `stda_cli.c` line 457: `gstate->turn = 0;` explicitly set before game loop
    - The skip-draw logic uses `gstate->turn == 1 && gstate->current_player == PLAYER_A` (line 42 of `turn_logic.c`)
    
    **The Real Issue:**
    
    - Turn counter starts at 0, incremented to 1 before first draw
    - Round calculation `(turn-1)*0.5+1` works correctly: turn 1 → round 1, turn 2 → round 1, turn 3 → round 2
    - **Problem:** Lack of consistent documentation about this initialization pattern creates confusion for future maintainers
    
    **Impact:** Low - Works correctly but documentation/comments could cause confusion
    
    ---
    
    Thanks for the catch! The turn counter actually works correctly - I was confused by the round calculation formula. The real issue there is just documentation clarity, not a functional bug.
    
    
    
    # Honorable Mentions:

12. **Cash Card Selection**: `select_champion_for_cash_exchange()` is in `card_actions.c` with TODO "this code could be moved to the strategy" - architectural boundary violation

These issues range from blocking bugs (recall not implemented) to design debt (file sizes, TODO comments). The most material ones (#1-5) should be addressed before advancing to Phase 3+ features.





---

## 
