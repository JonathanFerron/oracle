// stda_cli_helpers.c
// Shared helper functions for CLI interactive mode

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stda_cli.h"
#include "game_constants.h"
#include "card_actions.h"

// ANSI color codes
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define CYAN    "\033[36m"

/* ========================================================================
   Card Display and Selection Helpers
   ======================================================================== */

// Display a single card with index for selection
void display_card_with_index(uint8_t card_idx, int display_num, int show_power)
{ const struct card* c = &fullDeck[card_idx];
  
  if(c->card_type == CHAMPION_CARD)
  { const char* color = (c->color == COLOR_INDIGO) ? BLUE :
                        (c->color == COLOR_ORANGE) ? YELLOW : RED;
    if(show_power)
      printf("  [%d] %s%s" RESET " (D%d+%d, " CYAN "☾%d" RESET 
             ", pwr:%.1f)\n",
             display_num, color, CHAMPION_SPECIES_NAMES[c->species],
             c->defense_dice, c->attack_base, c->cost, c->power);
    else
      printf("  [%d] %s%s" RESET " (D%d+%d, " CYAN "☾%d" RESET ")\n",
             display_num, color, CHAMPION_SPECIES_NAMES[c->species],
             c->defense_dice, c->attack_base, c->cost);
  }
  else if(c->card_type == DRAW_CARD)
  { if(show_power)
      printf("  [%d] " YELLOW "Draw %d" RESET " (" CYAN "☾%d" RESET 
             ", pwr:%.1f)\n",
             display_num, c->draw_num, c->cost, c->power);
    else
      printf("  [%d] " YELLOW "Draw %d" RESET " (" CYAN "☾%d" RESET ")\n",
             display_num, c->draw_num, c->cost);
  }
  else if(c->card_type == CASH_CARD)
  { if(show_power)
      printf("  [%d] " GREEN "Exchange for %d lunas" RESET 
             " (" CYAN "☾%d" RESET ", pwr:%.1f)\n",
             display_num, c->exchange_cash, c->cost, c->power);
    else
      printf("  [%d] " GREEN "Exchange for %d lunas" RESET 
             " (" CYAN "☾%d" RESET ")\n",
             display_num, c->exchange_cash, c->cost);
  }
}

// Parse card indices from command string (1-based to 0-based)
int parse_card_indices(char* input, uint8_t* indices, int max_count, 
                       int hand_size)
{ int count = 0;
  char* token = strtok(input, " ");
  
  while(token != NULL && count < max_count)
  { int idx = atoi(token);
    if(idx < 1 || idx > hand_size)
    { printf(RED "Error: Invalid card number %d (must be 1-%d)\n" RESET,
             idx, hand_size);
      return -1;
    }
    
    // Check for duplicates
    for(int i = 0; i < count; i++)
    { if(indices[i] == (idx - 1))
      { printf(RED "Error: Duplicate card number %d\n" RESET, idx);
        return -1;
      }
    }
    
    indices[count++] = idx - 1;  // Convert to 0-based
    token = strtok(NULL, " ");
  }
  
  return count;
}

// Discard selected cards and optionally draw replacements
void discard_and_draw(struct gamestate* gstate, PlayerID player,
                     uint8_t* indices, int count, int draw_replacements,
                     GameContext* ctx)
{ // Sort indices in descending order to avoid index shifting issues
  for(int i = 0; i < count - 1; i++)
  { for(int j = i + 1; j < count; j++)
    { if(indices[i] < indices[j])
      { uint8_t temp = indices[i];
        indices[i] = indices[j];
        indices[j] = temp;
      }
    }
  }
  
  // Discard cards (from highest index to lowest)
  for(int i = 0; i < count; i++)
  { uint8_t card_idx = gstate->hand[player].cards[indices[i]];
    HDCLL_removeNodeByValue(&gstate->hand[player], card_idx);
    HDCLL_insertNodeAtBeginning(&gstate->discard[player], card_idx);
  }
  
  // Draw replacement cards if requested
  if(draw_replacements)
  { for(int i = 0; i < count; i++)
      draw_1_card(gstate, player, ctx);
  }
}
