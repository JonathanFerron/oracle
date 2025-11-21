# Recall Card Functionality Implementation Plan for CLI Mode

## Overview

This plan outlines implementing recall functionality for draw/recall cards in CLI mode, allowing players to choose between drawing cards or recalling champions from their discard pile. The implementation follows the project's established patterns from mulligan/discard commands and builds upon the upcoming HDCLL-to-fixed-arrays migration.

## Prerequisites

**Must complete first:**

1. HDCLL to fixed arrays migration (document index 8)
2. Mulligan and discard commands (document index 9-10)
3. Display discard pile functionality (document index 11)

These provide the foundation for:

- Direct array access patterns (no `HDCLL_toArray()` + `free()`)
- Consistent user input parsing (`parse_card_indices_cli()`)
- Visual display of discard pile contents

## Core Design Decisions

### 1. Two-Stage Command Structure

**Stage 1: Choose action**

```
> draw 3
Do you want to (d)raw or (r)ecall? [d]: _
```

**Stage 2: If recall chosen**

```
Available champions in discard (sorted by power):
  [1] Dragon (D20+5, ☾3, pwr:15.5)
  [2] Elf (D20+4, ☾3, pwr:14.5)
  [3] Cyclops (D12+5, ☾3, pwr:11.5)
  ...

Recall up to 2 champion(s).
Enter indices (e.g., '1 3') or 'pass': _
```

### 2. Shared Helper Functions

Reuse existing patterns from mulligan/discard implementation:

```c
// From mulligan/discard (already implemented)
void display_card_with_index(uint8_t card_idx, int display_num, int show_power);
int parse_card_indices_cli(char* input, uint8_t* indices, 
                           int max_count, int hand_size);
```

**New helper needed:**

```c
// Display champions available for recall
void display_recallable_champions(Discard* discard, 
                                  PlayerID player);
```

### 3. Modified `handle_draw_command()`

Current function only handles drawing. Modify to:

```c
int handle_draw_command(struct gamestate* gstate, PlayerID player,
                       char* input, GameContext* ctx, config_t* cfg)
{
  // ... existing validation code ...

  if(fullDeck[card_idx].card_type != DRAW_CARD)
  { /* error message */ return NO_ACTION; }

  // NEW: Prompt for draw vs recall
  char choice = prompt_draw_or_recall(cfg);

  if(choice == 'd') {
    play_draw_card(gstate, player, card_idx, ctx);
  } else {
    int result = handle_recall_choice(gstate, player, card_idx, ctx, cfg);
    if(result != ACTION_TAKEN) 
      return NO_ACTION;
  }

  return ACTION_TAKEN;
}
```

## Implementation Structure

### Part 1: New Helper Functions

#### Function 1: `prompt_draw_or_recall()`

```c
// Prompt user for draw vs recall choice
// Returns: 'd' for draw, 'r' for recall
// Location: stda_cli.c
// Lines: ~15 (within limits)

char prompt_draw_or_recall(config_t* cfg)
{
  printf("\n%s\n",
         LOCALIZED_STRING(
           "Choose: (d)raw cards or (r)ecall champions? [d]: ",
           "Choisir: (p)iocher ou (r)appeler? [p]: ",
           "Elegir: (r)obar o (recuper)ar? [r]: "));

  char input[10];
  if(fgets(input, sizeof(input), stdin) == NULL)
    return 'd'; // Default to draw

  input[strcspn(input, "\n")] = 0;

  if(strlen(input) == 0) return 'd';

  char c = tolower(input[0]);
  return (c == 'r' || c == 'p' || c == 'a') ? 'r' : 'd';
}
```

#### Function 2: `display_recallable_champions()`

