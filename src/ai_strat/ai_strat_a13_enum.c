// ai_strat_a13_enum.c
// Implementation of ai_strat_a13_enum.h. Reuses A7's own hbt_advantage()/
// predicted_damage()/predicted_block()/is_held_combo()/
// evaluate_defense_subset()/build_affordable_champions() verbatim
// (ai_strat_hbt_enum.h) -- nothing here re-derives A7's mechanism, only
// adds Layer K (draw)/Layer K (block)/Layer R on top. See ai_strat_a13.h's
// header comment for the full derivation of every addition below.
//
// consider_move()/evaluate_attack_subset() are re-declared locally rather
// than reused from ai_strat_hbt_enum.c: the former is private to that file
// (not part of its exported surface, ai_strat_hbt_enum.h), and the latter
// must inject this agent's own Jensen-corrected damage and race-modulated
// state, so reusing A7's copy verbatim isn't an option either way.

#include <math.h>

#include "ai_strat_a13_enum.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"

// A13's own HBTState with Layer R's eps_race folded into eps_eff -- every
// hbt_advantage() call in this file passes this rather than state->base
// directly, composing Layer R's epsilon modulation on top of A7's own state
// without touching any A7 file. eps_race == 1.0f (a no-op multiply)
// whenever params->race_scale <= 0, so this is exact-identity to A7's own
// state in the neutral configuration.
static HBTState race_modulated_state(const A13State* state)
{ HBTState modulated = state->base;
  modulated.eps_eff *= state->eps_race;
  return modulated;
} // race_modulated_state

// Jensen-corrected expected net damage for one candidate attack subset --
// ai_strat_a13.h's "Layer K (block)" section:
//   E[net(S)] = Sum_k p_k * min(max(raw(S) - e_block_given_k[k], 0), opp_energy)
// hplus_trust <= 0 returns `raw` (A7's own predicted_damage()) exactly and
// skips the belief-dependent loop entirely, matching the superset guarantee.
static float expected_net_damage(float raw, float opp_energy,
                                 const A13Belief* belief, float hplus_trust)
{ if(hplus_trust <= 0.0f) return raw;

  float e_net = 0.0f;
  for(uint8_t k = 0; k <= belief->k_max; k++)
  { float net = raw - belief->e_block_given_k[k];
    if(net < 0.0f) net = 0.0f;
    if(net > opp_energy) net = opp_energy;
    e_net += belief->p_k[k] * net;
  }

  return (1.0f - hplus_trust) * raw + hplus_trust * e_net;
} // expected_net_damage

// Local re-declaration of A7's own private consider_move() (ai_strat_hbt_
// enum.c) -- not part of that file's exported surface (ai_strat_hbt_enum.h),
// so this agent needs its own copy. Identical shape.
static void consider_move(HBTBestMove* best, float advantage, HBTMoveType type,
                          const uint8_t* cards, uint8_t count)
{ if(advantage <= best->advantage) return;

  best->advantage = advantage;
  best->type = type;
  best->count = count;
  for(uint8_t i = 0; i < count; i++) best->cards[i] = cards[i];
} // consider_move

static void evaluate_attack_subset(const uint8_t* cards, uint8_t count, float own_energy,
                                   float opp_energy, float own_hand, float opp_hand,
                                   float own_cash, float opp_cash, PlayerID opponent,
                                   struct gamestate* gstate, const A13Params* params,
                                   const A13Belief* belief, const HBTState* modulated,
                                   HBTBestMove* best)
{ float cost = 0.0f;
  for(uint8_t i = 0; i < count; i++) cost += (float)fullDeck[cards[i]].cost;
  if(cost > own_cash) return;

  float raw_dmg = predicted_damage(cards, count, opp_energy);
  if(is_held_combo(cards, count, raw_dmg, opponent, gstate, &params->base)) return;

  float dmg = expected_net_damage(raw_dmg, opp_energy, belief, params->hplus_trust);

  float adv = hbt_advantage(own_energy, opp_energy - dmg, own_hand - (float)count,
                            opp_hand, own_cash - cost, opp_cash, &params->base, modulated);
  consider_move(best, adv, HBT_MOVE_CHAMPIONS, cards, count);
} // evaluate_attack_subset

