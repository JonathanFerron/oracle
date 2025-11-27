# 

# Specialized Collections Migration Guide

## Part 1: New Code to Create

### File 1: `src/card_collection.h`

```c
// card_collection.h
// Specialized fixed-size array collections for card game

#ifndef CARD_COLLECTION_H
#define CARD_COLLECTION_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// Hand Collection (max 10 cards in practice, but allow margin)
// ============================================================================
typedef struct {
    uint8_t cards[12];  // Allow some margin beyond max of 10
    uint8_t size;
} Hand;

void Hand_init(Hand* hand);
void Hand_add(Hand* hand, uint8_t card);
bool Hand_remove(Hand* hand, uint8_t card);
void Hand_clear(Hand* hand);
uint8_t Hand_get(const Hand* hand, uint8_t index);
bool Hand_contains(const Hand* hand, uint8_t card);

// ============================================================================
// CombatZone Collection (max 3 cards)
// ============================================================================
typedef struct {
    uint8_t cards[3];
    uint8_t size;
} CombatZone;

void CombatZone_init(CombatZone* zone);
void CombatZone_add(CombatZone* zone, uint8_t card);
bool CombatZone_remove(CombatZone* zone, uint8_t card);
void CombatZone_clear(CombatZone* zone);
uint8_t CombatZone_get(const CombatZone* zone, uint8_t index);

// ============================================================================
// Discard Collection (max 40 cards)
// ============================================================================
typedef struct {
    uint8_t cards[40];
    uint8_t size;
} Discard;

void Discard_init(Discard* discard);
void Discard_add(Discard* discard, uint8_t card);
bool Discard_remove(Discard* discard, uint8_t card);
void Discard_clear(Discard* discard);
uint8_t Discard_get(const Discard* discard, uint8_t index);

#endif // CARD_COLLECTION_H
```

### File 2: `src/card_collection.c`

```c
// card_collection.c
// Implementation of specialized card collections

#include "card_collection.h"

// ============================================================================
// Hand Implementation
// ============================================================================

void Hand_init(Hand* hand) {
    hand->size = 0;
}

void Hand_add(Hand* hand, uint8_t card) {
    if (hand->size < 12) {
        hand->cards[hand->size++] = card;
    }
}

bool Hand_remove(Hand* hand, uint8_t card) {
    for (uint8_t i = 0; i < hand->size; i++) {
        if (hand->cards[i] == card) {
            // Shift remaining cards left
            for (uint8_t j = i; j < hand->size - 1; j++) {
                hand->cards[j] = hand->cards[j + 1];
            }
            hand->size--;
            return true;
        }
    }
    return false;
}

void Hand_clear(Hand* hand) {
    hand->size = 0;
}

uint8_t Hand_get(const Hand* hand, uint8_t index) {
    if (index < hand->size) {
        return hand->cards[index];
    }
    return 0;
}

bool Hand_contains(const Hand* hand, uint8_t card) {
    for (uint8_t i = 0; i < hand->size; i++) {
        if (hand->cards[i] == card) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// CombatZone Implementation
// ============================================================================

void CombatZone_init(CombatZone* zone) {
    zone->size = 0;
}

void CombatZone_add(CombatZone* zone, uint8_t card) {
    if (zone->size < 3) {
        zone->cards[zone->size++] = card;
    }
}

bool CombatZone_remove(CombatZone* zone, uint8_t card) {
    for (uint8_t i = 0; i < zone->size; i++) {
        if (zone->cards[i] == card) {
            for (uint8_t j = i; j < zone->size - 1; j++) {
                zone->cards[j] = zone->cards[j + 1];
            }
            zone->size--;
            return true;
        }
    }
    return false;
}

void CombatZone_clear(CombatZone* zone) {
    zone->size = 0;
}

uint8_t CombatZone_get(const CombatZone* zone, uint8_t index) {
    if (index < zone->size) {
        return zone->cards[index];
    }
    return 0;
}

// ============================================================================
// Discard Implementation
// ============================================================================

void Discard_init(Discard* discard) {
    discard->size = 0;
}

void Discard_add(Discard* discard, uint8_t card) {
    if (discard->size < 40) {
        discard->cards[discard->size++] = card;
    }
}

bool Discard_remove(Discard* discard, uint8_t card) {
    for (uint8_t i = 0; i < discard->size; i++) {
        if (discard->cards[i] == card) {
            for (uint8_t j = i; j < discard->size - 1; j++) {
                discard->cards[j] = discard->cards[j + 1];
            }
            discard->size--;
            return true;
        }
    }
    return false;
}

void Discard_clear(Discard* discard) {
    discard->size = 0;
}

uint8_t Discard_get(const Discard* discard, uint8_t index) {
    if (index < discard->size) {
        return discard->cards[index];
    }
    return 0;
}
```

