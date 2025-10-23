// proposed stda_cli refactoring

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
  #include <windows.h>
#endif

#include "stda_cli.h"
#include "game_types.h"
#include "mtwister.h"
#include "game_constants.h"
#include "strategy.h"
#include "strat_random.h"
#include "game_state.h"
#include "turn_logic.h"
#include "combat.h"
#include "game_state.h"
#include "card_actions.h"

#define MAX_COMMAND_LEN 256
#define EXIT_SIGNAL -1
#define ACTION_TAKEN 1
#define NO_ACTION 0

// ANSI color codes
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define BOLD_WHITE   "\033[1;37m"
#define COLOR_P1      "\033[1;36m"  // Cyan for Player 1
#define COLOR_P2      "\033[1;33m"  // Yellow for Player 2
#define COLOR_ENERGY  MAGENTA
#define COLOR_LUNA    CYAN

extern MTRand MTwister_rand_struct;

/* ========================================================================
   Display Functions
   ======================================================================== */

// Display player prompt with status
void display_player_prompt(PlayerID player, struct gamestate* gstate, int is_defense)
{ const char* player_color = (player == PLAYER_A) ? COLOR_P1 : COLOR_P2;
  const char* player_name = (player == PLAYER_A) ? "Player A" : "Player B";
  const char* phase_icon = is_defense ? "🛡" : "⚔";

  printf("%s%s" RESET " [" COLOR_ENERGY "❤ %d" RESET " "
         COLOR_LUNA "☾%d" RESET "] %s ⟡ ",
         player_color, player_name,
         gstate->current_energy[player],
         gstate->current_cash_balance[player],
         phase_icon);
}

// Display player's hand
void display_player_hand(PlayerID player, struct gamestate* gstate)
{ printf("\nYour hand:\n");
  uint8_t* hand_array = HDCLL_toArray(&gstate->hand[player]);

  for(uint8_t i = 0; i < gstate->hand[player].size; i++)
  { uint8_t card_idx = hand_array[i];
    const struct card* c = &fullDeck[card_idx];

    if(c->card_type == CHAMPION_CARD)
    { const char* color = (c->color == COLOR_INDIGO) ? BLUE :
                          (c->color == COLOR_ORANGE) ? YELLOW : RED;
      printf("  [%d] %s%s" RESET " (D%d+%d, " CYAN "☾%d" RESET ")\n",
             i + 1, color, CHAMPION_SPECIES_NAMES[c->species],
             c->defense_dice, c->attack_base, c->cost);
    }
    else if(c->card_type == DRAW_CARD)
    { printf("  [%d] " YELLOW "Draw %d" RESET " (" CYAN "☾%d" RESET ")\n",
             i + 1, c->draw_num, c->cost);
    }
    else if(c->card_type == CASH_CARD)
    { printf("  [%d] " GREEN "Exchange for %d lunas" RESET " (" CYAN "☾%d" RESET ")\n",
             i + 1, c->exchange_cash, c->cost);
    }
  }
  free(hand_array);
}

// Display attacker's champions in combat
void display_attack_state(struct gamestate* gstate)
{ printf("\n" RED "=== Combat! You are being attacked ===" RESET "\n");
  printf("Attacker's champions in combat:\n");
  struct LLNode* current = gstate->combat_zone[1 - gstate->current_player].head;

  for(uint8_t i = 0; i < gstate->combat_zone[1 - gstate->current_player].size; i++)
  { const struct card* c = &fullDeck[current->data];
    printf("  - %s (D%d+%d)\n", CHAMPION_SPECIES_NAMES[c->species],
           c->defense_dice, c->attack_base);
    current = current->next;
  }
}

// Display game status
void display_game_status(struct gamestate* gstate)
{ printf("\n" BOLD_WHITE "=== Game Status ===" RESET "\n");
  printf(COLOR_P1 "Player A" RESET ": " COLOR_ENERGY "❤ %d" RESET
         " " COLOR_LUNA "☾%d" RESET " Hand:%d Deck:%d\n",
         gstate->current_energy[PLAYER_A],
         gstate->current_cash_balance[PLAYER_A],
         gstate->hand[PLAYER_A].size,
         gstate->deck[PLAYER_A].top + 1);
  printf(COLOR_P2 "Player B" RESET ": " COLOR_ENERGY "❤ %d" RESET
         " " COLOR_LUNA "☾%d" RESET " Hand:%d Deck:%d\n",
         gstate->current_energy[PLAYER_B],
         gstate->current_cash_balance[PLAYER_B],
         gstate->hand[PLAYER_B].size,
         gstate->deck[PLAYER_B].top + 1);
}