// DRAW/CASH candidates -- A7's own logic verbatim, plus Layer K (draw)'s
// belief_draw_weight bonus on DRAW candidates only. belief_draw_weight == 0
// contributes exactly 0 (0 times a finite factor, never NaN even against a
// zeroed/unbuilt belief) in the neutral configuration.
static void consider_draw_and_cash_moves(struct gamestate* gstate, PlayerID player,
                                         float own_energy, float opp_energy, float own_hand,
                                         float opp_hand, float own_cash, float opp_cash,
                                         const A13Params* params, const A13Belief* belief,
                                         const HBTState* modulated, HBTBestMove* best)
{ bool has_champion = has_champion_in_hand(&gstate->hand[player]);
  const Hand* hand = &gstate->hand[player];

  for(uint8_t i = 0; i < hand->size; i++)
  { uint8_t card_idx = hand->cards[i];
    if(fullDeck[card_idx].cost > gstate->current_cash_balance[player]) continue;

    if(fullDeck[card_idx].card_type == DRAW_CARD)
    { float new_hand = own_hand - 1.0f + (float)fullDeck[card_idx].draw_num;
      float new_cash = own_cash - (float)fullDeck[card_idx].cost;
      float adv = hbt_advantage(own_energy, opp_energy, new_hand, opp_hand,
                                new_cash, opp_cash, &params->base, modulated);
      adv += params->belief_draw_weight * (float)fullDeck[card_idx].draw_num *
             (belief->draw_value - A13_AVERAGE_CARD_VALUE);
      consider_move(best, adv, HBT_MOVE_DRAW, &card_idx, 1);
    }
    else if(fullDeck[card_idx].card_type == CASH_CARD && has_champion)
    { float new_hand = own_hand - 2.0f;
      float new_cash = own_cash - (float)fullDeck[card_idx].cost +
                       (float)fullDeck[card_idx].exchange_cash;
      float adv = hbt_advantage(own_energy, opp_energy, new_hand, opp_hand,
                                new_cash, opp_cash, &params->base, modulated);
      consider_move(best, adv, HBT_MOVE_CASH, &card_idx, 1);
    }
  }
} // consider_draw_and_cash_moves

HBTBestMove a13_best_attack_move(struct gamestate* gstate, PlayerID player,
                                 const A13Params* params, const A13State* state,
                                 const A13Belief* belief)
{ PlayerID opp = 1 - player;
  float own_energy = (float)gstate->current_energy[player];
  float opp_energy = (float)gstate->current_energy[opp];
  float own_hand = (float)gstate->hand[player].size;
  float opp_hand = (float)gstate->hand[opp].size;
  float own_cash = (float)gstate->current_cash_balance[player];
  float opp_cash = (float)gstate->current_cash_balance[opp];
  HBTState modulated = race_modulated_state(state);

  HBTBestMove best =
  { .advantage = hbt_advantage(own_energy, opp_energy, own_hand, opp_hand,
                               own_cash, opp_cash, &params->base, &modulated),
    .type = HBT_MOVE_PASS, .count = 0
  };

  uint8_t affordable[12];
  uint8_t n = build_affordable_champions(gstate, player,
                                         gstate->current_cash_balance[player], affordable);

  for(uint8_t i = 0; i < n; i++)
  { uint8_t c1[1] = { affordable[i] };
    evaluate_attack_subset(c1, 1, own_energy, opp_energy, own_hand, opp_hand,
                           own_cash, opp_cash, opp, gstate, params, belief, &modulated, &best);

    for(uint8_t j = i + 1; j < n; j++)
    { uint8_t c2[2] = { affordable[i], affordable[j] };
      evaluate_attack_subset(c2, 2, own_energy, opp_energy, own_hand, opp_hand,
                             own_cash, opp_cash, opp, gstate, params, belief, &modulated, &best);

      for(uint8_t k = j + 1; k < n; k++)
      { uint8_t c3[3] = { affordable[i], affordable[j], affordable[k] };
        evaluate_attack_subset(c3, 3, own_energy, opp_energy, own_hand, opp_hand,
                               own_cash, opp_cash, opp, gstate, params, belief, &modulated, &best);
      }
    }
  }

  consider_draw_and_cash_moves(gstate, player, own_energy, opp_energy, own_hand, opp_hand,
                               own_cash, opp_cash, params, belief, &modulated, &best);

  return best;
} // a13_best_attack_move

