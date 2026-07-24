/* ========================================================================
   Shared Interactive Command Grammar and Rules
   Moved out of ui/cli/cli_input.c so both the CLI and TUI can drive the
   same attack/defense/recall/cash-exchange logic through a UiIO* instead
   of each re-implementing the rules against their own stdio/ncurses I/O.
   ======================================================================== */

#include <stdlib.h>
#include <string.h>

#include "game_commands.h"
#include "../shared/ui_constants.h"
#include "../shared/localization.h"
#include "../../core/game_constants.h"
#include "../../core/card_actions.h"

/* ========================================================================
   Card Selection Input Helpers
   ======================================================================== */

int parse_champion_indices(char* input, uint8_t* indices, int max_count,
                           int hand_size, config_t* cfg, UiIO* io)
{ int count = 0;
  char* token = strtok(input, " ");

  while(token != NULL && count < max_count)
  { int idx = atoi(token);
    if(idx < 1 || idx > hand_size)
    { io->message(io, UI_MSG_ERROR, "%s %d (%s 1-%d)",
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

// Parse multiple card indices with duplicate detection
int parse_card_indices_with_validation(char* input, uint8_t* indices,
                                       int max_count, int hand_size,
                                       config_t* cfg, UiIO* io)
{ int count = 0;
  char* token = strtok(input, " ");

  while(token != NULL && count < max_count)
  { int idx = atoi(token);
    if(idx < 1 || idx > hand_size)
    { io->message(io, UI_MSG_ERROR, "%s %d (%s 1-%d)",
                 LOCALIZED_STRING("Error: Invalid card number",
                                  "Erreur: Numero invalide",
                                  "Error: Numero invalido"),
                 idx,
                 LOCALIZED_STRING("must be", "doit etre", "debe ser"),
                 hand_size);
      return -1;
    }

    // Check for duplicates
    for(int i = 0; i < count; i++)
    { if(indices[i] == (idx - 1))
      { io->message(io, UI_MSG_ERROR, "%s %d",
                   LOCALIZED_STRING("Error: Duplicate card number",
                                    "Erreur: Numero en double",
                                    "Error: Numero duplicado"),
                   idx);
        return -1;
      }
    }

    indices[count++] = idx - 1;
    token = strtok(NULL, " ");
  }

  return count;
}

// Discard selected cards and optionally draw replacements (pure state
// mutation -- no I/O, used as-is by mulligan and discard-to-7).
void discard_and_draw_cards(struct gamestate* gstate, PlayerID player,
                            uint8_t* indices, int count,
                            bool draw_replacements, GameContext* ctx)
{ // Sort indices descending to avoid index shifting issues
  for(int i = 0; i < count - 1; i++)
  { for(int j = i + 1; j < count; j++)
    { if(indices[i] < indices[j])
      { uint8_t temp = indices[i];
        indices[i] = indices[j];
        indices[j] = temp;
      }
    }
  }

  // Discard cards from highest index to lowest
  for(int i = 0; i < count; i++)
  { uint8_t card_idx = gstate->hand[player].cards[indices[i]];
    Hand_remove(&gstate->hand[player], card_idx);
    Discard_add(&gstate->discard[player], card_idx);
  }

  // Draw replacement cards if requested
  if(draw_replacements)
  { for(int i = 0; i < count; i++)
      draw_1_card(gstate, player, ctx);
  }
}

/* ========================================================================
   Champion Play
   ======================================================================== */

// Comparison function for qsort - descending order
static int compare_desc(const void* a, const void* b)
{ return (*(uint8_t*)b - *(uint8_t*)a);
}

int validate_and_play_champions(struct gamestate* gstate, PlayerID player,
                                uint8_t* indices, int count, GameContext* ctx,
                                config_t* cfg, UiIO* io)
{ if(count <= 0) return NO_ACTION;

  int total_cost = 0;

  for(int i = 0; i < count; i++)
  { uint8_t card_idx = gstate->hand[player].cards[indices[i]];
    if(fullDeck[card_idx].card_type != CHAMPION_CARD)
    { io->message(io, UI_MSG_ERROR, "%s %d %s",
                 LOCALIZED_STRING("Error: Card", "Erreur: Carte", "Error: Carta"),
                 indices[i] + 1,
                 LOCALIZED_STRING("is not a champion", "n'est pas un champion",
                                  "no es un campeon"));
      return NO_ACTION;
    }
    total_cost += fullDeck[card_idx].cost;
  }

  if(total_cost > gstate->current_cash_balance[player])
  { io->message(io, UI_MSG_ERROR, "%s (%s %d, %s %d)",
               LOCALIZED_STRING("Error: Not enough lunas",
                                "Erreur: Pas assez de lunas",
                                "Error: No hay suficientes lunas"),
               LOCALIZED_STRING("need", "besoin", "necesita"),
               total_cost,
               LOCALIZED_STRING("have", "avoir", "tienes"),
               gstate->current_cash_balance[player]);
    return NO_ACTION;
  }

  /* CRITICAL FIX: Sort indices in descending order before removal
     This ensures we remove from highest index to lowest, preventing
     index invalidation as cards shift in the array */
  qsort(indices, count, sizeof(uint8_t), compare_desc);

  /* Now remove cards from highest to lowest index */
  for(int i = 0; i < count; i++)
    play_champion(gstate, player, gstate->hand[player].cards[indices[i]], ctx);

  io->message(io, UI_MSG_SUCCESS, "%s %d %s",
             LOCALIZED_STRING("Played", "Joue", "Jugado"),
             count,
             LOCALIZED_STRING("champion(s)", "champion(s)", "campeon(es)"));
  return ACTION_TAKEN;
}

static int process_champion_command(char* input, struct gamestate* gstate,
                                    PlayerID player, GameContext* ctx,
                                    config_t* cfg, UiIO* io)
{ uint8_t indices[3];
  int count = parse_champion_indices(input, indices, 3,
                                     gstate->hand[player].size, cfg, io);
  if(count > 0 && validate_and_play_champions(gstate, player, indices, count, ctx, cfg, io))
    return ACTION_TAKEN;
  return NO_ACTION;
}

/* Recall (draw/recall cards) and cash-exchange command handling lives in
   game_commands_cards.c -- declared in game_commands.h, split out to keep
   both files under the project's file-size guideline. */

/* ========================================================================
   Shared Turn-Action Grammar (attack / defense)
   ======================================================================== */

int game_process_attack_command(char* input_buffer, struct gamestate* gstate,
                                PlayerID player, GameContext* ctx,
                                config_t* cfg, UiIO* io)
{ input_buffer[strcspn(input_buffer, "\n")] = 0;

  if(strncmp(input_buffer, "cham ", 5) == 0)
    return process_champion_command(input_buffer + 5, gstate, player, ctx, cfg, io);
  else if(strncmp(input_buffer, "draw ", 5) == 0)
    return handle_draw_command(gstate, player, input_buffer + 5, ctx, cfg, io);
  else if(strncmp(input_buffer, "cash ", 5) == 0)
    return handle_cash_command(gstate, player, input_buffer + 5, ctx, cfg, io);
  else if(strcmp(input_buffer, "pass") == 0)
  { io->message(io, UI_MSG_WARNING, "%s",
               LOCALIZED_STRING("Passed turn", "Tour passe", "Turno pasado"));
    return ACTION_TAKEN;
  }
  else if(strcmp(input_buffer, "exit") == 0)
    return EXIT_SIGNAL;

  io->message(io, UI_MSG_ERROR, "%s",
             LOCALIZED_STRING("Unknown command. Type 'help' for commands.",
                              "Commande inconnue. Tapez 'help' pour les commandes.",
                              "Comando desconocido. Escribe 'help' para comandos."));
  return NO_ACTION;
}

int game_process_defense_command(char* input_buffer, struct gamestate* gstate,
                                 PlayerID player, GameContext* ctx,
                                 config_t* cfg, UiIO* io)
{ input_buffer[strcspn(input_buffer, "\n")] = 0;

  if(strcmp(input_buffer, "exit") == 0)
    return EXIT_SIGNAL;
  else if(strcmp(input_buffer, "pass") == 0)
  { io->message(io, UI_MSG_WARNING, "%s",
               LOCALIZED_STRING("Taking damage without defending",
                                "Prendre des degats sans defendre",
                                "Recibir dano sin defender"));
    return NO_ACTION;
  }
  else if(strncmp(input_buffer, "cham ", 5) == 0)
  { uint8_t indices[3];
    int count = parse_champion_indices(input_buffer + 5, indices, 3,
                                       gstate->hand[player].size, cfg, io);
    if(count > 0)
    { if(!validate_and_play_champions(gstate, player, indices, count, ctx, cfg, io))
        io->message(io, UI_MSG_WARNING, "%s",
                   LOCALIZED_STRING("Taking damage without defending",
                                    "Prendre des degats sans defendre",
                                    "Recibir dano sin defender"));
    }
    else if(count == 0)
      io->message(io, UI_MSG_WARNING, "%s",
                 LOCALIZED_STRING("No defenders specified, taking damage",
                                  "Aucun defenseur specifie, prendre des degats",
                                  "No se especificaron defensores, recibir dano"));
  }
  else
  { io->message(io, UI_MSG_ERROR, "%s",
               LOCALIZED_STRING("Unknown command. Use 'cham <indices>' or 'pass'",
                                "Commande inconnue. Utilisez 'cham <indices>' ou 'pass'",
                                "Comando desconocido. Usa 'cham <indices>' o 'pass'"));
    io->message(io, UI_MSG_WARNING, "%s",
               LOCALIZED_STRING("Taking damage without defending",
                                "Prendre des degats sans defendre",
                                "Recibir dano sin defender"));
  }

  return NO_ACTION;
}

/* ========================================================================
   Mulligan / Discard-to-7 Grammar
   ======================================================================== */

// Returns 1 when the mulligan phase is done (pass or a valid mulligan was
// applied), 0 to keep prompting. CLI-only "help" (full hand redisplay) is
// intercepted by the caller before falling through to this.
int game_process_mulligan_command(char* input_buffer, struct gamestate* gstate,
                                  GameContext* ctx, config_t* cfg, UiIO* io)
{ input_buffer[strcspn(input_buffer, "\n")] = 0;
  PlayerID player = PLAYER_B;

  if(strcmp(input_buffer, "pass") == 0)
  { io->message(io, UI_MSG_SUCCESS, "%s",
               LOCALIZED_STRING("Keeping current hand", "Conservation de la main",
                                "Manteniendo mano actual"));
    return 1;
  }
  else if(strncmp(input_buffer, "mull ", 5) == 0)
  { uint8_t indices[2];
    int count = parse_card_indices_with_validation(input_buffer + 5, indices, 2,
                                                   gstate->hand[player].size, cfg, io);
    if(count < 0) return 0;
    if(count == 0)
    { io->message(io, UI_MSG_ERROR, "%s",
                 LOCALIZED_STRING("Error: Must specify at least 1 card",
                                  "Erreur: Specifier au moins 1 carte",
                                  "Error: Debe especificar al menos 1 carta"));
      return 0;
    }

    io->message(io, UI_MSG_SUCCESS, "%s %d %s...",
               LOCALIZED_STRING("Mulliganing", "Defausse de", "Descartando"),
               count,
               LOCALIZED_STRING("card(s)", "carte(s)", "carta(s)"));

    discard_and_draw_cards(gstate, player, indices, count, true, ctx);
    return 1;
  }

  io->message(io, UI_MSG_ERROR, "%s",
             LOCALIZED_STRING("Unknown command. Type 'help' for commands.",
                              "Commande inconnue. 'help' pour aide.",
                              "Comando desconocido. 'help' para ayuda."));
  return 0;
}

// Returns 1 when discard-to-7 is done (a valid, exact-count discard was
// applied), 0 to keep prompting. CLI-only "help" is intercepted by the
// caller before falling through to this.
int game_process_discard_command(char* input_buffer, struct gamestate* gstate,
                                 int cards_to_discard, GameContext* ctx,
                                 config_t* cfg, UiIO* io)
{ input_buffer[strcspn(input_buffer, "\n")] = 0;
  PlayerID player = gstate->current_player;

  if(strncmp(input_buffer, "disc ", 5) == 0)
  { uint8_t indices[15];
    int count = parse_card_indices_with_validation(input_buffer + 5, indices,
                                                   cards_to_discard,
                                                   gstate->hand[player].size, cfg, io);
    if(count < 0) return 0;
    if(count != cards_to_discard)
    { io->message(io, UI_MSG_ERROR, "%s %d %s",
                 LOCALIZED_STRING("Error: Must discard exactly",
                                  "Erreur: Doit defausser exactement",
                                  "Error: Debe descartar exactamente"),
                 cards_to_discard,
                 LOCALIZED_STRING(cards_to_discard > 1 ? "cards" : "card",
                                  cards_to_discard > 1 ? "cartes" : "carte",
                                  cards_to_discard > 1 ? "cartas" : "carta"));
      return 0;
    }

    io->message(io, UI_MSG_SUCCESS, "%s %d %s...",
               LOCALIZED_STRING("Discarding", "Defausse de", "Descartando"),
               count,
               LOCALIZED_STRING("card(s)", "carte(s)", "carta(s)"));

    discard_and_draw_cards(gstate, player, indices, count, false, ctx);
    return 1;
  }

  io->message(io, UI_MSG_ERROR, "%s",
             LOCALIZED_STRING("Unknown command. Type 'help' for commands.",
                              "Commande inconnue. 'help' pour aide.",
                              "Comando desconocido. 'help' para ayuda."));
  return 0;
}
