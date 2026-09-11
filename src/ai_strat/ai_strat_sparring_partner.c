// ai_strat_sparring_partner.c
// Sparring Partner gap-filler strategy ("HBT Lite" / The Sparring Partner)
// -- see doc/ai_agents.md's gap-3 section.
//
// A7 Hybrid HBT ("The Grandmaster") synthesizes three layers: T (A6
// Tactical's aggression factor) modulates H's (A5 Heuristic's) advantage
// weights, B (A4 Balanced Rules' resource targets) contributes a
// shortfall penalty, and a lethal-combo hold (ported from A3 Borealis) is
// applied on attack. The Sparring Partner keeps exactly the T->H coupling
// (verbatim: the same aggression factor, the same eps_eff/gamma_eff/
// delta_eff weight-scaling formula) but drops Layer B's penalty entirely
// and drops the lethal-combo hold -- two of A7's three ingredients, not
// three, and no combo-holding sophistication. "Sparring partner": stands
// in as a training opponent that reads the position (aggression) and
// weighs advantage (A5's shape) reasonably well, but hasn't learned
// resource discipline or when to hold a finishing blow.
//
// Self-contained: its own SparringPartnerParams, not a variant sharing
// A5/A6/A7's own per-player g_params[2] (see ai_strat_junior.c's design
// note on why gap-filler agents never reuse another registered agent's
// per-player override hooks).

#include <math.h>

#include "ai_strat_sparring_partner.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"

#define SP_LETHAL_BONUS 100000.0f

// T's hand-power ratio triggers and opponent-power cash-tier adjustment --
// same fixed secondary constants A6/A7 both use (TAC_HAND_POWER_*/
// TAC_OPP_CASH_*, ai_strat_tactical.c), not tunable dials.
#define SP_HAND_POWER_STRONG_RATIO 1.5f
#define SP_HAND_POWER_WEAK_RATIO 0.7f
#define SP_OPP_CASH_HIGH_THRESHOLD 35
#define SP_OPP_CASH_HIGH_MULT 1.1f
#define SP_OPP_CASH_LOW_THRESHOLD 15
#define SP_OPP_CASH_LOW_MULT 0.9f

// T's phase thresholds and aggression weights: A6's own TACTICAL_DEFAULTS
// values (ai_strat_tactical.c), reused as-is -- this layer is ported
// verbatim, same as A7 itself ports it verbatim.
#define SP_PHASE_MID_THRESHOLD 67
#define SP_PHASE_LATE_THRESHOLD 41
#define SP_PHASE_CRITICAL_THRESHOLD 18
#define SP_AGGR_ENERGY_DIFF_WEIGHT 0.0008022129f
#define SP_AGGR_OPP_LATE_BONUS 0.1262423f
#define SP_AGGR_OPP_CRITICAL_BONUS 0.2819330f
#define SP_AGGR_SELF_LATE_PENALTY 0.0530097f
#define SP_AGGR_SELF_CRITICAL_PENALTY 0.1475105f
#define SP_AGGR_HAND_POWER_BONUS 0.2479543f
#define SP_AGGR_HAND_POWER_PENALTY 0.1542592f
#define SP_AGGR_CASH_SURPLUS_THRESHOLD 10
#define SP_AGGR_CASH_SURPLUS_BONUS 0.2301680f

// H's weights: A5's own HEURISTIC_DEFAULTS values (ai_strat_heuristic.c),
// except weight_cards_advantage, which turned out extremely sensitive:
// A5's own 1.96 unchanged measured 62 (barely below full A5/A7); cutting
// to 0.6 overshot down to 42. Interpolating between those two points for
// gap-3's ~54-58 target band -- see doc/ai_agents.md's gap-3 section for
// the full measurement history.
#define SP_WEIGHT_ENERGY_ADVANTAGE 0.34929208f
#define SP_WEIGHT_CARDS_ADVANTAGE 1.4f
#define SP_WEIGHT_CASH_ADVANTAGE 1.0f
#define SP_WEIGHT_TAPER_EXPONENT 0.10115113f
#define SP_OPP_CARD_DISCOUNT 0.98660043f