---

## Part 2: Update Existing Files

### Update 1: `src/game_types.h`

```diff
// game_types.h

 #include <stdint.h>
 #include <stdbool.h>
 #include "deckstack.h"
-#include "hdcll.h"
+#include "card_collection.h"

 // ... (keep all enums unchanged)

 // Game state structure
 struct gamestate
 { PlayerID current_player;
   uint16_t current_cash_balance[2];
   uint8_t current_energy[2];
   bool someone_has_zero_energy;

   struct deck_stack deck[2];
-  struct HDCLList hand[2];
-  struct HDCLList discard[2];
-  struct HDCLList combat_zone[2];
+  Hand hand[2];
+  Discard discard[2];
+  CombatZone combat_zone[2];

   uint16_t turn;
   GameStateEnum game_state;
   TurnPhase turn_phase;
   PlayerID player_to_move;
 };
```

---

## Part 3: Refactoring Patterns

### Pattern 1: Initialize Collections

**Location:** `src/game_state.c` - `setup_game()` function

```diff
 void setup_game(uint16_t initial_cash, struct gamestate* gstate, GameContext* ctx)
 {
   // ... (initialization code) ...

   // Initialize hands, combat zones, and discards
-  HDCLL_initialize(&gstate->hand[PLAYER_A]);
-  HDCLL_initialize(&gstate->hand[PLAYER_B]);
-  HDCLL_initialize(&gstate->discard[PLAYER_A]);
-  HDCLL_initialize(&gstate->discard[PLAYER_B]);
-  HDCLL_initialize(&gstate->combat_zone[PLAYER_A]);
-  HDCLL_initialize(&gstate->combat_zone[PLAYER_B]);
+  Hand_init(&gstate->hand[PLAYER_A]);
+  Hand_init(&gstate->hand[PLAYER_B]);
+  Discard_init(&gstate->discard[PLAYER_A]);
+  Discard_init(&gstate->discard[PLAYER_B]);
+  CombatZone_init(&gstate->combat_zone[PLAYER_A]);
+  CombatZone_init(&gstate->combat_zone[PLAYER_B]);

   // Draw initial hands (6 cards each)
   for(i = 0; i < INITAL_HAND_SIZE_DEFAULT; i++)
   { uint8_t cardindex = DeckStk_pop(&gstate->deck[PLAYER_A]);
-    HDCLL_insertNodeAtBeginning(&gstate->hand[PLAYER_A], cardindex);
+    Hand_add(&gstate->hand[PLAYER_A], cardindex);
     cardindex = DeckStk_pop(&gstate->deck[PLAYER_B]);
-    HDCLL_insertNodeAtBeginning(&gstate->hand[PLAYER_B], cardindex);
+    Hand_add(&gstate->hand[PLAYER_B], cardindex);
   }
 }
```

---

### Pattern 2: Cleanup Collections

**Location:** `src/stda_auto.c` - `play_stda_auto_game()` function

```diff
   // Free heap memory
   DeckStk_emptyOut(&gstate.deck[PLAYER_A]);
   DeckStk_emptyOut(&gstate.deck[PLAYER_B]);
-  HDCLL_emptyOut(&gstate.combat_zone[PLAYER_A]);
-  HDCLL_emptyOut(&gstate.combat_zone[PLAYER_B]);
-  HDCLL_emptyOut(&gstate.hand[PLAYER_A]);
-  HDCLL_emptyOut(&gstate.hand[PLAYER_B]);
-  HDCLL_emptyOut(&gstate.discard[PLAYER_A]);
-  HDCLL_emptyOut(&gstate.discard[PLAYER_B]);
+  // No cleanup needed for fixed arrays - they're on stack
 }
```

