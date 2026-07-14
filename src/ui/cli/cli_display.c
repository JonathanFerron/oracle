/* ========================================================================
   Display Functions
   ======================================================================== */

#include <stdio.h>
#include "cli_display.h"
#include "../../core/game_constants.h"
#include "../../core/card_actions.h"
#include "../shared/localization.h"
#include "../shared/player_config.h"

/* ========================================================================
   Basic Display Functions
   ======================================================================== */

void display_player_prompt(PlayerID player, struct gamestate* gstate,
                           int is_defense, config_t* cfg)
{ const char* player_color = (player == PLAYER_A) ? COLOR_P1 : COLOR_P2;

  PlayerConfig* pconfig = (PlayerConfig*)cfg->player_config;
  const char* player_name = pconfig->player_names[player];
  const char* position = (player == PLAYER_A) ? "A" : "B";

  const char* phase_icon = is_defense ?
                           LOCALIZED_STRING("[DEF]", "[DEF]", "[DEF]") :
                           LOCALIZED_STRING("[ATK]", "[ATQ]", "[ATQ]");

  printf("%s%s (%s)" RESET " [" COLOR_ENERGY "HP:%d" RESET " "
         COLOR_LUNA "L:%d" RESET "] %s " ICON_PROMPT " ",
         player_color, player_name, position,
         gstate->current_energy[player],
         gstate->current_cash_balance[player],
         phase_icon);
}

void display_player_hand(PlayerID player, struct gamestate* gstate, config_t* cfg)
{ printf("\n%s\n", LOCALIZED_STRING("Your hand:", "Votre main:", "Tu mano:"));

  for(uint8_t i = 0; i < gstate->hand[player].size; i++)
  { uint8_t card_idx = gstate->hand[player].cards[i];
    const struct card* c = &fullDeck[card_idx];

    if(c->card_type == CHAMPION_CARD)
    { const char* color = (c->color == COLOR_INDIGO) ? BLUE :
                          (c->color == COLOR_ORANGE) ? YELLOW : RED;
      printf("  [%d] %s%s" RESET " (D%d+%d, " CYAN "L%d" RESET ")\n",
             i + 1, color, CHAMPION_SPECIES_NAMES[c->species],
             c->defense_dice, c->attack_base, c->cost);
    }
    else if(c->card_type == DRAW_CARD)
    { printf("  [%d] " GREEN "%s %d" RESET " (" CYAN "L%d" RESET ")\n",
             i + 1, LOCALIZED_STRING("Draw", "Piocher", "Robar"),
             c->draw_num, c->cost);
    }
    else if(c->card_type == CASH_CARD)
    { printf("  [%d] " GRAY "%s %d %s" RESET " (" CYAN "L%d" RESET ")\n",
             i + 1,
             LOCALIZED_STRING("Exchange for", "Echanger pour", "Cambiar por"),
             c->exchange_cash,
             LOCALIZED_STRING("lunas", "lunas", "lunas"),
             c->cost);
    }
  }
}

void display_attack_state(struct gamestate* gstate, config_t* cfg)
{ printf("\n" RED "=== %s ===" RESET "\n",
         LOCALIZED_STRING("Combat! You are being attacked",
                          "Combat! Vous etes attaque",
                          "Combate! Estas siendo atacado"));
  printf("%s\n", LOCALIZED_STRING("Attacker's champions in combat:",
                                  "Champions de l'attaquant au combat:",
                                  "Campeones del atacante en combate:"));

  for(uint8_t i = 0; i < gstate->combat_zone[gstate->current_player].size; i++)
  { uint8_t card_idx = gstate->combat_zone[gstate->current_player].cards[i];
    const struct card* c = &fullDeck[card_idx];
    printf("  - %s (D%d+%d)\n", CHAMPION_SPECIES_NAMES[c->species],
           c->defense_dice, c->attack_base);
  }
}

