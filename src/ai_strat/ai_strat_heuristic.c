// ai_strat_heuristic.c
// A5 Heuristic strategy ("Eps-Gam-Del") -- see ai_strat_heuristic.h for the
// full spec (advantage formula, the "effective cards" reading, the
// information-hiding constraint, why no opponent block is modelled, why
// delta is pinned, and how the about.md-vs-design-docs tensions were
// resolved). Calibrated -- see aicalibsrc/heuristic/ and HEURISTIC_DEFAULTS's
// own comment below.

#include <math.h>

#include "ai_strat_heuristic.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"
#include "../core/card_actions.h"

// Terminal-win/-loss bonus in EnergyAdv -- the stub's own number, not a
// calibratable dial (same status as A1's VB_COST_FLOOR / A4's
// BR_COST_FLOOR): its only job is to dominate every other term once either
// player is at 0 energy, and any large-enough constant does that.
#define HEUR_LETHAL_BONUS 100000.0f

// Calibrated via aicalibsrc/heuristic/calibrate_heuristic.py's
// `optimize --identity-safe --opponent borealis`. weight_cash_advantage
// (delta) stays pinned at the spec default throughout -- see
// ai_strat_heuristic.h's header comment on why it is redundant with the
// other two weights.
//
// A manual univariate sweep of weight_cards_advantage (gamma) first, at the
// spec defaults otherwise, found a much larger useful range than the spec's
// own 0.15: win rate vs `borealis` climbed from 26.8% at gamma=1 to a peak
// of ~48.7% around gamma=6-8 before collapsing to 19.1% at gamma=12 (a
// clear unimodal shape) -- so BOUNDS was widened to 15.0 before searching.
// An unconstrained `optimize` run (all four free params) then found
// gamma=9.815, well past that identity-safe ceiling, at 59.67% [59.18%,
// 60.14%] validated vs `borealis` (40,000 games) -- flagged by
// check_personality_flags() as far outside the spec default's range.
// Playtracing that candidate (turn-count histograms via `-sa -p`, not just
// the aggregate rate) ruled out the failure mode the flag exists to catch
// (a degenerate "hoard forever, never attack" agent): it still finishes
// nearly every game in under 20 turns and wins 99.8% vs `rand`, i.e. a fast,
// decisive strategy, not a stall -- a high gamma changes which cards this
// agent prioritises within its one mechanism, it does not disable or add a
// mechanism (contrast A2's/A4's rejected extremes, which broke an explicit
// rule the agent's identity depends on). Still, `optimize --identity-safe`
// (gamma capped at 2.0, epsilon kept away from 0, taper capped at 2.0) was
// run as the character-preserving comparison per protocol: it converged to
// gamma=1.962 (pinned against its own ceiling) at a statistically
// indistinguishable 58.99% [58.51%, 59.47%] vs `borealis`. A 3-way
// self-play round-robin (defaults vs both candidates, 48,000 games/pairing)
// confirmed the two are inseparable (Bradley-Terry strength 1.1294 vs
// 1.1098) and both far ahead of the spec defaults (0.0) -- so the
// identity-safe candidate ships, keeping every weight closer to the
// stub's own illustrative numbers while giving up none of the measured
// strength.
//
// Measured (validated, both seats): vs `borealis` -> 58.99% [58.51%,
// 59.47%] (40,000 games); vs `rand` -> 99.85% [99.78%, 99.90%] (18,000
// games); vs `combo` -> 77.09% [76.47%, 77.70%] (18,000 games); vs `value`
// -> 81.81% [81.24%, 82.37%] (18,000 games); vs `balanced` -> 74.24%
// [73.59%, 74.87%] (18,000 games). The vs-`borealis` result lands this
// agent's Bradley-Terry rating above the anchor (above 50) -- run
// --stda.rating for the roster-wide fit before trusting this pairwise
// estimate as the shipped rating.
#define HEURISTIC_DEFAULTS \
  { .weight_energy_advantage = 0.34929208f, \
    .weight_cards_advantage = 1.96227051f, \
    .weight_cash_advantage = 1.0f, \
    .weight_taper_exponent = 0.10115113f, \
    .opp_card_discount = 0.98660043f }

