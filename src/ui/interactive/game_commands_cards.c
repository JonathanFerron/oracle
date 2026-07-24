/* ========================================================================
   Shared Interactive Command Grammar and Rules -- Recall & Cash Exchange
   Split out of game_commands.c to keep both files under the project's
   file-size guideline (mirrors the cli_display.c / cli_action_display.c
   split: core dispatch vs. action-flow/card-selection logic).
   ======================================================================== */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "game_commands.h"
#include "../shared/ui_constants.h"
#include "../shared/localization.h"
#include "../../core/game_constants.h"
#include "../../core/card_actions.h"

/* ========================================================================
   Recall Functionality (draw/recall cards)
   ======================================================================== */

// Ask the player to choose between drawing cards or recalling champions.
// Only called when recall is actually available (enough champions in discard).
char prompt_draw_or_recall(config_t* cfg, UiIO* io)
{ char prompt[128];
  snprintf(prompt, sizeof(prompt), "\n%s > ",
           LOCALIZED_STRING("Choose: (d)raw cards or (r)ecall champions? [d]",
                            "Choisir: (p)iocher ou (r)appeler? [p]",
                            "Elegir: (r)obar o (recu)perar? [r]"));

  char input[MAX_INPUT_LEN_SHORT];
  if(!io->read_line(io, prompt, input, sizeof(input)))
    return 'd';

  if(strlen(input) == 0)
    return 'd';

  char c = tolower((unsigned char)input[0]);
  return (c == 'r' || c == 'a') ? 'r' : 'd';
}

// Recall is exact and mandatory: choosing recall on a card requires selecting
// exactly recall_num champions from discard (no partial recall, no pass).
int handle_recall_choice(struct gamestate* gstate, PlayerID player,
                         uint8_t card_idx, GameContext* ctx, config_t* cfg,
                         UiIO* io)
{ uint8_t recall_num = fullDeck[card_idx].choose_num;
  uint8_t indices[2];
  char input[MAX_COMMAND_LEN];

  uint8_t recallable[40];
  uint8_t recallable_count = collect_champions(gstate->discard[player].cards,
                                               gstate->discard[player].size,
                                               recallable, true);
  io->show_card_list(io,
                     LOCALIZED_STRING("Champions available to recall (sorted by power):",
                                      "Champions disponibles a rappeler (par pouvoir):",
                                      "Campeones disponibles para recuperar (por poder):"),
                     recallable, recallable_count, -1);

  for(;;)
  { uint8_t champions[40];
    uint8_t champ_count = collect_champions(gstate->discard[player].cards,
                                            gstate->discard[player].size,
                                            champions, false);

    char prompt[128];
    snprintf(prompt, sizeof(prompt), "\n%s %d %s: ",
             LOCALIZED_STRING("Select exactly", "Selectionnez exactement",
                              "Selecciona exactamente"),
             recall_num,
             LOCALIZED_STRING("champion(s) to recall (e.g., '1 3')",
                              "champion(s) a rappeler (ex: '1 3')",
                              "campeon(es) a recuperar (ej: '1 3')"));

    if(!io->read_line(io, prompt, input, sizeof(input)))
      return NO_ACTION;

    int count = parse_card_indices_with_validation(input, indices, recall_num,
                                                   champ_count, cfg, io);
    if(count < 0) continue;

    if(count != recall_num)
    { io->message(io, UI_MSG_ERROR, "%s %d %s",
                 LOCALIZED_STRING("Error: You must recall exactly",
                                  "Erreur: Vous devez rappeler exactement",
                                  "Error: Debes recuperar exactamente"),
                 recall_num,
                 LOCALIZED_STRING("champion(s)", "champion(s)", "campeon(es)"));
      continue;
    }

    return validate_and_recall_champions(gstate, player, card_idx, indices,
                                         count, ctx, cfg, io);
  }
}

int validate_and_recall_champions(struct gamestate* gstate, PlayerID player,
                                  uint8_t draw_card_idx, uint8_t* indices,
                                  int count, GameContext* ctx, config_t* cfg,
                                  UiIO* io)
{ uint8_t champions[40];
  collect_champions(gstate->discard[player].cards, gstate->discard[player].size,
                    champions, true);

  // Remove draw/recall card from hand, pay cost, discard it
  Hand_remove(&gstate->hand[player], draw_card_idx);
  gstate->current_cash_balance[player] -= fullDeck[draw_card_idx].cost;
  Discard_add(&gstate->discard[player], draw_card_idx);

  // Recall the selected champions from discard to hand
  for(int i = 0; i < count; i++)
  { uint8_t champion_idx = champions[indices[i]];
    Discard_remove(&gstate->discard[player], champion_idx);
    Hand_add(&gstate->hand[player], champion_idx);
  }

  io->message(io, UI_MSG_SUCCESS, "%s %d %s",
             LOCALIZED_STRING("Recalled", "Rappele", "Recuperado"),
             count,
             LOCALIZED_STRING("champion(s)", "champion(s)", "campeon(es)"));

  return ACTION_TAKEN;
}

