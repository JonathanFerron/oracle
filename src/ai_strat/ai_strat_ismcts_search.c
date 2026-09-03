// ai_strat_ismcts_search.c
// A10 IS-MCTS's SO-ISMCTS iteration loop -- see ai_strat_ismcts_search.h.

#include <math.h>
#include <stdlib.h>

#include "ai_strat_ismcts_search.h"
#include "ai_strat_ismcts_tree.h"
#include "ai_strat_ismctsnn_net.h"
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

typedef struct
{ uint32_t child;       // an existing child to descend into (ISMCTS_NO_NODE if not)
  GameMove expand_move; // valid only when expand is true
  bool     expand;      // create a new child for expand_move
  bool     stuck;       // defensive: no legal child available and widening forbids expansion
} SelectOutcome;

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
  if(params->nn_value_trust >= 1.0f)
    return ismctsnn_net_value(sim, player);

  float rollout = mc_playout_from(sim, player, local_strats, sim_ctx, params->rollout_max_turns);
  float nn_value = ismctsnn_net_value(sim, player);
  return (1.0f - params->nn_value_trust) * rollout + params->nn_value_trust * nn_value;
} // leaf_value

// Descends/grows the tree for exactly one determinization, then scores the
// resulting leaf via a rollout (or the terminal outcome, if the descent
// itself ended the game) and backpropagates. Returns the sampled result
// purely for DEBUG_PRINT's benefit -- callers don't need it.
static float run_one_iteration(ISMCTSArena* arena, const struct gamestate* root_gstate,
                               uint32_t root_idx, PlayerID player, GameContext* sim_ctx,
                               const ISMCTSParams* params, const StrategySet* rollout_strats)
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
  { SelectOutcome outcome = select_or_expand(arena, node, &sim, &limits, params);
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

  float result = alive
                 ? leaf_value(&sim, player, &local_strats, sim_ctx, params)
                 : mc_outcome_for(&sim, player);
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

GameMove ismcts_search_best_move(const struct gamestate* gstate, PlayerID player,
                                 GameContext* sim_ctx, const ISMCTSParams* params,
                                 const StrategySet* rollout_strats)
{ ISMCTSNode* storage = (ISMCTSNode*)malloc(sizeof(ISMCTSNode) * params->limit_max_nodes);
  ISMCTSArena arena;
  ismcts_arena_init(&arena, storage, params->limit_max_nodes, player);
  uint32_t root = ismcts_create_root(&arena, player);

  for(uint32_t i = 0; i < params->limit_iterations; i++)
    run_one_iteration(&arena, gstate, root, player, sim_ctx, params, rollout_strats);

  uint32_t chosen = most_visited_child(&arena, root);
  GameMove result = (chosen != ISMCTS_NO_NODE) ? arena.nodes[chosen].move
                    : (GameMove)
  { .type = MOVE_PASS
  };

  DEBUG_PRINT(" ISMCTS: turn %u, player %u, %u phase, %u iterations, %u nodes used, "
              "chose move type %u (visits %u)\n",
              gstate->turn, player, gstate->turn_phase, params->limit_iterations, arena.count,
              result.type, (chosen != ISMCTS_NO_NODE) ? arena.nodes[chosen].visits : 0);

  free(storage);
  return result;
} // ismcts_search_best_move