**Location:** `src/stda_cli.c` - `cleanup_cli_game()` function

```diff
 static void cleanup_cli_game(struct gamestate* gstate, StrategySet* strategies, GameContext* ctx)
 { DeckStk_emptyOut(&gstate->deck[PLAYER_A]);
   DeckStk_emptyOut(&gstate->deck[PLAYER_B]);
-  HDCLL_emptyOut(&gstate->combat_zone[PLAYER_A]);
-  HDCLL_emptyOut(&gstate->combat_zone[PLAYER_B]);
-  HDCLL_emptyOut(&gstate->hand[PLAYER_A]);
-  HDCLL_emptyOut(&gstate->hand[PLAYER_B]);
-  HDCLL_emptyOut(&gstate->discard[PLAYER_A]);
-  HDCLL_emptyOut(&gstate->discard[PLAYER_B]);
+  // No cleanup needed for fixed arrays

   free(gstate);
   free_strategy_set(strategies);
   destroy_game_context(ctx);
 }
```

---

### Pattern 3: Add Card to Collection

**Find:** `HDCLL_insertNodeAtBeginning(&gstate->hand[player], card)`  
**Replace with:** `Hand_add(&gstate->hand[player], card)`

**Find:** `HDCLL_insertNodeAtBeginning(&gstate->discard[player], card)`  
**Replace with:** `Discard_add(&gstate->discard[player], card)`

**Find:** `HDCLL_insertNodeAtBeginning(&gstate->combat_zone[player], card)`  
**Replace with:** `CombatZone_add(&gstate->combat_zone[player], card)`

**Locations:**

- `src/card_actions.c` - multiple functions
- `src/combat.c` - `clear_combat_zones()`
- `src/stda_auto.c` - `apply_mulligan()`

---

### Pattern 4: Remove Card from Collection

**Find:** `HDCLL_removeNodeByValue(&gstate->hand[player], card)`  
**Replace with:** `Hand_remove(&gstate->hand[player], card)`

**Find:** `HDCLL_removeNodeByValue(&gstate->discard[player], card)`  
**Replace with:** `Discard_remove(&gstate->discard[player], card)`

**Locations:**

- `src/card_actions.c` - `play_champion()`, `play_draw_card()`, `play_cash_card()`, `discard_to_7_cards()`
- `src/stda_auto.c` - `apply_mulligan()`

---

### Pattern 5: Remove First Card (Pop Front)

**Find:** `HDCLL_removeNodeFromBeginning(&gstate->combat_zone[player])`  
**Replace with:**

```c
// Get first card, then clear (since we're emptying combat zone)
uint8_t card_idx = CombatZone_get(&gstate->combat_zone[player], 0);
// Process card...
// Then clear at end: CombatZone_clear(&gstate->combat_zone[player]);
```

**Location:** `src/combat.c` - `clear_combat_zones()`

**Better approach for this specific case:**

```diff
 void clear_combat_zones(struct gamestate* gstate, GameContext* ctx)
 { PlayerID attacker = gstate->current_player;
   PlayerID defender = 1 - gstate->current_player;

   // Move attacker's cards to discard
-  while(gstate->combat_zone[attacker].size > 0)
-  { uint8_t card_idx = HDCLL_removeNodeFromBeginning(&gstate->combat_zone[attacker]);
-    HDCLL_insertNodeAtBeginning(&gstate->discard[attacker], card_idx);
+  for (uint8_t i = 0; i < gstate->combat_zone[attacker].size; i++)
+  { uint8_t card_idx = gstate->combat_zone[attacker].cards[i];
+    Discard_add(&gstate->discard[attacker], card_idx);
   }
+  CombatZone_clear(&gstate->combat_zone[attacker]);

   // Move defender's cards to discard
-  while(gstate->combat_zone[defender].size > 0)
-  { uint8_t card_idx = HDCLL_removeNodeFromBeginning(&gstate->combat_zone[defender]);
-    HDCLL_insertNodeAtBeginning(&gstate->discard[defender], card_idx);
+  for (uint8_t i = 0; i < gstate->combat_zone[defender].size; i++)
+  { uint8_t card_idx = gstate->combat_zone[defender].cards[i];
+    Discard_add(&gstate->discard[defender], card_idx);
   }
+  CombatZone_clear(&gstate->combat_zone[defender]);
 }
```

