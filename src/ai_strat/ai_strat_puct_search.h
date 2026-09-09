// ai_strat_puct_search.h
// A14 AlphaOracle Prime Plus I -- PUCT selection/expansion and leaf
// evaluation, kept in their own file so ai_strat_ismcts_search.c's shared
// A10/A11 path stays untouched in shape and this file can grow with its
// own concerns without pushing the shared file past the project's file-
// length limits. See doc/ai_agents.md's A14 section for the full design.

#ifndef AI_STRAT_PUCT_SEARCH_H
#define AI_STRAT_PUCT_SEARCH_H

#include "ai_strat_ismcts_tree.h" // ISMCTSArena, SelectOutcome, ISMCTS_NO_NODE
#include "ai_strat_ismcts1.h" // ISMCTSParams
#include "ai_strat_puct.h" // PUCTParams
#include "ai_strat_puct_policy.h" // PUCT_POLICY_DIM
#include "../actions/move_gen.h" // MoveGenLimits

// PUCT-flavoured counterpart of ai_strat_ismcts_search.c's own
// select_or_expand() -- see that function's header comment for the shared
// enumerate/determinize contract. Scores EVERY legal move (existing
// children AND untried moves alike) via ismcts_puct_score() using the
// prior stored at `node` (arena->policy row, computed when `node` was
// itself created -- see puct_leaf_value()/puct_evaluate_root_policy()
// below), then either descends into the best-scoring existing child or
// expands the best-scoring untried move -- unlike A10/A11's own
// select_or_expand(), where an untried move always wins over any existing
// sibling whenever widening allows it, regardless of the two options'
// relative promise.
SelectOutcome puct_select_or_expand(ISMCTSArena* arena, uint32_t node,
                                    const struct gamestate* sim,
                                    const MoveGenLimits* limits,
                                    const ISMCTSParams* ismcts_params,
                                    const PUCTParams* puct_params);

// Evaluates `sim` from its own mover's seat via the two-head net
// (ai_strat_puct_net.c), flips the value to `player`'s (the search root's)
// perspective, and writes PUCT_POLICY_DIM raw policy logits for `sim`'s
// state into `policy_out` -- one forward pass yields both (see
// doc/ai_agents.md's A14 section). Never runs a rollout: this agent's
// entire leaf cost is one net forward pass, by design.
float puct_leaf_value(struct gamestate* sim, PlayerID player,
                      float policy_out[PUCT_POLICY_DIM]);

// Evaluates the search ROOT's own policy row -- the one node
// select_or_expand()/puct_select_or_expand() never reaches via
// puct_leaf_value() (a fresh root is never itself the outcome of an
// expansion). Called once per decision, before the iteration loop
// (ai_strat_ismcts_search.c's own ismcts_search_best_move()). A no-op if
// `arena`'s policy storage isn't set (i.e. puct_params was NULL there).
void puct_evaluate_root_policy(ISMCTSArena* arena, uint32_t root,
                               const struct gamestate* gstate, PlayerID player);

#endif // AI_STRAT_PUCT_SEARCH_H