void display_game_status(struct gamestate* gstate, config_t* cfg)
{ printf("\n" BOLD_WHITE "=== %s ===" RESET "\n",
         LOCALIZED_STRING("Game Status", "Statut du jeu", "Estado del juego"));
  PlayerConfig* pconfig = (PlayerConfig*)cfg->player_config;
  printf(COLOR_P1 "%s (A)" RESET ": " COLOR_ENERGY "HP:%d" RESET
         " " COLOR_LUNA "L:%d" RESET " %s:%d %s:%d\n",
         pconfig->player_names[PLAYER_A],
         gstate->current_energy[PLAYER_A],
         gstate->current_cash_balance[PLAYER_A],
         LOCALIZED_STRING("Hand", "Main", "Mano"),
         gstate->hand[PLAYER_A].size,
         LOCALIZED_STRING("Deck", "Paquet", "Mazo"),
         gstate->deck[PLAYER_A].top + 1);
  display_player_discard(PLAYER_A, gstate, cfg);
  printf(COLOR_P2 "%s (B)" RESET ": " COLOR_ENERGY "HP:%d" RESET
         " " COLOR_LUNA "L:%d" RESET " %s:%d %s:%d\n",
         pconfig->player_names[PLAYER_B],
         gstate->current_energy[PLAYER_B],
         gstate->current_cash_balance[PLAYER_B],
         LOCALIZED_STRING("Hand", "Main", "Mano"),
         gstate->hand[PLAYER_B].size,
         LOCALIZED_STRING("Deck", "Paquet", "Mazo"),
         gstate->deck[PLAYER_B].top + 1);
  display_player_discard(PLAYER_B, gstate, cfg);
}

void display_cli_help(int is_defense, config_t* cfg)
{ printf("\n" BOLD_WHITE "=== %s ===" RESET "\n",
         LOCALIZED_STRING("Commands", "Commandes", "Comandos"));
  if(is_defense)
  { printf("  cham <indices>  - %s\n",
           LOCALIZED_STRING("Defend with 1-3 champions (e.g., 'cham 1 2')",
                            "Defendre avec 1-3 champions (ex: 'cham 1 2')",
                            "Defender con 1-3 campeones (ej: 'cham 1 2')"));
    printf("  pass            - %s\n",
           LOCALIZED_STRING("Take damage without defending",
                            "Prendre des degats sans defendre",
                            "Recibir dano sin defender"));
  }
  else
  { printf("  cham <indices>  - %s\n",
           LOCALIZED_STRING("Attack with 1-3 champions (e.g., 'cham 1 3')",
                            "Attaquer avec 1-3 champions (ex: 'cham 1 3')",
                            "Atacar con 1-3 campeones (ej: 'cham 1 3')"));
    printf("  draw <index>    - %s\n",
           LOCALIZED_STRING("Play draw/recall card (e.g., 'draw 2'). Offers recall if enough champions in discard.",
                            "Jouer carte piocher/rappeler (ex: 'draw 2'). Propose le rappel si assez de champions en defausse.",
                            "Jugar carta robar/recuperar (ej: 'draw 2'). Ofrece recuperar si hay suficientes campeones en descarte."));
    printf("  cash <index>    - %s\n",
           LOCALIZED_STRING("Play exchange card (e.g., 'cash 1'). Choose champion to exchange.",
                            "Jouer carte echange (ex: 'cash 1'). Choisir champion a echanger.",
                            "Jugar carta intercambio (ej: 'cash 1'). Elegir campeon a cambiar."));
    printf("  pass            - %s\n",
           LOCALIZED_STRING("Pass your turn", "Passer votre tour", "Pasar tu turno"));
    printf("  gmst            - %s\n",
           LOCALIZED_STRING("Show game status", "Afficher statut", "Mostrar estado"));
    printf("  shod            - %s\n",
           LOCALIZED_STRING("Show detailed discard piles",
                            "Afficher defausses en detail",
                            "Mostrar descartes en detalle"));
  }
  printf("  help            - %s\n",
         LOCALIZED_STRING("Show this help", "Afficher cette aide", "Mostrar esta ayuda"));
  printf("  exit            - %s\n\n",
         LOCALIZED_STRING("Quit game", "Quitter le jeu", "Salir del juego"));
}

/* ========================================================================
   New Display Functions
   ======================================================================== */

void display_turn_header(PlayerID player, PlayerID opponent,
                         struct gamestate* gstate, config_t* cfg)
{ PlayerConfig* pconfig = (PlayerConfig*)cfg->player_config;
  const char* player_name = pconfig->player_names[player];
  const char* opponent_name = pconfig->player_names[opponent];

  printf("\n=== %s's %s (%s %d, %s %d) ===\n",
         player_name,
         LOCALIZED_STRING("Turn", "Tour", "Turno"),
         LOCALIZED_STRING("Turn", "Tour", "Turno"),
         gstate->turn,
         LOCALIZED_STRING("Round", "Manche", "Ronda"),
         (uint16_t)((gstate->turn - 1) * 0.5 + 1));

