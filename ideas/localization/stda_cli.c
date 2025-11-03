/* ============================================================
   stda_cli.c - Standalone CLI mode implementation
   ============================================================ */

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
#include "localization.h"

#define MAX_COMMAND_LEN 256
#define EXIT_SIGNAL -1
#define ACTION_TAKEN 1
#define NO_ACTION 0

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
#define ICON_PROMPT  ">"
#define ICON_SUCCESS "[OK]"

/* ========================================================================
   Display Functions
   ======================================================================== */

void display_player_prompt(PlayerID player, struct gamestate* gstate, 
                          int is_defense, config_t* cfg)
{ const char* player_color = (player == PLAYER_A) ? COLOR_P1 : COLOR_P2;
  const char* player_name = (player == PLAYER_A) ? 
    LOCALIZED_STRING("Player A", "Joueur A", "Jugador A") :
    LOCALIZED_STRING("Player B", "Joueur B", "Jugador B");
  const char* phase_icon = is_defense ? 
    LOCALIZED_STRING("[DEF]", "[DEF]", "[DEF]") : 
    LOCALIZED_STRING("[ATK]", "[ATQ]", "[ATQ]");

  printf("%s%s" RESET " [" COLOR_ENERGY "HP:%d" RESET " "
         COLOR_LUNA "L:%d" RESET "] %s " ICON_PROMPT " ",
         player_color, player_name,
         gstate->current_energy[player],
         gstate->current_cash_balance[player],
         phase_icon);
}