// Display CLI help
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
  }
  printf("  help            - Show this help\n");
  printf("  exit            - Quit game\n\n");
}

/* ========================================================================
   Input Parsing and Validation Functions
   ======================================================================== */

// Parse champion indices from input string (1-based to 0-based)
int parse_champion_indices(char* input, uint8_t* indices, int max_count, int hand_size)
{ int count = 0;
  char* token = strtok(input, " ");

  while(token != NULL && count < max_count)
  { int idx = atoi(token);
    if(idx < 1 || idx > hand_size)
    { printf(RED "Error: Invalid card number %d (must be 1-%d)\n" RESET,
             idx, hand_size);
      return -1;
    }
    indices[count++] = idx - 1;  // Convert to 0-based
    token = strtok(NULL, " ");
  }

  return count;
}

// Validate and play champions
int validate_and_play_champions(struct gamestate* gstate, PlayerID player,
                                uint8_t* indices, int count)
{ if(count <= 0) return NO_ACTION;

  // Validate cards and calculate cost
  int total_cost = 0;
  uint8_t* hand_array = HDCLL_toArray(&gstate->hand[player]);

  for(int i = 0; i < count; i++)
  { uint8_t card_idx = hand_array[indices[i]];
    if(fullDeck[card_idx].card_type != CHAMPION_CARD)
    { printf(RED "Error: Card %d is not a champion\n" RESET, indices[i] + 1);
      free(hand_array);
      return NO_ACTION;
    }
    total_cost += fullDeck[card_idx].cost;
  }

  if(total_cost > gstate->current_cash_balance[player])
  { printf(RED "Error: Not enough lunas (need %d, have %d)\n" RESET,
           total_cost, gstate->current_cash_balance[player]);
    free(hand_array);
    return NO_ACTION;
  }

  // Play the champions in reverse order
  for(int i = count - 1; i >= 0; i--)
    play_champion(gstate, player, hand_array[indices[i]]);

  free(hand_array);
  printf(GREEN "✓ Played %d champion(s)\n" RESET, count);
  return ACTION_TAKEN;
}

/* ========================================================================
   Card Action Handlers
   ======================================================================== */

// Handle draw command
int handle_draw_command(struct gamestate* gstate, PlayerID player, char* input)
{ int idx = atoi(input);
  if(idx < 1 || idx > gstate->hand[player].size)
  { printf(RED "Error: Invalid card number (must be 1-%d)\n" RESET,
           gstate->hand[player].size);
    return NO_ACTION;
  }

  uint8_t* hand_array = HDCLL_toArray(&gstate->hand[player]);
  uint8_t card_idx = hand_array[idx - 1];
  free(hand_array);

  if(fullDeck[card_idx].card_type != DRAW_CARD)
  { printf(RED "Error: Not a draw card\n" RESET);
    return NO_ACTION;
  }

  if(fullDeck[card_idx].cost > gstate->current_cash_balance[player])
  { printf(RED "Error: Not enough lunas\n" RESET);
    return NO_ACTION;
  }

  play_draw_card(gstate, player, card_idx);
  printf(GREEN "✓ Played draw card\n" RESET);
  return ACTION_TAKEN;
}

// Handle cash command
int handle_cash_command(struct gamestate* gstate, PlayerID player, char* input)
{ int idx = atoi(input);
  if(idx < 1 || idx > gstate->hand[player].size)
  { printf(RED "Error: Invalid card number (must be 1-%d)\n" RESET,
           gstate->hand[player].size);
    return NO_ACTION;
  }

  uint8_t* hand_array = HDCLL_toArray(&gstate->hand[player]);
  uint8_t card_idx = hand_array[idx - 1];
  free(hand_array);

  if(fullDeck[card_idx].card_type != CASH_CARD)
  { printf(RED "Error: Not an exchange card\n" RESET);
    return NO_ACTION;
  }

  if(!has_champion_in_hand(&gstate->hand[player]))
  { printf(RED "Error: No champions to exchange\n" RESET);
    return NO_ACTION;
  }

  play_cash_card(gstate, player, card_idx);
  printf(GREEN "✓ Played exchange card\n" RESET);
  return ACTION_TAKEN;
}