  printf("\n=== %s (%s) ===\n",
         opponent_name,
         LOCALIZED_STRING("Defender", "Defenseur", "Defensor"));
}

void display_game_summary(struct gamestate* gstate, config_t* cfg)
{ PlayerConfig* pconfig = (PlayerConfig*)cfg->player_config;

  printf("\n" BOLD_WHITE "=== %s ===" RESET "\n",
         LOCALIZED_STRING("Game Over", "Fin du jeu", "Juego terminado"));

  const char* winner_name = NULL;

  switch(gstate->game_state)
  { case PLAYER_A_WINS:
      winner_name = pconfig->player_names[PLAYER_A];
      printf(GREEN "%s %s!\n" RESET,
             winner_name,
             LOCALIZED_STRING("wins", "gagne", "gana"));
      break;

    case PLAYER_B_WINS:
      winner_name = pconfig->player_names[PLAYER_B];
      printf(GREEN "%s %s!\n" RESET,
             winner_name,
             LOCALIZED_STRING("wins", "gagne", "gana"));
      break;

    case DRAW:
      printf(YELLOW "%s\n" RESET,
             LOCALIZED_STRING("Game ended in a draw",
                              "Partie terminee par un match nul",
                              "Juego termino en empate"));
      break;

    default:
      break;
  }

  /* Display final statistics */
  printf("\n%s:\n",
         LOCALIZED_STRING("Final Status", "Statut final", "Estado final"));

  for(int i = 0; i < 2; i++)
  { PlayerID pid = (PlayerID)i;
    const char* name = pconfig->player_names[pid];
    const char* pos = (pid == PLAYER_A) ? "A" : "B";
    const char* color = (pid == PLAYER_A) ? COLOR_P1 : COLOR_P2;

    printf("  %s%s (%s)" RESET ": " COLOR_ENERGY "HP:%d" RESET
           " " COLOR_LUNA "L:%d" RESET " %s:%d\n",
           color, name, pos,
           gstate->current_energy[pid],
           gstate->current_cash_balance[pid],
           LOCALIZED_STRING("Cards", "Cartes", "Cartas"),
           gstate->hand[pid].size);
  }

  printf("\n%s: %d (%s: %d)\n",
         LOCALIZED_STRING("Total turns", "Tours totaux", "Turnos totales"),
         gstate->turn,
         LOCALIZED_STRING("Rounds", "Manches", "Rondas"),
         (uint16_t)((gstate->turn - 1) * 0.5 + 1));
}

/* ========================================================================
   Card Display with Power
   ======================================================================== */

void display_card_with_power(uint8_t card_idx, int display_num,
                             int show_power, config_t* cfg)
{ const struct card* c = &fullDeck[card_idx];

  if(c->card_type == CHAMPION_CARD)
  { const char* color = (c->color == COLOR_INDIGO) ? BLUE :
                        (c->color == COLOR_ORANGE) ? YELLOW : RED;
    if(show_power)
    { printf("  [%d] %s%s" RESET " (D%d+%d, " CYAN "L%d" RESET
             ", pwr:%.1f)\n",
             display_num, color, CHAMPION_SPECIES_NAMES[c->species],
             c->defense_dice, c->attack_base, c->cost, c->power);
    }
    else
    { printf("  [%d] %s%s" RESET " (D%d+%d, " CYAN "L%d" RESET ")\n",
             display_num, color, CHAMPION_SPECIES_NAMES[c->species],
             c->defense_dice, c->attack_base, c->cost);
    }
  }
  else if(c->card_type == DRAW_CARD)
  { const char* label = LOCALIZED_STRING("Draw", "Piocher", "Robar");
    if(show_power)
    { printf("  [%d] " GREEN "%s %d" RESET " (" CYAN "L%d" RESET
             ", pwr:%.1f)\n",
             display_num, label, c->draw_num, c->cost, c->power);
    }
    else
    { printf("  [%d] " GREEN "%s %d" RESET " (" CYAN "L%d" RESET ")\n",
             display_num, label, c->draw_num, c->cost);
    }
  }
  else if(c->card_type == CASH_CARD)
  { const char* label = LOCALIZED_STRING("Exchange for",
                                         "Echanger pour",
                                         "Cambiar por");
    if(show_power)
    { printf("  [%d] " GRAY "%s %d lunas" RESET
             " (" CYAN "L%d" RESET ", pwr:%.1f)\n",
             display_num, label, c->exchange_cash, c->cost, c->power);
    }
    else
    { printf("  [%d] " GRAY "%s %d lunas" RESET
             " (" CYAN "L%d" RESET ")\n",
             display_num, label, c->exchange_cash, c->cost);
    }
  }
}

