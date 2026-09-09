// ai_strat_puct_policy.c
// See ai_strat_puct_policy.h for the full design rationale.

#include <math.h>

#include "ai_strat_puct_policy.h"
#include "../actions/move_gen.h" // MOVE_GEN_MAX_MOVES
#include "../core/game_constants.h" // fullDeck[]

_Static_assert(PUCT_POLICY_DIM == 218,
               "PUCT_POLICY_DIM drifted from the locked 218-logit layout -- "
               "see doc/ai_agents.md's A14 section before changing this");

static uint8_t catalog_of(uint8_t fulldeck_idx)
{ return ismctsnn_catalog_index(&fullDeck[fulldeck_idx]);
} // catalog_of

void puct_move_features(const GameMove* move, PUCTMoveFeatures* out)
{ *out = (PUCTMoveFeatures)
  { .type = move->type, .count = 0,
                          .play = { -1, -1, -1 }, .target = { -1, -1, -1 }
  };

  switch(move->type)
  { case MOVE_PASS:
      break;
    case MOVE_CHAMPIONS:
      out->count = move->count;
      for(uint8_t i = 0; i < move->count; i++) out->play[i] = (int8_t)catalog_of(move->cards[i]);
      break;
    case MOVE_DRAW:
      out->play[0] = (int8_t)catalog_of(move->card);
      break;
    case MOVE_RECALL:
      out->count = move->count;
      out->play[0] = (int8_t)catalog_of(move->card);
      for(uint8_t i = 0; i < move->count; i++)
        out->target[i] = (int8_t)catalog_of(move->recall[i]);
      break;
    case MOVE_CASH:
      out->play[0] = (int8_t)catalog_of(move->card);
      out->target[0] = (int8_t)catalog_of(move->cards[0]);
      break;
  }
} // puct_move_features

// Mean of logits[catalog_index] over every non-(-1) entry in `indices` --
// 0.0f if every slot is -1 (e.g. MOVE_PASS/MOVE_DRAW's empty target set).
static float mean_over_indices(const float* logits, const int8_t* indices, uint8_t max_slots)
{ float sum = 0.0f;
  uint8_t n = 0;
  for(uint8_t i = 0; i < max_slots; i++)
  { if(indices[i] < 0) continue;
    sum += logits[indices[i]];
    n++;
  }
  return (n > 0) ? (sum / (float)n) : 0.0f;
} // mean_over_indices

float puct_move_score(const float* logits, const GameMove* move)
{ PUCTMoveFeatures f;
  puct_move_features(move, &f);

  const float* play = logits + PUCT_POLICY_PLAY_OFFSET;
  const float* target = logits + PUCT_POLICY_TARGET_OFFSET;
  const float* type = logits + PUCT_POLICY_TYPE_OFFSET;
  const float* count = logits + PUCT_POLICY_COUNT_OFFSET;

  float score = type[f.type];
  if(f.count > 0) score += count[f.count - 1];
  score += mean_over_indices(play, f.play, PUCT_MOVE_MAX_PLAY);
  score += mean_over_indices(target, f.target, PUCT_MOVE_MAX_TARGET);
  return score;
} // puct_move_score

void puct_compose_priors(const float* logits, const GameMove* moves, uint8_t n,
                         float temperature, float* out_probs)
{ if(n == 0) return;

  float safe_temp = (temperature > 0.0f) ? temperature : 1.0f;
  float scores[MOVE_GEN_MAX_MOVES];
  float max_score = -INFINITY;

  for(uint8_t i = 0; i < n; i++)
  { scores[i] = puct_move_score(logits, &moves[i]) / safe_temp;
    if(scores[i] > max_score) max_score = scores[i];
  }

  float sum = 0.0f;
  for(uint8_t i = 0; i < n; i++)
  { out_probs[i] = expf(scores[i] - max_score);
    sum += out_probs[i];
  }
  for(uint8_t i = 0; i < n; i++) out_probs[i] /= sum;
} // puct_compose_priors

void puct_uniform_priors(uint8_t n, float* out_probs)
{ if(n == 0) return;

  float p = 1.0f / (float)n;
  for(uint8_t i = 0; i < n; i++) out_probs[i] = p;
} // puct_uniform_priors
