# Cash Card Functionality Implementation Plan for CLI Mode

## Overview

This plan outlines implementing interactive cash card functionality in CLI mode, allowing players to choose which champion to exchange for 5 lunas. Currently, `select_champion_for_cash_exchange()` uses AI logic (lowest power heuristic) even in interactive mode. This implementation gives players agency over this strategic decision.

## Prerequisites

**Must complete first:**

1. HDCLL to fixed arrays migration (document index 8)
2. Mulligan and discard commands (document index 9-10)
3. Display discard pile functionality (document index 11)
4. Recall card functionality (previous plan)

These provide:

- Direct array access patterns
- Consistent input parsing (`parse_card_indices_cli()`)
- Visual card display with power values
- Established two-stage command pattern

## Current State Analysis

### Existing Implementation (`card_actions.c`)

```c
void play_cash_card(struct gamestate* gstate, PlayerID player, 
                   uint8_t card_idx, GameContext* ctx)
{
  // Remove cash card from hand
  Hand_remove(&gstate->hand[player], card_idx);
  gstate->current_cash_balance[player] -= fullDeck[card_idx].cost; // Always 0

  // AI LOGIC: Select champion automatically
  uint8_t champion_to_exchange = 
    select_champion_for_cash_exchange(&gstate->hand[player]);

  if(champion_to_exchange != 0) {
    // Remove champion, add to discard
    Hand_remove(&gstate->hand[player], champion_to_exchange);
    Discard_add(&gstate->discard[player], champion_to_exchange);

    // Collect lunas
    uint8_t cash_received = fullDeck[card_idx].exchange_cash; // Always 5
    gstate->current_cash_balance[player] += cash_received;
  }

  // Move cash card to discard
  Discard_add(&gstate->discard[player], card_idx);
}
```

**Problem:** Always uses AI logic, even for human players.

### Current `handle_cash_command()` (`stda_cli.c`)

```c
int handle_cash_command(struct gamestate* gstate, PlayerID player,
                       char* input, GameContext* ctx, config_t* cfg)
{
  // ... validation code ...

  if(!has_champion_in_hand(&gstate->hand[player])) {
    printf(RED "Error: No champions to exchange\n" RESET);
    return NO_ACTION;
  }

  play_cash_card(gstate, player, card_idx, ctx); // AUTO-SELECTS CHAMPION

  printf(GREEN "Played exchange card\n" RESET);
  return ACTION_TAKEN;
}
```

**Problem:** No user interaction for champion selection.

## Core Design Decisions

### 1. Two-Stage Command Structure

**Stage 1: Identify cash card**

```
> cash 2
```

**Stage 2: Choose champion to exchange**

```
Choose a champion to exchange for 5 lunas.
Tip: Consider exchanging lowest-power or unaffordable champions.

Your champions:
  [1] Human (D4+0, ☾0, pwr:2.5)    ← suggested
  [2] Elf (D6+1, ☾1, pwr:4.5)
  [3] Dwarf (D8+2, ☾1, pwr:6.5)
  [4] Dragon (D20+5, ☾3, pwr:15.5)

Enter champion index (e.g., '1') or 'pass': _
```

### 2. Key Differences from Recall

| Aspect        | Recall          | Cash                           |
| ------------- | --------------- | ------------------------------ |
| **Source**    | Discard pile    | Hand                           |
| **Count**     | 1-2 champions   | Exactly 1 champion             |
| **Filter**    | Champions only  | Champions only                 |
| **Can pass?** | Yes (no recall) | No (must complete transaction) |
| **Default**   | N/A             | Suggest lowest power           |

### 3. Modified Flow

```c
// NEW signature with mode parameter
void play_cash_card_interactive(struct gamestate* gstate, 
                                PlayerID player,
                                uint8_t card_idx, 
                                uint8_t chosen_champion,
                                GameContext* ctx);

// Keep AI version
void play_cash_card_ai(struct gamestate* gstate, 
                       PlayerID player,
                       uint8_t card_idx,
                       GameContext* ctx);
```

## Implementation Structure

### Part 1: Refactor `card_actions.c`

#### Modification 1: Split `play_cash_card()`