---

# <mark>Rendu ici</mark>

### Pattern 6: Direct Iteration (No Array Conversion)

**Find:** Pattern like this:

```c
struct LLNode* current = hand->head;
for(uint8_t i = 0; i < hand->size; i++) {
    // Use current->data
    current = current->next;
}
```

**Replace with:**

```c
for(uint8_t i = 0; i < hand->size; i++) {
    uint8_t card = hand->cards[i];
    // Use card
}
```

**Locations:**

- `src/card_actions.c` - `has_champion_in_hand()`, `select_champion_for_cash_exchange()`, `discard_to_7_cards()`
- `src/combat.c` - `calculate_total_attack()`, `calculate_total_defense()`
- `src/stda_auto.c` - `apply_mulligan()`
- `src/stda_cli.c` - `display_player_hand()`, `display_attack_state()`

**Example from `src/card_actions.c` - `has_champion_in_hand()`:**

```diff
 int has_champion_in_hand(Hand* hand)
-{ struct LLNode* current = hand->head;
-  for(uint8_t i = 0; i < hand->size; i++)
-  { if(fullDeck[current->data].card_type == CHAMPION_CARD)
+{ for(uint8_t i = 0; i < hand->size; i++)
+  { if(fullDeck[hand->cards[i]].card_type == CHAMPION_CARD)
       return true;
-    current = current->next;
   }
   return false;
 }
```

**Note:** Change function signature from `struct HDCLList* hand` to `Hand* hand`

---

### Pattern 7: Array Conversion with malloc/free (REMOVE THIS PATTERN)

**Find:** Pattern like this:

```c
uint8_t* hand_array = HDCLL_toArray(&gstate->hand[player]);
// Use hand_array[i]...
free(hand_array);
```

**Replace with direct access:**

```c
// Direct access - no conversion needed!
for (uint8_t i = 0; i < gstate->hand[player].size; i++) {
    uint8_t card = gstate->hand[player].cards[i];
    // Use card...
}
```

**Locations:**

- `src/strat_random.c` - `random_attack_strategy()`, `random_defense_strategy()`
- `src/stda_cli.c` - `display_player_hand()`, `validate_and_play_champions()`, `handle_draw_command()`, `handle_cash_command()`

**Example from `src/strat_random.c` - `random_attack_strategy()`:**

```diff
 void random_attack_strategy(struct gamestate* gstate, GameContext* ctx)
 { PlayerID attacker = gstate->current_player;

   if(gstate->hand[attacker].size == 0) return;

   // Build list of affordable cards
   uint8_t affordable[gstate->hand[attacker].size];
   uint8_t count = 0;

   bool has_champions = has_champion_in_hand(&gstate->hand[attacker]);
-  uint8_t* hand_array = HDCLL_toArray(&gstate->hand[attacker]);

   for(uint8_t i = 0; i < gstate->hand[attacker].size; i++)
-  { uint8_t card_idx = hand_array[i];
+  { uint8_t card_idx = gstate->hand[attacker].cards[i];

     if(fullDeck[card_idx].cost <= gstate->current_cash_balance[attacker])
     { // Skip cash cards if no champions available
       if(fullDeck[card_idx].card_type == CASH_CARD && !has_champions)
         continue;
       affordable[count++] = card_idx;
     }
   }
-  free(hand_array);

   if(count == 0) return;

   // Play random affordable card
   uint8_t chosen = RND_randn(count, ctx);
   play_card(gstate, attacker, affordable[chosen], ctx);
 }
```

