// ai_strat_clairvoyant1.c
// A12 Clairvoyant -- see ai_strat_clairvoyant1.h.

#include <math.h>
#include <stdio.h>

#include "ai_strat_clairvoyant1.h"
#include "ai_strat_simplemc_search.h"
#include "ai_strat_playout.h"
#include "ai_strat_common.h"
#include "ai_strat_random.h"
#include "ai_strat_lib_heuristics.h"
#include "../actions/move_apply.h"
#include "../core/card_actions.h"
#include "../core/game_constants.h"
#include "../util/debug.h"

static SimpleMcParams g_params[2] = { SIMPLEMC_DEFAULTS, SIMPLEMC_DEFAULTS };

SimpleMcParams clairvoyant_get_default_params(void)
{ SimpleMcParams defaults = SIMPLEMC_DEFAULTS;
  return defaults;
} // clairvoyant_get_default_params

void clairvoyant_set_params(PlayerID player, const SimpleMcParams* params)
{ g_params[player] = *params;
} // clairvoyant_set_params

void clairvoyant_reset_params(void)
{ SimpleMcParams defaults = SIMPLEMC_DEFAULTS;
  g_params[PLAYER_A] = defaults;
  g_params[PLAYER_B] = defaults;
} // clairvoyant_reset_params

// -- Cheap opponent-side rollout policy -------------------------------------
// No move enumeration, no per-subset search: a single fixed candidate (the
// top up to 3 affordable champions by `power`) scored by one closed-form
// formula. Deliberately not A5's or any other agent's real mechanism.

#define ROLLOUT_ENERGY_WEIGHT 0.01f  // small aggression nudge, doesn't dominate
#define ROLLOUT_DRAW_MIN_HAND 5
#define ROLLOUT_DRAW_OPP_FLOOR 1
// Cost weight for both the attack and defense score. Started from Borealis's
// own calibrated luna_value/lambda (ai_strat_borealis.h: "damage-units per
// luna", BOREALIS_DEFAULTS.luna_value = 4.5846f) as a first estimate, since
// it's exactly the same quantity: an expected-value-per-cost trade-off,
// already tuned via real calibration for this game. But that value was
// tuned for a different job -- helping Borealis choose ITS OWN moves well --
// not for "what makes the rollout's opponent-model produce value estimates
// that lead this agent to its best decisions", so a sweep (both seats,
// n=700/point, vs borealis, 2026-08-25 -- see doc/changelog.md) checked it
// directly: a quadratic fit was genuinely unimodal (R^2=0.73), implying an
// optimum near 3.86, and 4.5846 measured near the low edge of a fairly flat
// 1-7 plateau rather than its center (28.0% vs the plateau's ~30-32%).
// Shipped at 3.0 (empirical peak, close to the fitted optimum) -- a real,
// modest improvement over the borrowed value, not a large one. Still a
// runtime-settable sweep target (clairvoyant_set_cost_weight()), not a
// compile-time constant, in case it's revisited. Without any cost term at
// all, the first smoke test found the attack score always positive
// regardless of cost -- the simulated opponent committed on ~100% of
// decisions (see doc/changelog.md's 2026-08-25 entry for that trace),
// modeling a strawman always-all-in opponent rather than a sharper one.
static float g_cost_weight = 3.0f;

void clairvoyant_set_cost_weight(float weight)
{ g_cost_weight = weight;
} // clairvoyant_set_cost_weight

float clairvoyant_get_cost_weight(void)
{ return g_cost_weight;
} // clairvoyant_get_cost_weight

// Selects up to 3 champions from `affordable` by descending `power` -- a
// single fixed candidate, not a search over candidates. `count` is capped at
// 12 by Hand's own size limit, so this is cheap regardless.
static uint8_t pick_top_by_power(const uint8_t* affordable, uint8_t count, uint8_t* out)
{ uint8_t remaining[12];
  for(uint8_t i = 0; i < count; i++) remaining[i] = affordable[i];
  uint8_t remaining_n = count;
  uint8_t out_n = 0;

  while(out_n < 3 && remaining_n > 0)
  { uint8_t best_idx = 0;
    for(uint8_t i = 1; i < remaining_n; i++)
      if(fullDeck[remaining[i]].power > fullDeck[remaining[best_idx]].power)
        best_idx = i;
    out[out_n++] = remaining[best_idx];
    remaining[best_idx] = remaining[--remaining_n];
  }
  return out_n;
} // pick_top_by_power

