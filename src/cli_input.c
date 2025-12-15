/* ========================================================================
   CLI Input Processing
   ======================================================================== */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cli_input.h"
#include "cli_display.h"
#include "game_constants.h"
#include "card_actions.h"
#include "localization.h"

#define EXIT_SIGNAL -1
#define ACTION_TAKEN 1
#define NO_ACTION 0

/* ANSI color codes */
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"

/* Visual indicators */
#define ICON_SUCCESS "[OK]"

/* ========================================================================
   Input Parsing and Validation Functions
   ======================================================================== */

int parse_champion_indices(char* input, uint8_t* indices, int max_count,
                           int hand_size, config_t* cfg)
{ int count = 0;
  char* token = strtok(input, " ");

  while(token != NULL && count < max_count)
  { int idx = atoi(token);
    if(idx < 1 || idx > hand_size)
    { printf(RED "%s %d (%s 1-%d)\n" RESET,
             LOCALIZED_STRING("Error: Invalid card number",
                              "Erreur: Numero de carte invalide",
                              "Error: Numero de carta invalido"),
             idx,
             LOCALIZED_STRING("must be", "doit etre", "debe ser"),
             hand_size);
      return -1;
    }
    indices[count++] = idx - 1; // convert to 0-based
    token = strtok(NULL, " ");
  }

  return count;
}

int validate_and_play_champions(struct gamestate* gstate, PlayerID player,
                                uint8_t* indices, int count, GameContext* ctx,
                                config_t* cfg)
{ if(count <= 0) return NO_ACTION;

  int total_cost = 0;

  for(int i = 0; i < count; i++)
  { uint8_t card_idx = gstate->hand[player].cards[indices[i]];
    if(fullDeck[card_idx].card_type != CHAMPION_CARD)
    { printf(RED "%s %d %s\n" RESET,
             LOCALIZED_STRING("Error: Card", "Erreur: Carte", "Error: Carta"),
             indices[i] + 1,
             LOCALIZED_STRING("is not a champion", "n'est pas un champion",
                              "no es un campeon"));
      return NO_ACTION;
    }
    total_cost += fullDeck[card_idx].cost;
  }

  if(total_cost > gstate->current_cash_balance[player])
  { printf(RED "%s (%s %d, %s %d)\n" RESET,
           LOCALIZED_STRING("Error: Not enough lunas",
                            "Erreur: Pas assez de lunas",
                            "Error: No hay suficientes lunas"),
           LOCALIZED_STRING("need", "besoin", "necesita"),
           total_cost,
           LOCALIZED_STRING("have", "avoir", "tienes"),
           gstate->current_cash_balance[player]);
    return NO_ACTION;
  }

  for(int i = count - 1; i >= 0; i--)
    play_champion(gstate, player, gstate->hand[player].cards[indices[i]], ctx);

  printf(GREEN ICON_SUCCESS " %s %d %s\n" RESET,
         LOCALIZED_STRING("Played", "Joue", "Jugado"),
         count,
         LOCALIZED_STRING("champion(s)", "champion(s)", "campeon(es)"));
  return ACTION_TAKEN;
}

/* ========================================================================
   Card Action Handlers
   ======================================================================== */

int handle_draw_command(struct gamestate* gstate, PlayerID player,
                        char* input, GameContext* ctx, config_t* cfg)
{ int idx = atoi(input);
  if(idx < 1 || idx > gstate->hand[player].size)
  { printf(RED "%s (must be 1-%d)\n" RESET,
           LOCALIZED_STRING("Error: Invalid card number",
                            "Erreur: Numero de carte invalide",
                            "Error: Numero de carta invalido"),
           gstate->hand[player].size);
    return NO_ACTION;
  }

  uint8_t card_idx = gstate->hand[player].cards[idx - 1];

  if(fullDeck[card_idx].card_type != DRAW_CARD)
  { printf(RED "%s\n" RESET,
           LOCALIZED_STRING("Error: Not a draw card",
                            "Erreur: Pas une carte piocher",
                            "Error: No es una carta de robar"));
    return NO_ACTION;
  }

  if(fullDeck[card_idx].cost > gstate->current_cash_balance[player])
  { printf(RED "%s\n" RESET,
           LOCALIZED_STRING("Error: Not enough lunas",
                            "Erreur: Pas assez de lunas",
                            "Error: No hay suficientes lunas"));
    return NO_ACTION;
  }

  play_draw_card(gstate, player, card_idx, ctx);
  printf(GREEN ICON_SUCCESS " %s\n" RESET,
         LOCALIZED_STRING("Played draw card", "Carte piocher jouee",
                          "Carta de robar jugada"));
  return ACTION_TAKEN;
}

