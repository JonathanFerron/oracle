// ai_strat_combo_threshold.c
// A2 Combo Threshold strategy ("The Showboat") -- see
// doc/ai_agents.md's A2 section
//
// On attack, candidate plays are pruned to single champions plus
// pairs/triples whose combo bonus clears a tunable threshold -- it will
// pass up a strong solid single play to wait for a combo above that bar.
// On defense, a correct block is declined some fraction of the time (the
// "probabilistic decline", handout §6.5 step 3). Both traits are this
// agent's designed personality (handout §3, §8), not defects: they are
// exactly what got this design bumped from rating-scale benchmark to
// roster rung in favour of Borealis's diffuse suboptimality. Do not
// replace the decline with a deterministic rule, and do not widen the
// combo search to full exhaustive enumeration -- that would collapse this
// agent's character into a weaker Borealis.
//
// Unlike Value Based's efficiency ratio or Borealis's lambda-penalised
// score, this agent deliberately has no single monotone strength dial --
// see the handout §6.3's note on aggression_level. Never rank candidate
// plays by fullDeck[].power/*_efficiency (handout §5.1): those are
// cash-constrained efficiency ratios, not combat strength, and would make
// a d4+0 cost-0 champion outrank a d20+5 cost-3 one.

#include "ai_strat_combo_threshold.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"
#include "../util/mtwister.h"

// Calibrated 2026-08-22 via aicalibsrc/combo/calibrate_combo_threshold.py's
// `optimize` (differential evolution, all nine params free, vs `value` --
// vs-Random is ceiling-effected like A1's was, ~85-91% almost regardless of
// parameters, so it can't discriminate candidates). Single source of truth
// for both g_params[]'s initializer and combo_threshold_get_default_params()
// -- the earlier draft duplicated this (handout §5.5).
//
// The raw optimizer winner (aggression_level=2.21, save_big_combos_for_lethal
// =false) measured 77.3% vs value (up from ~50% at the handout's untuned
// defaults) but was rejected: aggression_level divides both combo thresholds
// (§5.4), and at 2.21 that division collapses combo_bonus_threshold's
// effective value to ~5.0 -- low enough to admit every 2-card combo bonus,
// including a plain color pair, gutting the "chases only the spectacular"
// selectivity that's this agent's stated identity (handout §1, §8;
// about.md's "hoards big combos for a finishing blow"). Disabling
// save_big_combos_for_lethal removed that trait outright. Both passed the
// mechanical personality-band check (defend_probability_base/
// defend_damage_threshold, the only two the tooling checks automatically)
// but were a manual-review catch, same as A1's VB_DEFEND_THRESHOLD
// (doc/changelog.md, 2026-08-21) -- automatic band checks don't see a
// multiplicative interaction like aggression_level's.
//
// Shipped values below are the same winner with aggression_level hand-set to
// 1.3 (not the found 2.21) and save_big_combos_for_lethal restored to true;
// the other seven values are the optimizer's output, kept as found.
// aggression_level=1.3 keeps 2-card selectivity identical to the untuned
// defaults (11/1.3 ~= 8.46 and 10/1.0 = 10 both admit only the bonus-10
// species-pair tier) while still passing through the optimizer's gains on
// the other seven dials. Measured (80,000 games, both seats): 58.77% vs
// value (was 49.94%, +8.83pp) and 92.78% vs rand (was 88.11%, +4.67pp).
#define CT_DEFAULTS \
  { .aggression_level = 1.3f, \
    .combo_bonus_threshold = 11, \
    .combo3_bonus_threshold = 16, \
    .combo_weight = 2.3626f, \
    .min_play_score_floor = 2.1184f, \
    .defend_probability_base = 0.4085f, \
    .defend_damage_threshold = 8, \
    .min_hand_size_target = 5, \
    .save_big_combos_for_lethal = true }

// Opponent energy floor gating the draw-card play (handout §6.4 step 1) and
// the save-for-lethal skip (§6.4 step 4) -- matches A1's VB_DRAW_ENERGY_FLOOR
// and the handout's own "opponent energy > 20"/"> 25" thresholds.
#define CT_DRAW_ENERGY_FLOOR 20
#define CT_LETHAL_ENERGY_FLOOR 25
#define CT_LETHAL_COMBO_BONUS 16
// Only consider 2-card defensive combos above this incoming threat (§6.5 step 5).
#define CT_DEFEND_PAIR_THREAT_FLOOR 15.0f

