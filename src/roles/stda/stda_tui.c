// stda_tui.c
// Standalone TUI mode entry point (Milestone 1: AI-vs-AI display skeleton --
// renders the full ncurses layout and steps one turn per keypress).

#include <stdio.h>
#include <stdlib.h>

#include "stda_tui.h"
#include "stda_auto.h"
#include "../../core/game_constants.h"
#include "../../core/game_context.h"
#include "../../core/turn_logic.h"
#include "../../ui/cli/cli_game.h"
#include "../../ui/tui/tui_render.h"
#include "../../ui/shared/localization.h"

static bool tui_setup(config_t* cfg, GameContext** ctx_out,
                      struct gamestate** gstate_out,
                      StrategySet** strategies_out, TuiScreen** screen_out)
{ *ctx_out = create_game_context(cfg);
  if(*ctx_out == NULL) return false;

  *gstate_out = initialize_cli_game(INITIAL_CASH_DEFAULT, strategies_out,
                                    cfg, *ctx_out);
  if(*gstate_out == NULL)
  { destroy_game_context(*ctx_out);
    return false;
  }

  apply_mulligan(*gstate_out, *ctx_out);
  (*gstate_out)->turn = 0;
  /* setup_game() doesn't set these (only begin_of_turn(), the first turn's
     opener, does) -- initialize them here since the TUI draws once before
     the first play_turn() call, unlike the CLI, which always draws after
     begin_of_turn() has already run. */
  (*gstate_out)->turn_phase = ATTACK;
  (*gstate_out)->player_to_move = (*gstate_out)->current_player;

  *screen_out = tui_screen_create();
  if(*screen_out == NULL)
  { cleanup_cli_game(*gstate_out, *strategies_out, *ctx_out);
    return false;
  }
  tui_layout(*screen_out);

  return true;
}

static void tui_log_turn(TuiScreen* screen, struct gamestate* gstate,
                         config_t* cfg)
{ tui_add_message(screen, "%s %d: %s %d, %s %d",
                  LOCALIZED_STRING("Turn", "Tour", "Turno"), gstate->turn,
                  LOCALIZED_STRING("energy A", "energie A", "energia A"),
                  gstate->current_energy[PLAYER_A],
                  LOCALIZED_STRING("energy B", "energie B", "energia B"),
                  gstate->current_energy[PLAYER_B]);
}

static void tui_run_loop(TuiScreen* screen, struct gamestate* gstate,
                         StrategySet* strategies, GameContext* ctx,
                         config_t* cfg)
{ struct gamestats gstats = {0};

  tui_add_message(screen, "%s",
                  LOCALIZED_STRING(
                    "Game started. Press any key to advance a turn, q to quit.",
                    "Partie commencee. Une touche pour avancer, q pour quitter.",
                    "Partida iniciada. Una tecla para avanzar, q para salir."));
  tui_draw_all(screen, gstate, cfg);

  bool quit = false;

  while(!quit && !gstate->someone_has_zero_energy &&
        gstate->turn < MAX_NUMBER_OF_TURNS)
  { int ch = tui_get_input();

    if(tui_input_is_quit(ch))
    { quit = true;
      break;
    }
    if(tui_input_is_resize(ch))
    { tui_layout(screen);
      tui_draw_all(screen, gstate, cfg);
      continue;
    }
    if(screen->too_small) continue; /* nothing visible to advance into */

    play_turn(&gstats, gstate, strategies, ctx);
    tui_log_turn(screen, gstate, cfg);
    tui_draw_all(screen, gstate, cfg);
  }

  if(gstate->someone_has_zero_energy)
  { tui_add_message(screen, "%s",
                    LOCALIZED_STRING("Game over.", "Partie terminee.",
                                     "Juego terminado."));
    tui_draw_all(screen, gstate, cfg);
    if(!quit) tui_get_input();
  }
}

int run_mode_stda_tui(config_t* cfg)
{ GameContext* ctx;
  struct gamestate* gstate;
  StrategySet* strategies;
  TuiScreen* screen;

  if(!tui_setup(cfg, &ctx, &gstate, &strategies, &screen))
  { fprintf(stderr, "%s\n",
            LOCALIZED_STRING("Failed to initialize TUI game",
                             "Echec de l'initialisation du jeu TUI",
                             "Error al inicializar el juego TUI"));
    return EXIT_FAILURE;
  }

  tui_run_loop(screen, gstate, strategies, ctx, cfg);

  tui_screen_destroy(screen);
  cleanup_cli_game(gstate, strategies, ctx);
  return EXIT_SUCCESS;
}
