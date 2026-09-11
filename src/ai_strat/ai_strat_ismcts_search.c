// ai_strat_ismcts_search.c
// A10 IS-MCTS's SO-ISMCTS iteration loop -- see ai_strat_ismcts_search.h.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai_strat_ismcts_search.h"
#include "ai_strat_ismcts_tree.h"
#include "ai_strat_ismctsnn_net.h"
#include "ai_strat_puct_search.h"
#include "ai_strat_playout.h"
#include "../actions/move_gen.h"
#include "../core/game_constants.h"
#include "../util/debug.h"

// visits < search_expand_threshold: only the first child is ever allowed
// (don't spread budget across siblings before the first one's value is even
// roughly known). Past that, Oracle's ~93-typical branching factor is capped
// by the usual progressive-widening formula.
static uint32_t widening_cap(uint32_t visits, const ISMCTSParams* params)
{ if(visits < params->search_expand_threshold) return 1;

  float raw = ceilf(params->threshold_widening_k *
                    powf((float)visits, params->threshold_widening_alpha));
  return (raw < 1.0f) ? 1 : (uint32_t)raw;
} // widening_cap

// Enumerates this iteration's legal moves at `node` (its own determinized
// `sim`, since the deciding player's moves depend only on their own known
// hand/cash/discard -- never on the search's hidden-information guess), bumps
// availability on every existing child among them, and either names the
// UCT-best one to descend into or the first untried one to expand -- capped
// by progressive widening, so an untried move only wins over an existing
// child once child_count has room to grow.
static SelectOutcome select_or_expand(ISMCTSArena* arena, uint32_t node,
                                      const struct gamestate* sim,
                                      const MoveGenLimits* limits,
                                      const ISMCTSParams* params)
{ GameMove moves[MOVE_GEN_MAX_MOVES];
  uint8_t max_out = (uint8_t)oraclemin(MOVE_GEN_MAX_MOVES, params->limit_max_candidates);
  uint8_t n = get_available_moves(sim, arena->nodes[node].player_to_move, limits, moves, max_out);

  uint32_t cap = widening_cap(arena->nodes[node].visits, params);
  uint32_t parent_visits = arena->nodes[node].visits;
  int16_t first_untried = -1;
  uint32_t best_child = ISMCTS_NO_NODE;
  float best_score = -INFINITY;

  for(uint8_t i = 0; i < n; i++)
  { uint32_t child = ismcts_find_child(arena, node, &moves[i]);
    if(child == ISMCTS_NO_NODE)
    { if(first_untried < 0) first_untried = (int16_t)i;
      continue;
    }
    arena->nodes[child].availability++;
    uint32_t denom = params->search_use_availability ? arena->nodes[child].availability
                     : parent_visits;
    float score = ismcts_uct_score(arena, child, denom, params->search_exploration_constant);
    if(score > best_score)
    { best_score = score;
      best_child = child;
    }
  }

  if(first_untried >= 0 && arena->nodes[node].child_count < cap)
    return (SelectOutcome)
  { .expand = true, .expand_move = moves[first_untried]
  };
  if(best_child != ISMCTS_NO_NODE)
    return (SelectOutcome)
  { .child = best_child
  };
  return (SelectOutcome)
  { .stuck = true
  };
} // select_or_expand

// A11's net (Stage 2) evaluated from `player`'s seat, always -- the shipped
// behaviour, kept bit-for-bit reachable via
// params->nn_value_use_mover_seat == false.
static float nn_value_root_seat(struct gamestate* sim, PlayerID player)
{ return ismctsnn_net_value(sim, player);
} // nn_value_root_seat

// A14 Stage 0 fix (see ai_strat_ismcts1.h's nn_value_use_mover_seat comment
// and doc/ai_agents.md's A14 section): evaluates from whoever is actually
// to move at this leaf -- matching how gen_corpus.c labels every training
// record (observer == player_to_move) -- then flips back to root-seat so
// the blend/backprop below stays in `player`'s perspective either way.
static float nn_value_mover_seat(struct gamestate* sim, PlayerID player)
{ float mover_value = ismctsnn_net_value(sim, sim->player_to_move);
  return (sim->player_to_move == player) ? mover_value : (1.0f - mover_value);
} // nn_value_mover_seat