void display_player_hand(PlayerID player, struct gamestate* gstate, config_t* cfg)
{ printf("\n%s\n", LOCALIZED_STRING("Your hand:", "Votre main:", "Tu mano:"));
  uint8_t* hand_array = HDCLL_toArray(&gstate->hand[player]);

  for(uint8_t i = 0; i < gstate->hand[player].size; i++)
  { uint8_t card_idx = hand_array[i];
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
  free(hand_array);
}

void display_attack_state(struct gamestate* gstate, config_t* cfg)
{ printf("\n" RED "=== %s ===" RESET "\n",
         LOCALIZED_STRING("Combat! You are being attacked",
                         "Combat! Vous etes attaque",
                         "Combate! Estas siendo atacado"));
  printf("%s\n", LOCALIZED_STRING("Attacker's champions in combat:",
                                  "Champions de l'attaquant au combat:",
                                  "Campeones del atacante en combate:"));
  
  struct LLNode* current = gstate->combat_zone[gstate->current_player].head;

  for(uint8_t i = 0; i < gstate->combat_zone[gstate->current_player].size; i++)
  { const struct card* c = &fullDeck[current->data];
    printf("  - %s (D%d+%d)\n", CHAMPION_SPECIES_NAMES[c->species],
           c->defense_dice, c->attack_base);
    current = current->next;
  }
}

void display_game_status(struct gamestate* gstate, config_t* cfg)
{ printf("\n" BOLD_WHITE "=== %s ===" RESET "\n",
         LOCALIZED_STRING("Game Status", "Statut du jeu", "Estado del juego"));
  printf(COLOR_P1 "%s" RESET ": " COLOR_ENERGY "HP:%d" RESET
         " " COLOR_LUNA "L:%d" RESET " %s:%d %s:%d\n",
         LOCALIZED_STRING("Player A", "Joueur A", "Jugador A"),
         gstate->current_energy[PLAYER_A],
         gstate->current_cash_balance[PLAYER_A],
         LOCALIZED_STRING("Hand", "Main", "Mano"),
         gstate->hand[PLAYER_A].size,
         LOCALIZED_STRING("Deck", "Paquet", "Mazo"),
         gstate->deck[PLAYER_A].top + 1);
  printf(COLOR_P2 "%s" RESET ": " COLOR_ENERGY "HP:%d" RESET
         " " COLOR_LUNA "L:%d" RESET " %s:%d %s:%d\n",
         LOCALIZED_STRING("Player B", "Joueur B", "Jugador B"),
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
    indices[count++] = idx - 1;
    token = strtok(NULL, " ");
  }

  return count;
}

int validate_and_play_champions(struct gamestate* gstate, PlayerID player,
                                uint8_t* indices, int count, GameContext* ctx,
                                config_t* cfg)
{ if(count <= 0) return NO_ACTION;

  int total_cost = 0;
  uint8_t* hand_array = HDCLL_toArray(&gstate->hand[player]);

  for(int i = 0; i < count; i++)
  { uint8_t card_idx = hand_array[indices[i]];
    if(fullDeck[card_idx].card_type != CHAMPION_CARD)
    { printf(RED "%s %d %s\n" RESET,
             LOCALIZED_STRING("Error: Card", "Erreur: Carte", "Error: Carta"),
             indices[i] + 1,
             LOCALIZED_STRING("is not a champion", "n'est pas un champion", 
                             "no es un campeon"));
      free(hand_array);
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
    free(hand_array);
    return NO_ACTION;
  }

  for(int i = count - 1; i >= 0; i--)
    play_champion(gstate, player, hand_array[indices[i]], ctx);

  free(hand_array);
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

  uint8_t* hand_array = HDCLL_toArray(&gstate->hand[player]);
  uint8_t card_idx = hand_array[idx - 1];
  free(hand_array);

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

  uint8_t* hand_array = HDCLL_toArray(&gstate->hand[player]);
  uint8_t card_idx = hand_array[idx - 1];
  free(hand_array);

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

static int process_attack_command(char* input_buffer, struct gamestate* gstate, 
                                  GameContext* ctx, config_t* cfg)
{ input_buffer[strcspn(input_buffer, "\n")] = 0;

  if(strncmp(input_buffer, "cham ", 5) == 0)
    return process_champion_command(input_buffer + 5, gstate, PLAYER_A, ctx, cfg);
  else if(strncmp(input_buffer, "draw ", 5) == 0)
    return handle_draw_command(gstate, PLAYER_A, input_buffer + 5, ctx, cfg);
  else if(strncmp(input_buffer, "cash ", 5) == 0)
    return handle_cash_command(gstate, PLAYER_A, input_buffer + 5, ctx, cfg);
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

static int process_defense_command(char* input_buffer, struct gamestate* gstate, 
                                   GameContext* ctx, config_t* cfg)
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
                                       gstate->hand[PLAYER_A].size, cfg);
    if(count > 0)
    { if(!validate_and_play_champions(gstate, PLAYER_A, indices, count, ctx, cfg))
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

/* ========================================================================
   Game Phase Handlers
   ======================================================================== */

static int handle_interactive_attack(struct gamestate* gstate, 
                                     GameContext* ctx, config_t* cfg)
{ char input_buffer[MAX_COMMAND_LEN];
  int action_taken = NO_ACTION;

  while(!action_taken && !gstate->someone_has_zero_energy)
  { printf("\n=== %s %s (%s %d, %s %d) ===\n",
           LOCALIZED_STRING("Player A", "Joueur A", "Jugador A"),
           LOCALIZED_STRING("Turn", "Tour", "Turno"),
           LOCALIZED_STRING("Turn", "Tour", "Turno"),
           gstate->turn,
           LOCALIZED_STRING("Round", "Manche", "Ronda"),
           (uint16_t)((gstate->turn - 1) * 0.5 + 1));
    printf("\n=== %s ===\n",
           LOCALIZED_STRING("Opponent (defender) Status",
                           "Statut de l'adversaire (defenseur)",
                           "Estado del oponente (defensor)"));
    display_player_prompt(PLAYER_B, gstate, 1, cfg);
    printf(" %s:%d\n",
           LOCALIZED_STRING("Hand", "Main", "Mano"),
           gstate->hand[PLAYER_B].size);
    printf("\n");
    display_player_prompt(PLAYER_A, gstate, 0, cfg);
    display_player_hand(PLAYER_A, gstate, cfg);
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

    action_taken = process_attack_command(input_buffer, gstate, ctx, cfg);
    if(action_taken == EXIT_SIGNAL) return EXIT_SIGNAL;
  }

  return EXIT_SUCCESS;
}

static int handle_interactive_defense(struct gamestate* gstate, 
                                      GameContext* ctx, config_t* cfg)
{ char input_buffer[MAX_COMMAND_LEN];

  printf("\n=== %s %s (%s %d, %s %d) ===\n",
         LOCALIZED_STRING("Player A", "Joueur A", "Jugador A"),
         LOCALIZED_STRING("Turn", "Tour", "Turno"),
         LOCALIZED_STRING("Turn", "Tour", "Turno"),
         gstate->turn,
         LOCALIZED_STRING("Round", "Manche", "Ronda"),
         (uint16_t)((gstate->turn - 1) * 0.5 + 1));
  display_attack_state(gstate, cfg);
  
  printf("\n=== %s ===\n",
         LOCALIZED_STRING("Opponent (attacker) Status",
                         "Statut de l'adversaire (attaquant)",
                         "Estado del oponente (atacante)"));
  display_player_prompt(PLAYER_B, gstate, 0, cfg);
  printf(" %s:%d\n",
         LOCALIZED_STRING("Hand", "Main", "Mano"),
         gstate->hand[PLAYER_B].size);
  
  printf("\n\n");
  display_player_prompt(PLAYER_A, gstate, 1, cfg);
  display_player_hand(PLAYER_A, gstate, cfg);
  printf("\n%s\n" ICON_PROMPT " ",
         LOCALIZED_STRING("Defend: 'cham <indices>' (e.g., 'cham 1 2') or 'pass' to take damage",
                         "Defendre: 'cham <indices>' (ex: 'cham 1 2') ou 'pass' pour prendre des degats",
                         "Defender: 'cham <indices>' (ej: 'cham 1 2') o 'pass' para recibir dano"));

  if(fgets(input_buffer, sizeof(input_buffer), stdin) == NULL)
    return EXIT_SUCCESS;

  int result = process_defense_command(input_buffer, gstate, ctx, cfg);
  return (result == EXIT_SIGNAL) ? EXIT_SIGNAL : EXIT_SUCCESS;
}

/* ========================================================================
   Game Turn Execution
   ======================================================================== */

static int execute_game_turn(struct gamestate* gstate, StrategySet* strategies, 
                             GameContext* ctx, config_t* cfg)
{ begin_of_turn(gstate, ctx);

  if(gstate->current_player == PLAYER_A)
  { int result = handle_interactive_attack(gstate, ctx, cfg);
    if(result == EXIT_SIGNAL) return EXIT_SIGNAL;
  }
  else
    attack_phase(gstate, strategies, ctx);

  if(gstate->combat_zone[gstate->current_player].size > 0)
  { if(gstate->current_player == PLAYER_A)
      defense_phase(gstate, strategies, ctx);
    else
    { int result = handle_interactive_defense(gstate, ctx, cfg);
      if(result == EXIT_SIGNAL) return EXIT_SIGNAL;
    }

    resolve_combat(gstate, ctx);
  }

  return EXIT_SUCCESS;
}

/* ========================================================================
   Game Initialization and Cleanup
   ======================================================================== */

static struct gamestate* initialize_cli_game(uint16_t initial_cash,
                                             StrategySet** strategies_out, 
                                             GameContext** ctx_out,
                                             config_t* cfg)
{ GameContext* ctx = create_game_context(cfg->prng_seed, NULL);
  if(ctx == NULL) return NULL;

  StrategySet* strategies = create_strategy_set();
  set_player_strategy(strategies, PLAYER_A,
                      random_attack_strategy, random_defense_strategy);
  set_player_strategy(strategies, PLAYER_B,
                      random_attack_strategy, random_defense_strategy);

  struct gamestate* gstate = malloc(sizeof(struct gamestate));
  setup_game(initial_cash, gstate, ctx);

  *strategies_out = strategies;
  *ctx_out = ctx;
  return gstate;
}

static void cleanup_cli_game(struct gamestate* gstate, StrategySet* strategies, 
                            GameContext* ctx)
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
  destroy_game_context(ctx);
}

/* ========================================================================
   Main CLI Mode Entry Point
   ======================================================================== */

int run_mode_stda_cli(config_t* cfg)
{
  #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
  #endif

  printf("%s\n", LOCALIZED_STRING("Running in command line interface mode...",
                                  "Execution en mode interface de ligne de commande...",
                                  "Ejecutando en modo interfaz de linea de comandos..."));

  StrategySet* strategies;
  GameContext* ctx;
  struct gamestate* gstate = initialize_cli_game(INITIAL_CASH_DEFAULT,
                                                 &strategies, &ctx, cfg);
  if(gstate == NULL)
  { fprintf(stderr, "%s\n", LOCALIZED_STRING("Failed to initialize CLI game",
                                             "Echec de l'initialisation du jeu CLI",
                                             "Error al inicializar el juego CLI"));
    return EXIT_FAILURE;
  }
  
  printf("\n%s %s\n", 
         LOCALIZED_STRING("You are", "Vous etes", "Tu eres"),
         LOCALIZED_STRING("Player A", "Joueur A", "Jugador A"));
  printf("%s %s %s\n",
         LOCALIZED_STRING("Player B", "Joueur B", "Jugador B"),
         LOCALIZED_STRING("is the", "est le moteur", "es el motor"),
         LOCALIZED_STRING("RANDOM AI engine", "IA ALEATOIRE", "IA ALEATORIO"));
  
  printf("\n=== %s ===\n", LOCALIZED_STRING("Game Start", "Debut du jeu", "Inicio del juego"));
  gstate->turn = 0;

  while(gstate->turn < MAX_NUMBER_OF_TURNS &&
        !gstate->someone_has_zero_energy)
  { int result = execute_game_turn(gstate, strategies, ctx, cfg);
    if(result == EXIT_SIGNAL) break;

    if(gstate->someone_has_zero_energy) break;

    collect_1_luna(gstate);
    change_current_player(gstate);
  }

  if(!gstate->someone_has_zero_energy)
    gstate->game_state = DRAW;

  cleanup_cli_game(gstate, strategies, ctx);
  return EXIT_SUCCESS;
}