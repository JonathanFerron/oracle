// ai_strat_ismcts_tree.h
// A10 IS-MCTS ("The Omniscient") -- node arena and UCT primitives. See
// doc/ai_agents.md's A10 section for why nodes store
// no gamestate (re-determinized and replayed from the root every iteration,
// superseding the original design-stub's "store gamestate in child node").
// A pre-allocated flat array with uint32_t parent/first_child/next_sibling
// indices, rather than per-node malloc, keeps a ~10^5-iteration search's
// memory bounded and contiguous (~40 bytes/node).

#ifndef AI_STRAT_ISMCTS_TREE_H
#define AI_STRAT_ISMCTS_TREE_H

#include "../core/game_types.h"
#include "../actions/game_move.h"

#define ISMCTS_NO_NODE UINT32_MAX

// total_score/visits are always accumulated from the search's root_player's
// own seat (ISMCTSArena-wide, not per-node) -- ismcts_uct_score() flips to
// the opponent's perspective at selection time when a node's own
// player_to_move isn't root_player. child_count backs progressive widening
// (search.c's threshold_widening_k/_alpha) with an O(1) check instead of
// walking the sibling list.
typedef struct
{ GameMove move;            // the move that led here from the parent (unused at the root)
  uint32_t parent;
  uint32_t first_child;
  uint32_t next_sibling;
  float    total_score;
  uint32_t visits;
  uint32_t availability;    // SO-ISMCTS: iterations in which this move was legal
  uint16_t child_count;
  PlayerID player_to_move;  // whose decision THIS node is
} ISMCTSNode;

typedef struct
{ ISMCTSNode* nodes;
  uint32_t capacity;
  uint32_t count;
  PlayerID root_player;

  // A14 AlphaOracle Prime Plus I only (see doc/ai_agents.md's A14 section):
  // one row of `policy_dim` floats per node, indexed the same way as
  // `nodes` (node index * policy_dim). NULL/0 for A10/A11 -- their arenas
  // never touch these fields, so their memory profile is unchanged.
  // Caller-owned (ismcts_search_best_move() mallocs/frees it alongside
  // `nodes`), same convention as `nodes` itself.
  float* policy;
  uint32_t policy_dim;
} ISMCTSArena;

// Outcome of one selection step at a node: either an existing child to
// descend into, a not-yet-created move to expand, or (defensively) neither
// -- shared between A10/A11's plain-UCT select_or_expand()
// (ai_strat_ismcts_search.c) and A14's PUCT counterpart
// (ai_strat_puct_search.c), since both walk the same arena/move-list shape
// and only differ in how they SCORE candidates.
typedef struct
{ uint32_t child; // an existing child to descend into (ISMCTS_NO_NODE if not)
  GameMove expand_move; // valid only when expand is true
  bool expand; // create a new child for expand_move
  bool stuck; // defensive: no legal child available and widening forbids expansion
} SelectOutcome;

// `storage` must hold at least `capacity` elements and outlive `arena` --
// the arena never allocates or frees memory itself.
void ismcts_arena_init(ISMCTSArena* arena, ISMCTSNode* storage, uint32_t capacity,
                       PlayerID root_player);

// Creates the (parent-less, move-less) root node. Always succeeds if
// capacity > 0 -- callers own that precondition.
uint32_t ismcts_create_root(ISMCTSArena* arena, PlayerID player_to_move);

// Creates a child of `parent` for `move`/`player_to_move`, linking it into
// the parent's child list (prepended -- O(1)). Returns ISMCTS_NO_NODE if the
// arena is full; callers must treat that as "stop growing the tree, simulate
// from here instead" (search.c's fallback -- this is the memory-bounded
// MCTS literature's "stunting" degradation once the node pool is exhausted).
uint32_t ismcts_create_child(ISMCTSArena* arena, uint32_t parent, const GameMove* move,
                             PlayerID player_to_move);

// Field-wise comparison of the two moves' type-relevant GameMove fields
// only, not memcmp -- move_gen.c/move_apply.c never zero-pad the fields a
// given MoveType doesn't use, so a full memcmp would risk a false negative
// against stack garbage. Exposed (not just ismcts_find_child()'s own
// internal use) for A14 AlphaOracle Prime Plus I's corpus generator
// (aicalibsrc/puct/gen_policy_corpus.c), which must cross-reference
// get_available_moves()'s own freshly re-enumerated list against
// ismcts_last_root_visit_distribution()'s recorded (move, visits) pairs
// (ai_strat_ismcts_search.h) -- two independently built GameMove lists for
// the same decision that need matching by value, not by node index.
bool ismcts_move_equal(const GameMove* a, const GameMove* b);

// Linear scan of `node`'s children for one whose move equals `move` (see
// ismcts_move_equal() above). Returns ISMCTS_NO_NODE if none match.
uint32_t ismcts_find_child(const ISMCTSArena* arena, uint32_t node, const GameMove* move);

// UCT value of `child`, from the perspective of whoever chooses among its
// siblings (its parent's player_to_move, which need not be root_player --
// the mean is flipped to that player's own win rate first, since scores are
// always stored from root_player's seat). An unvisited child (visits == 0)
// always scores +infinity, so it is chosen before any visited sibling.
// `denom` is the exploration term's denominator -- pass child.availability
// for SO-ISMCTS proper, or the parent's own visits for plain UCT (see
// about.md's search_use_availability ablation switch; search.c picks which
// to pass).
float ismcts_uct_score(const ISMCTSArena* arena, uint32_t child, uint32_t denom,
                       float exploration_constant);

// PUCT value of one candidate move at `parent` -- A14 AlphaOracle Prime
// Plus I's counterpart to ismcts_uct_score() above (see doc/ai_agents.md's
// A14 section). Unlike that function, `child` may be ISMCTS_NO_NODE (an
// untried move has no node yet): first-play urgency then values it at
// parent's own mean Q minus fpu_reduction, so the LEARNED PRIOR (not
// enumeration order, and not +infinity) decides whether an untried move
// looks better than an already-explored sibling -- the mechanism change
// this agent exists for. `prior` is P(a) for this specific move, already
// composed+softmaxed by the caller (puct_compose_priors(),
// ai_strat_puct_policy.c) over the full legal move list. `denom` is always
// parent visits here -- PUCT's own exploration term is sqrt(N(parent)) in
// the AlphaZero literature; SO-ISMCTS's availability refinement
// (search_use_availability, ismcts_uct_score()'s own denom convention)
// doesn't apply to an untried move (there is no node to have accumulated
// availability in), so this function does not offer that ablation.
float ismcts_puct_score(const ISMCTSArena* arena, uint32_t parent, uint32_t child,
                        uint32_t denom, float prior, float c_puct, float fpu_reduction);

// Backpropagates `result` (always from root_player's own seat) from `leaf`
// up through every ancestor: visits++, total_score += result at each.
void ismcts_backprop(ISMCTSArena* arena, uint32_t leaf, float result);

#endif // AI_STRAT_ISMCTS_TREE_H