// Descends/grows the tree for exactly one determinization, then scores the
// A11 IS-MCTS+NN's leaf-evaluation blend (Stage 2, about.md's "Confirmed
// plan" step 2). params->nn_value_trust == 0.0f (A10's own
// ISMCTS_DEFAULTS) takes the exact same call this function always made --
// the superset guarantee, bit-for-bit identical to A10, no wasted NN
// evaluation. 1.0f skips the rollout entirely (cheaper: no simulation to
// terminal). Anything in between computes and blends both.
static float leaf_value(struct gamestate* sim, PlayerID player, StrategySet* local_strats,
                        GameContext* sim_ctx, const ISMCTSParams* params)
{ if(params->nn_value_trust <= 0.0f)
    return mc_playout_from(sim, player, local_strats, sim_ctx, params->rollout_max_turns);

  float nn_value = params->nn_value_use_mover_seat
                   ? nn_value_mover_seat(sim, player)
                   : nn_value_root_seat(sim, player);
  if(params->nn_value_trust >= 1.0f) return nn_value;

  float rollout = mc_playout_from(sim, player, local_strats, sim_ctx, params->rollout_max_turns);
  return (1.0f - params->nn_value_trust) * rollout + params->nn_value_trust * nn_value;
} // leaf_value

// Stores `policy` into `arena`'s policy row for `node` -- a no-op if
// `arena` wasn't given policy storage (A10/A11's own calls, or A14's own
// call with puct_params == NULL). See ai_strat_ismcts_tree.h's
// ISMCTSArena.policy comment for why this row is what
// puct_select_or_expand() later reads to score `node`'s own children.
static void store_node_policy(ISMCTSArena* arena, uint32_t node,
                              const float policy[PUCT_POLICY_DIM])
{ if(arena->policy == NULL || arena->policy_dim != PUCT_POLICY_DIM) return;
  memcpy(&arena->policy[(size_t)node * PUCT_POLICY_DIM], policy, sizeof(float) * PUCT_POLICY_DIM);
} // store_node_policy

// Descends/grows the tree for exactly one determinization, then scores the
// resulting leaf (or the terminal outcome, if the descent itself ended the
// game) and backpropagates. Returns the sampled result purely for
// DEBUG_PRINT's benefit -- callers don't need it. `puct_params` is NULL
// for A10/A11 (plain UCT selection, the shared leaf_value() blend);
// non-NULL routes selection through puct_select_or_expand() and leaf
// evaluation through puct_leaf_value() instead -- see
// ai_strat_ismcts_search.h's own header comment.
static float run_one_iteration(ISMCTSArena* arena, const struct gamestate* root_gstate,
                               uint32_t root_idx, PlayerID player, GameContext* sim_ctx,
                               const ISMCTSParams* params, const StrategySet* rollout_strats,
                               const PUCTParams* puct_params)
{ MoveGenLimits limits = { .max_recall_variants = params->limit_recall_variants,
                           .max_cash_variants = params->limit_cash_variants
                         };
  StrategySet local_strats = *rollout_strats;

  struct gamestate sim = *root_gstate;
  mc_determinize(&sim, player, sim_ctx);

  uint32_t node = root_idx;
  bool alive = true;
  uint32_t steps = 0;

  while(alive && steps < params->limit_playout_steps)
  { // puct_params->use_puct (not just puct_params != NULL) gates PUCT
    // selection specifically -- leaf evaluation below always uses the
    // two-head net whenever this agent is active, regardless of
    // use_puct, so this ablation isolates the net's own contribution from
    // PUCT's (see PUCTParams.use_puct's own doc comment, ai_strat_puct.h).
    SelectOutcome outcome = (puct_params != NULL && puct_params->use_puct)
                            ? puct_select_or_expand(arena, node, &sim, &limits, params, puct_params)
                            : select_or_expand(arena, node, &sim, &limits, params);
    if(outcome.stuck) break;

    PlayerID mover = arena->nodes[node].player_to_move;
    if(outcome.expand)
    { alive = mc_advance_to_decision(&sim, mover, &outcome.expand_move, &local_strats, sim_ctx);
      uint32_t child = ismcts_create_child(arena, node, &outcome.expand_move,
                                           alive ? sim.player_to_move : mover);
      if(child != ISMCTS_NO_NODE) node = child; // else: arena full -- stunt, simulate in place
      steps++;
      break; // exactly one expansion per iteration
    }

    alive = mc_advance_to_decision(&sim, mover, &arena->nodes[outcome.child].move,
                                   &local_strats, sim_ctx);
    node = outcome.child;
    steps++;
  }

  float result;
  if(!alive)
    result = mc_outcome_for(&sim, player);
  else if(puct_params != NULL)
  { float policy[PUCT_POLICY_DIM];
    result = puct_leaf_value(&sim, player, policy);
    store_node_policy(arena, node, policy);
  }
  else
    result = leaf_value(&sim, player, &local_strats, sim_ctx, params);

  ismcts_backprop(arena, node, result);
  return result;
} // run_one_iteration