// Per-player parameter overrides for calibration (see aicalibsrc/combo/ once
// it exists), defaulting to CT_DEFAULTS -- same pattern as
// ai_strat_valuebased.c's g_cost_floor[]/g_defend_threshold[]. Not part of
// the public strategy-framework API.
static ComboThresholdParams g_params[2] = { CT_DEFAULTS, CT_DEFAULTS };

ComboThresholdParams combo_threshold_get_default_params(void)
{ ComboThresholdParams defaults = CT_DEFAULTS;
  return defaults;
} // combo_threshold_get_default_params

void combo_threshold_set_params(PlayerID player, const ComboThresholdParams* params)
{ g_params[player] = *params;
} // combo_threshold_set_params

void combo_threshold_reset_params(void)
{ ComboThresholdParams defaults = CT_DEFAULTS;
  g_params[PLAYER_A] = defaults;
  g_params[PLAYER_B] = defaults;
} // combo_threshold_reset_params

// A candidate play: up to 3 champions (attack or defense), always stored in
// ascending fullDeck[] index order (see build_sorted_affordable_champions())
// so candidate_ranks_before()'s lexicographic tie-break is meaningful.
// `score` means different things for attack (Sigma expected_attack +
// combo_weight*bonus) vs defense ((Sigma expected_defense + bonus) /
// (cost+1)) candidates -- never compare across the two.
typedef struct
{ uint8_t cards[3];
  uint8_t count;
  uint16_t total_cost;
  float score;
} PlayCandidate;

// True if `a` should replace `b` as the running best: higher score; tie on
// lower total cost; tie on lexicographically lower card indices. Fully
// deterministic -- the attack phase consumes no RNG, matching A1's
// discipline (handout §6.6 requires all randomness to go through ctx->rng,
// which this comparator uses none of).
static bool candidate_ranks_before(const PlayCandidate* a, const PlayCandidate* b)
{ if(a->score != b->score) return a->score > b->score;
  if(a->total_cost != b->total_cost) return a->total_cost < b->total_cost;

  uint8_t n = a->count < b->count ? a->count : b->count;
  for(uint8_t i = 0; i < n; i++)
  { if(a->cards[i] != b->cards[i]) return a->cards[i] < b->cards[i];
  }
  return a->count < b->count;
} // candidate_ranks_before

// build_affordable_champions() in hand order, then insertion-sorted
// ascending by fullDeck[] index -- needed so the nested i<j<k enumeration
// below never revisits a pair/triple and candidate_ranks_before()'s
// tie-break is deterministic regardless of hand order.
static uint8_t build_sorted_affordable_champions(const struct gamestate* gstate,
                                                 PlayerID player, uint16_t budget,
                                                 uint8_t* out)
{ uint8_t count = build_affordable_champions(gstate, player, budget, out);

  for(uint8_t i = 1; i < count; i++)
  { uint8_t key = out[i];
    int j = i - 1;
    while(j >= 0 && out[j] > key)
    { out[j + 1] = out[j];
      j--;
    }
    out[j + 1] = key;
  }

  return count;
} // build_sorted_affordable_champions

// combo_bonus_threshold/combo3_bonus_threshold scaled by aggression_level
// (handout §5.4): higher aggression -> lower effective threshold -> chases
// combos more eagerly.
static float effective_threshold(int base_threshold, float aggression_level)
{ float divisor = aggression_level > 0.0f ? aggression_level : 1.0f;
  return (float)base_threshold / divisor;
} // effective_threshold

static PlayCandidate make_attack_candidate(const uint8_t* cards, uint8_t count,
                                           int combo_bonus, float combo_weight)
{ PlayCandidate c = { .count = count };
  float attack_sum = 0.0f;

  for(uint8_t i = 0; i < count; i++)
  { c.cards[i] = cards[i];
    attack_sum += fullDeck[cards[i]].expected_attack;
    c.total_cost += fullDeck[cards[i]].cost;
  }
  c.score = attack_sum + combo_weight * (float)combo_bonus;

  return c;
} // make_attack_candidate

