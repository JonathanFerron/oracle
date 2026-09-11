// ai_strat_adept.c
// Adept gap-filler strategy ("Reduced Heuristic" / The Adept) -- see
// doc/ai_agents.md's gap-3 section.
//
// A5 Heuristic ("Eps-Gam-Del") scores every candidate move by a weighted
// sum of energy/cards/cash advantage, the cards/cash terms tapered down
// as the opponent's energy drops (ai_strat_heuristic.c's
// heuristic_advantage()). The Adept keeps A5's exact enumeration shape
// (pass/1-3 champion subsets/draw/cash on attack; decline/1-3 subsets on
// defense, same PASS-charges-damage baseline) and A5's exact formula
// shape (energy + taper*cards + taper*cash, all three terms), but with a
// much smaller weight_cards_advantage than A5's own tuned 1.96 --
// "grasped the core idea, hasn't picked up how much the card-count
// nuance actually matters."
//
// Two earlier, more aggressive cuts were tried and rejected as too
// destructive to read as "reduced" rather than "broken": dropping
// cards_advantage AND taper together measured 10 (catastrophic -- without
// taper, cash_advantage's raw weight of 1.0 outweighs energy_advantage's
// 0.349 even deep in the endgame, so this agent could get swayed toward
// hoarding cash instead of finishing the game); restoring taper but still
// dropping cards_advantage to exactly 0 still measured 11 -- so the cards
// term's near-total absence, not specifically the missing taper, is what
// devastates this formula. A small residual weight (this file's shipped
// value) keeps the intended handicap without total collapse -- see
// doc/ai_agents.md's gap-3 section for the full measurement history.

#include <math.h>

#include "ai_strat_adept.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"

#define ADEPT_LETHAL_BONUS 100000.0f

// A5's own HEURISTIC_DEFAULTS values (ai_strat_heuristic.c), except
// weight_cards_advantage -- see this file's header comment for why it's
// a small residual value rather than A5's own 1.96 or a flat 0.
#define ADEPT_WEIGHT_ENERGY_ADVANTAGE 0.34929208f
#define ADEPT_WEIGHT_CARDS_ADVANTAGE 1.6f
#define ADEPT_WEIGHT_CASH_ADVANTAGE 1.0f
#define ADEPT_WEIGHT_TAPER_EXPONENT 0.10115113f
#define ADEPT_OPP_CARD_DISCOUNT 0.98660043f

static float adept_advantage(float own_energy, float opp_energy, float own_hand,
                             float opp_hand, float own_cash, float opp_cash)
{ float energy_adv = own_energy - opp_energy;
  if(opp_energy <= 0.0f) energy_adv += ADEPT_LETHAL_BONUS;
  if(own_energy <= 0.0f) energy_adv -= ADEPT_LETHAL_BONUS;

  float taper = powf(opp_energy / (float)INITIAL_ENERGY_DEFAULT, ADEPT_WEIGHT_TAPER_EXPONENT);

  float cards_adv = own_hand - opp_hand * ADEPT_OPP_CARD_DISCOUNT;
  float cash_adv = own_cash - opp_cash;

  return ADEPT_WEIGHT_ENERGY_ADVANTAGE * energy_adv +
         taper * ADEPT_WEIGHT_CARDS_ADVANTAGE * cards_adv +
         taper * ADEPT_WEIGHT_CASH_ADVANTAGE * cash_adv;
} // adept_advantage

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
{ ADEPT_MOVE_PASS = 0,
  ADEPT_MOVE_CHAMPIONS,
  ADEPT_MOVE_DRAW,
  ADEPT_MOVE_CASH
} AdeptMoveType;

typedef struct
{ float advantage;
  AdeptMoveType type;
  uint8_t cards[3];
  uint8_t count;
} AdeptBestMove;

static void consider_move(AdeptBestMove* best, float advantage, AdeptMoveType type,
                          const uint8_t* cards, uint8_t count)
{ if(advantage <= best->advantage) return;

  best->advantage = advantage;
  best->type = type;
  best->count = count;
  for(uint8_t i = 0; i < count; i++) best->cards[i] = cards[i];
} // consider_move

/* ========================================================================
   Attack: pass / every 1-3 affordable-champion subset / every affordable
   draw card / every affordable cash card -- A5's exact enumeration shape.
   ======================================================================== */

static void evaluate_attack_subset(const uint8_t* cards, uint8_t count, float own_energy,
                                   float opp_energy, float own_hand, float opp_hand,
                                   float own_cash, float opp_cash, AdeptBestMove* best)
{ float cost = 0.0f;
  for(uint8_t i = 0; i < count; i++) cost += (float)fullDeck[cards[i]].cost;
  if(cost > own_cash) return;

  float dmg = predicted_damage(cards, count, opp_energy);
  float adv = adept_advantage(own_energy, opp_energy - dmg, own_hand - (float)count,
                              opp_hand, own_cash - cost, opp_cash);
  consider_move(best, adv, ADEPT_MOVE_CHAMPIONS, cards, count);
} // evaluate_attack_subset

