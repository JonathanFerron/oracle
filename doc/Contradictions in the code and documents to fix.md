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

## 6. **File Size Targets Violated**

**Contradiction:**

- Design guideline: "Maximum 500 lines per source file (ideally ≤400)"
- **Violations:**
  - `stda_cli.c`: 550 lines 
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

# Honorable Mentions:

Cash Card Selection**: `select_champion_for_cash_exchange()` is in `card_actions.c` with TODO "this code could be moved to the strategy" - architectural boundary violation



## 