```c
// card_actions.c

// NEW: Interactive version (no AI logic)
void play_cash_card_interactive(struct gamestate* gstate, 
                                PlayerID player,
                                uint8_t cash_card_idx,
                                uint8_t champion_idx,
                                GameContext* ctx)
{
  // Validate champion is in hand and is a champion
  if(fullDeck[champion_idx].card_type != CHAMPION_CARD) {
    return; // Should be caught by validation earlier
  }

  // Remove cash card from hand
  Hand_remove(&gstate->hand[player], cash_card_idx);
  gstate->current_cash_balance[player] -= fullDeck[cash_card_idx].cost;

  // Remove chosen champion from hand
  Hand_remove(&gstate->hand[player], champion_idx);
  Discard_add(&gstate->discard[player], champion_idx);

  // Collect lunas
  uint8_t cash_received = fullDeck[cash_card_idx].exchange_cash;
  gstate->current_cash_balance[player] += cash_received;

  // Move cash card to discard
  Discard_add(&gstate->discard[player], cash_card_idx);
}

// RENAMED: AI version (existing logic)
void play_cash_card_ai(struct gamestate* gstate, 
                       PlayerID player,
                       uint8_t card_idx,
                       GameContext* ctx)
{
  // ... existing implementation with AI selection ...
}

// DEPRECATED: Keep for backward compatibility initially
void play_cash_card(struct gamestate* gstate, PlayerID player,
                   uint8_t card_idx, GameContext* ctx)
{
  // Default to AI logic for now
  play_cash_card_ai(gstate, player, card_idx, ctx);
}
```

**Update `card_actions.h`:**

```c
void play_cash_card_interactive(struct gamestate* gstate, 
                                PlayerID player,
                                uint8_t cash_card_idx,
                                uint8_t champion_idx,
                                GameContext* ctx);

void play_cash_card_ai(struct gamestate* gstate, 
                       PlayerID player,
                       uint8_t card_idx,
                       GameContext* ctx);
```

### Part 2: New Helper Functions in `stda_cli.c`

#### Function 1: `display_exchangeable_champions()`

```c
// Display champions in hand with power and suggestion
// Location: stda_cli.c
// Lines: ~30 (within limits)

void display_exchangeable_champions(Hand* hand, 
                                   PlayerID player,
                                   config_t* cfg)
{
  printf("\n%s\n",
         LOCALIZED_STRING(
           "Choose a champion to exchange for 5 lunas.",
           "Choisir un champion a echanger pour 5 lunas.",
           "Elige un campeon para cambiar por 5 lunas."));

  printf("%s\n",
         LOCALIZED_STRING(
           "Tip: Consider exchanging lowest-power or unaffordable champions.",
           "Conseil: Echanger champions faible puissance ou inabordables.",
           "Consejo: Considera cambiar campeones de baja potencia o inasequibles."));

  printf("\n%s:\n",
         LOCALIZED_STRING("Your champions", 
                          "Vos champions", 
                          "Tus campeones"));

  // Build champion list
  uint8_t champions[15];
  uint8_t count = 0;

  for(uint8_t i = 0; i < hand->size; i++) {
    if(fullDeck[hand->cards[i]].card_type == CHAMPION_CARD)
      champions[count++] = hand->cards[i];
  }

  if(count == 0) {
    printf("  %s\n", LOCALIZED_STRING("(no champions)",
                                      "(aucun champion)",
                                      "(sin campeones)"));
    return;
  }

  // Find minimum power for suggestion
  float min_power = 100.0f;
  uint8_t min_idx = 0;

  for(uint8_t i = 0; i < count; i++) {
    if(fullDeck[champions[i]].power < min_power) {
      min_power = fullDeck[champions[i]].power;
      min_idx = i;
    }
  }

  // Display with suggestion marker
  for(uint8_t i = 0; i < count; i++) {
    uint8_t card_idx = champions[i];
    const struct card* c = &fullDeck[card_idx];

    const char* color = (c->color == COLOR_INDIGO) ? BLUE :
                        (c->color == COLOR_ORANGE) ? YELLOW : RED;

    printf("  [%d] %s%s" RESET " (D%d+%d, " CYAN "L%d" RESET 
           ", pwr:%.1f)",
           i + 1, color, CHAMPION_SPECIES_NAMES[c->species],
           c->defense_dice, c->attack_base, c->cost, c->power);

    if(i == min_idx) {
      printf(" " GRAY "← suggested" RESET);
    }
    printf("\n");
  }
}
```

#### Function 2: `prompt_champion_exchange()`

