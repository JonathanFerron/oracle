// ai_strat_ismcts_search.h
// A10 IS-MCTS's SO-ISMCTS iteration loop -- see ai_strat_ismcts_tree.h for
// the node arena and doc/ai_agents.md's A10 section
// for the architecture note.
//
// Why SO-ISMCTS (Cowling, Powley & Whitehouse) rather than MO-ISMCTS: Oracle
// has no partially-hidden *moves* -- champions go to the public combat_zone,
// draw/recall/cash cards are played face-up. All hidden information lives in
// hands/decks, never in move history, so a single tree over information
// sets (rather than one tree per player) applies cleanly and needs no
// hand-written opponent model: the tree searches the opponent's replies
// itself and, the game being zero-sum, minimises our value there.
//
// Each iteration: mc_determinize() the root from `player`'s point of view,
// then SELECT (descend via UCT among children legal in this iteration's
// determinization, bumping availability as candidates are enumerated) until
// hitting an untried legal move or the tree's own leaf, EXPAND (create one
// child for that move, or fall back to simulating in place if the arena is
// full), SIMULATE (mc_playout_from() from the new leaf via `rollout_strats`
// -- no separate closed-form leaf-evaluation cutoff in v1, but `rollout_strats`
// itself is NOT uniformly random as of Phase 6 (2026-08-27): a controlled
// diagnostic found a random rollout policy plateaus at 46-48% vs Borealis,
// unmoved by a 64x budget increase, the same rollout-policy-bias signature
// A8's own diagnosis found; ai_strat_ismcts1.c's shipped
// heuristic_rollout_strategy_set() (A5 Heuristic on both seats) measured
// 63.0% under the same conditions -- see about.md/doc/changelog.md),
// BACKPROP (walk parents, updating visits/total_score). Root move choice is
// the most-visited child, ties broken by first-created (matches the
// project's first-enumerated-wins convention elsewhere). `sim_ctx` is a
// forked stream that advances across every iteration -- same
// one-fork-per-decision contract as A8's mc_search_best_move().

#ifndef AI_STRAT_ISMCTS_SEARCH_H
#define AI_STRAT_ISMCTS_SEARCH_H

#include "../core/game_types.h"
#include "../core/game_context.h"
#include "../actions/game_move.h"
#include "ai_strategy.h"
#include "ai_strat_ismcts1.h" // ISMCTSParams

GameMove ismcts_search_best_move(const struct gamestate* gstate, PlayerID player,
                                 GameContext* sim_ctx, const ISMCTSParams* params,
                                 const StrategySet* rollout_strats);

#endif // AI_STRAT_ISMCTS_SEARCH_H