// T->H coupling: HBT's own HBT_DEFAULTS values (ai_strat_hbt.c) -- the
// piece that makes this "HBT lite" rather than a re-derivation from
// scratch. No Layer B fields (target_cash/cards_*, penalty_*) and no
// lethal-hold fields exist here at all -- not pinned to neutral, simply
// absent, since this agent doesn't have those layers.
#define SP_AGGR_ENERGY_GAIN 0.11170919f
#define SP_AGGR_RESOURCE_FADE 0.10308390f
#define SP_CRITICAL_EPSILON_MULT 1.91039995f

#define SP_DRAW_MIN_HAND_SIZE 5

/* ========================================================================
   Layer T: aggression factor, ported verbatim from A6/A7.
   ======================================================================== */

typedef enum
{ SP_PHASE_EARLY,
  SP_PHASE_MID,
  SP_PHASE_LATE,
  SP_PHASE_CRITICAL
} SpPhase;

static SpPhase game_phase(uint8_t energy)
{ if(energy >= SP_PHASE_MID_THRESHOLD) return SP_PHASE_EARLY;
  if(energy >= SP_PHASE_LATE_THRESHOLD) return SP_PHASE_MID;
  if(energy >= SP_PHASE_CRITICAL_THRESHOLD) return SP_PHASE_LATE;
  return SP_PHASE_CRITICAL;
} // game_phase

static float hand_power_sum(const Hand* hand)
{ float total = 0.0f;
  for(uint8_t i = 0; i < hand->size; i++)
    total += fullDeck[hand->cards[i]].power;
  return total;
} // hand_power_sum

static float estimate_opponent_power(const struct gamestate* gstate, PlayerID opponent)
{ float estimate = (float)gstate->hand[opponent].size * (float)AVERAGE_POWER_FOR_MULLIGAN;

  if(gstate->current_cash_balance[opponent] > SP_OPP_CASH_HIGH_THRESHOLD)
    estimate *= SP_OPP_CASH_HIGH_MULT;
  else if(gstate->current_cash_balance[opponent] < SP_OPP_CASH_LOW_THRESHOLD)
    estimate *= SP_OPP_CASH_LOW_MULT;

  return estimate;
} // estimate_opponent_power

static float aggression_factor(const struct gamestate* gstate, PlayerID player)
{ PlayerID opp = 1 - player;
  uint8_t own_energy = gstate->current_energy[player];
  uint8_t opp_energy = gstate->current_energy[opp];

  SpPhase my_phase = game_phase(own_energy);
  SpPhase opp_phase = game_phase(opp_energy);
  float my_hand_power = hand_power_sum(&gstate->hand[player]);
  float opp_estimated_power = estimate_opponent_power(gstate, opp);

  float aggression = 0.5f;
  aggression += ((float)own_energy - (float)opp_energy) * SP_AGGR_ENERGY_DIFF_WEIGHT;

  if(opp_phase == SP_PHASE_CRITICAL) aggression += SP_AGGR_OPP_CRITICAL_BONUS;
  else if(opp_phase == SP_PHASE_LATE) aggression += SP_AGGR_OPP_LATE_BONUS;

  if(my_phase == SP_PHASE_CRITICAL) aggression -= SP_AGGR_SELF_CRITICAL_PENALTY;
  else if(my_phase == SP_PHASE_LATE) aggression -= SP_AGGR_SELF_LATE_PENALTY;

  if(my_hand_power > opp_estimated_power * SP_HAND_POWER_STRONG_RATIO)
    aggression += SP_AGGR_HAND_POWER_BONUS;
  if(my_hand_power < opp_estimated_power * SP_HAND_POWER_WEAK_RATIO)
    aggression -= SP_AGGR_HAND_POWER_PENALTY;

  if(gstate->current_cash_balance[player] > SP_AGGR_CASH_SURPLUS_THRESHOLD)
    aggression += SP_AGGR_CASH_SURPLUS_BONUS;

  if(aggression < 0.0f) aggression = 0.0f;
  if(aggression > 1.0f) aggression = 1.0f;
  return aggression;
} // aggression_factor