static void rollout_opponent_attack(struct gamestate* gstate, GameContext* ctx)
{ PlayerID player = gstate->current_player;
  PlayerID opponent = 1 - player;

  if(try_play_draw_card(gstate, player, ROLLOUT_DRAW_MIN_HAND, ROLLOUT_DRAW_OPP_FLOOR, ctx))
    return;

  uint16_t budget = gstate->current_cash_balance[player];
  uint8_t affordable[12];
  uint8_t count = build_affordable_champions(gstate, player, budget, affordable);

  uint8_t top[3];
  uint8_t top_n = pick_top_by_power(affordable, count, top);
  if(top_n == 0)
  { try_play_cash_fallback(gstate, player, count, ctx);
    return;
  }

  float total_cost = 0.0f;
  for(uint8_t i = 0; i < top_n; i++) total_cost += fullDeck[top[i]].cost;

  float score = 0.0f;
  for(uint8_t i = 0; i < top_n; i++) score += fullDeck[top[i]].expected_attack;
  score += (float)combo_bonus_for_selection(top, top_n);
  score += ROLLOUT_ENERGY_WEIGHT *
           ((float)gstate->current_energy[player] - (float)gstate->current_energy[opponent]);
  score -= g_cost_weight * total_cost;

  DEBUG_PRINT(" CVRollout ATTACK: top_n=%u score=%.2f cost=%.1f energy=%u/%u -> %s\n",
              top_n, (double)score, (double)total_cost, gstate->current_energy[player],
              gstate->current_energy[opponent], score > 0.0f ? "COMMIT" : "cash/pass");

  if(score > 0.0f)
  { for(uint8_t i = 0; i < top_n; i++) play_champion(gstate, player, top[i], ctx);
  }
  else
    try_play_cash_fallback(gstate, player, count, ctx);
} // rollout_opponent_attack

static void rollout_opponent_defense(struct gamestate* gstate, GameContext* ctx)
{ PlayerID attacker = gstate->current_player;
  PlayerID defender = 1 - attacker;

  uint16_t budget = gstate->current_cash_balance[defender];
  uint8_t affordable[12];
  uint8_t count = build_affordable_champions(gstate, defender, budget, affordable);

  uint8_t top[3];
  uint8_t top_n = pick_top_by_power(affordable, count, top);
  if(top_n == 0) return; // decline -- nothing affordable to block with

  float total_cost = 0.0f;
  for(uint8_t i = 0; i < top_n; i++) total_cost += fullDeck[top[i]].cost;

  float raw_defense = 0.0f;
  for(uint8_t i = 0; i < top_n; i++) raw_defense += fullDeck[top[i]].expected_defense;
  raw_defense += (float)combo_bonus_for_selection(top, top_n);
  float incoming = expected_incoming_attack(gstate, defender);

  // Mirrors Borealis's own defense evaluation (ai_strat_borealis_enum.h:
  // "value(S) is capped at the incoming threat" -- no point overcommitting
  // beyond stopping the whole attack), then the same lambda*cost term as the
  // attack side, so both halves of this heuristic share one formula shape.
  float capped = fminf(raw_defense, incoming);
  float net = capped - g_cost_weight * total_cost;

  DEBUG_PRINT(" CVRollout DEFENSE: top_n=%u raw=%.2f capped=%.2f cost=%.1f net=%.2f -> %s\n",
              top_n, (double)raw_defense, (double)capped, (double)total_cost, (double)net,
              net > 0.0f ? "COMMIT" : "decline");

  if(net > 0.0f)
    for(uint8_t i = 0; i < top_n; i++) play_champion(gstate, defender, top[i], ctx);
  // else: decline (0 champions)
} // rollout_opponent_defense

// A "me": random future moves, per about.md -- the search stays a simple MC
// approach. "Opponent": the cheap heuristic above. Mulligan/discard default
// to the same shared power-based heuristic AI_STRATEGY_RANDOM resolves to.
static StrategySet clairvoyant_rollout_strategy_set(PlayerID me)
{ PlayerID opponent = 1 - me;
  StrategySet strats = {0};

  strats.attack_strategy[me] = random_attack_strategy;
  strats.defense_strategy[me] = random_defense_strategy;
  strats.attack_strategy[opponent] = rollout_opponent_attack;
  strats.defense_strategy[opponent] = rollout_opponent_defense;
  strats.mulligan_strategy[PLAYER_A] = strat_lib_mulligan;
  strats.mulligan_strategy[PLAYER_B] = strat_lib_mulligan;
  strats.discard_strategy[PLAYER_A] = strat_lib_discard_to_7;
  strats.discard_strategy[PLAYER_B] = strat_lib_discard_to_7;

  return strats;
} // clairvoyant_rollout_strategy_set

// -- Agent entry points ------------------------------------------------------

// Same contract as ai_strat_simplemc1.c's fork_for_decision(): the live
// context advances by exactly this one draw per decision.
static GameContext fork_for_decision(GameContext* ctx)
{ uint32_t seed = genRandLong(&ctx->rng);
  return mc_fork_context(ctx, seed);
} // fork_for_decision

static void decide_and_apply(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ const SimpleMcParams* params = &g_params[player];
  GameContext sim_ctx = fork_for_decision(ctx);
  StrategySet rollout_strats = clairvoyant_rollout_strategy_set(player);

  GameMove move = mc_search_best_move(gstate, player, &sim_ctx, params, &rollout_strats);
  apply_move(gstate, player, &move, ctx);
} // decide_and_apply

void clairvoyant_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ decide_and_apply(gstate, gstate->current_player, ctx);
} // clairvoyant_attack_strategy

void clairvoyant_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ decide_and_apply(gstate, 1 - gstate->current_player, ctx);
} // clairvoyant_defense_strategy