static HeuristicParams g_params[2] = { HEURISTIC_DEFAULTS, HEURISTIC_DEFAULTS };

HeuristicParams heuristic_get_default_params(void)
{ HeuristicParams defaults = HEURISTIC_DEFAULTS;
  return defaults;
} // heuristic_get_default_params

void heuristic_set_params(PlayerID player, const HeuristicParams* params)
{ g_params[player] = *params;
} // heuristic_set_params

void heuristic_reset_params(void)
{ HeuristicParams defaults = HEURISTIC_DEFAULTS;
  g_params[PLAYER_A] = defaults;
  g_params[PLAYER_B] = defaults;
} // heuristic_reset_params

/* ========================================================================
   The advantage function itself -- one function shared by both phases and
   every move class, taking already-computed scalar features of a
   hypothetical resulting position (ai_strat_heuristic.h's formula).
   ======================================================================== */

static float heuristic_advantage(float own_energy, float opp_energy, float own_hand,
                                 float opp_hand, float own_cash, float opp_cash,
                                 const HeuristicParams* params)
{ float energy_adv = own_energy - opp_energy;
  if(opp_energy <= 0.0f) energy_adv += HEUR_LETHAL_BONUS;
  if(own_energy <= 0.0f) energy_adv -= HEUR_LETHAL_BONUS;

  float taper = powf(opp_energy / (float)INITIAL_ENERGY_DEFAULT,
                     params->weight_taper_exponent);

  float cards_adv = own_hand - opp_hand * params->opp_card_discount;
  float cash_adv = own_cash - opp_cash;

  return params->weight_energy_advantage * energy_adv +
         taper * params->weight_cards_advantage * cards_adv +
         taper * params->weight_cash_advantage * cash_adv;
} // heuristic_advantage

// Sigma(expected_attack) + combo bonus, clamped to opp_energy -- see
// ai_strat_heuristic.h's header comment on why no opponent block is
// modelled here.
static float predicted_damage(const uint8_t* cards, uint8_t count, float opp_energy)
{ float total = 0.0f;
  for(uint8_t i = 0; i < count; i++)
    total += fullDeck[cards[i]].expected_attack;
  total += (float)combo_bonus_for_selection(cards, count);

  if(total > opp_energy) total = opp_energy;
  if(total < 0.0f) total = 0.0f;
  return total;
} // predicted_damage

// Sigma(expected_defense) + combo bonus this selection would score.
static float predicted_block(const uint8_t* cards, uint8_t count)
{ float total = 0.0f;
  for(uint8_t i = 0; i < count; i++)
    total += fullDeck[cards[i]].expected_defense;
  total += (float)combo_bonus_for_selection(cards, count);
  return total;
} // predicted_block

/* ========================================================================
   Move representation and argmax bookkeeping, shared by both phases.
   ======================================================================== */

typedef enum
{ HEUR_MOVE_PASS = 0,
  HEUR_MOVE_CHAMPIONS,
  HEUR_MOVE_DRAW,
  HEUR_MOVE_CASH
} HeuristicMoveType;

typedef struct
{ float advantage;
  HeuristicMoveType type;
  uint8_t cards[3];
  uint8_t count;
} BestMove;

// Deterministic argmax -- strictly greater only, so the first-enumerated
// candidate wins ties. No epsilon tie-break: that is a Borealis-specific
// mechanism (tiebreak_epsilon, ai_strat_borealis.h), not part of this
// agent's about.md-stated identity.
static void consider_move(BestMove* best, float advantage, HeuristicMoveType type,
                          const uint8_t* cards, uint8_t count)
{ if(advantage <= best->advantage) return;

  best->advantage = advantage;
  best->type = type;
  best->count = count;
  for(uint8_t i = 0; i < count; i++) best->cards[i] = cards[i];
} // consider_move

/* ========================================================================
   Attack phase: pass / every 1-3 affordable-champion subset (no pruning,
   mirrors A3's collect_candidates()) / each affordable held draw card /
   each affordable held cash card, all scored by the same advantage function
   and compared directly against each other and against pass.
   ======================================================================== */