/* ========================================================================
   Layer H: A5's advantage function shape, weights scaled by T's
   aggression via HBT's own coupling formula -- no Layer B penalty term.
   ======================================================================== */

typedef struct
{ float eps_eff;
  float gamma_eff;
  float delta_eff;
} SpState;

static SpState evaluate_state(const struct gamestate* gstate, PlayerID player)
{ float aggression = aggression_factor(gstate, player);
  float centered = 2.0f * (aggression - 0.5f);

  PlayerID opp = 1 - player;
  SpPhase opp_phase = game_phase(gstate->current_energy[opp]);

  SpState state;
  state.eps_eff = SP_WEIGHT_ENERGY_ADVANTAGE * (1.0f + SP_AGGR_ENERGY_GAIN * centered);
  if(opp_phase == SP_PHASE_CRITICAL) state.eps_eff *= SP_CRITICAL_EPSILON_MULT;

  float resource_fade = 1.0f - SP_AGGR_RESOURCE_FADE * centered;
  state.gamma_eff = SP_WEIGHT_CARDS_ADVANTAGE * resource_fade;
  state.delta_eff = SP_WEIGHT_CASH_ADVANTAGE * resource_fade;

  return state;
} // evaluate_state

static float sp_advantage(float own_energy, float opp_energy, float own_hand,
                          float opp_hand, float own_cash, float opp_cash,
                          const SpState* state)
{ float energy_adv = own_energy - opp_energy;
  if(opp_energy <= 0.0f) energy_adv += SP_LETHAL_BONUS;
  if(own_energy <= 0.0f) energy_adv -= SP_LETHAL_BONUS;

  float taper = powf(opp_energy / (float)INITIAL_ENERGY_DEFAULT, SP_WEIGHT_TAPER_EXPONENT);

  float cards_adv = own_hand - opp_hand * SP_OPP_CARD_DISCOUNT;
  float cash_adv = own_cash - opp_cash;

  return state->eps_eff * energy_adv +
         taper * state->gamma_eff * cards_adv +
         taper * state->delta_eff * cash_adv;
} // sp_advantage

// Sigma(expected_attack) + combo bonus, clamped to opp_energy -- identical
// to A5's predicted_damage().
static float predicted_damage(const uint8_t* cards, uint8_t count, float opp_energy)
{ float total = 0.0f;
  for(uint8_t i = 0; i < count; i++)
    total += fullDeck[cards[i]].expected_attack;
  total += (float)combo_bonus_for_selection(cards, count);

  if(total > opp_energy) total = opp_energy;
  if(total < 0.0f) total = 0.0f;
  return total;
} // predicted_damage

// Sigma(expected_defense) + combo bonus -- identical to A5's predicted_block().
static float predicted_block(const uint8_t* cards, uint8_t count)
{ float total = 0.0f;
  for(uint8_t i = 0; i < count; i++)
    total += fullDeck[cards[i]].expected_defense;
  total += (float)combo_bonus_for_selection(cards, count);
  return total;
} // predicted_block

typedef enum
{ SP_MOVE_PASS = 0,
  SP_MOVE_CHAMPIONS,
  SP_MOVE_DRAW,
  SP_MOVE_CASH
} SpMoveType;

typedef struct
{ float advantage;
  SpMoveType type;
  uint8_t cards[3];
  uint8_t count;
} SpBestMove;

static void consider_move(SpBestMove* best, float advantage, SpMoveType type,
                          const uint8_t* cards, uint8_t count)
{ if(advantage <= best->advantage) return;

  best->advantage = advantage;
  best->type = type;
  best->count = count;
  for(uint8_t i = 0; i < count; i++) best->cards[i] = cards[i];
} // consider_move

/* ========================================================================
   Attack: pass / every 1-3 affordable-champion subset (no lethal-hold
   exclusion, unlike A7) / every affordable draw card / every affordable
   cash card -- same enumeration shape as A5.
   ======================================================================== */