static void eval_single_champions(const uint8_t* affordable, uint8_t count,
                                  PlayCandidate* best)
{ for(uint8_t i = 0; i < count; i++)
  { uint8_t cards[1] = { affordable[i] };
    PlayCandidate c = make_attack_candidate(cards, 1, 0, 0.0f);
    if(best->count == 0 || candidate_ranks_before(&c, best)) *best = c;
  }
} // eval_single_champions

static void eval_two_card_combos(const uint8_t* affordable, uint8_t count, uint16_t cash,
                                 float threshold, float combo_weight, PlayCandidate* best)
{ for(uint8_t i = 0; i < count; i++)
  { for(uint8_t j = i + 1; j < count; j++)
    { uint8_t cards[2] = { affordable[i], affordable[j] };
      uint16_t total_cost = fullDeck[cards[0]].cost + fullDeck[cards[1]].cost;
      if(total_cost > cash) continue;

      int bonus = combo_bonus_for_selection(cards, 2);
      if((float)bonus < threshold) continue;

      PlayCandidate c = make_attack_candidate(cards, 2, bonus, combo_weight);
      if(best->count == 0 || candidate_ranks_before(&c, best)) *best = c;
    }
  }
} // eval_two_card_combos

// Also applies the save-big-combos-for-lethal skip (handout §6.4 step 4):
// a +16 triple is withheld while the opponent's energy is still high, on
// the theory it will matter more as a finishing blow later.
static void eval_three_card_combos(const uint8_t* affordable, uint8_t count, uint16_t cash,
                                   const ComboThresholdParams* p, float threshold,
                                   uint8_t opp_energy, PlayCandidate* best)
{ for(uint8_t i = 0; i < count; i++)
  { for(uint8_t j = i + 1; j < count; j++)
    { for(uint8_t k = j + 1; k < count; k++)
      { uint8_t cards[3] = { affordable[i], affordable[j], affordable[k] };
        uint16_t total_cost = fullDeck[cards[0]].cost + fullDeck[cards[1]].cost +
                              fullDeck[cards[2]].cost;
        if(total_cost > cash) continue;

        int bonus = combo_bonus_for_selection(cards, 3);
        if((float)bonus < threshold) continue;
        if(p->save_big_combos_for_lethal && bonus >= CT_LETHAL_COMBO_BONUS &&
           opp_energy > CT_LETHAL_ENERGY_FLOOR)
          continue;

        PlayCandidate c = make_attack_candidate(cards, 3, bonus, p->combo_weight);
        if(best->count == 0 || candidate_ranks_before(&c, best)) *best = c;
      }
    }
  }
} // eval_three_card_combos

// Handout §6.4 step 5: when the best combat play scores below the floor,
// prefer cashing in a champion for lunas over committing to a weak play --
// but only when there's a champion available to trade away (a cash card
// with an empty champion side is a no-op, per play_cash_card_ai()).
static bool eval_cash_fallback(struct gamestate* gstate, PlayerID player, GameContext* ctx,
                               const PlayCandidate* best, float min_play_score_floor)
{ if(best->count > 0 && best->score >= min_play_score_floor) return false;
  if(!has_champion_in_hand(&gstate->hand[player])) return false;

  const Hand* hand = &gstate->hand[player];
  for(uint8_t i = 0; i < hand->size; i++)
  { uint8_t card_idx = hand->cards[i];
    if(fullDeck[card_idx].card_type == CASH_CARD &&
       fullDeck[card_idx].cost <= gstate->current_cash_balance[player])
    { play_cash_card_ai(gstate, player, card_idx, ctx);
      return true;
    }
  }

  return false;
} // eval_cash_fallback

void combo_threshold_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID attacker = gstate->current_player;
  PlayerID opponent = 1 - attacker;
  const ComboThresholdParams* p = &g_params[attacker];

  if(try_play_draw_card(gstate, attacker, (uint8_t)p->min_hand_size_target,
                        CT_DRAW_ENERGY_FLOOR, ctx))
    return;

  uint16_t cash = gstate->current_cash_balance[attacker];
  uint8_t affordable[12];
  uint8_t count = build_sorted_affordable_champions(gstate, attacker, cash, affordable);

  PlayCandidate best = { 0 };
  eval_single_champions(affordable, count, &best);
  eval_two_card_combos(affordable, count, cash,
                       effective_threshold(p->combo_bonus_threshold, p->aggression_level),
                       p->combo_weight, &best);
  eval_three_card_combos(affordable, count, cash, p,
                         effective_threshold(p->combo3_bonus_threshold, p->aggression_level),
                         gstate->current_energy[opponent], &best);

  if(eval_cash_fallback(gstate, attacker, ctx, &best, p->min_play_score_floor))
    return;

  for(uint8_t i = 0; i < best.count; i++)
    play_champion(gstate, attacker, best.cards[i], ctx);
} // combo_threshold_attack_strategy