```c
// Display champions in discard pile for recall selection
// Location: stda_cli.c
// Lines: ~25 (within limits)

void display_recallable_champions(Discard* discard, 
                                  PlayerID player, 
                                  config_t* cfg)
{
  printf("\n%s:\n",
         LOCALIZED_STRING("Available champions in discard",
                          "Champions disponibles dans la defausse",
                          "Campeones disponibles en descarte"));

  // Count champions only
  uint8_t champion_count = 0;
  for(uint8_t i = 0; i < discard->size; i++) {
    if(fullDeck[discard->cards[i]].card_type == CHAMPION_CARD)
      champion_count++;
  }

  if(champion_count == 0) {
    printf("  %s\n", LOCALIZED_STRING("(no champions)",
                                      "(aucun champion)",
                                      "(sin campeones)"));
    return;
  }

  // Display sorted by power (descending)
  // Build temp array of champion indices
  uint8_t champions[40];
  uint8_t count = 0;

  for(uint8_t i = 0; i < discard->size; i++) {
    if(fullDeck[discard->cards[i]].card_type == CHAMPION_CARD)
      champions[count++] = discard->cards[i];
  }

  // Bubble sort by power (descending)
  for(uint8_t i = 0; i < count; i++) {
    for(uint8_t j = i + 1; j < count; j++) {
      if(fullDeck[champions[i]].power < fullDeck[champions[j]].power) {
        uint8_t temp = champions[i];
        champions[i] = champions[j];
        champions[j] = temp;
      }
    }
  }

  // Display with 1-based indices
  for(uint8_t i = 0; i < count; i++) {
    display_card_with_index(champions[i], i + 1, 1);
  }
}
```

#### Function 3: `handle_recall_choice()`

```c
// Process recall card selection
// Returns: ACTION_TAKEN or NO_ACTION
// Location: stda_cli.c
// Lines: ~30 (within limits)

int handle_recall_choice(struct gamestate* gstate, 
                        PlayerID player,
                        uint8_t card_idx, 
                        GameContext* ctx,
                        config_t* cfg)
{
  uint8_t recall_max = fullDeck[card_idx].choose_num;

  // Show available champions
  display_recallable_champions(&gstate->discard[player], player, cfg);

  printf("\n%s %d %s\n",
         LOCALIZED_STRING("Recall up to", "Rappeler jusqu'a", 
                          "Recuperar hasta"),
         recall_max,
         LOCALIZED_STRING("champion(s).", "champion(s).", "campeon(es)."));

  printf("%s: ",
         LOCALIZED_STRING("Enter indices (e.g., '1 3') or 'pass'",
                          "Entrez indices (ex: '1 3') ou 'pass'",
                          "Ingresa indices (ej: '1 3') o 'pass'"));

  char input[MAX_COMMAND_LEN];
  if(fgets(input, sizeof(input), stdin) == NULL)
    return NO_ACTION;

  input[strcspn(input, "\n")] = 0;

  // Handle pass
  if(strcmp(input, "pass") == 0) {
    printf(YELLOW "%s\n" RESET,
           LOCALIZED_STRING("No champions recalled",
                            "Aucun champion rappele",
                            "Ningun campeon recuperado"));
    return NO_ACTION;
  }

  // Parse indices
  uint8_t indices[2]; // Max 2 for recall
  int count = parse_card_indices_cli(input, indices, recall_max,
                                     /* champion_count from discard */);

  if(count <= 0) return NO_ACTION;

  // Validate and perform recall
  return validate_and_recall_champions(gstate, player, card_idx,
                                      indices, count, ctx, cfg);
}
```

#### Function 4: `validate_and_recall_champions()`

