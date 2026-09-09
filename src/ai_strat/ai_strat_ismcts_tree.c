// ai_strat_ismcts_tree.c
// A10 IS-MCTS's node arena and UCT primitives -- see ai_strat_ismcts_tree.h.

#include <math.h>
#include <stddef.h>

#include "ai_strat_ismcts_tree.h"

void ismcts_arena_init(ISMCTSArena* arena, ISMCTSNode* storage, uint32_t capacity,
                       PlayerID root_player)
{ arena->nodes = storage;
  arena->capacity = capacity;
  arena->count = 0;
  arena->root_player = root_player;
  arena->policy = NULL; // A14-only; ismcts_search_best_move() sets these
  arena->policy_dim = 0; // two fields itself when a PUCTParams* is passed
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

bool ismcts_move_equal(const GameMove* a, const GameMove* b)
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
} // ismcts_move_equal

uint32_t ismcts_find_child(const ISMCTSArena* arena, uint32_t node, const GameMove* move)
{ uint32_t child = arena->nodes[node].first_child;
  while(child != ISMCTS_NO_NODE)
  { if(ismcts_move_equal(&arena->nodes[child].move, move)) return child;
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

float ismcts_puct_score(const ISMCTSArena* arena, uint32_t parent, uint32_t child,
                        uint32_t denom, float prior, float c_puct, float fpu_reduction)
{ const ISMCTSNode* p = &arena->nodes[parent];
  // parent's own mean is stored from root_player's seat (same convention
  // ismcts_uct_score() relies on); flip it to whoever actually chooses
  // among parent's children -- parent's own player_to_move.
  float parent_mean = (p->visits > 0) ? (p->total_score / (float)p->visits) : 0.5f;
  float parent_q = (p->player_to_move == arena->root_player) ? parent_mean : (1.0f - parent_mean);

  float q, child_visits;
  if(child == ISMCTS_NO_NODE)
  { q = parent_q - fpu_reduction; // first-play urgency, not +infinity
    child_visits = 0.0f;
  }
  else
  { const ISMCTSNode* c = &arena->nodes[child];
    float mean = (c->visits > 0) ? (c->total_score / (float)c->visits) : parent_q;
    q = (p->player_to_move == arena->root_player) ? mean : (1.0f - mean);
    child_visits = (float)c->visits;
  }

  float u = c_puct * prior * sqrtf((float)denom) / (1.0f + child_visits);
  return q + u;
} // ismcts_puct_score

void ismcts_backprop(ISMCTSArena* arena, uint32_t leaf, float result)
{ uint32_t node = leaf;
  while(node != ISMCTS_NO_NODE)
  { arena->nodes[node].visits++;
    arena->nodes[node].total_score += result;
    node = arena->nodes[node].parent;
  }
} // ismcts_backprop