// Scores one 1-3 champion subset as an attack play against the running
// best, skipping it if unaffordable.
static void evaluate_attack_subset(const uint8_t* cards, uint8_t count, float own_energy,
                                   float opp_energy, float own_hand, float opp_hand,
                                   float own_cash, float opp_cash,
                                   const HeuristicParams* params, BestMove* best)
{ float cost = 0.0f;
  for(uint8_t i = 0; i < count; i++) cost += (float)fullDeck[cards[i]].cost;
  if(cost > own_cash) return;

  float dmg = predicted_damage(cards, count, opp_energy);
  float adv = heuristic_advantage(own_energy, opp_energy - dmg, own_hand - (float)count,
                                  opp_hand, own_cash - cost, opp_cash, params);
  consider_move(best, adv, HEUR_MOVE_CHAMPIONS, cards, count);
} // evaluate_attack_subset

static BestMove best_attack_move(struct gamestate* gstate, PlayerID player,
                                 const HeuristicParams* params)
{ PlayerID opp = 1 - player;
  float own_energy = (float)gstate->current_energy[player];
  float opp_energy = (float)gstate->current_energy[opp];
  float own_hand = (float)gstate->hand[player].size;
  float opp_hand = (float)gstate->hand[opp].size;
  float own_cash = (float)gstate->current_cash_balance[player];
  float opp_cash = (float)gstate->current_cash_balance[opp];

  BestMove best =
  { .advantage = heuristic_advantage(own_energy, opp_energy, own_hand, opp_hand,
                                     own_cash, opp_cash, params),
    .type = HEUR_MOVE_PASS, .count = 0
  };

  uint8_t affordable[12];
  uint8_t n = build_affordable_champions(gstate, player,
                                         gstate->current_cash_balance[player], affordable);

  for(uint8_t i = 0; i < n; i++)
  { uint8_t c1[1] = { affordable[i] };
    evaluate_attack_subset(c1, 1, own_energy, opp_energy, own_hand, opp_hand,
                           own_cash, opp_cash, params, &best);

    for(uint8_t j = i + 1; j < n; j++)
    { uint8_t c2[2] = { affordable[i], affordable[j] };
      evaluate_attack_subset(c2, 2, own_energy, opp_energy, own_hand, opp_hand,
                             own_cash, opp_cash, params, &best);

      for(uint8_t k = j + 1; k < n; k++)
      { uint8_t c3[3] = { affordable[i], affordable[j], affordable[k] };
        evaluate_attack_subset(c3, 3, own_energy, opp_energy, own_hand, opp_hand,
                               own_cash, opp_cash, params, &best);
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
      float adv = heuristic_advantage(own_energy, opp_energy, new_hand, opp_hand,
                                      new_cash, opp_cash, params);
      consider_move(&best, adv, HEUR_MOVE_DRAW, &card_idx, 1);
    }
    // A cash card with no champion in hand is a pure hand+cash loss
    // (play_cash_card_ai() pays the cost and discards the card with no
    // exchange when champion_to_exchange == UINT8_MAX) -- never a candidate.
    else if(fullDeck[card_idx].card_type == CASH_CARD && has_champion)
    { float new_hand = own_hand - 2.0f;
      float new_cash = own_cash - (float)fullDeck[card_idx].cost +
                       (float)fullDeck[card_idx].exchange_cash;
      float adv = heuristic_advantage(own_energy, opp_energy, new_hand, opp_hand,
                                      new_cash, opp_cash, params);
      consider_move(&best, adv, HEUR_MOVE_CASH, &card_idx, 1);
    }
  }

  return best;
} // best_attack_move

void heuristic_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID player = gstate->current_player;
  const HeuristicParams* params = &g_params[player];

  BestMove move = best_attack_move(gstate, player, params);

  switch(move.type)
  { case HEUR_MOVE_CHAMPIONS:
      for(uint8_t i = 0; i < move.count; i++)
        play_champion(gstate, player, move.cards[i], ctx);
      return;
    case HEUR_MOVE_DRAW:
      play_draw_card(gstate, player, move.cards[0], ctx);
      return;
    case HEUR_MOVE_CASH:
      play_cash_card_ai(gstate, player, move.cards[0], ctx);
      return;
    case HEUR_MOVE_PASS:
    default:
      return;
  }
} // heuristic_attack_strategy

