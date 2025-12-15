/* ========================================================================
   Display Functions
   ======================================================================== */

#include <stdio.h>
#include "cli_display.h"
#include "game_constants.h"
#include "localization.h"
#include "player_config.h"

/* ANSI color codes */
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define GRAY    "\033[38;2;128;128;128m"
#define BOLD_WHITE   "\033[1;37m"
#define COLOR_P1     "\033[1;36m"
#define COLOR_P2     "\033[1;33m"
#define COLOR_ENERGY MAGENTA
#define COLOR_LUNA   CYAN

/* Visual indicators */
#define ICON_PROMPT ">"

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
  printf(COLOR_P2 "%s (B)" RESET ": " COLOR_ENERGY "HP:%d" RESET
         " " COLOR_LUNA "L:%d" RESET " %s:%d %s:%d\n",
         pconfig->player_names[PLAYER_B],
         gstate->current_energy[PLAYER_B],
         gstate->current_cash_balance[PLAYER_B],
         LOCALIZED_STRING("Hand", "Main", "Mano"),
         gstate->hand[PLAYER_B].size,
         LOCALIZED_STRING("Deck", "Paquet", "Mazo"),
         gstate->deck[PLAYER_B].top + 1);
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
           LOCALIZED_STRING("Play draw/recall card (e.g., 'draw 2')",
                            "Jouer carte piocher/rappeler (ex: 'draw 2')",
                            "Jugar carta robar/recuperar (ej: 'draw 2')"));
    printf("  cash <index>    - %s\n",
           LOCALIZED_STRING("Play exchange card (e.g., 'cash 1')",
                            "Jouer carte echange (ex: 'cash 1')",
                            "Jugar carta intercambio (ej: 'cash 1')"));
    printf("  pass            - %s\n",
           LOCALIZED_STRING("Pass your turn", "Passer votre tour", "Pasar tu turno"));
    printf("  gmst            - %s\n",
           LOCALIZED_STRING("Show game status", "Afficher statut", "Mostrar estado"));
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
