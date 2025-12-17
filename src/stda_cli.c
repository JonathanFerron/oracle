/* ============================================================
   stda_cli.c - Standalone CLI mode implementation
   ============================================================ */

#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
  #include <windows.h>
#endif

#include "stda_cli.h"
#include "cli_display.h"
#include "cli_input.h"
#include "cli_game.h"
#include "game_types.h"
#include "game_constants.h"
#include "strategy.h"
#include "game_state.h"
#include "turn_logic.h"
#include "localization.h"
#include "player_selection.h"
#include "player_config.h"
#include "card_actions.h"
#include "stda_auto.h"

/* ANSI color codes */
#define RESET   "\033[0m"
#define COLOR_P1     "\033[1;36m"
#define COLOR_P2     "\033[1;33m"

/* ========================================================================
   Player Configuration Setup
   ======================================================================== */

static int setup_player_configuration(config_t* cfg, PlayerConfig* pconfig)
{ /* Initialize player configuration */
  init_player_config(pconfig);
  cfg->player_config = pconfig;

  /* Get player type selection from user */
  display_player_selection_menu(cfg);
  int choice = get_player_type_choice(cfg);
  apply_player_selection(pconfig, cfg, choice);

  /* Get player names */
  get_player_names(cfg, pconfig);

  /* Get AI strategies for AI players */
  get_ai_strategies(cfg, pconfig);

  return EXIT_SUCCESS;
}

static void display_configuration_summary(PlayerConfig* pconfig, config_t* cfg)
{ printf("\n=== %s ===\n",
         LOCALIZED_STRING("Game Configuration",
                          "Configuration du jeu",
                          "Configuracion del juego"));

  for(int i = 0; i < 2; i++)
  { PlayerID pid = (PlayerID)i;
    const char* pos = (pid == PLAYER_A) ? "A" : "B";
    const char* name = pconfig->player_names[pid];

    if(pconfig->player_types[pid] == INTERACTIVE_PLAYER)
    { printf("%s %s: %s (%s)\n",
             LOCALIZED_STRING("Player", "Joueur", "Jugador"),
             pos, name,
             LOCALIZED_STRING("Human", "Humain", "Humano"));
    }
    else
    { const char* strat = get_strategy_display_name(
                            pconfig->ai_strategies[pid], cfg->language);
      printf("%s %s: %s (AI - %s)\n",
             LOCALIZED_STRING("Player", "Joueur", "Jugador"),
             pos, name, strat);
    }
  }
}

/* ========================================================================
   Main Game Loop
   ======================================================================== */

static int run_game_loop(struct gamestate* gstate, StrategySet* strategies,
                        GameContext* ctx, config_t* cfg)
{ /*printf("\n=== %s ===\n",
         LOCALIZED_STRING("Game Start", "Début du jeu", "Inicio del juego"));
  gstate->turn = 0;*/

  while(gstate->turn < MAX_NUMBER_OF_TURNS &&
        !gstate->someone_has_zero_energy)
  { int result = execute_game_turn(gstate, strategies, ctx, cfg);
    
    /* Check for EXIT_SIGNAL and break out of game loop */
    if(result == EXIT_SIGNAL)
    { printf("\n%s\n",
             LOCALIZED_STRING("Game exited by player",
                              "Jeu quitté par le joueur",
                              "Juego cerrado por el jugador"));
      return EXIT_SUCCESS;  /* Clean exit */
    }
    
    if(result == EXIT_FAILURE) return EXIT_FAILURE;

    if(gstate->someone_has_zero_energy) break;

    collect_1_luna(gstate);
    
    /* Discard-to-7 phase */
    PlayerConfig* pconfig = (PlayerConfig*)cfg->player_config;
    if (pconfig->player_types[gstate->current_player] == INTERACTIVE_PLAYER) {
        handle_interactive_discard_to_7(gstate, ctx, cfg);
    } else {
        discard_to_7_cards(gstate, ctx);
    }
    
    change_current_player(gstate);
  }

  if(!gstate->someone_has_zero_energy)
    gstate->game_state = DRAW;

  return EXIT_SUCCESS;
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

  /* Setup player configuration */
  PlayerConfig pconfig;
  if(setup_player_configuration(cfg, &pconfig) != EXIT_SUCCESS)
    return EXIT_FAILURE;

  /* Create game context (needed for random assignment) */
  GameContext* ctx = create_game_context(cfg);
  if(ctx == NULL)
  { fprintf(stderr, "%s\n",
            LOCALIZED_STRING("Failed to create game context",
                             "Echec de creation du contexte",
                             "Error al crear contexto"));
    return EXIT_FAILURE;
  }

  /* Get player assignment mode and apply */
  get_player_assignment(&pconfig, cfg);
  apply_player_assignment(&pconfig, cfg, ctx);

  /* Initialize game */
  StrategySet* strategies;
  struct gamestate* gstate = initialize_cli_game(INITIAL_CASH_DEFAULT,
                                                 &strategies, cfg, ctx);
  if(gstate == NULL)
  { fprintf(stderr, "%s\n",
            LOCALIZED_STRING("Failed to initialize CLI game",
                             "Echec de l'initialisation du jeu CLI",
                             "Error al inicializar el juego CLI"));
    destroy_game_context(ctx);
    return EXIT_FAILURE;
  }

  /* Display player configuration summary */
  display_configuration_summary(&pconfig, cfg);

  /* Display game start */
  printf("\n=== %s ===\n",
         LOCALIZED_STRING("Game Start", "Début du jeu", "Inicio del juego"));
  gstate->turn = 0;

  /* Mulligan phase for Player B */
  if (pconfig.player_types[PLAYER_B] == INTERACTIVE_PLAYER) {
      handle_interactive_mulligan(gstate, ctx, cfg);
  } else {
      apply_mulligan(gstate, ctx);
  }

  /* Run main game loop */
  int result = run_game_loop(gstate, strategies, ctx, cfg);
  if(result != EXIT_SUCCESS)
  { cleanup_cli_game(gstate, strategies, ctx);
    return EXIT_FAILURE;
  }

  /* Display game summary */
  display_game_summary(gstate, cfg);

  /* Cleanup */
  cleanup_cli_game(gstate, strategies, ctx);
  return EXIT_SUCCESS;
}