int handle_cash_command(struct gamestate* gstate, PlayerID player,
                        char* input, GameContext* ctx, config_t* cfg)
{ int idx = atoi(input);
  if(idx < 1 || idx > gstate->hand[player].size)
  { printf(RED "%s (must be 1-%d)\n" RESET,
           LOCALIZED_STRING("Error: Invalid card number",
                            "Erreur: Numero de carte invalide",
                            "Error: Numero de carta invalido"),
           gstate->hand[player].size);
    return NO_ACTION;
  }

  uint8_t card_idx = gstate->hand[player].cards[idx - 1];

  if(fullDeck[card_idx].card_type != CASH_CARD)
  { printf(RED "%s\n" RESET,
           LOCALIZED_STRING("Error: Not an exchange card",
                            "Erreur: Pas une carte echange",
                            "Error: No es una carta de intercambio"));
    return NO_ACTION;
  }

  if(!has_champion_in_hand(&gstate->hand[player]))
  { printf(RED "%s\n" RESET,
           LOCALIZED_STRING("Error: No champions to exchange",
                            "Erreur: Aucun champion a echanger",
                            "Error: No hay campeones para intercambiar"));
    return NO_ACTION;
  }

  play_cash_card(gstate, player, card_idx, ctx);
  printf(GREEN ICON_SUCCESS " %s\n" RESET,
         LOCALIZED_STRING("Played exchange card", "Carte echange jouee",
                          "Carta de intercambio jugada"));
  return ACTION_TAKEN;
}

static int process_champion_command(char* input, struct gamestate* gstate,
                                    PlayerID player, GameContext* ctx,
                                    config_t* cfg)
{ uint8_t indices[3];
  int count = parse_champion_indices(input, indices, 3,
                                     gstate->hand[player].size, cfg);
  if(count > 0 && validate_and_play_champions(gstate, player, indices, count, ctx, cfg))
    return ACTION_TAKEN;
  return NO_ACTION;
}

/* ========================================================================
   Command Processing Functions
   ======================================================================== */

int process_attack_command(char* input_buffer, struct gamestate* gstate,
                           PlayerID player, GameContext* ctx, config_t* cfg)
{ input_buffer[strcspn(input_buffer, "\n")] = 0;

  if(strncmp(input_buffer, "cham ", 5) == 0)
    return process_champion_command(input_buffer + 5, gstate, player, ctx, cfg);
  else if(strncmp(input_buffer, "draw ", 5) == 0)
    return handle_draw_command(gstate, player, input_buffer + 5, ctx, cfg);
  else if(strncmp(input_buffer, "cash ", 5) == 0)
    return handle_cash_command(gstate, player, input_buffer + 5, ctx, cfg);
  else if(strcmp(input_buffer, "pass") == 0)
  { printf(YELLOW "%s\n" RESET, LOCALIZED_STRING("Passed turn", "Tour passe", "Turno pasado"));
    return ACTION_TAKEN;
  }
  else if(strcmp(input_buffer, "gmst") == 0)
  { display_game_status(gstate, cfg);
    return NO_ACTION;
  }
  else if(strcmp(input_buffer, "help") == 0)
  { display_cli_help(0, cfg);
    return NO_ACTION;
  }
  else if(strcmp(input_buffer, "exit") == 0)
    return EXIT_SIGNAL;

  printf(RED "%s\n" RESET,
         LOCALIZED_STRING("Unknown command. Type 'help' for commands.",
                          "Commande inconnue. Tapez 'help' pour les commandes.",
                          "Comando desconocido. Escribe 'help' para comandos."));
  return NO_ACTION;
}

int process_defense_command(char* input_buffer, struct gamestate* gstate,
                           PlayerID player, GameContext* ctx, config_t* cfg)
{ input_buffer[strcspn(input_buffer, "\n")] = 0;

  if(strcmp(input_buffer, "exit") == 0)
    return EXIT_SIGNAL;
  else if(strcmp(input_buffer, "pass") == 0)
  { printf(YELLOW "%s\n" RESET,
           LOCALIZED_STRING("Taking damage without defending",
                            "Prendre des degats sans defendre",
                            "Recibir dano sin defender"));
    return NO_ACTION;
  }
  else if(strncmp(input_buffer, "cham ", 5) == 0)
  { uint8_t indices[3];
    int count = parse_champion_indices(input_buffer + 5, indices, 3,
                                       gstate->hand[player].size, cfg);
    if(count > 0)
    { if(!validate_and_play_champions(gstate, player, indices, count, ctx, cfg))
        printf(YELLOW "%s\n" RESET,
               LOCALIZED_STRING("Taking damage without defending",
                                "Prendre des degats sans defendre",
                                "Recibir dano sin defender"));
    }
    else if(count == 0)
      printf(YELLOW "%s\n" RESET,
             LOCALIZED_STRING("No defenders specified, taking damage",
                              "Aucun defenseur specifie, prendre des degats",
                              "No se especificaron defensores, recibir dano"));
  }
  else if(strcmp(input_buffer, "help") == 0)
    display_cli_help(1, cfg);
  else
  { printf(RED "%s\n" RESET,
           LOCALIZED_STRING("Unknown command. Use 'cham <indices>' or 'pass'",
                            "Commande inconnue. Utilisez 'cham <indices>' ou 'pass'",
                            "Comando desconocido. Usa 'cham <indices>' o 'pass'"));
    printf(YELLOW "%s\n" RESET,
           LOCALIZED_STRING("Taking damage without defending",
                            "Prendre des degats sans defendre",
                            "Recibir dano sin defender"));
  }

  return NO_ACTION;
}