static AdeptBestMove best_attack_move(struct gamestate* gstate, PlayerID player)
{ PlayerID opp = 1 - player;
  float own_energy = (float)gstate->current_energy[player];
  float opp_energy = (float)gstate->current_energy[opp];
  float own_hand = (float)gstate->hand[player].size;
  float opp_hand = (float)gstate->hand[opp].size;
  float own_cash = (float)gstate->current_cash_balance[player];
  float opp_cash = (float)gstate->current_cash_balance[opp];

  AdeptBestMove best =
  { .advantage = adept_advantage(own_energy, opp_energy, own_hand, opp_hand,
                                 own_cash, opp_cash),
    .type = ADEPT_MOVE_PASS, .count = 0
  };

  uint8_t affordable[12];
  uint8_t n = build_affordable_champions(gstate, player,
                                         gstate->current_cash_balance[player], affordable);

  for(uint8_t i = 0; i < n; i++)
  { uint8_t c1[1] = { affordable[i] };
    evaluate_attack_subset(c1, 1, own_energy, opp_energy, own_hand, opp_hand,
                           own_cash, opp_cash, &best);

    for(uint8_t j = i + 1; j < n; j++)
    { uint8_t c2[2] = { affordable[i], affordable[j] };
      evaluate_attack_subset(c2, 2, own_energy, opp_energy, own_hand, opp_hand,
                             own_cash, opp_cash, &best);

      for(uint8_t k = j + 1; k < n; k++)
      { uint8_t c3[3] = { affordable[i], affordable[j], affordable[k] };
        evaluate_attack_subset(c3, 3, own_energy, opp_energy, own_hand, opp_hand,
                               own_cash, opp_cash, &best);
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
      float adv = adept_advantage(own_energy, opp_energy, new_hand, opp_hand,
                                  new_cash, opp_cash);
      consider_move(&best, adv, ADEPT_MOVE_DRAW, &card_idx, 1);
    }
    else if(fullDeck[card_idx].card_type == CASH_CARD && has_champion)
    { float new_hand = own_hand - 2.0f;
      float new_cash = own_cash - (float)fullDeck[card_idx].cost +
                       (float)fullDeck[card_idx].exchange_cash;
      float adv = adept_advantage(own_energy, opp_energy, new_hand, opp_hand,
                                  new_cash, opp_cash);
      consider_move(&best, adv, ADEPT_MOVE_CASH, &card_idx, 1);
    }
  }

  return best;
} // best_attack_move

void adept_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID player = gstate->current_player;

  AdeptBestMove move = best_attack_move(gstate, player);

  switch(move.type)
  { case ADEPT_MOVE_CHAMPIONS:
      for(uint8_t i = 0; i < move.count; i++)
        play_champion(gstate, player, move.cards[i], ctx);
      return;
    case ADEPT_MOVE_DRAW:
      play_draw_card(gstate, player, move.cards[0], ctx);
      return;
    case ADEPT_MOVE_CASH:
      play_cash_card_ai(gstate, player, move.cards[0], ctx);
      return;
    case ADEPT_MOVE_PASS:
    default:
      return;
  }
} // adept_attack_strategy

/* ========================================================================
   Defense: pass (decline) / every 0-3 affordable-champion subset, PASS
   charging the full incoming attack against own_energy -- the same
   corrected baseline A5/A7 both ship
   (project_a5_a7_defense_pass_dominance).
   ======================================================================== */

static void evaluate_defense_subset(const uint8_t* cards, uint8_t count, float own_energy,
                                    float opp_energy, float own_hand, float opp_hand,
                                    float own_cash, float opp_cash, float incoming,
                                    AdeptBestMove* best)
{ float cost = 0.0f;
  for(uint8_t i = 0; i < count; i++) cost += (float)fullDeck[cards[i]].cost;
  if(cost > own_cash) return;

  float damage = incoming - predicted_block(cards, count);
  if(damage < 0.0f) damage = 0.0f;

  float new_energy = own_energy - damage;
  if(new_energy < 0.0f) new_energy = 0.0f;

  float adv = adept_advantage(new_energy, opp_energy, own_hand - (float)count,
                              opp_hand, own_cash - cost, opp_cash);
  consider_move(best, adv, ADEPT_MOVE_CHAMPIONS, cards, count);
} // evaluate_defense_subset

static AdeptBestMove best_defense_move(struct gamestate* gstate, PlayerID defender)
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

  AdeptBestMove best =
  { .advantage = adept_advantage(damaged_energy, opp_energy, own_hand, opp_hand,
                                 own_cash, opp_cash),
    .type = ADEPT_MOVE_PASS, .count = 0
  };

  uint8_t affordable[12];
  uint8_t n = build_affordable_champions(gstate, defender,
                                         gstate->current_cash_balance[defender], affordable);

  for(uint8_t i = 0; i < n; i++)
  { uint8_t c1[1] = { affordable[i] };
    evaluate_defense_subset(c1, 1, own_energy, opp_energy, own_hand, opp_hand,
                            own_cash, opp_cash, incoming, &best);

    for(uint8_t j = i + 1; j < n; j++)
    { uint8_t c2[2] = { affordable[i], affordable[j] };
      evaluate_defense_subset(c2, 2, own_energy, opp_energy, own_hand, opp_hand,
                              own_cash, opp_cash, incoming, &best);

      for(uint8_t k = j + 1; k < n; k++)
      { uint8_t c3[3] = { affordable[i], affordable[j], affordable[k] };
        evaluate_defense_subset(c3, 3, own_energy, opp_energy, own_hand, opp_hand,
                                own_cash, opp_cash, incoming, &best);
      }
    }
  }

  return best;
} // best_defense_move

void adept_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID defender = 1 - gstate->current_player;

  AdeptBestMove move = best_defense_move(gstate, defender);
  if(move.type != ADEPT_MOVE_CHAMPIONS) return;

  for(uint8_t i = 0; i < move.count; i++)
    play_champion(gstate, defender, move.cards[i], ctx);
} // adept_defense_strategy