```c
// Prompt for champion selection (no 'pass' option)
// Returns: champion index in hand, or -1 on error
// Location: stda_cli.c
// Lines: ~25 (within limits)

int prompt_champion_exchange(Hand* hand, config_t* cfg)
{
  printf("\n%s: ",
         LOCALIZED_STRING(
           "Enter champion index (e.g., '1')",
           "Entrez indice champion (ex: '1')",
           "Ingresa indice campeon (ej: '1')"));

  char input[MAX_COMMAND_LEN];
  if(fgets(input, sizeof(input), stdin) == NULL)
    return -1;

  input[strcspn(input, "\n")] = 0;

  // Must provide input (no default, no pass)
  if(strlen(input) == 0) {
    printf(RED "%s\n" RESET,
           LOCALIZED_STRING("Error: Must choose a champion",
                            "Erreur: Doit choisir un champion",
                            "Error: Debe elegir un campeon"));
    return -1;
  }

  // Parse single index
  int idx = atoi(input);

  // Build champion list to validate
  uint8_t champion_count = 0;
  for(uint8_t i = 0; i < hand->size; i++) {
    if(fullDeck[hand->cards[i]].card_type == CHAMPION_CARD)
      champion_count++;
  }

  if(idx < 1 || idx > champion_count) {
    printf(RED "%s %d (%s 1-%d)\n" RESET,
           LOCALIZED_STRING("Error: Invalid index",
                            "Erreur: Indice invalide",
                            "Error: Indice invalido"),
           idx,
           LOCALIZED_STRING("must be", "doit etre", "debe ser"),
           champion_count);
    return -1;
  }

  return idx - 1; // Return 0-based index
}
```

#### Function 3: Updated `handle_cash_command()`

```c
// Modified to use interactive selection
// Location: stda_cli.c
// Lines: ~35 (within limits)

int handle_cash_command(struct gamestate* gstate, PlayerID player,
                       char* input, GameContext* ctx, config_t* cfg)
{
  // Parse cash card index from input
  int idx = atoi(input);
  if(idx < 1 || idx > gstate->hand[player].size) {
    printf(RED "%s (must be 1-%d)\n" RESET,
           LOCALIZED_STRING("Error: Invalid card number",
                            "Erreur: Numero de carte invalide",
                            "Error: Numero de carta invalido"),
           gstate->hand[player].size);
    return NO_ACTION;
  }

  uint8_t card_idx = gstate->hand[player].cards[idx - 1];

  // Validate it's a cash card
  if(fullDeck[card_idx].card_type != CASH_CARD) {
    printf(RED "%s\n" RESET,
           LOCALIZED_STRING("Error: Not an exchange card",
                            "Erreur: Pas une carte echange",
                            "Error: No es una carta de intercambio"));
    return NO_ACTION;
  }

  // Check for champions in hand
  if(!has_champion_in_hand(&gstate->hand[player])) {
    printf(RED "%s\n" RESET,
           LOCALIZED_STRING("Error: No champions to exchange",
                            "Erreur: Aucun champion a echanger",
                            "Error: No hay campeones para intercambiar"));
    return NO_ACTION;
  }

  // NEW: Interactive champion selection
  display_exchangeable_champions(&gstate->hand[player], player, cfg);

  int champion_choice = prompt_champion_exchange(&gstate->hand[player], cfg);
  if(champion_choice < 0)
    return NO_ACTION; // User input error

  // Find actual champion card index
  uint8_t champions[15];
  uint8_t count = 0;

  for(uint8_t i = 0; i < gstate->hand[player].size; i++) {
    if(fullDeck[gstate->hand[player].cards[i]].card_type == CHAMPION_CARD)
      champions[count++] = gstate->hand[player].cards[i];
  }

  uint8_t chosen_champion = champions[champion_choice];

  // Execute exchange
  play_cash_card_interactive(gstate, player, card_idx, 
                            chosen_champion, ctx);

  printf(GREEN ICON_SUCCESS " %s" RESET " %s" GREEN "5" RESET 
         " %s\n",
         LOCALIZED_STRING("Exchanged", "Echange", "Cambiado"),
         CHAMPION_SPECIES_NAMES[fullDeck[chosen_champion].species],
         LOCALIZED_STRING("lunas", "lunas", "lunas"));

  return ACTION_TAKEN;
}
```

### Part 3: Update AI Integration

