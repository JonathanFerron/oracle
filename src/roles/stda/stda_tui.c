// stda_tui.c
// Standalone TUI mode entry point. Milestone 1 (AI-vs-AI display skeleton)
// is done; Milestone 2 adds human interaction: pre-ncurses player
// configuration (reusing the CLI's setup flow), a phase-by-phase human-turn
// orchestrator driven through the shared ui/interactive/game_commands.c
// grammar via the TUI's UiIO backend (ui/tui/tui_input.c), and PLAY-mode
// digit-staging for champion selection (TAB still reaches full COMMAND mode
// for draw/cash/recall/pass/exit). AI-vs-AI games still use the M1
// play_turn() fast path unchanged.

#include <stdio.h>
#include <stdlib.h>

#include "stda_tui.h"
#include "stda_auto.h"
#include "stda_tui_interactive.h"
#include "stda_rating_track.h"
#include "../../core/game_constants.h"
#include "../../core/game_context.h"
#include "../../core/turn_logic.h"
#include "../../ui/cli/cli_game.h"
#include "../../ui/tui/tui_render.h"
#include "../../ui/shared/localization.h"
#include "../../ui/shared/player_config.h"
#include "../../ui/shared/player_selection.h"

/* ========================================================================
   Pre-ncurses Player Configuration (mirrors stda_cli.c's setup flow --
   plain stdio prompts, run before tui_screen_create() touches the terminal)
   ======================================================================== */

static void tui_setup_player_configuration(config_t* cfg, PlayerConfig* pconfig)
{ init_player_config(pconfig);
  cfg->player_config = pconfig;

  display_player_selection_menu(cfg);
  int choice = get_player_type_choice(cfg);
  apply_player_selection(pconfig, cfg, choice);

  get_player_names(cfg, pconfig);
  get_ai_strategies(cfg, pconfig);
}

/* ========================================================================
   Setup / Cleanup
   ======================================================================== */

static bool tui_setup(config_t* cfg, PlayerConfig* pconfig, GameContext* ctx,
                      struct gamestate** gstate_out, StrategySet** strategies_out,
                      TuiScreen** screen_out)
{ *gstate_out = initialize_cli_game(INITIAL_CASH_DEFAULT, strategies_out, cfg, ctx);
  if(*gstate_out == NULL)
  { destroy_game_context(ctx);
    return false;
  }

  *screen_out = tui_screen_create();
  if(*screen_out == NULL)
  { cleanup_cli_game(*gstate_out, *strategies_out, ctx);
    return false;
  }
  tui_layout(*screen_out);

  /* setup_game() doesn't set these (only begin_of_turn(), the first turn's
     opener, does) -- initialize them here since the TUI draws once before
     the first turn, unlike the CLI, which always draws after
     begin_of_turn() has already run. */
  (*gstate_out)->turn = 0;
  (*gstate_out)->turn_phase = ATTACK;
  (*gstate_out)->player_to_move = (*gstate_out)->current_player;

  if(pconfig->player_types[PLAYER_B] == INTERACTIVE_PLAYER)
  { tui_draw_all(*screen_out, *gstate_out, cfg);
    tui_handle_interactive_mulligan(*screen_out, *gstate_out, ctx, cfg);
  }
  else
    apply_mulligan(*gstate_out, *strategies_out, ctx);

  return true;
}

/* ========================================================================
   Game Loop
   ======================================================================== */

// Turn summary is narrative (Game Messages), not interaction (Console).
static void tui_log_turn(TuiScreen* screen, struct gamestate* gstate,
                         config_t* cfg)
{ tui_add_game_message(screen, "%s %d: %s %d, %s %d",
                       LOCALIZED_STRING("Turn", "Tour", "Turno"), gstate->turn,
                       LOCALIZED_STRING("energy A", "energie A", "energia A"),
                       gstate->current_energy[PLAYER_A],
                       LOCALIZED_STRING("energy B", "energie B", "energia B"),
                       gstate->current_energy[PLAYER_B]);
}

static void tui_run_loop(TuiScreen* screen, struct gamestate* gstate,
                         StrategySet* strategies, GameContext* ctx,
                         config_t* cfg, PlayerConfig* pconfig)
{ struct gamestats gstats = {0};
  bool any_human = (pconfig->player_types[PLAYER_A] == INTERACTIVE_PLAYER ||
                    pconfig->player_types[PLAYER_B] == INTERACTIVE_PLAYER);

  // Full shortcut hints live in the always-visible Shortcuts box now, so the
  // human-player console line stays short; the AI-vs-AI line keeps its hint
  // since that mode has no Shortcuts-box equivalent for "press any key".
  tui_add_message(screen, "%s",
                  any_human ?
                  LOCALIZED_STRING("Game started.", "Partie commencee.",
                                   "Partida iniciada.") :
                  LOCALIZED_STRING(
                    "Game started. Press any key to advance a turn, q to quit.",
                    "Partie commencee. Une touche pour avancer, q pour quitter.",
                    "Partida iniciada. Una tecla para avanzar, q para salir."));
  tui_draw_all(screen, gstate, cfg);

  bool quit = false;

  while(!quit && !gstate->someone_has_zero_energy &&
        gstate->turn < MAX_NUMBER_OF_TURNS)
  { if(!any_human)
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
      if(screen->too_small) continue;

      play_turn(&gstats, gstate, strategies, ctx);
    }
    else if(!tui_play_turn_with_humans(screen, gstate, strategies, ctx, cfg, pconfig))
    { quit = true;
      break;
    }

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

/* ========================================================================
   Entry Point
   ======================================================================== */

int run_mode_stda_tui(config_t* cfg)
{ PlayerConfig pconfig;
  tui_setup_player_configuration(cfg, &pconfig);

  GameContext* ctx = create_game_context(cfg);
  if(ctx == NULL)
  { fprintf(stderr, "%s\n",
            LOCALIZED_STRING("Failed to create game context",
                             "Echec de creation du contexte",
                             "Error al crear contexto"));
    return EXIT_FAILURE;
  }

  get_player_assignment(&pconfig, cfg);
  apply_player_assignment(&pconfig, cfg, ctx);

  /* Bradley-Terry human rating tracking (--rating.track, off by default;
     a no-op for anything other than a single human-vs-AI game -- see
     stda_rating_track.h). Set up before tui_screen_create() touches the
     terminal, same as player configuration above. */
  RatingSystem rating_sys;
  RatingTrackState rating_track = stda_rating_track_start(cfg, &pconfig, &rating_sys);

  struct gamestate* gstate;
  StrategySet* strategies;
  TuiScreen* screen;

  if(!tui_setup(cfg, &pconfig, ctx, &gstate, &strategies, &screen))
  { fprintf(stderr, "%s\n",
            LOCALIZED_STRING("Failed to initialize TUI game",
                             "Echec de l'initialisation du jeu TUI",
                             "Error al inicializar el juego TUI"));
    return EXIT_FAILURE;
  }

  tui_run_loop(screen, gstate, strategies, ctx, cfg, &pconfig);

  /* Print after tui_screen_destroy() -- ncurses must be torn down first
     so the rating summary lands on the normal terminal, not the ncurses
     screen buffer. */
  tui_screen_destroy(screen);
  stda_rating_track_finish(cfg, &rating_track, &rating_sys, gstate);

  cleanup_cli_game(gstate, strategies, ctx);
  return EXIT_SUCCESS;
}
