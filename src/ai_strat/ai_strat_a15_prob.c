// ai_strat_a15_prob.c
// See ai_strat_a15_prob.h.

#include <math.h>
#include <string.h>

#include "ai_strat_a15_prob.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"

void a15_build_dice_pmf(const uint8_t* card_indices, uint8_t count, A15DicePMF* out)
{ memset(out->pmf, 0, sizeof(out->pmf));
  out->pmf[0] = 1.0f;
  out->lo = 0;
  out->hi = 0;

  for(uint8_t i = 0; i < count; i++)
  { uint8_t sides = fullDeck[card_indices[i]].defense_dice;
    float new_pmf[A15_MAX_DICE_SUM + 1] = {0};

    for(uint8_t s = out->lo; s <= out->hi; s++)
    { if(out->pmf[s] == 0.0f) continue;
      for(uint8_t face = 1; face <= sides; face++)
        new_pmf[s + face] += out->pmf[s] / (float)sides;
    }

    out->hi += sides;
    out->lo += 1;
    memcpy(out->pmf, new_pmf, sizeof(new_pmf));
  }
} // a15_build_dice_pmf

// Sum of fixed (non-random) scalars a combat-zone selection contributes:
// attack_base is attack-only (0 on defense, see combat.c's
// calculate_total_defense()), combo bonus applies to both sides identically.
static int fixed_contribution(const uint8_t* cards, uint8_t count, bool include_attack_base)
{ int total = combo_bonus_for_selection(cards, count);
  if(include_attack_base)
    for(uint8_t i = 0; i < count; i++)
      total += fullDeck[cards[i]].attack_base;
  return total;
} // fixed_contribution

float a15_p_death_undefended(const struct gamestate* gstate, PlayerID defender)
{ PlayerID attacker = 1 - defender;
  const CombatZone* zone = &gstate->combat_zone[attacker];

  int fixed = fixed_contribution(zone->cards, zone->size, true);
  int threshold = (int)gstate->current_energy[defender] - fixed;

  A15DicePMF dice;
  a15_build_dice_pmf(zone->cards, zone->size, &dice);

  float p = 0.0f;
  for(uint8_t s = dice.lo; s <= dice.hi; s++)
    if((int)s >= threshold) p += dice.pmf[s];

  return p;
} // a15_p_death_undefended

float a15_p_death_with_defense(const struct gamestate* gstate, PlayerID defender,
                               const A15DicePMF* attacker_pmf,
                               const uint8_t* defense_cards, uint8_t defense_count)
{ PlayerID attacker = 1 - defender;
  const CombatZone* zone = &gstate->combat_zone[attacker];

  int fixed_attack = fixed_contribution(zone->cards, zone->size, true);
  int fixed_defense = fixed_contribution(defense_cards, defense_count, false);
  int energy = (int)gstate->current_energy[defender];

  A15DicePMF def_pmf;
  a15_build_dice_pmf(defense_cards, defense_count, &def_pmf);

  float p = 0.0f;
  for(uint8_t da = attacker_pmf->lo; da <= attacker_pmf->hi; da++)
  { float pa = attacker_pmf->pmf[da];
    if(pa == 0.0f) continue;
    int total_attack = fixed_attack + (int)da;

    for(uint8_t dd = def_pmf.lo; dd <= def_pmf.hi; dd++)
    { float pd = def_pmf.pmf[dd];
      if(pd == 0.0f) continue;
      int total_defense = fixed_defense + (int)dd;
      int damage = total_attack - total_defense;
      if(damage < 0) damage = 0;
      if(damage >= energy) p += pa * pd;
    }
  }

  return p;
} // a15_p_death_with_defense

// Once the endgame trigger fires, the opponent is modeled as aware they're
// facing a kill sequence: over the N-attack window they defend as often as
// they attack, rather than always attacking (Jonathan, 2026-09-10). A fixed
// heuristic, not a calibration dial -- see ai_strat_a15_endgame.h for why.
#define A15_ENDGAME_OPP_BLOCK_FREQ 0.5f