int handle_draw_command(struct gamestate* gstate, PlayerID player,
                        char* input, GameContext* ctx, config_t* cfg,
                        UiIO* io)
{ int idx = atoi(input);
  if(idx < 1 || idx > gstate->hand[player].size)
  { io->message(io, UI_MSG_ERROR, "%s (must be 1-%d)",
               LOCALIZED_STRING("Error: Invalid card number",
                                "Erreur: Numero de carte invalide",
                                "Error: Numero de carta invalido"),
               gstate->hand[player].size);
    return NO_ACTION;
  }

  uint8_t card_idx = gstate->hand[player].cards[idx - 1];

  if(fullDeck[card_idx].card_type != DRAW_CARD)
  { io->message(io, UI_MSG_ERROR, "%s",
               LOCALIZED_STRING("Error: Not a draw card",
                                "Erreur: Pas une carte piocher",
                                "Error: No es una carta de robar"));
    return NO_ACTION;
  }

  if(fullDeck[card_idx].cost > gstate->current_cash_balance[player])
  { io->message(io, UI_MSG_ERROR, "%s",
               LOCALIZED_STRING("Error: Not enough lunas",
                                "Erreur: Pas assez de lunas",
                                "Error: No hay suficientes lunas"));
    return NO_ACTION;
  }

  uint8_t recall_num = fullDeck[card_idx].choose_num;
  uint8_t champions[40];
  uint8_t champ_count = collect_champions(gstate->discard[player].cards,
                                          gstate->discard[player].size,
                                          champions, false);

  if(champ_count >= recall_num && prompt_draw_or_recall(cfg, io) == 'r')
    return handle_recall_choice(gstate, player, card_idx, ctx, cfg, io);

  play_draw_card(gstate, player, card_idx, ctx);
  io->message(io, UI_MSG_SUCCESS, "%s",
             LOCALIZED_STRING("Played draw card", "Carte piocher jouee",
                              "Carta de robar jugada"));
  return ACTION_TAKEN;
}

/* ========================================================================
   Cash Exchange
   ======================================================================== */

// Prompt for a single champion index to exchange (mandatory, no pass).
// Returns the chosen champion's card index, or -1 on invalid/EOF input.
int prompt_champion_exchange(Hand* hand, config_t* cfg, UiIO* io)
{ char input[MAX_COMMAND_LEN];
  char prompt[128];
  snprintf(prompt, sizeof(prompt), "\n%s: ",
           LOCALIZED_STRING("Enter champion index (e.g., '1')",
                            "Entrez indice champion (ex: '1')",
                            "Ingresa indice campeon (ej: '1')"));

  if(!io->read_line(io, prompt, input, sizeof(input)))
    return -1;

  uint8_t champions[15];
  uint8_t count = collect_champions(hand->cards, hand->size, champions, false);

  int idx = atoi(input);
  if(idx < 1 || idx > count)
  { io->message(io, UI_MSG_ERROR, "%s %d (%s 1-%d)",
               LOCALIZED_STRING("Error: Invalid index",
                                "Erreur: Indice invalide",
                                "Error: Indice invalido"),
               idx,
               LOCALIZED_STRING("must be", "doit etre", "debe ser"),
               count);
    return -1;
  }

  return champions[idx - 1];
}

int handle_cash_command(struct gamestate* gstate, PlayerID player,
                        char* input, GameContext* ctx, config_t* cfg,
                        UiIO* io)
{ int idx = atoi(input);
  if(idx < 1 || idx > gstate->hand[player].size)
  { io->message(io, UI_MSG_ERROR, "%s (must be 1-%d)",
               LOCALIZED_STRING("Error: Invalid card number",
                                "Erreur: Numero de carte invalide",
                                "Error: Numero de carta invalido"),
               gstate->hand[player].size);
    return NO_ACTION;
  }

  uint8_t card_idx = gstate->hand[player].cards[idx - 1];

  if(fullDeck[card_idx].card_type != CASH_CARD)
  { io->message(io, UI_MSG_ERROR, "%s",
               LOCALIZED_STRING("Error: Not an exchange card",
                                "Erreur: Pas une carte echange",
                                "Error: No es una carta de intercambio"));
    return NO_ACTION;
  }

  if(!has_champion_in_hand(&gstate->hand[player]))
  { io->message(io, UI_MSG_ERROR, "%s",
               LOCALIZED_STRING("Error: No champions to exchange",
                                "Erreur: Aucun champion a echanger",
                                "Error: No hay campeones para intercambiar"));
    return NO_ACTION;
  }

  uint8_t champions[15];
  uint8_t count = collect_champions(gstate->hand[player].cards,
                                    gstate->hand[player].size, champions, false);

  // Mark the lowest-power champion as suggested, matching the AI's own
  // cash-exchange heuristic (select_champion_for_cash_exchange()).
  uint8_t suggested_card = select_champion_for_cash_exchange(&gstate->hand[player]);
  int suggested_idx = -1;
  for(int i = 0; i < count; i++)
  { if(champions[i] == suggested_card)
    { suggested_idx = i;
      break;
    }
  }

  io->show_card_list(io,
                     LOCALIZED_STRING("Choose a champion to exchange for 5 lunas.",
                                      "Choisir un champion a echanger pour 5 lunas.",
                                      "Elige un campeon para cambiar por 5 lunas."),
                     champions, count, suggested_idx);

  int champion_idx = prompt_champion_exchange(&gstate->hand[player], cfg, io);
  if(champion_idx < 0)
    return NO_ACTION;

  play_cash_card_interactive(gstate, player, card_idx, (uint8_t)champion_idx, ctx);
  io->message(io, UI_MSG_SUCCESS, "%s %s",
             LOCALIZED_STRING("Exchanged", "Echange", "Cambiado"),
             CHAMPION_SPECIES_NAMES[fullDeck[champion_idx].species]);
  return ACTION_TAKEN;
}