/* ========================================================================
   Mulligan Display
   ======================================================================== */

void display_mulligan_prompt(struct gamestate* gstate,
                             PlayerID player, config_t* cfg)
{ printf("\n" YELLOW "=== %s ===" RESET "\n",
         LOCALIZED_STRING("Mulligan Phase (Player B)",
                          "Phase de Mulligan (Joueur B)",
                          "Fase de Mulligan (Jugador B)"));

  printf("%s\n",
         LOCALIZED_STRING("You may discard up to 2 cards and draw replacements.",
                          "Vous pouvez defausser jusqu'a 2 cartes et en piocher.",
                          "Puedes descartar hasta 2 cartas y robar reemplazos."));

  printf("Tip: %s %.2f\n\n",
         LOCALIZED_STRING("Consider discarding cards with power <",
                          "Envisagez de defausser les cartes avec pouvoir <",
                          "Considera descartar cartas con poder <"),
         AVERAGE_POWER_FOR_MULLIGAN);

  printf("%s:\n",
         LOCALIZED_STRING("Your starting hand",
                          "Votre main initiale",
                          "Tu mano inicial"));

  for(uint8_t i = 0; i < gstate->hand[player].size; i++)
    display_card_with_power(gstate->hand[player].cards[i], i + 1, 1, cfg);

  printf("\n%s:\n",
         LOCALIZED_STRING("Commands",
                          "Commandes",
                          "Comandos"));
  printf("  mull <indices>  - %s\n",
         LOCALIZED_STRING("Mulligan 1-2 cards (e.g., 'mull 1 3')",
                          "Defausser 1-2 cartes (ex: 'mull 1 3')",
                          "Descartar 1-2 cartas (ej: 'mull 1 3')"));
  printf("  pass            - %s\n",
         LOCALIZED_STRING("Keep current hand",
                          "Garder la main actuelle",
                          "Mantener mano actual"));
  printf("  help            - %s\n\n%s ",
         LOCALIZED_STRING("Show this help",
                          "Afficher cette aide",
                          "Mostrar ayuda"),
         ICON_PROMPT);
}

/* ========================================================================
   Discard-to-7 Display
   ======================================================================== */

void display_discard_prompt(struct gamestate* gstate,
                            PlayerID player, config_t* cfg)
{ int excess = gstate->hand[player].size - 7;

  printf("\n" YELLOW "=== %s ===" RESET "\n",
         LOCALIZED_STRING("Discard Phase",
                          "Phase de Defausse",
                          "Fase de Descarte"));

  printf("%s %d %s. %s %d %s.\n",
         LOCALIZED_STRING("You have", "Vous avez", "Tienes"),
         gstate->hand[player].size,
         LOCALIZED_STRING("cards", "cartes", "cartas"),
         LOCALIZED_STRING("You must discard", "Vous devez defausser",
                          "Debes descartar"),
         excess,
         LOCALIZED_STRING(excess > 1 ? "cards" : "card",
                          excess > 1 ? "cartes" : "carte",
                          excess > 1 ? "cartas" : "carta"));

  printf("Tip: %s\n\n",
         LOCALIZED_STRING("Consider discarding lowest power cards",
                          "Envisagez de defausser les cartes faibles",
                          "Considera descartar las cartas mas debiles"));

  printf("%s:\n",
         LOCALIZED_STRING("Your hand", "Votre main", "Tu mano"));

  for(uint8_t i = 0; i < gstate->hand[player].size; i++)
    display_card_with_power(gstate->hand[player].cards[i], i + 1, 1, cfg);

  printf("\n%s:\n",
         LOCALIZED_STRING("Commands", "Commandes", "Comandos"));
  printf("  disc <indices>  - %s\n",
         LOCALIZED_STRING("Discard cards (e.g., 'disc 2 5')",
                          "Defausser cartes (ex: 'disc 2 5')",
                          "Descartar cartas (ej: 'disc 2 5')"));
  printf("  help            - %s\n\n%s ",
         LOCALIZED_STRING("Show this help",
                          "Afficher cette aide",
                          "Mostrar ayuda"),
         ICON_PROMPT);
}