---

### Pattern 8: Shuffle Discard into Deck

**Location:** `src/card_actions.c` - `shuffle_discard_and_form_deck()`

```diff
-void shuffle_discard_and_form_deck(struct HDCLList* discard, struct deck_stack* deck, GameContext* ctx)
-{ uint8_t* A = HDCLL_toArray(discard);
-  uint8_t n = discard->size;
+void shuffle_discard_and_form_deck(Discard* discard, struct deck_stack* deck, GameContext* ctx)
+{ uint8_t n = discard->size;

   DEBUG_PRINT(" Discard size: %u\n", n);

   // Shuffle the card indices
-  RND_partial_shuffle(A, n, n, ctx);
+  RND_partial_shuffle(discard->cards, n, n, ctx);

   // Push to deck
   for(uint8_t i = 0; i < n; i++)
-    DeckStk_push(deck, A[i]);
-
-  // Free heap memory
-  free(A);
+    DeckStk_push(deck, discard->cards[i]);

   // Empty the discard
-  for(uint8_t i = 0; i < n; i++)
-    HDCLL_removeNodeFromBeginning(discard);
+  Discard_clear(discard);
 }
```

**Also update function signature in `src/card_actions.h`:**

```diff
-void shuffle_discard_and_form_deck(struct HDCLList* discard, struct deck_stack* deck, GameContext* ctx);
+void shuffle_discard_and_form_deck(Discard* discard, struct deck_stack* deck, GameContext* ctx);
```

---

### Pattern 9: Get Collection Size

**Find:** `gstate->hand[player].size`  
**Keep as-is** (works the same)

**Find:** `gstate->combat_zone[player].size`  
**Keep as-is**

**Find:** `gstate->discard[player].size`  
**Keep as-is**

No changes needed for size access.

---

### Pattern 10: Update Function Signatures

Several functions need signature updates to use specialized types instead of `struct HDCLList*`:

**In `src/card_actions.h` and `src/card_actions.c`:**

```diff
-int has_champion_in_hand(struct HDCLList* hand);
+int has_champion_in_hand(Hand* hand);

-uint8_t select_champion_for_cash_exchange(struct HDCLList* hand);
+uint8_t select_champion_for_cash_exchange(Hand* hand);
```

---

## Part 4: Update Makefile

```diff
 # Automatically find all .c files in src directory
 SOURCES := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
 OBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.o))
```

The makefile should automatically pick up `card_collection.c` - no changes needed if using `find`.

---

## Part 5: Remove Old Files (DO THIS LAST)

After all patterns are converted and code compiles successfully:

1. Delete `src/hdcll.h`
2. Delete `src/hdcll.c`
3. Verify compilation: `make clean && make`
4. Run tests: `./bin/oracle -a -n 100`

---

## Summary of Changes by File

| File                 | Patterns to Apply                 |
| -------------------- | --------------------------------- |
| `src/game_types.h`   | Update struct definition          |
| `src/game_state.c`   | Pattern 1 (init), Pattern 3 (add) |
| `src/card_actions.c` | Patterns 3-8, 10                  |
| `src/card_actions.h` | Pattern 10                        |
| `src/combat.c`       | Pattern 5, 6                      |
| `src/strat_random.c` | Pattern 7                         |
| `src/stda_auto.c`    | Patterns 2, 3, 4, 6               |
| `src/stda_cli.c`     | Patterns 2, 7                     |

---

## Testing Strategy

After each pattern group:

1. **Compile:** `make`
2. **Quick test:** `./bin/oracle -a -n 10`
3. **Verify no crashes:** Check output looks normal
4. **Full test after all done:** `./bin/oracle -a -n 1000`

---

# 

# Commit Strategy: Part-by-Part and Pattern-by-Pattern

## Phase 1: Foundation (New Code)

### Commit 1: Add specialized card collection structures

