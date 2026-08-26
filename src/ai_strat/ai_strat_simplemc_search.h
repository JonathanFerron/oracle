// ai_strat_simplemc_search.h
// A8 Simple Monte Carlo's progressive-pruning search, split out of
// ai_strat_simplemc1.c/.h per the ~400-line file-length guidance -- same
// split pattern as A3 Borealis's ai_strat_borealis/ai_strat_borealis_enum
// and A7 Hybrid HBT's ai_strat_hbt/ai_strat_hbt_enum. Internal to this
// agent: nothing outside ai_strat_simplemc1.c should include this.

#ifndef AI_STRAT_SIMPLEMC_SEARCH_H
#define AI_STRAT_SIMPLEMC_SEARCH_H

#include "../core/game_types.h"
#include "../core/game_context.h"
#include "../actions/game_move.h"
#include "ai_strat_simplemc1.h" // SimpleMcParams

// Enumerates every legal move in gstate's current phase for `player`
// (move_gen.h's get_available_moves(), capped by params->limit_*_variants/
// limit_max_candidates) and searches it via progressive pruning: an initial
// small seed round (params->rollout_seed_simulations) drops every 0-win
// candidate outright (params->prune_zero_win_seed), then repeated rounds
// each add params->rollout_round_simulations sims per surviving candidate
// and drop any candidate whose upper confidence bound (normal approximation
// to the binomial, z = params->threshold_confidence_level) falls below the
// current leader's lower bound. At three cumulative-simulation checkpoints
// (params->limit_stageK_simulations), survivors are additionally truncated
// to at most min(ceil(Nm^threshold_stageK_keep_ratio), limit_stageK_keep)
// candidates by win rate, matching the design stub's fixed
// 100/200/400/800-simulation, Nm^(3/4)/Nm^(1/2)/Nm^(1/4)-survivor schedule
// as a hard ceiling layered on top of CI-based pruning rather than the sole
// pruning mechanism. Stops when one candidate remains, params->
// limit_max_simulations cumulative sims/candidate is reached, or
// params->limit_total_rollouts total rollouts is reached -- whichever comes
// first. Every rollout runs through `sim_ctx` (a forked stream -- see
// ai_strat_playout.h); `gstate` is read-only throughout, never mutated.
GameMove mc_search_best_move(const struct gamestate* gstate, PlayerID player,
                             GameContext* sim_ctx, const SimpleMcParams* params);

#endif // AI_STRAT_SIMPLEMC_SEARCH_H