// Base defend probability, adjusted by mutually exclusive energy tiers
// (handout §6.5 step 3 -- "own energy < 20" implies "< 40", so these must be
// else-if, not independent additions).
static float defend_probability(const struct gamestate* gstate, PlayerID defender,
                                const ComboThresholdParams* p)
{ PlayerID opponent = 1 - defender;
  float prob = p->defend_probability_base;

  if(gstate->current_energy[defender] < 20) prob += 0.15f;
  else if(gstate->current_energy[defender] < 40) prob += 0.08f;

  if(gstate->current_energy[opponent] < 15) prob -= 0.20f;
  else if(gstate->current_energy[opponent] < 30) prob -= 0.10f;

  if(prob < 0.0f) prob = 0.0f;
  if(prob > 1.0f) prob = 1.0f;
  return prob;
} // defend_probability

// Deliberately a cash-conservation ratio, not fullDeck[].power (handout
// §5.1's warning is about ranking by precomputed efficiency fields; this is
// a locally-derived ratio over the actual candidate, which is what §6.5
// step 4 specifies).
static PlayCandidate make_defense_candidate(const uint8_t* cards, uint8_t count)
{ PlayCandidate c = { .count = count };
  float defense_sum = 0.0f;

  for(uint8_t i = 0; i < count; i++)
  { c.cards[i] = cards[i];
    defense_sum += fullDeck[cards[i]].expected_defense;
    c.total_cost += fullDeck[cards[i]].cost;
  }
  int bonus = combo_bonus_for_selection(cards, count);
  c.score = (defense_sum + (float)bonus) / (float)(c.total_cost + 1);

  return c;
} // make_defense_candidate

static void eval_defense_singles(const uint8_t* affordable, uint8_t count,
                                 PlayCandidate* best)
{ for(uint8_t i = 0; i < count; i++)
  { uint8_t cards[1] = { affordable[i] };
    PlayCandidate c = make_defense_candidate(cards, 1);
    if(best->count == 0 || candidate_ranks_before(&c, best)) *best = c;
  }
} // eval_defense_singles

static void eval_defense_pairs(const uint8_t* affordable, uint8_t count, uint16_t cash,
                               PlayCandidate* best)
{ for(uint8_t i = 0; i < count; i++)
  { for(uint8_t j = i + 1; j < count; j++)
    { uint8_t cards[2] = { affordable[i], affordable[j] };
      uint16_t total_cost = fullDeck[cards[0]].cost + fullDeck[cards[1]].cost;
      if(total_cost > cash) continue;

      PlayCandidate c = make_defense_candidate(cards, 2);
      if(best->count == 0 || candidate_ranks_before(&c, best)) *best = c;
    }
  }
} // eval_defense_pairs

// The probabilistic decline (handout §6.5 step 3, §8): this is the agent's
// defining weakness, kept deliberately. Do not remove the RNG roll.
void combo_threshold_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID defender = 1 - gstate->current_player;
  const ComboThresholdParams* p = &g_params[defender];

  float threat = expected_incoming_attack(gstate, defender);
  if(threat < (float)p->defend_damage_threshold) return;

  if(genRand(&ctx->rng) > defend_probability(gstate, defender, p)) return;

  uint16_t cash = gstate->current_cash_balance[defender];
  uint8_t affordable[12];
  uint8_t count = build_sorted_affordable_champions(gstate, defender, cash, affordable);
  if(count == 0) return;

  PlayCandidate best = { 0 };
  eval_defense_singles(affordable, count, &best);
  if(threat >= CT_DEFEND_PAIR_THREAT_FLOOR)
    eval_defense_pairs(affordable, count, cash, &best);

  for(uint8_t i = 0; i < best.count; i++)
    play_champion(gstate, defender, best.cards[i], ctx);
} // combo_threshold_defense_strategy
