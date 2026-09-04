# Display Discard Pile Enhancement for CLI Mode

Based on the refactoring methodology, here's a clean approach to add discard pile display to the `gmst` command:

## Part 1: New Display Function

Add this function to `src/stda_cli.c` after the existing display functions:

```c
/* ========================================================================
   Display Functions (continued)
   ======================================================================== */

// Display player's discard pile
void display_player_discard(PlayerID player, struct gamestate* gstate)
{ const char* player_color = (player == PLAYER_A) ? COLOR_P1 : COLOR_P2;
  const char* player_name = (player == PLAYER_A) ? "Player A" : "Player B";

  printf("%s%s Discard" RESET " (%d cards):\n", 
         player_color, player_name, gstate->discard[player].size);

  if(gstate->discard[player].size == 0)
  { printf("  (empty)\n");
    return;
  }

  // Count card types for summary
  uint8_t champion_count = 0;
  uint8_t draw_count = 0;
  uint8_t cash_count = 0;

  struct LLNode* current = gstate->discard[player].head;
  for(uint8_t i = 0; i < gstate->discard[player].size; i++)
  { const struct card* c = &fullDeck[current->data];
    switch(c->card_type)
    { case CHAMPION_CARD: champion_count++; break;
      case DRAW_CARD: draw_count++; break;
      case CASH_CARD: cash_count++; break;
    }
    current = current->next;
  }

  printf("  Champions: %d, Draw: %d, Cash: %d\n", 
         champion_count, draw_count, cash_count);
}
```

## Part 2: Update Function Signature

Add to `src/stda_cli.h`:

```c
void display_player_discard(PlayerID player, struct gamestate* gstate);
```

## Part 3: Integrate into Game Status Display

**Location:** `src/stda_cli.c` - `display_game_status()` function

```diff
 void display_game_status(struct gamestate* gstate)
 { printf("\n" BOLD_WHITE "=== Game Status ===" RESET "\n");
+  
+  // Player A status
   printf(COLOR_P1 "Player A" RESET ": " COLOR_ENERGY "❤ %d" RESET
          " " COLOR_LUNA "☾%d" RESET " Hand:%d Deck:%d\n",
          gstate->current_energy[PLAYER_A],
          gstate->current_cash_balance[PLAYER_A],
          gstate->hand[PLAYER_A].size,
          gstate->deck[PLAYER_A].top + 1);
+  display_player_discard(PLAYER_A, gstate);
+  
+  printf("\n");
+  
+  // Player B status
   printf(COLOR_P2 "Player B" RESET ": " COLOR_ENERGY "❤ %d" RESET
          " " COLOR_LUNA "☾%d" RESET " Hand:%d Deck:%d\n",
          gstate->current_energy[PLAYER_B],
          gstate->current_cash_balance[PLAYER_B],
          gstate->hand[PLAYER_B].size,
          gstate->deck[PLAYER_B].top + 1);
+  display_player_discard(PLAYER_B, gstate);
 }
```

## Design Rationale

### Why This Approach Works for TUI Adaptation

1. **Separate Display Logic**: `display_player_discard()` is self-contained and focuses only on formatting discard data
2. **No Direct Console I/O in Data Processing**: All `printf()` calls are isolated in display functions
3. **Easy to Convert**: For TUI, you can replace `printf()` with ncurses `wprintw()` calls

### TUI Adaptation Example

When building the TUI version, the conversion would be:

```c
// TUI version in src/tui_display.c
void tui_display_player_discard(WINDOW* win, int start_y, int start_x,
                                PlayerID player, struct gamestate* gstate)
{ const char* player_name = (player == PLAYER_A) ? "Player A" : "Player B";
  int attr = (player == PLAYER_A) ? COLOR_PAIR(1) : COLOR_PAIR(2);

  wattron(win, attr);
  mvwprintw(win, start_y, start_x, "%s Discard (%d cards):", 
            player_name, gstate->discard[player].size);
  wattroff(win, attr);

  if(gstate->discard[player].size == 0)
  { mvwprintw(win, start_y + 1, start_x + 2, "(empty)");
    return;
  }

  // Same counting logic...
  uint8_t champion_count = 0;
  uint8_t draw_count = 0;
  uint8_t cash_count = 0;

  struct LLNode* current = gstate->discard[player].head;
  for(uint8_t i = 0; i < gstate->discard[player].size; i++)
  { const struct card* c = &fullDeck[current->data];
    switch(c->card_type)
    { case CHAMPION_CARD: champion_count++; break;
      case DRAW_CARD: draw_count++; break;
      case CASH_CARD: cash_count++; break;
    }
    current = current->next;
  }

  mvwprintw(win, start_y + 1, start_x + 2, 
            "Champions: %d, Draw: %d, Cash: %d",
            champion_count, draw_count, cash_count);
}
```