// Process champion command (attack or defense)
static int process_champion_command(char* input, struct gamestate* gstate,
                                    PlayerID player)
{ uint8_t indices[3];
  int count = parse_champion_indices(input, indices, 3,
                                     gstate->hand[player].size);
  if(count > 0 && validate_and_play_champions(gstate, player, indices, count))
    return ACTION_TAKEN;
  return NO_ACTION;
}

/* ========================================================================
   Command Processing Functions
   ======================================================================== */

// Process attack phase command
static int process_attack_command(char* input_buffer, struct gamestate* gstate)
{ input_buffer[strcspn(input_buffer, "\n")] = 0;

  if(strncmp(input_buffer, "cham ", 5) == 0)
    return process_champion_command(input_buffer + 5, gstate, PLAYER_A);
  else if(strncmp(input_buffer, "draw ", 5) == 0)
    return handle_draw_command(gstate, PLAYER_A, input_buffer + 5);
  else if(strncmp(input_buffer, "cash ", 5) == 0)
    return handle_cash_command(gstate, PLAYER_A, input_buffer + 5);
  else if(strcmp(input_buffer, "pass") == 0)
  { printf(YELLOW "Passed turn\n" RESET);
    return ACTION_TAKEN;
  }
  else if(strcmp(input_buffer, "gmst") == 0)
  { display_game_status(gstate);
    return NO_ACTION;
  }
  else if(strcmp(input_buffer, "help") == 0)
  { display_cli_help(0);
    return NO_ACTION;
  }
  else if(strcmp(input_buffer, "exit") == 0)
    return EXIT_SIGNAL;

  printf(RED "Unknown command. Type 'help' for commands.\n" RESET);
  return NO_ACTION;
}

// Process defense phase command
static int process_defense_command(char* input_buffer, struct gamestate* gstate)
{ input_buffer[strcspn(input_buffer, "\n")] = 0;

  if(strcmp(input_buffer, "exit") == 0)
    return EXIT_SIGNAL;
  else if(strcmp(input_buffer, "pass") == 0)
  { printf(YELLOW "Taking damage without defending\n" RESET);
    return NO_ACTION;
  }
  else if(strncmp(input_buffer, "cham ", 5) == 0)
  { uint8_t indices[3];
    int count = parse_champion_indices(input_buffer + 5, indices, 3,
                                       gstate->hand[PLAYER_A].size);
    if(count > 0)
    { if(!validate_and_play_champions(gstate, PLAYER_A, indices, count))
        printf(YELLOW "Taking damage without defending\n" RESET);
    }
    else if(count == 0)
      printf(YELLOW "No defenders specified, taking damage\n" RESET);
  }
  else if(strcmp(input_buffer, "help") == 0)
    display_cli_help(1);
  else
  { printf(RED "Unknown command. Use 'cham <indices>' or 'pass'\n" RESET);
    printf(YELLOW "Taking damage without defending\n" RESET);
  }

  return NO_ACTION;
}

/* ========================================================================
   Game Phase Handlers
   ======================================================================== */

// Handle interactive attack phase
static int handle_interactive_attack(struct gamestate* gstate)
{ char input_buffer[MAX_COMMAND_LEN];
  int action_taken = NO_ACTION;

  while(!action_taken && !gstate->someone_has_zero_energy)
  { printf("\n=== %s's Turn (Turn %d) ===\n",
           PLAYER_NAMES[PLAYER_A], gstate->turn);
    display_player_prompt(PLAYER_A, gstate, 0);
    display_player_hand(PLAYER_A, gstate);
    printf("\nCommands: cham <indices>, draw <index>, cash <index>, "
           "pass, gmst, help, exit\n> ");

    if(fgets(input_buffer, sizeof(input_buffer), stdin) == NULL)
    { printf("Error reading input.\n");
      return EXIT_SIGNAL;
    }

    action_taken = process_attack_command(input_buffer, gstate);
    if(action_taken == EXIT_SIGNAL) return EXIT_SIGNAL;
  }

  return EXIT_SUCCESS;
}