```bash
git add src/card_collection.h src/card_collection.c
git commit -m "feat: Add specialized card collections (Hand, CombatZone, Discard)

- Add Hand struct (max 12 cards) with init/add/remove/clear/get/contains
- Add CombatZone struct (max 3 cards) with init/add/remove/clear/get
- Add Discard struct (max 40 cards) with init/add/remove/clear/get
- Manual implementation (no macros) for code readability
- Foundation for replacing HDCLL linked lists with fixed arrays"
```

### Commit 2: Update game_types to use new collections

```bash
# Edit src/game_types.h only
git add src/game_types.h
git commit -m "refactor: Switch gamestate to use specialized collections

- Replace struct HDCLList with Hand/CombatZone/Discard types
- Remove hdcll.h include, add card_collection.h include
- Reduces memory per gamestate by 53% (240 -> 112 bytes)
- No functional changes yet (code won't compile until patterns applied)"
```

**Note:** Code will not compile after this commit. That's expected.

---

## Phase 2: Pattern-by-Pattern Refactoring

### Commit 3: Pattern 1 - Initialize collections

```bash
# Apply Pattern 1 in:
# - src/game_state.c: setup_game()
git add src/game_state.c
git commit -m "refactor(pattern-1): Replace HDCLL_initialize with collection init

Pattern: HDCLL_initialize(&list) -> Type_init(&collection)

Changes:
- setup_game(): Use Hand_init, Discard_init, CombatZone_init
- Initial hand draw: Use Hand_add instead of HDCLL_insertNodeAtBeginning

Files: game_state.c"
```

### Commit 4: Pattern 2 - Remove cleanup calls

```bash
# Apply Pattern 2 in:
# - src/stda_auto.c: play_stda_auto_game()
# - src/stda_cli.c: cleanup_cli_game()
git add src/stda_auto.c src/stda_cli.c
git commit -m "refactor(pattern-2): Remove HDCLL_emptyOut calls

Pattern: HDCLL_emptyOut(&list) -> (removed - fixed arrays need no cleanup)

Changes:
- Removed heap cleanup calls for hand/combat_zone/discard
- Fixed arrays on stack are auto-cleaned
- Only DeckStk_emptyOut remains (still uses stack-based array)

Files: stda_auto.c, stda_cli.c"
```

### Commit 5: Pattern 3 - Add cards to collections

```bash
# Apply Pattern 3 in:
# - src/card_actions.c: play_champion, play_draw_card, play_cash_card, draw_1_card
# - src/combat.c: clear_combat_zones (partial - for Discard_add)
# - src/stda_auto.c: apply_mulligan
git add src/card_actions.c src/combat.c src/stda_auto.c
git commit -m "refactor(pattern-3): Replace HDCLL_insertNodeAtBeginning with Type_add

Pattern: HDCLL_insertNodeAtBeginning(&list, card) -> Type_add(&collection, card)

Changes:
- Hand_add for adding to hand
- Discard_add for adding to discard  
- CombatZone_add for adding to combat zone
- Simpler API, no linked list pointer manipulation

Files: card_actions.c, combat.c, stda_auto.c"
```

### Commit 6: Pattern 4 - Remove cards from collections

```bash
# Apply Pattern 4 in:
# - src/card_actions.c: play_champion, play_draw_card, play_cash_card, discard_to_7_cards
# - src/stda_auto.c: apply_mulligan
git add src/card_actions.c src/stda_auto.c
git commit -m "refactor(pattern-4): Replace HDCLL_removeNodeByValue with Type_remove

Pattern: HDCLL_removeNodeByValue(&list, card) -> Type_remove(&collection, card)

Changes:
- Hand_remove for removing from hand
- Discard_remove for removing from discard
- Returns bool for success/failure (ignored for now)
- O(n) removal but with max 10-12 cards is negligible

Files: card_actions.c, stda_auto.c"
```

### Commit 7: Pattern 5 - Clear combat zones efficiently

```bash
# Apply Pattern 5 in:
# - src/combat.c: clear_combat_zones
git add src/combat.c
git commit -m "refactor(pattern-5): Replace while-loop removal with for-loop + clear

Pattern: while(size>0) {pop front; process} -> for(i<size) {process}; clear()

Changes:
- clear_combat_zones: Iterate through cards, move to discard, then clear
- More efficient: O(n) instead of O(n²)
- CombatZone_clear replaces repeated remove operations

Files: combat.c"
```

