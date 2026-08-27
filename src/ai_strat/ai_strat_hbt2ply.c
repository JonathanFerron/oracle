// ai_strat_hbt2ply.c
// A9 HBT 2-Ply strategy ("The Grandmaster II") -- see ai_strat_hbt2ply.h for
// the full spec (the gap this closes, why the reply oracle is a local
// correction rather than A7's own hbt_best_defense_move(), the attack-only
// scope, the reply_trust blend). Attack/defense orchestration and parameter
// management; move enumeration/scoring lives in ai_strat_hbt2ply_reply.c
// (same file-length split as A7 Hybrid HBT's own ai_strat_hbt/
// ai_strat_hbt_enum split).

#include "ai_strat_hbt2ply.h"
#include "ai_strat_hbt2ply_reply.h"
#include "../core/card_actions.h"

static HBT2PlyParams g_params[2];
static bool g_params_initialized = false;

// Calibrated 2026-08-26 via aicalibsrc/hbt2ply/calibrate_hbt2ply.py's
// `optimize`, staged per doc/changelog.md: the 34 base HBTParams fields are
// hard-pinned at A7's own shipped values (this agent re-derives none of
// them, see about.md), and only reply_trust/surrogate_pessimism are
// searched -- ply_energy_ceiling/ply_beam_width are compute-budget dials,
// left at their "no restriction" defaults (99/0) since this agent's cost is
// close to A7's own closed-form cost (~3200 games/sec measured via the
// calibration harness), nowhere near A8's ~100x rollout overhead, so there
// was no budget pressure to restrict them.
//
// The search was run TWICE, against two different opponents, because the
// first (vs `borealis`, matching every other agent's calibration
// convention) produced a candidate that still lost to `hbt` (A7) head to
// head -- see doc/changelog.md's full diagnosis. Optimizing directly
// against `hbt` -- this agent's actual success criterion -- is what
// produced the values below. Validated: 47.19% [46.70%, 47.68%] vs `hbt`
// (40,000 games) -- BELOW the >55% design target. Root cause isolated via
// a controlled test (both sides given a corrected, actually-blocking
// defense, only the attack-side logic varied): the two-ply mechanism
// reaches near-parity with A7 there (49.6% vs 50.5%), so the model is sound
// in principle, but A7's own real hbt_best_defense_move() never blocks (a
// pre-existing PASS-dominance property of A7's own shipped formula, found
// while building this agent -- see ai_strat_hbt_enum.h's comment on that
// function), leaving this agent's ply nothing real to correct for against
// the actual opponent it's measured against. Shipped as a playable roster
// member regardless (matching the A4/A8/A12 precedent of reporting a
// below-target result honestly); fixing A7's (and A5's) defense formula is
// believed a genuine prerequisite for a future re-attempt at this agent's
// design target, not optional cleanup -- see doc/changelog.md.
HBT2PlyParams hbt2ply_get_default_params(void)
{ HBT2PlyParams defaults;
  defaults.base = hbt_get_default_params(); // A7's 34 fields, frozen -- see
  // ai_strat_hbt2ply.h's header comment on why this agent re-derives none
  // of them (a function call, not a compile-time constant, is why this
  // can't be a designated-initializer macro the way HBT_DEFAULTS is)
  defaults.reply_trust = 0.10358593f;
  defaults.surrogate_pessimism = 0.32276109f;
  defaults.ply_energy_ceiling = 99; // no restriction -- see calibration comment above
  defaults.ply_beam_width = 0;      // no restriction -- see calibration comment above
  return defaults;
} // hbt2ply_get_default_params

// g_params can't use a static designated initializer the way A7's
// g_params[2] = { HBT_DEFAULTS, HBT_DEFAULTS } does, since .base comes from
// a function call (hbt_get_default_params()) rather than a compile-time
// constant -- lazily filled on first real use instead, once, unless
// hbt2ply_set_params()/hbt2ply_reset_params() have already run.
static void hbt2ply_ensure_defaults(void)
{ if(g_params_initialized) return;

  HBT2PlyParams defaults = hbt2ply_get_default_params();
  g_params[PLAYER_A] = defaults;
  g_params[PLAYER_B] = defaults;
  g_params_initialized = true;
} // hbt2ply_ensure_defaults

void hbt2ply_set_params(PlayerID player, const HBT2PlyParams* params)
{ g_params_initialized = true; // caller supplied real values -- never let a
  // later lazy-init call clobber them
  g_params[player] = *params;
} // hbt2ply_set_params

void hbt2ply_reset_params(void)
{ HBT2PlyParams defaults = hbt2ply_get_default_params();
  g_params[PLAYER_A] = defaults;
  g_params[PLAYER_B] = defaults;
  g_params_initialized = true;
} // hbt2ply_reset_params

void hbt2ply_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ hbt2ply_ensure_defaults();
  PlayerID player = gstate->current_player;
  const HBT2PlyParams* params = &g_params[player];

  HBTState state = hbt_evaluate_state(gstate, player, &params->base);
  HBTBestMove move = hbt2ply_best_attack_move(gstate, player, params, &state);

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
} // hbt2ply_attack_strategy

void hbt2ply_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ hbt2ply_ensure_defaults();
  PlayerID defender = 1 - gstate->current_player;
  const HBT2PlyParams* params = &g_params[defender];

  // Attack only (ai_strat_hbt2ply.h's "Attack only" section): A9's own
  // defense is A7's exact formula, unchanged -- deliberately A7's REAL
  // hbt_best_defense_move(), not the corrected hbt2ply_reply_defense_move(),
  // which exists only to model what the OPPONENT might do during A9's own
  // attack-ply simulation, not how A9 itself plays defense.
  HBTState state = hbt_evaluate_state(gstate, defender, &params->base);
  HBTBestMove move = hbt_best_defense_move(gstate, defender, &params->base, &state);
  if(move.type != HBT_MOVE_CHAMPIONS) return; // decline

  for(uint8_t i = 0; i < move.count; i++)
    play_champion(gstate, defender, move.cards[i], ctx);
} // hbt2ply_defense_strategy