/* ========================================================================
   Defense phase: pass (decline) / every 0-3 affordable-champion subset,
   same advantage function. Declining, or blocking short of a full stop,
   falls straight out of the weighted sum -- no separate threshold
   parameter, unlike A4's defense_beta.
   ======================================================================== */

// Scores one 1-3 champion subset as a defensive play against the running
// best (the empty/decline set is the caller's own baseline, evaluated once
// up front exactly like MOVE_PASS on attack).
static void evaluate_defense_subset(const uint8_t* cards, uint8_t count, float own_energy,
                                    float opp_energy, float own_hand, float opp_hand,
                                    float own_cash, float opp_cash, float incoming,
                                    const HeuristicParams* params, BestMove* best)
{ float cost = 0.0f;
  for(uint8_t i = 0; i < count; i++) cost += (float)fullDeck[cards[i]].cost;
  if(cost > own_cash) return;

  float damage = incoming - predicted_block(cards, count);
  if(damage < 0.0f) damage = 0.0f;

  float new_energy = own_energy - damage;
  if(new_energy < 0.0f) new_energy = 0.0f;

  float adv = heuristic_advantage(new_energy, opp_energy, own_hand - (float)count,
                                  opp_hand, own_cash - cost, opp_cash, params);
  consider_move(best, adv, HEUR_MOVE_CHAMPIONS, cards, count);
} // evaluate_defense_subset

static BestMove best_defense_move(struct gamestate* gstate, PlayerID defender,
                                  const HeuristicParams* params)
{ PlayerID attacker = 1 - defender;
  float own_energy = (float)gstate->current_energy[defender];
  float opp_energy = (float)gstate->current_energy[attacker];
  float own_hand = (float)gstate->hand[defender].size;
  float opp_hand = (float)gstate->hand[attacker].size;
  float own_cash = (float)gstate->current_cash_balance[defender];
  float opp_cash = (float)gstate->current_cash_balance[attacker];
  float incoming = expected_incoming_attack(gstate, defender);

  BestMove best =
  { .advantage = heuristic_advantage(own_energy, opp_energy, own_hand, opp_hand,
                                     own_cash, opp_cash, params),
    .type = HEUR_MOVE_PASS, .count = 0
  };

  uint8_t affordable[12];
  uint8_t n = build_affordable_champions(gstate, defender,
                                         gstate->current_cash_balance[defender], affordable);

  for(uint8_t i = 0; i < n; i++)
  { uint8_t c1[1] = { affordable[i] };
    evaluate_defense_subset(c1, 1, own_energy, opp_energy, own_hand, opp_hand,
                            own_cash, opp_cash, incoming, params, &best);

    for(uint8_t j = i + 1; j < n; j++)
    { uint8_t c2[2] = { affordable[i], affordable[j] };
      evaluate_defense_subset(c2, 2, own_energy, opp_energy, own_hand, opp_hand,
                              own_cash, opp_cash, incoming, params, &best);

      for(uint8_t k = j + 1; k < n; k++)
      { uint8_t c3[3] = { affordable[i], affordable[j], affordable[k] };
        evaluate_defense_subset(c3, 3, own_energy, opp_energy, own_hand, opp_hand,
                                own_cash, opp_cash, incoming, params, &best);
      }
    }
  }

  return best;
} // best_defense_move

void heuristic_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID defender = 1 - gstate->current_player;
  const HeuristicParams* params = &g_params[defender];

  BestMove move = best_defense_move(gstate, defender, params);
  if(move.type != HEUR_MOVE_CHAMPIONS) return; // decline

  for(uint8_t i = 0; i < move.count; i++)
    play_champion(gstate, defender, move.cards[i], ctx);
} // heuristic_defense_strategy
