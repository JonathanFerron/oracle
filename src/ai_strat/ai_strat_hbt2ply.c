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
// the actual opponent it's measured against. Believed at the time that
// fixing A7's (and A5's) defense formula was a genuine prerequisite for a
// future re-attempt at this agent's design target, not optional cleanup.
//
// RE-ATTEMPTED 2026-08-28, after A7's defense formula was fixed
// (2026-08-27) AND its HBTParams recalibrated against the live fix
// (2026-08-28, 58->65 -- see ai_strat_hbt.c's HBT_DEFAULTS comment): the
// "genuine prerequisite" belief above did not hold. Re-ran optimize()
// vs the new `hbt` -- converged to reply_trust=0.013, validated 47.40%
// [46.91%, 47.89%] (40,000 games), statistically indistinguishable from
// the CURRENT shipped defaults' own 47.11% [46.34%, 47.89%] against the
// same new `hbt`, i.e. the search essentially rediscovered "turn the ply
// off." check_personality_flags() correctly flagged this candidate ("the
// second ply has been calibrated into irrelevance") -- NOT shipped, since
// a candidate that is both no-better and identity-destroying fails on both
// counts. A follow-up univariate sweep of reply_trust alone against the
// new `hbt` made the mechanism explicit and confirms this isn't a search
// artifact: win rate declines MONOTONICALLY as reply_trust rises (47.64%
// at 0.0 -> 43.40% at 0.25 -> 39.35% at 0.50 -> 37.29% at 0.75 -> 31.20%
// at 1.0, all 16,000 games/point) -- trusting the ply's simulated reply
// more doesn't help against a genuinely strong, well-calibrated A7, it
// actively hurts, and reply_trust=0 (which is proven to recover A7's own
// decision bit-for-bit, see test_hbt2ply_reply_trust_zero_matches_a7) is
// the actual optimum on the whole curve. Conclusion: A7's old broken
// defense was never the reason this agent fell short of its design
// target -- the two-ply mechanism itself doesn't have room to improve on
// A7 against an opponent this well-calibrated, at least via the two dials
// this driver searches. HBT2PLY_DEFAULTS below are UNCHANGED as a result.
// This agent's OWN Borealis-relative rating still moved, though, since it
// inherits A7's absolute strength gain automatically (hbt_get_default_
// params() below is a live call, not a frozen snapshot): re-measured
// 61.62% [60.87%, 62.38%] vs `borealis` (32,000 games) -- rating 59 -> 62,
// shipped in player_config.c despite no parameter change here. See
// doc/changelog.md's 2026-08-28 entry for the full record.
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
