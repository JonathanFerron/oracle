// ai_strat_a13_state.c
// Implementation of ai_strat_a13_state.h -- Layer R (race arithmetic). See
// ai_strat_a13.h's "Layer R: race arithmetic" section for the formula and
// the defense_stdev_mult sign convention.

#include <math.h>
#include <stdlib.h>

#include "ai_strat_a13_state.h"
#include "ai_strat_common.h"
#include "../core/game_constants.h"

// Deck-wide mean expected_attack across fullDeck[]'s 102 champion cards --
// same derivation as game_constants.h's AVERAGE_POWER_FOR_MULLIGAN (4.98,
// the deck-wide mean of the `power` field), applied to expected_attack
// instead. Used only inside opp_dpt_fallback() below, so this constant never
// depends on the belief module -- race_use_belief_opp=false must work even
// if Layer K's belief mechanism doesn't calibrate (ai_strat_a13.h).
#define A13_AVG_CHAMPION_EXPECTED_ATTACK 8.26f

// Same cash-tier convention A6/A7 already use for their own opponent-power
// estimate (ai_strat_hbt_enum.c's local HBT_OPP_CASH_* port of A6's own),
// duplicated locally rather than shared -- CLAUDE.md's "manual/duplicated
// code is preferred over macro-magic abstractions", and neither source is
// exported for reuse anyway.
#define A13_OPP_CASH_HIGH_THRESHOLD 35
#define A13_OPP_CASH_HIGH_MULT 1.1f
#define A13_OPP_CASH_LOW_THRESHOLD 15
#define A13_OPP_CASH_LOW_MULT 0.9f

static float clampf(float v, float lo, float hi)
{ if(v < lo) return lo;
  if(v > hi) return hi;
  return v;
} // clampf

static int cmp_attack_desc(const void* a, const void* b)
{ uint8_t ca = *(const uint8_t*)a;
  uint8_t cb = *(const uint8_t*)b;
  float ea = fullDeck[ca].expected_attack;
  float eb = fullDeck[cb].expected_attack;

  if(ea > eb) return -1;
  if(ea < eb) return 1;
  return (int)ca - (int)cb; // deterministic tie-break: no two calls may disagree
} // cmp_attack_desc

// A coarse, cheap race-position proxy -- NOT the enumeration's own optimal
// pick (ai_strat_a13_enum.c's job, combo-aware and penalty-weighted). Sum of
// expected_attack over the top-min(3,affordable) own hand champions, no
// combo bonus: good enough for a turns-to-kill ESTIMATE, not a move
// decision.
static float my_sustainable_damage(const struct gamestate* gstate, PlayerID player)
{ uint8_t affordable[12];
  uint8_t n = build_affordable_champions(gstate, player,
                                         gstate->current_cash_balance[player], affordable);
  if(n == 0) return 0.0f;

  qsort(affordable, n, sizeof(uint8_t), cmp_attack_desc);

  float total = 0.0f;
  uint8_t take = (n < 3) ? n : 3;
  for(uint8_t i = 0; i < take; i++)
    total += fullDeck[affordable[i]].expected_attack;
  return total;
} // my_sustainable_damage

// The race_use_belief_opp=false default: a plain public-info proxy (hand
// size, capped at the 3-champion combat zone limit, times the deck-wide
// mean expected_attack, cash-tier adjusted) that never touches A13Belief --
// this is what lets Layer R ship even if Layer K's opponent-hand belief
// doesn't calibrate (ai_strat_a13.h's "race_use_belief_opp" dial comment).
static float opp_dpt_fallback(const struct gamestate* gstate, PlayerID opponent)
{ uint8_t committed = gstate->hand[opponent].size;
  if(committed > 3) committed = 3;

  float estimate = (float)committed * A13_AVG_CHAMPION_EXPECTED_ATTACK;
  uint16_t cash = gstate->current_cash_balance[opponent];
  if(cash > A13_OPP_CASH_HIGH_THRESHOLD) estimate *= A13_OPP_CASH_HIGH_MULT;
  else if(cash < A13_OPP_CASH_LOW_THRESHOLD) estimate *= A13_OPP_CASH_LOW_MULT;
  return estimate;
} // opp_dpt_fallback

A13State a13_evaluate_state(struct gamestate* gstate, PlayerID player,
                            const A13Params* params, const A13Belief* belief)
{ A13State state;
  state.base = hbt_evaluate_state(gstate, player, &params->base);

  if(params->race_scale <= 0.0f)
  { state.stdev_eff = params->base.defense_stdev_mult;
    state.eps_race = 1.0f;
    return state;
  }

  PlayerID opp = 1 - player;
  float my_dpt = my_sustainable_damage(gstate, player)
                 - params->belief_opp_block_trust * belief->e_opp_block;
  float opp_dpt = params->race_use_belief_opp
                  ? belief->e_opp_attack
                  : opp_dpt_fallback(gstate, opp);

  float ttk_me = clampf((float)gstate->current_energy[opp] / fmaxf(my_dpt, 0.5f), 1.0f, 30.0f);
  float ttk_opp = clampf((float)gstate->current_energy[player] / fmaxf(opp_dpt, 0.5f), 1.0f, 30.0f);
  float tempo = (player == gstate->current_player) ? 0.5f : -0.5f;
  float race = ttk_opp - ttk_me + tempo;
  float u = clampf(race / params->race_scale, -1.0f, 1.0f);

  state.stdev_eff = params->base.defense_stdev_mult
                    + params->race_stdev_ahead * fmaxf(u, 0.0f)
                    + params->race_stdev_behind * fminf(u, 0.0f);
  state.eps_race = 1.0f + params->race_eps_gain * u;

  return state;
} // a13_evaluate_state