// Prefers the earliest-created node among ties: children are singly linked
// in reverse creation order (ismcts_create_child() prepends), so scanning
// with >= lets a later (earlier-created) match keep overwriting `best`.
static uint32_t most_visited_child(const ISMCTSArena* arena, uint32_t root)
{ uint32_t best = ISMCTS_NO_NODE;
  uint32_t best_visits = 0;
  uint32_t child = arena->nodes[root].first_child;

  while(child != ISMCTS_NO_NODE)
  { if(arena->nodes[child].visits >= best_visits)
    { best_visits = arena->nodes[child].visits;
      best = child;
    }
    child = arena->nodes[child].next_sibling;
  }
  return best;
} // most_visited_child

// See ai_strat_ismcts_search.h's own RootVisitRecord/
// ismcts_last_root_visit_distribution() comments for the file-static-is-
// safe-here rationale (process-level, not thread-level, parallelism).
static RootVisitRecord g_last_root_visits[MOVE_GEN_MAX_MOVES];
static uint8_t g_last_root_visit_count = 0;

static void record_root_visits(const ISMCTSArena* arena, uint32_t root)
{ uint32_t child = arena->nodes[root].first_child;
  uint8_t n = 0;
  while(child != ISMCTS_NO_NODE && n < MOVE_GEN_MAX_MOVES)
  { g_last_root_visits[n].move = arena->nodes[child].move;
    g_last_root_visits[n].visits = arena->nodes[child].visits;
    n++;
    child = arena->nodes[child].next_sibling;
  }
  g_last_root_visit_count = n;
} // record_root_visits

uint8_t ismcts_last_root_visit_distribution(RootVisitRecord* out, uint8_t max_out)
{ uint8_t n = (uint8_t)oraclemin(g_last_root_visit_count, max_out);
  memcpy(out, g_last_root_visits, sizeof(RootVisitRecord) * n);
  return n;
} // ismcts_last_root_visit_distribution

GameMove ismcts_search_best_move(const struct gamestate* gstate, PlayerID player,
                                 GameContext* sim_ctx, const ISMCTSParams* params,
                                 const StrategySet* rollout_strats,
                                 const PUCTParams* puct_params)
{ ISMCTSNode* storage = (ISMCTSNode*)malloc(sizeof(ISMCTSNode) * params->limit_max_nodes);
  ISMCTSArena arena;
  ismcts_arena_init(&arena, storage, params->limit_max_nodes, player);

  // A14 only: one PUCT_POLICY_DIM-wide row per node, mallocked/freed
  // alongside `storage` -- NULL/untouched for A10/A11 (puct_params NULL).
  float* policy_storage = NULL;
  if(puct_params != NULL)
  { policy_storage =
      (float*)malloc(sizeof(float) * (size_t)params->limit_max_nodes * PUCT_POLICY_DIM);
    arena.policy = policy_storage;
    arena.policy_dim = PUCT_POLICY_DIM;
  }

  uint32_t root = ismcts_create_root(&arena, player);
  if(puct_params != NULL) puct_evaluate_root_policy(&arena, root, gstate, player);

  for(uint32_t i = 0; i < params->limit_iterations; i++)
    run_one_iteration(&arena, gstate, root, player, sim_ctx, params, rollout_strats, puct_params);

  uint32_t chosen = most_visited_child(&arena, root);
  GameMove result = (chosen != ISMCTS_NO_NODE) ? arena.nodes[chosen].move
                    : (GameMove)
  { .type = MOVE_PASS
  };

  DEBUG_PRINT(" ISMCTS: turn %u, player %u, %u phase, %u iterations, %u nodes used, "
              "chose move type %u (visits %u)\n",
              gstate->turn, player, gstate->turn_phase, params->limit_iterations, arena.count,
              result.type, (chosen != ISMCTS_NO_NODE) ? arena.nodes[chosen].visits : 0);

  record_root_visits(&arena, root); // cheap; recorded regardless of caller
  // (A10/A11 included) -- only A14's corpus generator reads it back
  free(storage);
  free(policy_storage);
  return result;
} // ismcts_search_best_move
