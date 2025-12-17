/* ========================================================================
   CLI Game Logic
   ======================================================================== */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cli_game.h"
#include "cli_display.h"
#include "cli_input.h"
#include "game_constants.h"
#include "strategy.h"
#include "strat_random.h"
#include "game_state.h"
#include "turn_logic.h"
#include "combat.h"
#include "localization.h"
#include "player_config.h"

#define MAX_COMMAND_LEN 256
#define NO_ACTION 0

/* ANSI color codes */
#define RESET   "\033[0m"
#define YELLOW  "\033[33m"

/* Visual indicators */
#define ICON_PROMPT ">"

/* ========================================================================
   Game Phase Handlers
   ======================================================================== */

int handle_interactive_attack(struct gamestate* gstate,
                              PlayerID player, GameContext* ctx, config_t* cfg)
{ char input_buffer[MAX_COMMAND_LEN];
  int action_taken = NO_ACTION;

  PlayerConfig* pconfig = (PlayerConfig*)cfg->player_config;
  //const char* player_name = pconfig->player_names[player];
  PlayerID opponent = 1 - player;
  const char* opponent_name = pconfig->player_names[opponent];

  while(!action_taken && !gstate->someone_has_zero_energy)
  { printf("\n=== (%s %d, %s %d) ===\n",           
           LOCALIZED_STRING("Turn", "Tour", "Turno"),
           gstate->turn,
           LOCALIZED_STRING("Round", "Manche", "Ronda"),
           (uint16_t)((gstate->turn - 1) * 0.5 + 1));
    printf("\n=== %s (%s) ===\n",
           opponent_name,
           LOCALIZED_STRING("Defender", "Defenseur", "Defensor"));
    display_player_prompt(opponent, gstate, 1, cfg);
    printf(" %s:%d\n",
           LOCALIZED_STRING("Hand", "Main", "Mano"),
           gstate->hand[opponent].size);
    printf("\n");
    display_player_prompt(player, gstate, 0, cfg);
    display_player_hand(player, gstate, cfg);
    printf("\n%s\n" ICON_PROMPT " ",
           LOCALIZED_STRING("Commands: cham <indices>, draw <index>, cash <index>, pass, gmst, help, exit",
                            "Commandes: cham <indices>, draw <index>, cash <index>, pass, gmst, help, exit",
                            "Comandos: cham <indices>, draw <index>, cash <index>, pass, gmst, help, exit"));

    if(fgets(input_buffer, sizeof(input_buffer), stdin) == NULL)
    { printf("%s\n", LOCALIZED_STRING("Error reading input.",
                                      "Erreur de lecture.",
                                      "Error al leer entrada."));
      return EXIT_SIGNAL;
    }

    action_taken = process_attack_command(input_buffer, gstate, player, ctx, cfg);
    if(action_taken == EXIT_SIGNAL) return EXIT_SIGNAL;
  }

  return EXIT_SUCCESS;
}

int handle_interactive_defense(struct gamestate* gstate,
                               PlayerID player, GameContext* ctx, config_t* cfg)
{ char input_buffer[MAX_COMMAND_LEN];

  PlayerConfig* pconfig = (PlayerConfig*)cfg->player_config;
  //const char* player_name = pconfig->player_names[player];
  PlayerID opponent = 1 - player;
  const char* opponent_name = pconfig->player_names[opponent];

  printf("\n=== (%s %d, %s %d) ===\n",         
         LOCALIZED_STRING("Turn", "Tour", "Turno"),
         gstate->turn,
         LOCALIZED_STRING("Round", "Manche", "Ronda"),
         (uint16_t)((gstate->turn - 1) * 0.5 + 1));
  display_attack_state(gstate, cfg);

  printf("\n=== %s (%s) ===\n",
         opponent_name,
         LOCALIZED_STRING("Attacker", "Attaquant", "Atacante"));
  display_player_prompt(opponent, gstate, 0, cfg);
  printf(" %s:%d\n",
         LOCALIZED_STRING("Hand", "Main", "Mano"),
         gstate->hand[opponent].size);

  printf("\n\n");
  display_player_prompt(player, gstate, 1, cfg);
  
  /* Check if defender has any cards before showing hand/prompt
     If hand is empty, automatically pass (take damage) */
  if(gstate->hand[player].size == 0)
  { printf("\n%s\n",
           LOCALIZED_STRING("No cards in hand - taking damage without defending",
                            "Aucune carte en main - prendre des degats sans defendre",
                            "No hay cartas en mano - recibir dano sin defender"));
    return EXIT_SUCCESS;
  }
  
  display_player_hand(player, gstate, cfg);
  printf("\n%s\n" ICON_PROMPT " ",
         LOCALIZED_STRING("Defend: 'cham <indices>' (e.g., 'cham 1 2') or 'pass' to take damage",
                          "Defendre: 'cham <indices>' (ex: 'cham 1 2') ou 'pass' pour prendre des degats",
                          "Defender: 'cham <indices>' (ej: 'cham 1 2') o 'pass' para recibir dano"));

  if(fgets(input_buffer, sizeof(input_buffer), stdin) == NULL)
    return EXIT_SUCCESS;

  int result = process_defense_command(input_buffer, gstate, player, ctx, cfg);
  return (result == EXIT_SIGNAL) ? EXIT_SIGNAL : EXIT_SUCCESS;
}

/* ========================================================================
   Game Turn Execution
   ======================================================================== */

int execute_game_turn(struct gamestate* gstate, StrategySet* strategies,
                     GameContext* ctx, config_t* cfg)
{ begin_of_turn(gstate, ctx);

  PlayerConfig* pconfig = (PlayerConfig*)cfg->player_config;

  /* Attack phase - check if current player is interactive */
  if(pconfig->player_types[gstate->current_player] == INTERACTIVE_PLAYER)
  { int result = handle_interactive_attack(gstate, gstate->current_player, ctx, cfg);
    if(result == EXIT_SIGNAL) return EXIT_SIGNAL;
  }
  else
    attack_phase(gstate, strategies, ctx);

  /* Defense phase - check if defender is interactive */
  if(gstate->combat_zone[gstate->current_player].size > 0)
  { PlayerID defender = 1 - gstate->current_player;
    if(pconfig->player_types[defender] == INTERACTIVE_PLAYER)
    { int result = handle_interactive_defense(gstate, defender, ctx, cfg);
      if(result == EXIT_SIGNAL) return EXIT_SIGNAL;
    }
    else
      defense_phase(gstate, strategies, ctx);

    resolve_combat(gstate, ctx);
  }

  return EXIT_SUCCESS;
}

/* ========================================================================
   Game Initialization and Cleanup
   ======================================================================== */

struct gamestate* initialize_cli_game(uint16_t initial_cash,
                                      StrategySet** strategies_out,
                                      config_t* cfg,
                                      GameContext* ctx)
{ StrategySet* strategies = create_strategy_set();
  set_player_strategy(strategies, PLAYER_A,
                      random_attack_strategy, random_defense_strategy);
  set_player_strategy(strategies, PLAYER_B,
                      random_attack_strategy, random_defense_strategy);

  struct gamestate* gstate = malloc(sizeof(struct gamestate));
  setup_game(initial_cash, gstate, ctx);

  *strategies_out = strategies;
  return gstate;
}

void cleanup_cli_game(struct gamestate* gstate, StrategySet* strategies,
                     GameContext* ctx)
{ DeckStk_emptyOut(&gstate->deck[PLAYER_A]);
  DeckStk_emptyOut(&gstate->deck[PLAYER_B]);

  free(gstate);
  free_strategy_set(strategies);
  destroy_game_context(ctx);
}