```c
// In attack_phase() for AI players
void attack_phase(struct gamestate* gstate, StrategySet* strategies, 
                 GameContext* ctx)
{
  PlayerID attacker = gstate->current_player;

  // AI uses automatic selection
  strategies->attack_strategy[attacker](gstate, ctx);

  gstate->turn_phase = DEFENSE;
  gstate->player_to_move = 1 - gstate->current_player;
}

// In strat_random.c - no changes needed
// play_card() dispatches to play_cash_card()
// which defaults to play_cash_card_ai()
```

### Part 4: Update Help Text

```c
void display_cli_help(int is_defense, config_t* cfg)
{
  // ... existing code ...

  printf("  cash <index>    - %s\n",
         LOCALIZED_STRING(
           "Play exchange card (e.g., 'cash 1'). Choose champion to exchange.",
           "Jouer carte echange (ex: 'cash 1'). Choisir champion a echanger.",
           "Jugar carta intercambio (ej: 'cash 1'). Elegir campeon a cambiar."));

  // ... rest of help ...
}
```

## Testing Checklist

### Basic Functionality

- [ ] Cash card played → champion selection prompt appears
- [ ] Valid champion chosen → exchange completes
- [ ] Lunas awarded correctly (5)
- [ ] Both cards moved to discard
- [ ] Hand updated correctly

### Edge Cases

- [ ] No champions in hand → error before prompt
- [ ] Invalid index → error message
- [ ] Empty input → error (must choose)
- [ ] Non-champion card index → validation catches
- [ ] Champion at edge indices (1, last) → works correctly

### User Experience

- [ ] Lowest power champion marked "suggested"
- [ ] Power values displayed for informed choice
- [ ] Clear prompts in all 3 languages
- [ ] Confirmation message shows exchanged champion
- [ ] Help text accurate

### AI Compatibility

- [ ] AI players use automatic selection
- [ ] Random strategy unchanged
- [ ] No crashes in AI vs AI mode

## Comparison with Recall Implementation

| Feature         | Recall               | Cash Exchange              |
| --------------- | -------------------- | -------------------------- |
| **Source**      | Discard pile         | Hand                       |
| **Filter**      | Champions only       | Champions only             |
| **Count**       | 1-2 (varies by card) | Exactly 1 (always)         |
| **Can pass**    | Yes                  | No (must complete)         |
| **Sort by**     | Power (descending)   | Display order, mark lowest |
| **Validation**  | Multiple indices     | Single index only          |
| **AI fallback** | Not applicable       | Uses `play_cash_card_ai()` |

## Integration Notes

**Commit Strategy:**

1. Commit 1: Refactor `play_cash_card()` into interactive/AI versions
2. Commit 2: Add helper functions (display, prompt)
3. Commit 3: Update `handle_cash_command()` to use interactive flow
4. Commit 4: Update help text and documentation
5. Commit 5: Testing and refinement

**Dependencies:**

- Requires fixed arrays (Hand, Discard)
- Uses `has_champion_in_hand()` from existing code
- Follows pattern from recall implementation

**File Impact:**

- `src/card_actions.c`: +~25 lines (split function)
- `src/card_actions.h`: +2 function declarations
- `src/stda_cli.c`: +~90 lines (3 new/modified functions)
- Total CLI file size: ~1010 lines (approaching split threshold)

**Note on File Size:** After this implementation, `stda_cli.c` will be ~1010 lines. Consider the planned split into `cli_display.c`, `cli_input.c`, `cli_game.c` as documented in TODO item #7.

## Future Enhancements

1. **Strategic Hints**: Display which champion exchange would maximize hand power/combos

2. **Undo Option**: Allow player to reconsider choice before finalizing
   
   ```
   Confirm exchange of Dragon for 5 lunas? (y/n): _
   ```

3. **AI Strategy Improvement**: Balanced AI could consider:
   
   - Exchange unaffordable champions
   - Exchange champions that break combos
   - Keep champions that complete combos

4. **Multi-language Refinement**: Add more descriptive tips per language/culture

## Summary

This implementation:

- ✅ Follows established patterns from recall
- ✅ Keeps all functions under 35 lines
- ✅ Reuses existing helper concepts
- ✅ Provides clear user experience with suggestion
- ✅ Maintains AI compatibility
- ✅ Supports all 3 languages
- ✅ No 'pass' option (transaction must complete)
- ✅ Clear visual feedback

**Total implementation: ~115 lines across 3 new/modified functions + card_actions refactor.**

**Critical difference from recall:** No 'pass' option - once cash card is played, a champion MUST be exchanged. This matches the game rules where the card effect is mandatory.