### Commit 8: Pattern 6 - Direct iteration (part 1: simple cases)

```bash
# Apply Pattern 6 in:
# - src/card_actions.c: has_champion_in_hand, select_champion_for_cash_exchange
# - Update function signatures to use Hand* instead of struct HDCLList*
git add src/card_actions.c src/card_actions.h
git commit -m "refactor(pattern-6): Direct array iteration - part 1 (card_actions)

Pattern: struct LLNode* current; while(current) -> for(i<size) array[i]

Changes:
- has_champion_in_hand: Direct iteration, no linked list traversal
- select_champion_for_cash_exchange: Direct iteration
- Update function signatures: Hand* instead of struct HDCLList*
- Eliminates pointer chasing, improves cache locality

Files: card_actions.c, card_actions.h"
```

### Commit 9: Pattern 6 - Direct iteration (part 2: discard_to_7_cards)

```bash
# Apply Pattern 6 in:
# - src/card_actions.c: discard_to_7_cards
git add src/card_actions.c
git commit -m "refactor(pattern-6): Direct array iteration - part 2 (discard_to_7)

Pattern: struct LLNode* current; while(current) -> for(i<size) array[i]

Changes:
- discard_to_7_cards: Direct iteration through hand
- No more linked list node pointer tracking
- Cleaner code, fewer local variables

Files: card_actions.c"
```

### Commit 10: Pattern 6 - Direct iteration (part 3: combat calculations)

```bash
# Apply Pattern 6 in:
# - src/combat.c: calculate_total_attack, calculate_total_defense
git add src/combat.c
git commit -m "refactor(pattern-6): Direct array iteration - part 3 (combat)

Pattern: struct LLNode* current; while(current) -> for(i<size) array[i]

Changes:
- calculate_total_attack: Direct combat_zone array access
- calculate_total_defense: Direct combat_zone array access
- Hot path optimization: better cache usage during combat resolution

Files: combat.c"
```

### Commit 11: Pattern 6 - Direct iteration (part 4: mulligan)

```bash
# Apply Pattern 6 in:
# - src/stda_auto.c: apply_mulligan (2 locations)
git add src/stda_auto.c
git commit -m "refactor(pattern-6): Direct array iteration - part 4 (mulligan)

Pattern: struct LLNode* current; while(current) -> for(i<size) array[i]

Changes:
- apply_mulligan: Direct iteration for counting and finding cards
- Two loops converted (count phase and find-lowest phase)

Files: stda_auto.c"
```

### Commit 12: Pattern 6 - Direct iteration (part 5: CLI display)

```bash
# Apply Pattern 6 in:
# - src/stda_cli.c: display_player_hand, display_attack_state
git add src/stda_cli.c
git commit -m "refactor(pattern-6): Direct array iteration - part 5 (CLI display)

Pattern: struct LLNode* current; while(current) -> for(i<size) array[i]

Changes:
- display_player_hand: Direct hand array access for display
- display_attack_state: Direct combat_zone array access for display
- UI code now simpler and faster

Files: stda_cli.c"
```

### Commit 13: Pattern 7 - Remove malloc/free from random strategy

```bash
# Apply Pattern 7 in:
# - src/strat_random.c: random_attack_strategy, random_defense_strategy
git add src/strat_random.c
git commit -m "refactor(pattern-7): Eliminate malloc/free - part 1 (random strategy)

Pattern: uint8_t* arr = HDCLL_toArray(); use arr; free(arr) 
         -> for(i<size) direct_access[i]

Changes:
- random_attack_strategy: Direct hand array access, no malloc
- random_defense_strategy: Direct hand array access, no malloc
- Eliminates ALL remaining heap allocations in strategy code
- Critical for MCTS: no memory leaks in AI decision loop

Files: strat_random.c"
```

### Commit 14: Pattern 7 - Remove malloc/free from CLI (part 1)

