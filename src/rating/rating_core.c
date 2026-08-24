// rating_core.c
// Registration, lookup, and the pure strength<->rating math -- see
// rating.h. Incremental updates live in rating_update.c, the batch MLE
// fit in rating_batch.c, CSV persistence in rating_csv.c.

#include "rating.h"
#include "rating_internal.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

RatingConfig rating_default_config(void)
{ RatingConfig cfg;
  cfg.a_max = 1.30;
  cfg.a_min = 1.08;
  cfg.a_decay_rate = 150.0;
  cfg.convergence_threshold = 1e-6;
  cfg.max_iterations = 1000;
  cfg.gradient_learning_rate = 0.5;
  cfg.batch_method = RATING_BATCH_MM;
  cfg.prior_games = 0.0;
  return cfg;
} // rating_default_config

void rating_init(RatingSystem* rs, const RatingConfig* cfg)
{ memset(rs, 0, sizeof(RatingSystem));
  rs->config = cfg ? *cfg : rating_default_config();
  rs->borealis_id = RATING_INVALID_ID;
} // rating_init

static bool name_has_comma(const char* name)
{ return strchr(name, ',') != NULL;
} // name_has_comma

static uint32_t register_entrant(RatingSystem* rs, const char* name,
                                 RatingEntrantKind kind, AIStrategyType strategy)
{ if(rs->num_entrants >= RATING_MAX_ENTRANTS) return RATING_INVALID_ID;
  if(name_has_comma(name)) return RATING_INVALID_ID;

  uint32_t id = rs->num_entrants++;
  RatingEntry* e = &rs->entrants[id];
  e->id = id;
  strncpy(e->name, name, RATING_NAME_LEN - 1);
  e->name[RATING_NAME_LEN - 1] = '\0';
  e->kind = kind;
  e->strategy = strategy;
  e->bt_strength = 1.0;
  e->rating = BOREALIS_RATING;

  return id;
} // register_entrant

uint32_t rating_register_ai(RatingSystem* rs, const char* name, AIStrategyType strategy)
{ uint32_t id = register_entrant(rs, name, RATING_ENTRANT_AI, strategy);
  if(id != RATING_INVALID_ID && strategy == AI_STRATEGY_BOREALIS)
    rs->borealis_id = id;
  return id;
} // rating_register_ai

uint32_t rating_register_human(RatingSystem* rs, const char* name)
{ return register_entrant(rs, name, RATING_ENTRANT_HUMAN, AI_STRATEGY_COUNT);
} // rating_register_human

const RatingEntry* rating_get_entrant(const RatingSystem* rs, uint32_t id)
{ if(id >= rs->num_entrants) return NULL;
  return &rs->entrants[id];
} // rating_get_entrant

uint32_t rating_find_by_name(const RatingSystem* rs, const char* name)
{ for(uint32_t i = 0; i < rs->num_entrants; i++)
    if(strcmp(rs->entrants[i].name, name) == 0) return i;
  return RATING_INVALID_ID;
} // rating_find_by_name

uint32_t rating_find_by_strategy(const RatingSystem* rs, AIStrategyType strategy)
{ for(uint32_t i = 0; i < rs->num_entrants; i++)
    if(rs->entrants[i].kind == RATING_ENTRANT_AI && rs->entrants[i].strategy == strategy)
      return i;
  return RATING_INVALID_ID;
} // rating_find_by_strategy

int32_t rating_strength_to_display(double bt_strength)
{ double r = 100.0 * bt_strength / (bt_strength + 1.0);
  int32_t rating = (int32_t)lround(r);

  if(rating < RATING_MIN) return RATING_MIN;
  if(rating > RATING_MAX) return RATING_MAX;
  return rating;
} // rating_strength_to_display

double rating_display_to_strength(int32_t rating)
{ if(rating < RATING_MIN) rating = RATING_MIN;
  if(rating > RATING_MAX) rating = RATING_MAX;

  return (double)rating / (100 - rating);
} // rating_display_to_strength

double rating_win_probability(const RatingSystem* rs, uint32_t id1, uint32_t id2)
{ if(id1 >= rs->num_entrants || id2 >= rs->num_entrants) return -1.0;

  double s1 = rs->entrants[id1].bt_strength;
  double s2 = rs->entrants[id2].bt_strength;
  return s1 / (s1 + s2);
} // rating_win_probability

double rating_get_adaptive_a(const RatingSystem* rs, uint32_t id)
{ if(id >= rs->num_entrants) return rs->config.a_min;

  const RatingEntry* e = &rs->entrants[id];
  double decay = exp(-(double)e->games_played / rs->config.a_decay_rate);
  return rs->config.a_min + (rs->config.a_max - rs->config.a_min) * decay;
} // rating_get_adaptive_a

void rating_rebalance_to_borealis(RatingSystem* rs)
{ if(rs->borealis_id == RATING_INVALID_ID) return;

  double strength = rs->entrants[rs->borealis_id].bt_strength;
  if(strength < RATING_STRENGTH_FLOOR) strength = RATING_STRENGTH_FLOOR;
  double factor = 1.0 / strength;

  for(uint32_t i = 0; i < rs->num_entrants; i++)
  { rs->entrants[i].bt_strength *= factor;
    rs->entrants[i].rating = rating_strength_to_display(rs->entrants[i].bt_strength);
  }
} // rating_rebalance_to_borealis

double rating_confidence_interval(const RatingSystem* rs, uint32_t id, double z_score)
{ const RatingEntry* e = rating_get_entrant(rs, id);
  if(!e || e->games_played == 0) return -1.0;

  double n = (double)e->games_played;
  double p = (double)e->games_won / n;
  double z2 = z_score * z_score;
  double denom = 1.0 + z2 / n;
  double half_width = (z_score / denom) * sqrt(p * (1.0 - p) / n + z2 / (4.0 * n * n));

  return half_width * 100.0; // rating-point-equivalent half-width
} // rating_confidence_interval

uint32_t rating_find_opponent(const RatingSystem* rs, uint32_t id, int rating_range)
{ const RatingEntry* self = rating_get_entrant(rs, id);
  if(!self) return RATING_INVALID_ID;

  uint32_t best = RATING_INVALID_ID;
  int best_diff = INT32_MAX;

  for(uint32_t i = 0; i < rs->num_entrants; i++)
  { if(i == id || i == rs->borealis_id) continue;

    int diff = abs(rs->entrants[i].rating - self->rating);
    if(diff <= rating_range && diff < best_diff)
    { best_diff = diff;
      best = i;
    }
  }
  return best;
} // rating_find_opponent
