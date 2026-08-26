// ai_strat_simplemc1.h
// A8 Simple Monte Carlo ("The Soothsayer") -- see
// ideas/A8 ai agent simple monte carlo (the soothsayer)/about.md.
//
// Original design-stub comment, kept as provenance (the file used to be a
// comment-only stub; see about.md for the decisions since made against it,
// 2026-08-25, most notably that determinization turned out to be IN scope --
// about.md's own "out of scope" framing was superseded by this stub's own
// clone_and_randomize_gamestate() call, which is what actually shipped):
//
//   Monte Carlo Single Stage Analysis (strat_simplemc1): manually create
//   100 distinct 'attack' phase game states at various stages of the game.
//   For each game state, use the MonteCarloSingleStageAnalysis strategy:
//   make a list of all possible moves by player A (Nm, maximum of 93
//   moves): getAvailableMoves(). Perform 100 simulations with all of the
//   possible candidate moves: for each simulation, make a clone of the root
//   gamestate and randomize in this clone the information not seen by
//   player A at this stage (clone_and_randomize_gamestate(), which can use
//   clone_gamestate()); for each possible move, make a clone of the
//   randomized copy, apply the move, then randomly (strat_random) make
//   moves 2+ (among legal moves) for each player alternately until player A
//   wins (1 point), loses (0), or draws (0.5 points). Discard worst
//   candidate moves, keeping Nm^(3/4) moves (max 30); perform 200 more sims
//   with the remaining candidates. Prune to Nm^(1/2) moves (max 10),
//   perform 400 more sims. Prune to Nm^(1/4) best moves (max 4), perform
//   800 more sims (cumulative total of 1500 sims). Return the best move.
//   Modify this strategy to also be applicable to an arbitrary starting
//   game state to use as an "Interactive Mode AI assistant" -- similar to
//   IS-MCTS, but the 'tree' only has one parent node with pointers to all
//   of its children nodes (93 max), with 100-1500 simulations per child.
//
//   In single-stage Monte Carlo search, consider running 7 sims for each
//   possible move, then discard any move with 0 wins; run more sims for
//   remaining moves, discarding any whose win probability (via a
//   confidence interval) is well below the leader's. Use a normal
//   approximation to the binomial distribution for the confidence
//   interval. Stop on max sims or one candidate remaining -- see
//   progressive pruning.
//
// ---
//
// Implementation summary (2026-08-25):
//
// Move space: the full set src/actions/move_gen.h enumerates -- 0-3
// champion subsets, draw, recall (capped), cash (capped). Defense phase
// enumerates champion subsets only, per get_available_moves()'s own
// contract (this agent adds nothing extra there).
//
// Hidden information: each simulation determinizes a fresh clone from this
// agent's own point of view (ai_strat_playout.h's mc_determinize()) before
// rolling out -- deliberately not reshuffle-aware; that refinement belongs
// to A10 (ideas/A10 ai agent is-mcts (the omniscient)/about.md), not this
// agent.
//
// Rollout: plays to a terminal win/loss/draw (or MAX_NUMBER_OF_TURNS,
// scored as a draw) via uniformly-random play on both seats
// (ai_strat_playout.h's mc_playout()), never a heuristic leaf evaluation --
// this agent's whole point is sampling, per about.md's "deliberately out of
// scope" list.
//
// Search: progressive pruning (ai_strat_simplemc_search.h) reconciles the
// stub's two separately-sketched ideas -- the fixed N^(3/4)/N^(1/2)/N^(1/4)
// (capped 30/10/4) survivor schedule is enforced as a hard ceiling at three
// cumulative-simulation checkpoints, layered on top of the stub's *other*
// idea (drop 0-win candidates after a small seed round, then prune anything
// whose confidence-interval upper bound falls below the current leader's
// lower bound) as the mechanism that actually does most of the pruning.
//
// RNG: exactly one value is drawn from the live GameContext per decision,
// to seed a forked stream (ai_strat_playout.h's mc_fork_context()) that
// every simulation in the search runs through; nothing inside the search or
// its rollouts touches the live stream again, and the move this agent
// finally chooses is applied to the real game state with the *live*
// context (never the forked one), exactly like any other agent's card
// plays. This is what makes this agent implementable at all without
// perturbing every other agent's RNG-dependent behaviour -- see
// ai_strat_hbt.h/ai_strat_heuristic.h for why every earlier agent refused
// to simulate.
//
// Deliberately out of scope (about.md): a tree of any kind (A10's job),
// reshuffle-aware determinization (also A10's), and any exact/closed-form
// replacement for the dice-roll sampling itself -- see ideas/A10 .../
// mcts_depth_strategy.md for where closed-form dice statistics *do* apply
// (tree agents, not this one). Also deliberately deferred: the stub's
// "Interactive Mode AI assistant" display mode (printing the top-4
// candidate moves with their win rates) -- a UI feature, not part of making
// the agent play.

#ifndef AI_STRAT_SIMPLEMC1_H
#define AI_STRAT_SIMPLEMC1_H

#include "../core/game_types.h"
#include "../core/game_context.h"

typedef struct
{ // -- Candidate enumeration (move_gen.h's MoveGenLimits) --
  uint8_t  limit_recall_variants;       // 2 -- 0 disables recall entirely
  uint8_t  limit_cash_variants;         // 3
  uint8_t  limit_max_candidates;        // 128 -- see move_gen.h's MOVE_GEN_MAX_MOVES

  // -- Progressive pruning --
  uint16_t rollout_seed_simulations;    // 7 -- the stub's own seed-round idea
  uint16_t rollout_round_simulations;   // 25 -- added per surviving candidate per round
  bool     prune_zero_win_seed;         // true -- drop every 0-win candidate after the seed round
  float    threshold_confidence_level;  // 1.96 -- z, normal approximation to the binomial
  float    threshold_stage1_keep_ratio; // 0.75 -- Nm^(3/4)
  float    threshold_stage2_keep_ratio; // 0.50 -- Nm^(1/2)
  float    threshold_stage3_keep_ratio; // 0.25 -- Nm^(1/4)
  uint8_t  limit_stage1_keep;           // 30
  uint8_t  limit_stage2_keep;           // 10
  uint8_t  limit_stage3_keep;           // 4
  uint16_t limit_stage1_simulations;    // 100 -- cumulative sims/candidate at which cap 1 applies
  uint16_t limit_stage2_simulations;    // 300
  uint16_t limit_stage3_simulations;    // 700
  uint16_t limit_max_simulations;       // 1500 -- cumulative cap per candidate
  uint32_t limit_total_rollouts;        // 25000 -- hard per-decision cost backstop

  // -- Rollout --
  bool     rollout_determinize;         // true
  uint16_t rollout_max_turns;           // MAX_NUMBER_OF_TURNS
} SimpleMcParams;

void simplemc_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void simplemc_defense_strategy(struct gamestate* gstate, GameContext* ctx);

SimpleMcParams simplemc_get_default_params(void);

// Calibration-only override hook (see aicalibsrc/simplemc/), settable per
// player so two different parameter sets can play each other in one
// process/game -- same pattern as tactical_set_params()/hbt_set_params().
// Not part of the general strategy framework: normal play always uses the
// compiled defaults, since nothing else calls these.
void simplemc_set_params(PlayerID player, const SimpleMcParams* params);
void simplemc_reset_params(void);

#endif // AI_STRAT_SIMPLEMC1_H