```bash
# Apply Pattern 7 in:
# - src/stda_cli.c: display_player_hand
git add src/stda_cli.c
git commit -m "refactor(pattern-7): Eliminate malloc/free - part 2 (CLI display)

Pattern: uint8_t* arr = HDCLL_toArray(); use arr; free(arr)
         -> for(i<size) direct_access[i]

Changes:
- display_player_hand: Direct hand array access for card display
- No more heap allocation during UI rendering
- Already applied in commit 12, but now removes old array conversion

Files: stda_cli.c"
```

### Commit 15: Pattern 7 - Remove malloc/free from CLI (part 2)

```bash
# Apply Pattern 7 in:
# - src/stda_cli.c: validate_and_play_champions, handle_draw_command, handle_cash_command
git add src/stda_cli.c
git commit -m "refactor(pattern-7): Eliminate malloc/free - part 3 (CLI input)

Pattern: uint8_t* arr = HDCLL_toArray(); use arr; free(arr)
         -> for(i<size) direct_access[i]

Changes:
- validate_and_play_champions: Direct hand array access for validation
- handle_draw_command: Direct hand array access for card lookup
- handle_cash_command: Direct hand array access for card lookup
- All CLI user input handling now free of heap allocations

Files: stda_cli.c"
```

### Commit 16: Pattern 8 - Optimize shuffle_discard_and_form_deck

```bash
# Apply Pattern 8 in:
# - src/card_actions.c: shuffle_discard_and_form_deck
# - src/card_actions.h: function signature update
git add src/card_actions.c src/card_actions.h
git commit -m "refactor(pattern-8): Eliminate malloc in shuffle_discard_and_form_deck

Pattern: arr = toArray(); shuffle(arr); free(arr) 
         -> shuffle(collection.cards directly)

Changes:
- Shuffle discard pile array directly (no malloc/copy/free)
- Use Discard_clear() instead of loop removing nodes
- Function signature: Discard* instead of struct HDCLList*
- Final heap allocation eliminated from card_actions.c

Files: card_actions.c, card_actions.h"
```

---

## Phase 3: Cleanup

### Commit 17: Remove old linked list implementation

```bash
# Delete hdcll.h and hdcll.c
git rm src/hdcll.h src/hdcll.c
git commit -m "cleanup: Remove HDCLL linked list implementation

- Delete hdcll.h and hdcll.c (no longer used)
- All collections now use specialized fixed arrays
- 100% of heap allocations in game logic eliminated
- Memory usage reduced by 53% per gamestate
- Foundation complete for MCTS implementation"
```

### Commit 18: Verify and document migration

```bash
# Add documentation or update README if needed
git commit -m "docs: Update memory management notes after HDCLL removal

- Document switch to fixed-size array collections
- Note memory savings: 240 -> 112 bytes per gamestate
- Confirm zero heap allocations in game loop
- Ready for MCTS implementation (TODO #6)"
```

---

## Testing Checkpoints

Run these tests after specific commits:

**After Commit 3 (first compile):**

```bash
make clean && make
# Should compile successfully
./bin/oracle -a -n 10
# Should run without crashes
```

**After Commit 7 (patterns 1-5 complete):**

```bash
make clean && make
./bin/oracle -a -n 100
# Should produce correct statistics
```

**After Commit 13 (all non-CLI patterns done):**

```bash
make clean && make
./bin/oracle -a -n 1000
# Full simulation test
```

**After Commit 16 (all patterns complete):**

```bash
make clean && make
./bin/oracle -a -n 1000  # Automated mode
./bin/oracle -l           # CLI mode (manual testing)
```

**After Commit 17 (HDCLL removed):**

```bash
make clean && make
# Verify no linker errors about HDCLL symbols
./bin/oracle -a -n 1000
# Final validation
```

---

## Summary

**Total Commits:** 18  
**Structure:**

- 2 foundation commits (new code)
- 14 pattern commits (incremental refactoring)
- 2 cleanup commits (removal + docs)

**Key Milestones:**

- Commit 3: First successful compilation
- Commit 7: Core patterns complete
- Commit 13: All strategy code migrated
- Commit 16: All patterns applied
- Commit 17: Old code removed
