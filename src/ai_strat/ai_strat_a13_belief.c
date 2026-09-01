// ai_strat_a13_belief.c
// Implementation of ai_strat_a13_belief.h -- see that file and
// ai_strat_a13.h's "Layer K"/"Layer D" sections for the derivation.
//
// Every probability below comes from log-binomial-coefficient hypergeometric
// pmf evaluations (log_choose()/hypergeom_pmf()), not a recurrence over
// successive ratios: a recurrence seeded from an edge-case p(0) (e.g.
// successes == pop, where p(0) = 0 by construction) silently propagates that
// zero forward through every later term via multiplication, which is WRONG
// whenever the true remaining probability mass is nonzero (successes == pop
// with draws > 0 must place all mass at x = draws, not zero everywhere).
// The log-binomial-coefficient form has no such propagation: each P(X=x) is
// evaluated independently, and log_choose()'s -INFINITY-outside-[0,n]
// convention correctly zeroes only the actually-impossible outcomes for
// every degenerate case this module calls it with (pop==0, successes==0,
// draws==0, successes==pop, draws > pop-successes, ...) with no special-
// casing needed.

#include <math.h>
#include <stdlib.h>

#include "ai_strat_a13_belief.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../structures/card_collection.h"

// log(C(n,k)), returning -INFINITY (so expf() below gives exactly 0) when k
// is outside [0,n] -- see this file's header comment on why every degenerate
// case relies on this convention rather than being special-cased.
static float log_choose(int32_t n, int32_t k)
{ if(n < 0 || k < 0 || k > n) return -INFINITY;
  return lgammaf((float)n + 1.0f) - lgammaf((float)k + 1.0f) - lgammaf((float)(n - k) + 1.0f);
} // log_choose

// P(Hypergeometric(pop, successes, draws) = x) via log-binomial-coefficients
// -- never raw binomial coefficients, which overflow well inside this
// module's own pop <= FULL_DECK_SIZE range.
static float hypergeom_pmf(int32_t pop, int32_t successes, int32_t draws, int32_t x)
{ float log_p = log_choose(successes, x) + log_choose(pop - successes, draws - x)
                - log_choose(pop, draws);
  return expf(log_p);
} // hypergeom_pmf

// P(Hypergeometric(pop, successes, draws) >= i), for i in {1,2,3} -- this
// module only ever asks for i up to the combat zone's own 3-champion cap, so
// summing at most 3 pmf terms is cheaper than materialising a full array.
static float hypergeom_tail_at_least(int32_t pop, int32_t successes, int32_t draws, int32_t i)
{ float below = 0.0f;
  for(int32_t x = 0; x < i; x++)
    below += hypergeom_pmf(pop, successes, draws, x);
  return 1.0f - below;
} // hypergeom_tail_at_least

// E[sum of the largest min(sample_size,3) values in a random sample_size-
// subset drawn without replacement from the n_values champions in
// `sorted_asc`] -- the tail-sum order-statistic identity applied for
// i = 1..min(3,sample_size): E[Y_(i)] = Sum_t (v_t - v_{t-1}) *
// P(>= i of the sample have value >= v_t). `sorted_asc` must be ascending;
// duplicate values contribute a zero-width term, harmless.
static float order_stat_top3(const float* sorted_asc, uint16_t n_values, uint16_t sample_size)
{ if(sample_size == 0 || n_values == 0) return 0.0f;

  float total = 0.0f;
  uint16_t max_i = (sample_size < 3) ? sample_size : 3;

  for(uint16_t i = 1; i <= max_i; i++)
  { float e_yi = 0.0f;
    float prev_value = 0.0f;
    for(uint16_t t = 0; t < n_values; t++)
    { uint16_t count_gte = n_values - t;
      e_yi += (sorted_asc[t] - prev_value) *
              hypergeom_tail_at_least(n_values, count_gte, sample_size, (int32_t)i);
      prev_value = sorted_asc[t];
    }
    total += e_yi;
  }
  return total;
} // order_stat_top3

static int cmp_float_asc(const void* a, const void* b)
{ float fa = *(const float*)a;
  float fb = *(const float*)b;
  if(fa < fb) return -1;
  if(fa > fb) return 1;
  return 0;
} // cmp_float_asc

// Splits `pool` into two ascending-sorted arrays of its CHAMPION_CARD
// entries' expected_defense/expected_attack values -- the order-statistic
// input order_stat_top3() needs. Both output arrays must hold at least
// pool_n entries; returns the number of champions found.
static uint16_t split_pool_champion_values(const uint8_t* pool, uint8_t pool_n,
                                           float* out_defense, float* out_attack)
{ uint16_t n = 0;
  for(uint8_t i = 0; i < pool_n; i++)
  { uint8_t idx = pool[i];
    if(fullDeck[idx].card_type != CHAMPION_CARD) continue;
    out_defense[n] = fullDeck[idx].expected_defense;
    out_attack[n] = fullDeck[idx].expected_attack;
    n++;
  }
  qsort(out_defense, n, sizeof(float), cmp_float_asc);
  qsort(out_attack, n, sizeof(float), cmp_float_asc);
  return n;
} // split_pool_champion_values