static void evaluate_attack_subset(const uint8_t* cards, uint8_t count, float own_energy,
                                   float opp_energy, float own_hand, float opp_hand,
                                   float own_cash, float opp_cash,
                                   const SpState* state, SpBestMove* best)
{ float cost = 0.0f;
  for(uint8_t i = 0; i < count; i++) cost += (float)fullDeck[cards[i]].cost;
  if(cost > own_cash) return;

  float dmg = predicted_damage(cards, count, opp_energy);
  float adv = sp_advantage(own_energy, opp_energy - dmg, own_hand - (float)count,
                           opp_hand, own_cash - cost, opp_cash, state);
  consider_move(best, adv, SP_MOVE_CHAMPIONS, cards, count);
} // evaluate_attack_subset

static SpBestMove best_attack_move(struct gamestate* gstate, PlayerID player,
                                   const SpState* state)
{ PlayerID opp = 1 - player;
  float own_energy = (float)gstate->current_energy[player];
  float opp_energy = (float)gstate->current_energy[opp];
  float own_hand = (float)gstate->hand[player].size;
  float opp_hand = (float)gstate->hand[opp].size;
  float own_cash = (float)gstate->current_cash_balance[player];
  float opp_cash = (float)gstate->current_cash_balance[opp];

  SpBestMove best =
  { .advantage = sp_advantage(own_energy, opp_energy, own_hand, opp_hand,
                              own_cash, opp_cash, state),
    .type = SP_MOVE_PASS, .count = 0
  };

  uint8_t affordable[12];
  uint8_t n = build_affordable_champions(gstate, player,
                                         gstate->current_cash_balance[player], affordable);

  for(uint8_t i = 0; i < n; i++)
  { uint8_t c1[1] = { affordable[i] };
    evaluate_attack_subset(c1, 1, own_energy, opp_energy, own_hand, opp_hand,
                           own_cash, opp_cash, state, &best);

    for(uint8_t j = i + 1; j < n; j++)
    { uint8_t c2[2] = { affordable[i], affordable[j] };
      evaluate_attack_subset(c2, 2, own_energy, opp_energy, own_hand, opp_hand,
                             own_cash, opp_cash, state, &best);

      for(uint8_t k = j + 1; k < n; k++)
      { uint8_t c3[3] = { affordable[i], affordable[j], affordable[k] };
        evaluate_attack_subset(c3, 3, own_energy, opp_energy, own_hand, opp_hand,
                               own_cash, opp_cash, state, &best);
      }
    }
  }

  bool has_champion = has_champion_in_hand(&gstate->hand[player]);
  const Hand* hand = &gstate->hand[player];

  for(uint8_t i = 0; i < hand->size; i++)
  { uint8_t card_idx = hand->cards[i];
    if(fullDeck[card_idx].cost > gstate->current_cash_balance[player]) continue;

    if(fullDeck[card_idx].card_type == DRAW_CARD)
    { float new_hand = own_hand - 1.0f + (float)fullDeck[card_idx].draw_num;
      float new_cash = own_cash - (float)fullDeck[card_idx].cost;
      float adv = sp_advantage(own_energy, opp_energy, new_hand, opp_hand,
                               new_cash, opp_cash, state);
      consider_move(&best, adv, SP_MOVE_DRAW, &card_idx, 1);
    }
    else if(fullDeck[card_idx].card_type == CASH_CARD && has_champion)
    { float new_hand = own_hand - 2.0f;
      float new_cash = own_cash - (float)fullDeck[card_idx].cost +
                       (float)fullDeck[card_idx].exchange_cash;
      float adv = sp_advantage(own_energy, opp_energy, new_hand, opp_hand,
                               new_cash, opp_cash, state);
      consider_move(&best, adv, SP_MOVE_CASH, &card_idx, 1);
    }
  }

  return best;
} // best_attack_move

void sparring_partner_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID player = gstate->current_player;

  if(try_play_draw_card(gstate, player, SP_DRAW_MIN_HAND_SIZE,
                        SP_PHASE_CRITICAL_THRESHOLD, ctx))
    return;

  SpState state = evaluate_state(gstate, player);
  SpBestMove move = best_attack_move(gstate, player, &state);

  switch(move.type)
  { case SP_MOVE_CHAMPIONS:
      for(uint8_t i = 0; i < move.count; i++)
        play_champion(gstate, player, move.cards[i], ctx);
      return;
    case SP_MOVE_DRAW:
      play_draw_card(gstate, player, move.cards[0], ctx);
      return;
    case SP_MOVE_CASH:
      play_cash_card_ai(gstate, player, move.cards[0], ctx);
      return;
    case SP_MOVE_PASS:
    default:
      return;
  }
} // sparring_partner_attack_strategy

