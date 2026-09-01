// ai_strat_a13.c
// A13 Cartographer strategy ("The Cartographer") -- see ai_strat_a13.h for
// the full spec. Attack/defense orchestration, parameter management, and
// the mulligan/discard-to-7 overrides; belief construction lives in
// ai_strat_a13_belief.c, per-turn state derivation in ai_strat_a13_state.c,
// move enumeration/scoring in ai_strat_a13_enum.c (same file-length split
// as A7 Hybrid HBT's own ai_strat_hbt/ai_strat_hbt_enum split).
//
// Design-only status as of 2026-08-31 -- no calibration has run yet
// (aicalibsrc/carto/ doesn't exist), so every new dial below defaults to
// its neutral value. Until Step 5's staged calibration (ai_strat_a13.h's
// "Ship gates" section) tunes something, this agent is A7 bit-for-bit by
// construction -- see a13_belief_needed() and A13_DEFAULTS' own comment.

#include "ai_strat_a13.h"
#include "ai_strat_a13_belief.h"
#include "ai_strat_a13_state.h"
#include "ai_strat_a13_enum.h"
#include "../core/card_actions.h"

static A13Params g_params[2];
static bool g_params_initialized = false;

A13Params a13_get_default_params(void)
{ A13Params defaults;
  defaults.base = hbt_get_default_params(); // A7's 34 fields, frozen -- a
  // LIVE call, not a snapshot, so this agent inherits any future A7
  // improvement automatically (same pattern as A9's HBT2PlyParams.base,
  // ai_strat_hbt2ply.c).

  // Every new dial defaults to its neutral value -- no calibration has run
  // yet. A fresh checkout of this agent is therefore provably identical to
  // A7 until Step 5's staged calibration actually tunes something (see
  // ai_strat_a13.h's "The superset guarantee").
  defaults.race_scale = 0.0f;
  defaults.race_stdev_ahead = 0.0f;
  defaults.race_stdev_behind = 0.0f;
  defaults.race_eps_gain = 0.0f;
  defaults.race_use_belief_opp = false;
  defaults.belief_draw_weight = 0.0f;
  defaults.belief_reshuffle_trust = 0.0f;
  defaults.belief_opp_block_trust = 0.0f;
  defaults.hplus_trust = 0.0f;
  defaults.hplus_block_combo = 0.0f;

  return defaults;
} // a13_get_default_params

// g_params can't use a static designated initializer the way A7's own
// g_params[2] = { HBT_DEFAULTS, HBT_DEFAULTS } does, since .base comes from
// a function call (hbt_get_default_params()) rather than a compile-time
// constant -- lazily filled on first real use instead, once, unless
// a13_set_params()/a13_reset_params() have already run (same pattern as
// A9's hbt2ply_ensure_defaults(), ai_strat_hbt2ply.c).
static void a13_ensure_defaults(void)
{ if(g_params_initialized) return;

  A13Params defaults = a13_get_default_params();
  g_params[PLAYER_A] = defaults;
  g_params[PLAYER_B] = defaults;
  g_params_initialized = true;
} // a13_ensure_defaults

void a13_set_params(PlayerID player, const A13Params* params)
{ g_params_initialized = true; // caller supplied real values -- never let a
  // later lazy-init call clobber them
  g_params[player] = *params;
} // a13_set_params

void a13_reset_params(void)
{ A13Params defaults = a13_get_default_params();
  g_params[PLAYER_A] = defaults;
  g_params[PLAYER_B] = defaults;
  g_params_initialized = true;
} // a13_reset_params

// True if any dial that consumes the belief is non-neutral -- short-
// circuits the entire a13_build_belief() call (and its cost) otherwise,
// which is what makes the A7-recovery guarantee EXACT rather than merely
// "close to zero" (ai_strat_a13.h's "The superset guarantee").
// belief_reshuffle_trust needs no check of its own: it only affects
// belief.draw_value, which only ever matters when belief_draw_weight != 0.
static bool a13_belief_needed(const A13Params* params)
{ if(params->belief_draw_weight != 0.0f) return true;
  if(params->belief_opp_block_trust != 0.0f) return true;
  if(params->hplus_trust != 0.0f) return true;
  if(params->race_scale > 0.0f && params->race_use_belief_opp) return true;
  return false;
} // a13_belief_needed

static A13Belief a13_belief_for(const struct gamestate* gstate, PlayerID player,
                                const A13Params* params)
{ if(!a13_belief_needed(params))
  { A13Belief empty = {0};
    return empty;
  }
  return a13_build_belief(gstate, player, params->belief_reshuffle_trust,
                          params->hplus_block_combo);
} // a13_belief_for

void a13_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ a13_ensure_defaults();
  PlayerID player = gstate->current_player;
  const A13Params* params = &g_params[player];

  A13Belief belief = a13_belief_for(gstate, player, params);
  A13State state = a13_evaluate_state(gstate, player, params, &belief);
  HBTBestMove move = a13_best_attack_move(gstate, player, params, &state, &belief);

  switch(move.type)
  { case HBT_MOVE_CHAMPIONS:
      for(uint8_t i = 0; i < move.count; i++)
        play_champion(gstate, player, move.cards[i], ctx);
      return;
    case HBT_MOVE_DRAW:
      play_draw_card(gstate, player, move.cards[0], ctx);
      return;
    case HBT_MOVE_CASH:
      play_cash_card_ai(gstate, player, move.cards[0], ctx);
      return;
    case HBT_MOVE_PASS:
    default:
      return;
  }
} // a13_attack_strategy

void a13_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ a13_ensure_defaults();
  PlayerID defender = 1 - gstate->current_player;
  const A13Params* params = &g_params[defender];

  A13Belief belief = a13_belief_for(gstate, defender, params);
  A13State state = a13_evaluate_state(gstate, defender, params, &belief);
  HBTBestMove move = a13_best_defense_move(gstate, defender, params, &state);
  if(move.type != HBT_MOVE_CHAMPIONS) return; // decline

  for(uint8_t i = 0; i < move.count; i++)
    play_champion(gstate, defender, move.cards[i], ctx);
} // a13_defense_strategy

// StrategySet overrides -- thin wrappers over A7's own hbt_discard_to_7_
// with()/hbt_mulligan_with() (ai_strat_hbt.h), called with this agent's own
// .base, per ai_strat_a13.h's "Mulligan / discard-to-7" section. No new
// mulligan/discard behaviour: none of this agent's four new layers have
// anything to say about which cards to keep.
void a13_discard_to_7(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ a13_ensure_defaults();
  hbt_discard_to_7_with(gstate, player, ctx, &g_params[player].base);
} // a13_discard_to_7

void a13_mulligan(struct gamestate* gstate, PlayerID player, GameContext* ctx)
{ a13_ensure_defaults();
  hbt_mulligan_with(gstate, player, ctx, &g_params[player].base);
} // a13_mulligan
