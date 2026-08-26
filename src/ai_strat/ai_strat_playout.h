// ai_strat_playout.h
// Forked-RNG-stream playout infrastructure for search-based AI agents (A8
// Simple Monte Carlo and later A9-A11) -- see ai_strat_hbt.h/
// ai_strat_heuristic.h for why every earlier agent refused to
// clone-and-apply at all: cloning gstate is free (pure POD), but running a
// simulated turn through the engine draws dice/cards from GameContext's RNG,
// and every prior agent needed that stream untouched to stay reproducible.
// mc_fork_context() is what breaks that constraint: a simulation drives the
// engine through a separate MTRand stream instead of the live game's own.

#ifndef AI_STRAT_PLAYOUT_H
#define AI_STRAT_PLAYOUT_H

#include "../core/game_types.h"
#include "../core/game_context.h"
#include "../actions/game_move.h"

// A GameContext with its own independent RNG stream, seeded separately from
// `ctx`. Every call that threads the returned context through the engine
// (mc_determinize(), mc_playout(), or any play_turn()/resolve_combat() a
// caller drives directly) draws only from that fork -- ctx->rng is left
// byte-identical.
GameContext mc_fork_context(const GameContext* ctx, uint32_t seed);

// Re-deals the information hidden from `observer` in `sim`: everything
// except observer's own hand and both players' discards/combat zones (all
// public). Consistent with setup_game()'s per-player 40-card allocation
// (hand+discard+combat_zone+deck always sums to 40 for a given player), the
// unseen pool always splits exactly into opponent's hand + observer's own
// remaining deck + opponent's remaining deck, with the 40 cards
// setup_game() never dealt to begin with left untouched. Deliberately not
// reshuffle-aware (see ideas/A10 ai agent is-mcts (the omniscient)/about.md's
// narrowing note) -- that refinement belongs to A10, not this agent.
void mc_determinize(struct gamestate* sim, PlayerID observer, GameContext* sim_ctx);

// Plays `first` for `me` from `root`'s current phase, finishes that turn,
// then continues with uniformly-random play (both seats) until someone's
// energy hits 0 or `max_turns` is reached. Returns 1.0 (me wins), 0.5 (draw
// or the max_turns cap was hit without a winner), or 0.0 (me loses). `root`
// is never modified; `sim_ctx`'s RNG stream advances -- pass a forked
// context (mc_fork_context()), never the live game's own.
float mc_playout(const struct gamestate* root, PlayerID me, const GameMove* first,
                 GameContext* sim_ctx, uint16_t max_turns);

#endif // AI_STRAT_PLAYOUT_H
