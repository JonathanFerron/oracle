// stda_rating.c
// See stda_rating.h.

#include "stda_rating.h"
#include "stda_auto.h"
#include "../../core/game_context.h"
#include "../../core/game_constants.h"
#include "../../ai_strat/ai_strategy.h"
#include "../../ui/shared/player_config.h"
#include "../../ui/shared/rating_report.h"
#include "../../ui/shared/localization.h"
#include "../../rating/rating.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Registers one RatingEntry per implemented AI agent
// (ai_strategy_is_implemented()), named by its CLI shorthand -- an
// unimplemented agent is silently skipped, so the round-robin never rates
// eight silent aliases of Random (set_player_strategy_by_type()'s own
// unimplemented-agent fallback).
static void register_implemented_agents(RatingSystem* rs)
{ for(AIStrategyType type = AI_STRATEGY_RANDOM; type < AI_STRATEGY_COUNT; type++)
  { if(!ai_strategy_is_implemented(type)) continue;

    const char* shorthand = get_ai_strategy_shorthand(type);
    rating_register_ai(rs, shorthand ? shorthand : "unknown", type);
  }
} // register_implemented_agents

// --rating.agents: registers exactly the comma-separated shorthand list in
// `filter`, instead of every implemented agent -- mainly to exclude
// expensive tree-search agents (A10 ismcts/A11 ismctsnn, ~16x slower per
// decision than the closed-form agents) from a quick fit. Unlike
// register_implemented_agents()'s silent skip of unimplemented types, an
// explicit filter treats an unknown or unimplemented shorthand as an
// error and returns false -- a long rating run that silently dropped an
// agent would be worse than refusing to start (same reasoning as
// cmdline.c's parse_agent_option()).
static bool register_filtered_agents(RatingSystem* rs, const char* filter)
{ char* buf = strdup(filter);
  if(!buf) return false;

  bool ok = true;
  for(char* tok = strtok(buf, ","); tok; tok = strtok(NULL, ","))
  { AIStrategyType type = parse_ai_strategy_shorthand(tok);
    if(type == AI_STRATEGY_COUNT)
    { fprintf(stderr, "Error: unknown AI agent '%s' in --rating.agents\n", tok);
      ok = false;
      break;
    }
    if(!ai_strategy_is_implemented(type))
    { fprintf(stderr, "Error: AI agent '%s' (in --rating.agents) is not yet implemented\n", tok);
      ok = false;
      break;
    }

    const char* shorthand = get_ai_strategy_shorthand(type);
    rating_register_ai(rs, shorthand ? shorthand : tok, type);
  }

  free(buf);
  return ok;
} // register_filtered_agents

// Plays one seat orientation (id_a as PLAYER_A, id_b as PLAYER_B) and
// folds the resulting (wins_a, wins_b, draws) triple into `batch`. Reuses
// run_simulation() (stda_auto.c) exactly as aicalibsrc/*/calib_*.c does.
static void play_orientation(RatingSystem* rs, uint32_t id_a, uint32_t id_b,
                             int games, GameContext* ctx, RatingBatchData* batch)
{ StrategySet* strategies = create_strategy_set();
  set_player_strategy_by_type(strategies, PLAYER_A, rs->entrants[id_a].strategy);
  set_player_strategy_by_type(strategies, PLAYER_B, rs->entrants[id_b].strategy);

  struct gamestats gstats;
  memset(&gstats, 0, sizeof(gstats));
  run_simulation((uint16_t)games, INITIAL_CASH_DEFAULT, &gstats, strategies, ctx);

  MatchResult m = { id_a, id_b, gstats.cumul_player_wins[PLAYER_A],
                    gstats.cumul_player_wins[PLAYER_B], gstats.cumul_number_of_draws
                  };
  rating_batch_add_match(batch, &m);

  // rating_batch_compute() deliberately leaves games_played/games_won/
  // games_drawn alone (that bookkeeping belongs to the incremental path --
  // see rating.h), so the leaderboard's record columns are the caller's
  // responsibility here.
  rs->entrants[id_a].games_played += games;
  rs->entrants[id_b].games_played += games;
  rs->entrants[id_a].games_won += gstats.cumul_player_wins[PLAYER_A];
  rs->entrants[id_b].games_won += gstats.cumul_player_wins[PLAYER_B];
  rs->entrants[id_a].games_drawn += gstats.cumul_number_of_draws;
  rs->entrants[id_b].games_drawn += gstats.cumul_number_of_draws;

  free_strategy_set(strategies);
} // play_orientation

// Every unordered pair, both seats-swapped, so first-player advantage
// cancels out of the fit -- the same pattern
// aicalibsrc/*/build_selfplay_jobs() uses.
static void run_round_robin(RatingSystem* rs, int games, GameContext* ctx,
                            RatingBatchData* batch)
{ for(uint32_t i = 0; i < rs->num_entrants; i++)
    for(uint32_t j = i + 1; j < rs->num_entrants; j++)
    { play_orientation(rs, i, j, games, ctx, batch);
      play_orientation(rs, j, i, games, ctx, batch);
    }
} // run_round_robin

int run_mode_stda_rating(config_t* cfg)
{ GameContext* ctx = create_game_context(cfg);
  if(!ctx)
  { fprintf(stderr, "Failed to create game context\n");
    return EXIT_FAILURE;
  }

  RatingBatchData* batch = rating_batch_create();
  if(!batch)
  { fprintf(stderr, "Failed to allocate rating batch data\n");
    destroy_game_context(ctx);
    return EXIT_FAILURE;
  }

  int games = (cfg->rating_games > 0) ? cfg->rating_games
              : RATING_DEFAULT_GAMES_PER_ORIENTATION;

  RatingSystem rs;
  rating_init(&rs, NULL);
  rs.config.batch_method = cfg->rating_method_gradient ? RATING_BATCH_GRADIENT
                           : RATING_BATCH_MM;

  if(cfg->rating_agents)
  { if(!register_filtered_agents(&rs, cfg->rating_agents))
    { rating_batch_destroy(batch);
      destroy_game_context(ctx);
      return EXIT_FAILURE;
    }
  }
  else
    register_implemented_agents(&rs);

  printf("%s\n", LOCALIZED_STRING_L(cfg->language,
                                    "Running Bradley-Terry rating benchmark...",
                                    "Execution du banc d'essai de notation Bradley-Terry...",
                                    "Ejecutando el banco de pruebas de puntuacion Bradley-Terry..."));

  run_round_robin(&rs, games, ctx, batch);
  rating_batch_compute(&rs, batch);
  rating_print_leaderboard(&rs, cfg->language);

  if(cfg->rating_file && !rating_export_csv(&rs, cfg->rating_file))
    fprintf(stderr, "Warning: failed to write rating CSV to %s\n", cfg->rating_file);

  rating_batch_destroy(batch);
  destroy_game_context(ctx);
  return EXIT_SUCCESS;
} // run_mode_stda_rating