/* ========================================================================
   Defense: pass (decline) / every 0-3 affordable-champion subset, plain
   expected_incoming_attack() (no A6/A7-style variance inflation -- that
   sophistication belongs to Layer T's own defense mechanism, which this
   agent doesn't have). PASS charges the full incoming attack against
   own_energy, same corrected baseline A5/A7 both ship (the
   project_a5_a7_defense_pass_dominance fix).
   ======================================================================== */

static void evaluate_defense_subset(const uint8_t* cards, uint8_t count, float own_energy,
                                    float opp_energy, float own_hand, float opp_hand,
                                    float own_cash, float opp_cash, float incoming,
                                    const SpState* state, SpBestMove* best)
{ float cost = 0.0f;
  for(uint8_t i = 0; i < count; i++) cost += (float)fullDeck[cards[i]].cost;
  if(cost > own_cash) return;

  float damage = incoming - predicted_block(cards, count);
  if(damage < 0.0f) damage = 0.0f;

  float new_energy = own_energy - damage;
  if(new_energy < 0.0f) new_energy = 0.0f;

  float adv = sp_advantage(new_energy, opp_energy, own_hand - (float)count,
                           opp_hand, own_cash - cost, opp_cash, state);
  consider_move(best, adv, SP_MOVE_CHAMPIONS, cards, count);
} // evaluate_defense_subset

static SpBestMove best_defense_move(struct gamestate* gstate, PlayerID defender,
                                    const SpState* state)
{ PlayerID attacker = 1 - defender;
  float own_energy = (float)gstate->current_energy[defender];
  float opp_energy = (float)gstate->current_energy[attacker];
  float own_hand = (float)gstate->hand[defender].size;
  float opp_hand = (float)gstate->hand[attacker].size;
  float own_cash = (float)gstate->current_cash_balance[defender];
  float opp_cash = (float)gstate->current_cash_balance[attacker];
  float incoming = expected_incoming_attack(gstate, defender);

  float damaged_energy = own_energy - incoming;
  if(damaged_energy < 0.0f) damaged_energy = 0.0f;

  SpBestMove best =
  { .advantage = sp_advantage(damaged_energy, opp_energy, own_hand, opp_hand,
                              own_cash, opp_cash, state),
    .type = SP_MOVE_PASS, .count = 0
  };

  uint8_t affordable[12];
  uint8_t n = build_affordable_champions(gstate, defender,
                                         gstate->current_cash_balance[defender], affordable);

  for(uint8_t i = 0; i < n; i++)
  { uint8_t c1[1] = { affordable[i] };
    evaluate_defense_subset(c1, 1, own_energy, opp_energy, own_hand, opp_hand,
                            own_cash, opp_cash, incoming, state, &best);

    for(uint8_t j = i + 1; j < n; j++)
    { uint8_t c2[2] = { affordable[i], affordable[j] };
      evaluate_defense_subset(c2, 2, own_energy, opp_energy, own_hand, opp_hand,
                              own_cash, opp_cash, incoming, state, &best);

      for(uint8_t k = j + 1; k < n; k++)
      { uint8_t c3[3] = { affordable[i], affordable[j], affordable[k] };
        evaluate_defense_subset(c3, 3, own_energy, opp_energy, own_hand, opp_hand,
                                own_cash, opp_cash, incoming, state, &best);
      }
    }
  }

  return best;
} // best_defense_move

void sparring_partner_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID defender = 1 - gstate->current_player;

  SpState state = evaluate_state(gstate, defender);
  SpBestMove move = best_defense_move(gstate, defender, &state);
  if(move.type != SP_MOVE_CHAMPIONS) return;

  for(uint8_t i = 0; i < move.count; i++)
    play_champion(gstate, defender, move.cards[i], ctx);
} // sparring_partner_defense_strategy