// Mean per-champion stats over the CHAMPION cards in the shared unseen pool
// (strat_common_unseen_pool()), plus what fraction of the pool is champions
// at all (a future draw isn't automatically a champion). Both sides' future
// draws are valued from this one pool, symmetrically -- see
// a15_p_finish_within()'s header comment.
typedef struct
{ float champion_fraction;
  float mean_attack;
  float mean_defense;
  float mean_variance;
} A15PoolStats;

static A15PoolStats compute_pool_stats(const struct gamestate* gstate, PlayerID observer)
{ uint8_t pool[FULL_DECK_SIZE];
  uint8_t pool_n = strat_common_unseen_pool(gstate, observer, pool);

  uint32_t champ_count = 0;
  float sum_attack = 0.0f, sum_defense = 0.0f, sum_var = 0.0f;
  for(uint8_t i = 0; i < pool_n; i++)
  { if(fullDeck[pool[i]].card_type != CHAMPION_CARD) continue;
    champ_count++;
    sum_attack += fullDeck[pool[i]].expected_attack;
    sum_defense += fullDeck[pool[i]].expected_defense;
    sum_var += champion_variance(pool[i]);
  }

  A15PoolStats stats = {0};
  if(champ_count == 0) return stats; // degenerate (near-empty pool) -- all zero
  stats.champion_fraction = (float)champ_count / (float)pool_n;
  stats.mean_attack = sum_attack / (float)champ_count;
  stats.mean_defense = sum_defense / (float)champ_count;
  stats.mean_variance = sum_var / (float)champ_count;
  return stats;
} // compute_pool_stats

// Descending insertion sort by expected_attack -- hand is capped at 12, so
// O(n^2) is fine (same reasoning as A1 Value Based's own ranked-champion sort).
static void sort_by_expected_attack_desc(uint8_t* champions, uint8_t n)
{ for(uint8_t i = 1; i < n; i++)
  { uint8_t key = champions[i];
    float key_val = fullDeck[key].expected_attack;
    int j = i - 1;
    while(j >= 0 && fullDeck[champions[j]].expected_attack < key_val)
    { champions[j + 1] = champions[j];
      j--;
    }
    champions[j + 1] = key;
  }
} // sort_by_expected_attack_desc

float a15_p_finish_within(const struct gamestate* gstate, PlayerID player, uint8_t n_attacks)
{ if(n_attacks == 0) return 0.0f;
  PlayerID opponent = 1 - player;

  A15PoolStats pool = compute_pool_stats(gstate, player);

  uint8_t champions[12];
  uint8_t hand_n = collect_champions(gstate->hand[player].cards, gstate->hand[player].size,
                                     champions, false);
  sort_by_expected_attack_desc(champions, hand_n);

  uint8_t slots = (uint8_t)(3 * n_attacks); // n_attacks <= 4, so slots <= 12
  uint8_t used_from_hand = (hand_n < slots) ? hand_n : slots;

  float mu = 0.0f, var = 0.0f;
  for(uint8_t i = 0; i < used_from_hand; i++)
  { mu += fullDeck[champions[i]].expected_attack;
    var += champion_variance(champions[i]);
  }

  uint8_t remaining_slots = slots - used_from_hand;
  uint8_t extra_draws = (n_attacks > 1) ? (uint8_t)(n_attacks - 1) : 0;
  uint8_t future_plays = (extra_draws < remaining_slots) ? extra_draws : remaining_slots;

  mu += (float)future_plays * pool.champion_fraction * pool.mean_attack;
  var += (float)future_plays * pool.champion_fraction * pool.mean_variance;

  float opp_hand_size = (float)gstate->hand[opponent].size;
  float opp_blockers_per_attempt = fminf(3.0f, opp_hand_size * pool.champion_fraction);
  float blocking_attempts = (float)n_attacks * A15_ENDGAME_OPP_BLOCK_FREQ;

  mu -= blocking_attempts * opp_blockers_per_attempt * pool.mean_defense;
  var += blocking_attempts * opp_blockers_per_attempt * pool.mean_variance;

  if(var < 1.0f) var = 1.0f; // floor -- avoid a near-zero-variance step function
  float sigma = sqrtf(var);

  float z = (mu - (float)gstate->current_energy[opponent]) / sigma;
  return 0.5f * (1.0f + erff(z * 0.70710678f)); // Phi(z), 1/sqrt(2) inlined
} // a15_p_finish_within
