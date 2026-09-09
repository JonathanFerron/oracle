// ai_strat_puct_search.c
// See ai_strat_puct_search.h for the full design rationale.

#include <math.h>
#include <stddef.h>

#include "ai_strat_puct_search.h"
#include "ai_strat_puct_net.h"
#include "../core/game_constants.h" // oraclemin

// Same shape as ai_strat_ismcts_search.c's own widening_cap() -- kept as a
// small, deliberate duplicate (this project's manual-code-over-shared-
// tiny-helper convention, matching A11's own duplication of A10's
// fork_for_decision()/heuristic_rollout_strategy_set()) rather than
// exporting A10's private helper. Only consulted when
// puct_params->use_widening is true (default false -- see ai_strat_puct.h).
static uint32_t widening_cap(uint32_t visits, const ISMCTSParams* p)
{ if(visits < p->search_expand_threshold) return 1;

  float raw = ceilf(p->threshold_widening_k * powf((float)visits, p->threshold_widening_alpha));
  return (raw < 1.0f) ? 1 : (uint32_t)raw;
} // widening_cap

// Priors for every move in moves[0..n): the learned distribution (blended
// toward uniform by prior_trust < 1.0), or plain uniform when the net
// isn't loaded yet (arena->policy is NULL/wrong width -- see
// ai_strat_ismcts_tree.h's own ISMCTSArena.policy comment) or
// prior_trust <= 0.0 (the ablation rung with no learned signal at all).
static void compose_node_priors(const ISMCTSArena* arena, uint32_t node,
                                const GameMove* moves, uint8_t n,
                                const PUCTParams* puct_params, float* out_priors)
{ bool have_policy = arena->policy != NULL && arena->policy_dim == PUCT_POLICY_DIM;
  if(!have_policy || puct_params->prior_trust <= 0.0f)
  { puct_uniform_priors(n, out_priors);
    return;
  }

  const float* node_policy = &arena->policy[(size_t)node * PUCT_POLICY_DIM];
  puct_compose_priors(node_policy, moves, n, puct_params->policy_temperature, out_priors);
  if(puct_params->prior_trust >= 1.0f) return;

  float uniform[MOVE_GEN_MAX_MOVES];
  puct_uniform_priors(n, uniform);
  for(uint8_t i = 0; i < n; i++)
    out_priors[i] = puct_params->prior_trust * out_priors[i]
                    + (1.0f - puct_params->prior_trust) * uniform[i];
} // compose_node_priors

SelectOutcome puct_select_or_expand(ISMCTSArena* arena, uint32_t node,
                                    const struct gamestate* sim,
                                    const MoveGenLimits* limits,
                                    const ISMCTSParams* ismcts_params,
                                    const PUCTParams* puct_params)
{ GameMove moves[MOVE_GEN_MAX_MOVES];
  uint8_t max_out = (uint8_t)oraclemin(MOVE_GEN_MAX_MOVES, ismcts_params->limit_max_candidates);
  uint8_t n = get_available_moves(sim, arena->nodes[node].player_to_move, limits, moves, max_out);

  float priors[MOVE_GEN_MAX_MOVES];
  compose_node_priors(arena, node, moves, n, puct_params, priors);

  uint32_t parent_visits = arena->nodes[node].visits;
  uint8_t eligible_n = puct_params->use_widening
                       ? (uint8_t)oraclemin(n, widening_cap(parent_visits, ismcts_params))
                       : n;
  if(eligible_n == 0) return (SelectOutcome)
  { .stuck = true
  }; // defensive; n is never 0

  uint8_t best_i = 0;
  uint32_t best_child = ISMCTS_NO_NODE;
  bool best_is_new = true;
  float best_score = -INFINITY;

  for(uint8_t i = 0; i < eligible_n; i++)
  { uint32_t child = ismcts_find_child(arena, node, &moves[i]);
    float score = ismcts_puct_score(arena, node, child, parent_visits, priors[i],
                                    puct_params->c_puct, puct_params->fpu_reduction);
    if(score > best_score)
    { best_score = score;
      best_i = i;
      best_child = child;
      best_is_new = (child == ISMCTS_NO_NODE);
    }
  }

  if(best_is_new)
    return (SelectOutcome)
  { .expand = true, .expand_move = moves[best_i]
  };
  return (SelectOutcome)
  { .child = best_child
  };
} // puct_select_or_expand

float puct_leaf_value(struct gamestate* sim, PlayerID player, float policy_out[PUCT_POLICY_DIM])
{ float mover_value;
  puctnet_value_and_policy(sim, sim->player_to_move, &mover_value, policy_out);
  return (sim->player_to_move == player) ? mover_value : (1.0f - mover_value);
} // puct_leaf_value

void puct_evaluate_root_policy(ISMCTSArena* arena, uint32_t root,
                               const struct gamestate* gstate, PlayerID player)
{ if(arena->policy == NULL || arena->policy_dim != PUCT_POLICY_DIM) return;

  // The root's own mover is `player` (gstate->player_to_move already
  // equals it by the time a decision is requested -- see
  // begin_of_turn()/attack_phase()/defense_phase()), so this is the same
  // "evaluate from the mover's own seat" convention puct_leaf_value() uses.
  float discard_value;
  puctnet_value_and_policy(gstate, player, &discard_value,
                           &arena->policy[(size_t)root * PUCT_POLICY_DIM]);
} // puct_evaluate_root_policy