```c
// Validate selections and perform recall
// Location: stda_cli.c
// Lines: ~20 (within limits)

int validate_and_recall_champions(struct gamestate* gstate,
                                  PlayerID player,
                                  uint8_t draw_card_idx,
                                  uint8_t* indices,
                                  int count,
                                  GameContext* ctx,
                                  config_t* cfg)
{
  // Build champion list from discard
  uint8_t champions[40];
  uint8_t champ_count = 0;

  for(uint8_t i = 0; i < gstate->discard[player].size; i++) {
    if(fullDeck[gstate->discard[player].cards[i]].card_type == CHAMPION_CARD)
      champions[champ_count++] = gstate->discard[player].cards[i];
  }

  // Sort by power (same order as display)
  /* ... sorting code ... */

  // Remove draw card from hand
  Hand_remove(&gstate->hand[player], draw_card_idx);
  gstate->current_cash_balance[player] -= fullDeck[draw_card_idx].cost;
  Discard_add(&gstate->discard[player], draw_card_idx);

  // Recall champions
  for(int i = 0; i < count; i++) {
    uint8_t champion_idx = champions[indices[i]];
    Discard_remove(&gstate->discard[player], champion_idx);
    Hand_add(&gstate->hand[player], champion_idx);
  }

  printf(GREEN ICON_SUCCESS " %s %d %s\n" RESET,
         LOCALIZED_STRING("Recalled", "Rappele", "Recuperado"),
         count,
         LOCALIZED_STRING("champion(s)", "champion(s)", "campeon(es)"));

  return ACTION_TAKEN;
}
```

### Part 2: Update `card_actions.c`

Currently `play_draw_card()` only draws. No changes needed for CLI mode - the choice is made at the UI level. For AI modes, keep existing behavior (draw only until AI strategies implement recall logic).

### Part 3: Update Help Text

```c
void display_cli_help(int is_defense, config_t* cfg)
{
  // ... existing code ...

  printf("  draw <index>    - %s\n",
         LOCALIZED_STRING(
           "Play draw/recall card (e.g., 'draw 2'). Choose draw or recall.",
           "Jouer carte piocher/rappeler (ex: 'draw 2'). Choisir piocher ou rappeler.",
           "Jugar carta robar/recuperar (ej: 'draw 2'). Elegir robar o recuperar."));

  // ... rest of help ...
}
```

## Testing Checklist

### Basic Functionality

- [ ] Draw card played, choose draw → cards drawn correctly
- [ ] Draw card played, choose recall → recall prompt appears
- [ ] Recall 0 champions (pass) → no error
- [ ] Recall 1 champion → champion moved to hand
- [ ] Recall 2 champions (Draw-3 card) → both moved
- [ ] Discard pile updated correctly after recall

### Edge Cases

- [ ] Empty discard pile → shows "(no champions)"
- [ ] Discard with only non-champion cards → shows "(no champions)"
- [ ] Invalid champion index → error message
- [ ] Duplicate indices → validation catches
- [ ] Recall more than allowed → validation catches
- [ ] Not enough lunas → error before choice

### User Experience

- [ ] Default to draw if empty input
- [ ] Clear prompts in all 3 languages
- [ ] Power values displayed for informed choice
- [ ] Confirmation message after recall
- [ ] Help text updated and accurate

## Integration Notes

**Commit Strategy:**

1. Commit 1: Add helper functions (prompting, display, validation)
2. Commit 2: Modify `handle_draw_command()` to support recall
3. Commit 3: Update help text and localization
4. Commit 4: Testing and refinement

**Dependencies:**

- Requires fixed arrays (Part 1 of HDCLL migration)
- Uses `display_card_with_index()` from mulligan implementation
- Uses `parse_card_indices_cli()` pattern from mulligan

**File Impact:**

- `src/stda_cli.c`: +~120 lines (4 new functions)
- `src/stda_cli.h`: +4 function declarations
- Total file size: ~920 lines (within acceptable range)

## Future Enhancements

1. **AI Strategy Integration**: Once recall is working in CLI, add recall logic to AI strategies (could implement in random strategy initially just to test it: e.g. 20% chance to recall (if there are enough cards in the discard to recally for the draw/recall card being played) vs 80% chance to draw)

2. **Recall Heuristic**: For balanced AI, recall high-power champions (or champions that will enable a combo) when hand is weak, draw when hand is strong

3. 

## Summary

This implementation:

- ✅ Follows established patterns from mulligan/discard
- ✅ Keeps all functions under 35 lines
- ✅ Maintains file size within limits
- ✅ Reuses existing helper functions
- ✅ Provides clear user experience
- ✅ Supports all 3 languages
- ✅ Ready for AI integration later

**Total implementation: ~150 lines across 4 new functions, following project guidelines.**