## Enhanced Detailed Display Function

Replace the `display_player_discard_detailed()` function with this improved version:

```c
// Display detailed discard pile contents (sorted by type and power)
void display_player_discard_detailed(PlayerID player, struct gamestate* gstate)
{ const char* player_color = (player == PLAYER_A) ? COLOR_P1 : COLOR_P2;
  const char* player_name = (player == PLAYER_A) ? "Player A" : "Player B";

  printf("%s%s Discard" RESET " (%d cards):\n", 
         player_color, player_name, gstate->discard[player].size);

  if(gstate->discard[player].size == 0)
  { printf("  (empty)\n");
    return;
  }

  // Build arrays of card indices by type
  uint8_t champions[40];
  uint8_t draw_cards[15];
  uint8_t cash_cards[3];
  uint8_t champ_count = 0;
  uint8_t draw_count = 0;
  uint8_t cash_count = 0;

  struct LLNode* current = gstate->discard[player].head;
  for(uint8_t i = 0; i < gstate->discard[player].size; i++)
  { uint8_t card_idx = current->data;
    const struct card* c = &fullDeck[card_idx];

    switch(c->card_type)
    { case CHAMPION_CARD: champions[champ_count++] = card_idx; break;
      case DRAW_CARD: draw_cards[draw_count++] = card_idx; break;
      case CASH_CARD: cash_cards[cash_count++] = card_idx; break;
    }
    current = current->next;
  }

  // Sort champions by descending power
  for(uint8_t i = 0; i < champ_count; i++)
  { for(uint8_t j = i + 1; j < champ_count; j++)
    { if(fullDeck[champions[i]].power < fullDeck[champions[j]].power)
      { uint8_t temp = champions[i];
        champions[i] = champions[j];
        champions[j] = temp;
      }
    }
  }

  // Display champions
  if(champ_count > 0)
  { printf("  Champions (%d):\n", champ_count);
    for(uint8_t i = 0; i < champ_count; i++)
    { const struct card* c = &fullDeck[champions[i]];
      const char* color = (c->color == COLOR_INDIGO) ? BLUE :
                          (c->color == COLOR_ORANGE) ? YELLOW : RED;
      printf("    %s%s" RESET " [%c] (D%d+%d, ☾%d, pwr:%.1f)", 
             color, CHAMPION_SPECIES_NAMES[c->species],
             'A' + c->order,
             c->defense_dice, c->attack_base, c->cost, c->power);

      if((i + 1) % 2 == 0)
        printf("\n");
      else
        printf("  ");
    }
    if(champ_count % 2 != 0) printf("\n");
  }

  // Display draw cards
  if(draw_count > 0)
  { printf("  Draw cards (%d): ", draw_count);
    for(uint8_t i = 0; i < draw_count; i++)
    { const struct card* c = &fullDeck[draw_cards[i]];
      printf(YELLOW "Draw%d" RESET, c->draw_num);
      if(i < draw_count - 1) printf(", ");
    }
    printf("\n");
  }

  // Display cash cards
  if(cash_count > 0)
  { printf("  Cash cards (%d): ", cash_count);
    for(uint8_t i = 0; i < cash_count; i++)
    { printf(GREEN "Cash" RESET);
      if(i < cash_count - 1) printf(", ");
    }
    printf("\n");
  }
}
```

## And handle it in `process_attack_command()`:

```diff
   else if(strcmp(input_buffer, "gmst") == 0)
   { display_game_status(gstate);
     return NO_ACTION;
   }
+  else if(strcmp(input_buffer, "disc") == 0)
+  { display_player_discard_detailed(PLAYER_A, gstate);
+    printf("\n");
+    display_player_discard_detailed(PLAYER_B, gstate);
+    return NO_ACTION;
+  }
```

## Part 3: Add Command Handler