/* ========================================================================
   Discard Pile Display
   ======================================================================== */

// Summary counts (champions/draw/cash) for one player's discard pile
void display_player_discard(PlayerID player, struct gamestate* gstate, config_t* cfg)
{ PlayerConfig* pconfig = (PlayerConfig*)cfg->player_config;
  const char* player_color = (player == PLAYER_A) ? COLOR_P1 : COLOR_P2;
  const char* player_name = pconfig->player_names[player];
  Discard* discard = &gstate->discard[player];

  uint8_t champion_count = 0, draw_count = 0, cash_count = 0;
  for(uint8_t i = 0; i < discard->size; i++)
  { switch(fullDeck[discard->cards[i]].card_type)
    { case CHAMPION_CARD:
        champion_count++;
        break;
      case DRAW_CARD:
        draw_count++;
        break;
      case CASH_CARD:
        cash_count++;
        break;
    }
  }

  printf("%s%s %s" RESET " (%d): %s:%d %s:%d %s:%d\n",
         player_color, player_name,
         LOCALIZED_STRING("Discard", "Defausse", "Descarte"),
         discard->size,
         LOCALIZED_STRING("Champions", "Champions", "Campeones"), champion_count,
         LOCALIZED_STRING("Draw", "Piocher", "Robar"), draw_count,
         LOCALIZED_STRING("Cash", "Echange", "Intercambio"), cash_count);
}

// Detailed discard pile contents: champions sorted by descending power,
// followed by draw and cash cards
void display_player_discard_detailed(PlayerID player, struct gamestate* gstate, config_t* cfg)
{ PlayerConfig* pconfig = (PlayerConfig*)cfg->player_config;
  const char* player_color = (player == PLAYER_A) ? COLOR_P1 : COLOR_P2;
  const char* player_name = pconfig->player_names[player];
  Discard* discard = &gstate->discard[player];

  printf("%s%s %s" RESET " (%d %s):\n",
         player_color, player_name,
         LOCALIZED_STRING("Discard", "Defausse", "Descarte"),
         discard->size,
         LOCALIZED_STRING("cards", "cartes", "cartas"));

  if(discard->size == 0)
  { printf("  %s\n", LOCALIZED_STRING("(empty)", "(vide)", "(vacio)"));
    return;
  }

  uint8_t champions[40];
  uint8_t champ_count = collect_champions(discard->cards, discard->size, champions, true);
  uint8_t display_num = 1;
  for(uint8_t i = 0; i < champ_count; i++)
    display_card_with_power(champions[i], display_num++, 1, cfg);

  for(uint8_t i = 0; i < discard->size; i++)
  { uint8_t card_idx = discard->cards[i];
    if(fullDeck[card_idx].card_type != CHAMPION_CARD)
      display_card_with_power(card_idx, display_num++, 1, cfg);
  }
}

/* ========================================================================
   Recall Display
   ======================================================================== */

// List champions available in discard for recall, sorted by descending power
void display_recallable_champions(Discard* discard, config_t* cfg)
{ printf("\n%s:\n",
         LOCALIZED_STRING("Champions available to recall (sorted by power)",
                          "Champions disponibles a rappeler (par pouvoir)",
                          "Campeones disponibles para recuperar (por poder)"));

  uint8_t champions[40];
  uint8_t count = collect_champions(discard->cards, discard->size, champions, true);

  if(count == 0)
  { printf("  %s\n", LOCALIZED_STRING("(no champions)", "(aucun champion)", "(sin campeones)"));
    return;
  }

  for(uint8_t i = 0; i < count; i++)
    display_card_with_power(champions[i], i + 1, 1, cfg);
}

/* ========================================================================
   Cash Exchange Display
   ======================================================================== */

// List champions in hand available for cash exchange, marking the
// lowest-power one as suggested (matches the AI heuristic)
void display_exchangeable_champions(Hand* hand, config_t* cfg)
{ printf("\n%s\n",
         LOCALIZED_STRING("Choose a champion to exchange for 5 lunas.",
                          "Choisir un champion a echanger pour 5 lunas.",
                          "Elige un campeon para cambiar por 5 lunas."));

  uint8_t champions[15];
  uint8_t count = collect_champions(hand->cards, hand->size, champions, false);

  if(count == 0)
  { printf("  %s\n", LOCALIZED_STRING("(no champions)", "(aucun champion)", "(sin campeones)"));
    return;
  }

  float min_power = 1000.0;
  uint8_t min_idx = 0;
  for(uint8_t i = 0; i < count; i++)
  { if(fullDeck[champions[i]].power < min_power)
    { min_power = fullDeck[champions[i]].power;
      min_idx = i;
    }
  }

  for(uint8_t i = 0; i < count; i++)
  { display_card_with_power(champions[i], i + 1, 1, cfg);
    if(i == min_idx)
      printf("      " GRAY "%s" RESET "\n",
             LOCALIZED_STRING("^ suggested", "^ suggere", "^ sugerido"));
  }
}