// Local port of A7's variance_aware_incoming() (ai_strat_hbt_enum.c), taking
// a raw stdev_mult float instead of pulling defense_stdev_mult out of
// HBTParams -- needed so Layer R's state-dependent state->stdev_eff (not a
// static params field) can drive this estimate. Otherwise identical.
static float a13_variance_aware_incoming(const struct gamestate* gstate, PlayerID defender,
                                         PlayerID attacker, float stdev_mult)
{ float expected = expected_incoming_attack(gstate, defender);

  float variance = 0.0f;
  const CombatZone* zone = &gstate->combat_zone[attacker];
  for(uint8_t i = 0; i < zone->size; i++)
    variance += champion_variance(zone->cards[i]);

  float incoming = expected + stdev_mult * sqrtf(variance);
  return (incoming < 0.0f) ? 0.0f : incoming;
} // a13_variance_aware_incoming

HBTBestMove a13_best_defense_move(const struct gamestate* gstate, PlayerID defender,
                                  const A13Params* params, const A13State* state)
{ PlayerID attacker = 1 - defender;
  float own_energy = (float)gstate->current_energy[defender];
  float opp_energy = (float)gstate->current_energy[attacker];
  float own_hand = (float)gstate->hand[defender].size;
  float opp_hand = (float)gstate->hand[attacker].size;
  float own_cash = (float)gstate->current_cash_balance[defender];
  float opp_cash = (float)gstate->current_cash_balance[attacker];
  float incoming = a13_variance_aware_incoming(gstate, defender, attacker, state->stdev_eff);
  HBTState modulated = race_modulated_state(state);

  // Same PASS-charges-the-attack fix A7's own hbt_best_defense_move() ships
  // (ai_strat_hbt_enum.c's header comment has the full history) -- inherited
  // by construction here, not re-derived: this agent was never built
  // against the old broken baseline.
  float damaged_energy = own_energy - incoming;
  if(damaged_energy < 0.0f) damaged_energy = 0.0f;

  HBTBestMove best =
  { .advantage = hbt_advantage(damaged_energy, opp_energy, own_hand, opp_hand,
                               own_cash, opp_cash, &params->base, &modulated),
    .type = HBT_MOVE_PASS, .count = 0
  };

  uint8_t affordable[12];
  uint8_t n = build_affordable_champions(gstate, defender,
                                         gstate->current_cash_balance[defender], affordable);

  for(uint8_t i = 0; i < n; i++)
  { uint8_t c1[1] = { affordable[i] };
    evaluate_defense_subset(c1, 1, own_energy, opp_energy, own_hand, opp_hand,
                            own_cash, opp_cash, incoming, &params->base, &modulated, &best);

    for(uint8_t j = i + 1; j < n; j++)
    { uint8_t c2[2] = { affordable[i], affordable[j] };
      evaluate_defense_subset(c2, 2, own_energy, opp_energy, own_hand, opp_hand,
                              own_cash, opp_cash, incoming, &params->base, &modulated, &best);

      for(uint8_t k = j + 1; k < n; k++)
      { uint8_t c3[3] = { affordable[i], affordable[j], affordable[k] };
        evaluate_defense_subset(c3, 3, own_energy, opp_energy, own_hand, opp_hand,
                                own_cash, opp_cash, incoming, &params->base, &modulated, &best);
      }
    }
  }

  return best;
} // a13_best_defense_move
