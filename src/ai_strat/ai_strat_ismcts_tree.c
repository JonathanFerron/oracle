// ai_strat_ismcts_tree.c
// A10 IS-MCTS's node arena and UCT primitives -- see ai_strat_ismcts_tree.h.

#include <math.h>

#include "ai_strat_ismcts_tree.h"

void ismcts_arena_init(ISMCTSArena* arena, ISMCTSNode* storage, uint32_t capacity,
                       PlayerID root_player)
{ arena->nodes = storage;
  arena->capacity = capacity;
  arena->count = 0;
  arena->root_player = root_player;
} // ismcts_arena_init

static uint32_t new_node(ISMCTSArena* arena, uint32_t parent, const GameMove* move,
                         PlayerID player_to_move)
{ if(arena->count >= arena->capacity) return ISMCTS_NO_NODE;

  uint32_t idx = arena->count++;
  ISMCTSNode* node = &arena->nodes[idx];
  node->move = *move;
  node->parent = parent;
  node->first_child = ISMCTS_NO_NODE;
  node->next_sibling = ISMCTS_NO_NODE;
  node->total_score = 0.0f;
  node->visits = 0;
  node->availability = 0;
  node->child_count = 0;
  node->player_to_move = player_to_move;
  return idx;
} // new_node

uint32_t ismcts_create_root(ISMCTSArena* arena, PlayerID player_to_move)
{ GameMove none = {0};
  return new_node(arena, ISMCTS_NO_NODE, &none, player_to_move);
} // ismcts_create_root

uint32_t ismcts_create_child(ISMCTSArena* arena, uint32_t parent, const GameMove* move,
                             PlayerID player_to_move)
{ uint32_t idx = new_node(arena, parent, move, player_to_move);
  if(idx == ISMCTS_NO_NODE) return ISMCTS_NO_NODE;

  arena->nodes[idx].next_sibling = arena->nodes[parent].first_child;
  arena->nodes[parent].first_child = idx;
  arena->nodes[parent].child_count++;
  return idx;
} // ismcts_create_child

// Only the fields move_gen.c actually populates for a given MoveType are
// compared -- move_apply.c/move_gen.c never zero-pad the unused ones, so a
// full memcmp would risk a false negative against stack garbage.
static bool game_move_equal(const GameMove* a, const GameMove* b)
{ if(a->type != b->type) return false;

  switch(a->type)
  { case MOVE_PASS:
      return true;
    case MOVE_CHAMPIONS:
      if(a->count != b->count) return false;
      for(uint8_t i = 0; i < a->count; i++)
        if(a->cards[i] != b->cards[i]) return false;
      return true;
    case MOVE_DRAW:
      return a->card == b->card;
    case MOVE_RECALL:
      if(a->card != b->card || a->count != b->count) return false;
      for(uint8_t i = 0; i < a->count; i++)
        if(a->recall[i] != b->recall[i]) return false;
      return true;
    case MOVE_CASH:
      return a->card == b->card && a->cards[0] == b->cards[0];
  }
  return false;
} // game_move_equal

uint32_t ismcts_find_child(const ISMCTSArena* arena, uint32_t node, const GameMove* move)
{ uint32_t child = arena->nodes[node].first_child;
  while(child != ISMCTS_NO_NODE)
  { if(game_move_equal(&arena->nodes[child].move, move)) return child;
    child = arena->nodes[child].next_sibling;
  }
  return ISMCTS_NO_NODE;
} // ismcts_find_child

float ismcts_uct_score(const ISMCTSArena* arena, uint32_t child, uint32_t denom,
                       float exploration_constant)
{ const ISMCTSNode* c = &arena->nodes[child];
  if(c->visits == 0) return INFINITY;

  float mean = c->total_score / (float)c->visits;
  const ISMCTSNode* parent = &arena->nodes[c->parent];
  float q = (parent->player_to_move == arena->root_player) ? mean : (1.0f - mean);

  if(denom == 0) denom = 1; // defensive -- callers bump availability before scoring
  float exploration = exploration_constant * sqrtf(logf((float)denom) / (float)c->visits);
  return q + exploration;
} // ismcts_uct_score

void ismcts_backprop(ISMCTSArena* arena, uint32_t leaf, float result)
{ uint32_t node = leaf;
  while(node != ISMCTS_NO_NODE)
  { arena->nodes[node].visits++;
    arena->nodes[node].total_score += result;
    node = arena->nodes[node].parent;
  }
} // ismcts_backprop