**Location:** `src/stda_cli.c` - `process_attack_command()` function

```diff
   else if(strcmp(input_buffer, "gmst") == 0)
   { display_game_status(gstate);
     return NO_ACTION;
   }
+  else if(strcmp(input_buffer, "shod") == 0)
+  { display_player_discard_detailed(PLAYER_A, gstate);
+    printf("\n");
+    display_player_discard_detailed(PLAYER_B, gstate);
+    return NO_ACTION;
+  }
   else if(strcmp(input_buffer, "help") == 0)
   { display_cli_help(0);
     return NO_ACTION;
   }
```

## 

# Updated Display Discard Enhancement

#### Part 2: Update Help Display

**Location:** `src/stda_cli.c` - `display_cli_help()` function

```diff
 void display_cli_help(int is_defense)
 { printf("\n" BOLD_WHITE "=== Commands ===" RESET "\n");
   if(is_defense)
   { printf("  cham <indices>  - Defend with 1-3 champions (e.g., 'cham 1 2')\n");
     printf("  pass            - Take damage without defending\n");
   }
   else
   { printf("  cham <indices>  - Attack with 1-3 champions (e.g., 'cham 1 3')\n");
     printf("  draw <index>    - Play draw/recall card (e.g., 'draw 2')\n");
     printf("  cash <index>    - Play exchange card (e.g., 'cash 1')\n");
     printf("  pass            - Pass your turn\n");
     printf("  gmst            - Show game status\n");
+    printf("  shod            - Show detailed discard piles\n");
   }
   printf("  help            - Show this help\n");
   printf("  exit            - Quit game\n\n");
 }
```

## ## Part 4: Update Header File

**Location:** `src/stda_cli.h`

```diff
 void display_player_hand(PlayerID player, struct gamestate* gstate);
 void display_attack_state(struct gamestate* gstate);
+void display_player_discard(PlayerID player, struct gamestate* gstate);
+void display_player_discard_detailed(PlayerID player, struct gamestate* gstate);
 int parse_champion_indices(char* input, uint8_t* indices, int max_count, int hand_size);
```

## Example Output

```
Player A Discard (8 cards):
  Champions (6):
    Dragon [D] (D20+5, ☾3, pwr:15.5)  Elf [A] (D20+4, ☾3, pwr:14.5)
    Cyclops [D] (D12+5, ☾3, pwr:11.5)  Hobbit [B] (D8+5, ☾2, pwr:9.5)
    Human [A] (D6+3, ☾1, pwr:6.5)  Orc [C] (D4+1, ☾0, pwr:3.5)
  Draw cards (1): Draw2
  Cash cards (1): Cash

Player B Discard (5 cards):
  Champions (4):
    Fairy [D] (D20+2, ☾3, pwr:12.5)  Koatl [E] (D12+3, ☾2, pwr:9.5)
    Centaur [B] (D8+2, ☾1, pwr:6.5)  Dwarf [A] (D4+0, ☾0, pwr:2.5)
  Draw cards (1): Draw3
```

## Summary

**Minimal change:** Add `display_player_discard()` and integrate into `display_game_status()`

**Benefits:**

- ✅ Clean separation of concerns
- ✅ Easy to adapt for TUI (just change output target)
- ✅ Follows existing code patterns in `stda_cli.c`
- ✅ Keeps functions under 30 lines
- ✅ No changes to game logic, only presentation layer

The summary display is perfect for `gmst`, while the detailed version is useful for strategy analysis (optional command).

## Design Notes

### Features

- ✅ Champions sorted by descending power
- ✅ Order shown with letter [A-E]
- ✅ All champion stats displayed (dice, base, cost, power)
- ✅ Draw and cash cards grouped at the end
- ✅ Two champions per line for readability
- ✅ Color-coded by champion color

### TUI Adaptation

The same separation applies - for TUI you'd replace `printf()` with window printing:

```c
// TUI version pseudo-code
void tui_display_discard_detailed(WINDOW* win, int y, int x, 
                                  PlayerID player, struct gamestate* gstate)
{ // Same sorting logic...
  // Replace printf() with mvwprintw(win, y++, x, ...)
}
```

### Function Size

The function is ~65 lines but follows the guideline that "a long list of cases in a switch statement is fine" - in this case, it's straightforward display logic with no complex control flow.