/* ========================================================================
   Combat Results Display
   ======================================================================== */

static void display_combat_side(int count, const ChampionSpecies* species,
                                const uint8_t* dice, const uint8_t* rolls,
                                const uint8_t* base, const int16_t* totals,
                                int show_base)
{ for(int i = 0; i < count; i++)
  { printf("  - %s: D%d [%d]", CHAMPION_SPECIES_NAMES[species[i]], dice[i], rolls[i]);
    if(show_base)
      printf(" + %d = " CYAN "%d" RESET, base[i], totals[i]);
    printf("\n");
  }
}

// Detailed per-champion combat breakdown: rolls, combo bonus, damage,
// and the defender's energy transition. Interactive CLI only (see combat.h).
void display_combat_details_cli(struct gamestate* gstate, CombatDetails* details, config_t* cfg)
{ PlayerConfig* pconfig = (PlayerConfig*)cfg->player_config;
  PlayerID attacker = gstate->current_player;
  PlayerID defender = 1 - attacker;
  const char* attacker_color = (attacker == PLAYER_A) ? COLOR_P1 : COLOR_P2;
  const char* defender_color = (defender == PLAYER_A) ? COLOR_P1 : COLOR_P2;

  printf("\n" BOLD_WHITE "=== %s ===" RESET "\n",
         LOCALIZED_STRING("Combat Resolution", "Resolution du combat", "Resolucion del combate"));

  printf("%s%s" RESET " (%s):\n", attacker_color, pconfig->player_names[attacker],
         LOCALIZED_STRING("Attacker", "Attaquant", "Atacante"));
  display_combat_side(details->num_attackers, details->attacker_species,
                      details->attacker_dice, details->attacker_rolls,
                      details->attacker_base, details->attacker_total, 1);
  if(details->attack_combo > 0)
    printf("  " GREEN "%s: +%d" RESET "\n",
           LOCALIZED_STRING("Combo bonus", "Bonus combo", "Bono combo"), details->attack_combo);
  printf("  " BOLD_WHITE "%s: %d" RESET "\n",
         LOCALIZED_STRING("Total attack", "Attaque totale", "Ataque total"), details->total_attack);

  printf("\n%s%s" RESET " (%s):\n", defender_color, pconfig->player_names[defender],
         LOCALIZED_STRING("Defender", "Defenseur", "Defensor"));
  if(details->num_defenders == 0)
    printf("  %s\n", LOCALIZED_STRING("No defense", "Aucune defense", "Sin defensa"));
  else
  { display_combat_side(details->num_defenders, details->defender_species,
                        details->defender_dice, details->defender_rolls,
                        NULL, details->defender_total, 0);
    if(details->defense_combo > 0)
      printf("  " GREEN "%s: +%d" RESET "\n",
             LOCALIZED_STRING("Combo bonus", "Bonus combo", "Bono combo"), details->defense_combo);
    printf("  " BOLD_WHITE "%s: %d" RESET "\n",
           LOCALIZED_STRING("Total defense", "Defense totale", "Defensa total"), details->total_defense);
  }

  printf("\n");
  if(details->damage > 0)
    printf(RED "%s: %d" RESET " (%d - %d)\n",
           LOCALIZED_STRING("Damage", "Degats", "Dano"),
           details->damage, details->total_attack, details->total_defense);
  else
    printf(GREEN "%s\n" RESET,
           LOCALIZED_STRING("Attack blocked! No damage.", "Attaque bloquee! Aucun degat.",
                            "Ataque bloqueado! Sin dano."));

  printf("%s%s" RESET ": " COLOR_ENERGY "%d" RESET " -> " COLOR_ENERGY "%d" RESET "\n",
         defender_color, pconfig->player_names[defender],
         details->defender_energy_before, details->defender_energy_after);
}