// Champion-only, attack/defense-role-weighted mean card value -- see
// ai_strat_a13_belief.h's A13_ATTACK_ROLE_WEIGHT comment for the empirical
// weighting and why this replaces a naive `power` (50/50) average. Also
// champion-only for the same reason A7's own AVERAGE_POWER_FOR_MULLIGAN is:
// draw/cash cards carry much lower efficiency values than champions, so
// averaging over the whole pool would dilute this number relative to
// A13_AVERAGE_CARD_VALUE, the comparison baseline in ai_strat_a13_enum.c --
// an apples-to-oranges gap unrelated to real pool depletion. (This module's
// FIRST version used the `power` field directly with a 50/50 implicit
// weighting and averaged over the whole pool including non-champions; that
// combination of bugs produced a large, mostly-constant negative gap during
// Stage 2 calibration that made only strongly-negative belief_draw_weight
// look good and positive weight look catastrophic -- a bug signature, not a
// real finding. Both are fixed here.)
static float pool_mean_value(const uint8_t* pool, uint8_t pool_n)
{ uint16_t champ_n = 0;
  float sum = 0.0f;

  for(uint8_t i = 0; i < pool_n; i++)
  { if(fullDeck[pool[i]].card_type != CHAMPION_CARD) continue;
    sum += A13_ATTACK_ROLE_WEIGHT * fullDeck[pool[i]].attack_efficiency
           + A13_DEFENSE_ROLE_WEIGHT * fullDeck[pool[i]].defense_efficiency;
    champ_n++;
  }

  return (champ_n > 0) ? sum / (float)champ_n : 0.0f;
} // pool_mean_value

// Layer D: blends `pool_mean` toward the observer's own (fully visible)
// discard-pile mean value, in proportion to how much of a representative
// upcoming draw would come from a reshuffle. deck_size < 3 (Draw-3's own
// size, the largest draw card) is used as a fixed reference window rather
// than resolving the blend per specific candidate draw_num -- see
// ai_strat_a13.h's "Layer K (draw) + Layer D" section.
static float blend_reshuffle_value(const struct gamestate* gstate, PlayerID observer,
                                   float pool_mean, float reshuffle_trust)
{ uint8_t deck_size = (uint8_t)(gstate->deck[observer].top + 1);
  if(deck_size >= 3 || reshuffle_trust <= 0.0f) return pool_mean;

  const Discard* discard = &gstate->discard[observer];

  // Champion-only, role-weighted mean, same reasoning as pool_mean_value()
  // above -- both sides of this blend must be on the same scale for it (and
  // the caller's later comparison against A13_AVERAGE_CARD_VALUE) to mean
  // anything.
  uint16_t champ_n = 0;
  float discard_sum = 0.0f;
  for(uint8_t i = 0; i < discard->size; i++)
  { uint8_t idx = discard->cards[i];
    if(fullDeck[idx].card_type != CHAMPION_CARD) continue;
    discard_sum += A13_ATTACK_ROLE_WEIGHT * fullDeck[idx].attack_efficiency
                   + A13_DEFENSE_ROLE_WEIGHT * fullDeck[idx].defense_efficiency;
    champ_n++;
  }
  if(champ_n == 0) return pool_mean; // no champions in discard to blend toward
  float discard_mean = discard_sum / (float)champ_n;

  float blend = reshuffle_trust * (3.0f - (float)deck_size) / 3.0f;
  return (1.0f - blend) * pool_mean + blend * discard_mean;
} // blend_reshuffle_value

// Fills belief->p_k[]/e_block_given_k[]/e_opp_block/e_opp_attack for
// k = 0..belief->k_max, given the pool's sorted champion value arrays.
// block_combo_bonus applies only to e_block_given_k (ai_strat_a13.h's
// hplus_block_combo is documented as "per blocker", not per attacker) --
// e_opp_attack is left as the plain order-statistic sum.
static void fill_k_distribution(A13Belief* belief, const float* champ_defense,
                                const float* champ_attack, uint16_t champ_n,
                                float block_combo_bonus)
{ for(uint8_t k = 0; k <= belief->k_max; k++)
  { belief->p_k[k] = hypergeom_pmf(belief->pool_n, (int32_t)champ_n, belief->k_max, k);

    uint8_t committed = (k < 3) ? k : 3;
    float combo_term = (committed > 1) ? block_combo_bonus * (float)(committed - 1) : 0.0f;
    float e_block_k = order_stat_top3(champ_defense, champ_n, k) + combo_term;

    belief->e_block_given_k[k] = e_block_k;
    belief->e_opp_block += belief->p_k[k] * e_block_k;
    belief->e_opp_attack += belief->p_k[k] * order_stat_top3(champ_attack, champ_n, k);
  }
} // fill_k_distribution

A13Belief a13_build_belief(const struct gamestate* gstate, PlayerID observer,
                           float reshuffle_trust, float block_combo_bonus)
{ A13Belief belief = {0};

  uint8_t pool[FULL_DECK_SIZE];
  belief.pool_n = strat_common_unseen_pool(gstate, observer, pool);

  uint8_t opp_hand = gstate->hand[1 - observer].size;
  belief.k_max = (opp_hand > A13_MAX_HAND_K) ? A13_MAX_HAND_K : opp_hand;

  float pmean = pool_mean_value(pool, belief.pool_n);
  belief.pool_mean_value = pmean;
  belief.draw_value = blend_reshuffle_value(gstate, observer, pmean, reshuffle_trust);

  float champ_defense[FULL_DECK_SIZE];
  float champ_attack[FULL_DECK_SIZE];
  uint16_t champ_n = split_pool_champion_values(pool, belief.pool_n, champ_defense, champ_attack);

  fill_k_distribution(&belief, champ_defense, champ_attack, champ_n, block_combo_bonus);

  return belief;
} // a13_build_belief