// Handle interactive defense phase
static int handle_interactive_defense(struct gamestate* gstate)
{ char input_buffer[MAX_COMMAND_LEN];

  display_attack_state(gstate);
  printf("\n");
  display_player_prompt(PLAYER_A, gstate, 1);
  display_player_hand(PLAYER_A, gstate);
  printf("\nDefend: 'cham <indices>' (e.g., 'cham 1 2') or "
         "'pass' to take damage\n> ");

  if(fgets(input_buffer, sizeof(input_buffer), stdin) == NULL)
    return EXIT_SUCCESS;

  int result = process_defense_command(input_buffer, gstate);
  return (result == EXIT_SIGNAL) ? EXIT_SIGNAL : EXIT_SUCCESS;
}

/* ========================================================================
   Game Turn Execution
   ======================================================================== */

// Execute a single game turn
static int execute_game_turn(struct gamestate* gstate, StrategySet* strategies)
{ begin_of_turn(gstate);

  // Attack phase
  if(gstate->current_player == PLAYER_A)
  { int result = handle_interactive_attack(gstate);
    if(result == EXIT_SIGNAL) return EXIT_SIGNAL;
  }
  else
    attack_phase(gstate, strategies);

  // Defense and combat phase
  if(gstate->combat_zone[gstate->current_player].size > 0)
  { if(gstate->current_player == PLAYER_A)
      defense_phase(gstate, strategies);
    else
    { int result = handle_interactive_defense(gstate);
      if(result == EXIT_SIGNAL) return EXIT_SIGNAL;
    }

    resolve_combat(gstate);
  }

  return EXIT_SUCCESS;
}

/* ========================================================================
   Game Initialization and Cleanup
   ======================================================================== */

// Initialize CLI game
static struct gamestate* initialize_cli_game(uint16_t initial_cash,
                                             StrategySet** strategies_out)
{ MTwister_rand_struct = seedRand(M_TWISTER_SEED);

  StrategySet* strategies = create_strategy_set();
  set_player_strategy(strategies, PLAYER_A,
                      random_attack_strategy, random_defense_strategy);
  set_player_strategy(strategies, PLAYER_B,
                      random_attack_strategy, random_defense_strategy);

  struct gamestate* gstate = malloc(sizeof(struct gamestate));
  setup_game(initial_cash, gstate);

  *strategies_out = strategies;
  return gstate;
}

// Cleanup CLI game resources
static void cleanup_cli_game(struct gamestate* gstate, StrategySet* strategies)
{ DeckStk_emptyOut(&gstate->deck[PLAYER_A]);
  DeckStk_emptyOut(&gstate->deck[PLAYER_B]);
  HDCLL_emptyOut(&gstate->combat_zone[PLAYER_A]);
  HDCLL_emptyOut(&gstate->combat_zone[PLAYER_B]);
  HDCLL_emptyOut(&gstate->hand[PLAYER_A]);
  HDCLL_emptyOut(&gstate->hand[PLAYER_B]);
  HDCLL_emptyOut(&gstate->discard[PLAYER_A]);
  HDCLL_emptyOut(&gstate->discard[PLAYER_B]);

  free(gstate);
  free_strategy_set(strategies);
}

/* ========================================================================
   Main CLI Mode Entry Point
   ======================================================================== */

// Command line interface interactive mode
int run_mode_stda_cli(config_t* cfg)
{
  #ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
  #endif

  printf("Running in command line interface mode...\n");

  StrategySet* strategies;
  struct gamestate* gstate = initialize_cli_game(INITIAL_CASH_DEFAULT,
                                                 &strategies);

  printf("\n=== Game Start ===\n");
  gstate->turn = 0;

  // Main game loop
  while(gstate->turn < MAX_NUMBER_OF_TURNS &&
        !gstate->someone_has_zero_energy)
  { int result = execute_game_turn(gstate, strategies);
    if(result == EXIT_SIGNAL) break;

    if(gstate->someone_has_zero_energy) break;

    collect_1_luna(gstate);
    change_current_player(gstate);
  }

  // Determine final game state
  if(!gstate->someone_has_zero_energy)
    gstate->game_state = DRAW;

  cleanup_cli_game(gstate, strategies);
  return EXIT_SUCCESS;
}
