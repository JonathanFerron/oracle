// ai_strat_hbt2ply.h
// A9 HBT 2-Ply strategy ("The Grandmaster II") -- see
// doc/ai_agents.md's A9 section and A7 Hybrid HBT
// (ai_strat_hbt.h), the agent this one extends by exactly one opponent-
// response ply. Per doc/ai_agents.md's A9 section, this agent adds no new evaluation mechanism:
// "the only new thing this agent adds over A7 is the second ply."
//
// ==== The gap this closes ====
// A7's attack scoring (predicted_damage(), ai_strat_hbt_enum.c) models no
// opponent block at all -- its own header comment says so verbatim. So A7
// systematically overvalues an attack the defender will simply block, and
// has no way to prefer an attack that survives the reply over one that
// doesn't. This agent's one addition: for every candidate champion subset,
// clone the position, commit the subset, simulate the opponent's best
// reply, and score the resulting NET position instead of the undefended one.
//
// ==== Deviation from the original design: the reply oracle is NOT A7's
// hbt_best_defense_move() verbatim ====
// The plan was to call A7's own hbt_best_defense_move() (ai_strat_hbt_
// enum.h) from the opponent's side, unmodified -- "the ranking layer A7
// already has, applied from the other seat." Building this agent's tests
// surfaced a problem: that function's PASS/decline baseline scores the
// undamaged own_energy, never charging the incoming attack against
// declining. Worked through hbt_advantage()'s algebra, this makes PASS
// mathematically dominate every blocking option under the shipped
// HBTParams -- a hand card's positive weight always outweighs the energy
// saved, even for a perfect block -- confirmed empirically too (HBT-vs-HBT
// and Heuristic-vs-Heuristic both average 6 turns to burn 99 energy, vs
// Random-vs-Random's 26; A5's identical best_defense_move() has the same
// property, since A7's shape was copied from it). Reusing that function
// verbatim would make this agent's entire second ply a no-op: the reply
// oracle would always say "decline," collapsing two_ply to one_ply in
// virtually every position regardless of reply_trust.
//
// A7's own hbt_best_defense_move() is deliberately left untouched --
// fixing its (and A5's) shipped, measured formula is real scope with real
// stakes (both agents' ratings would need re-measuring) and is deferred to
// its own task, not folded into A9. Instead, this agent's reply oracle is
// hbt2ply_reply_defense_move() (ai_strat_hbt2ply_reply.c): identical to
// A7's function in every other respect -- same enumeration, same
// variance_aware_incoming(), same evaluate_defense_subset(), both reused
// verbatim -- except the PASS baseline correctly charges
// own_energy - incoming. See ai_strat_hbt_enum.h's own comment on
// hbt_best_defense_move() for the mirror of this note.
//
// ==== Attack only ====
// The ply applies to A9's own attack decisions only. On defense, the
// attacker's committed combat_zone is already public -- A7's
// variance-aware incoming-attack estimate already sees the real threat, so
// there is nothing hidden left for a second ply to reveal. hbt2ply_defense_
// strategy() is therefore A7's defense logic, unchanged.
//
// ==== The opponent-reply model: a deterministic surrogate hand ====
// A7's constraint is inherited verbatim: this agent must never read which
// cards are in gstate->hand[opponent]. The reply is instead estimated
// against a surrogate hand built from public information only
// (ai_strat_hbt2ply_reply.h/.c) -- no sampling, no RNG, so this agent stays
// fully deterministic like A7 (unlike A8/A12's determinize-and-rollout
// approach, which this agent deliberately does not take a dependency on:
// doc/ai_agents.md's A9 section rules out sampling/rollouts as out of scope, and the closed-form
// surrogate keeps this agent's cost close to A7's rather than A8's ~100x).
//
// ==== Scoring: a blend, not a replacement ====
// score = (1 - reply_trust) * one_ply + reply_trust * two_ply
// where one_ply is A7's own hbt_advantage() for the undefended post-move
// position (identical to what A7 itself would compute) and two_ply is the
// same advantage function after subtracting the surrogate defender's
// predicted block from the damage term ONLY -- own_hand/opp_hand/
// own_cash/opp_cash are identical between one_ply and two_ply. An earlier
// version also credited the reply's own hand/cash cost (opp_hand -
// reply.count, opp_cash - reply_int_cost), which a controlled test showed
// made the ply actively harmful: that credit is weighted by gamma_eff/
// delta_eff (~1.8-2.1) versus the damage term's eps_eff (~0.35), so
// "forced them to spend a resource" was overweighted roughly 5x relative
// to "actually reduced their energy," biasing the ranking toward
// resource-trading attacks that lose a straightforward damage race even
// when the block prediction was accurate. Damage-only credit makes
// two_ply <= one_ply a mathematical guarantee (block only ever reduces
// net damage, never turns negative), not just a design intention: the ply
// can only discourage committing champions relative to pass/draw/cash,
// never encourage them. reply_trust = 0 recovers A7's decision EXACTLY --
// same move choice, same tie-breaks -- which is both a safety valve
// against an overconfident opponent model and this agent's central
// regression test (see testsrc/test_moves.c).
//
// ==== The two gate/beam dials are pure compute-budget knobs ====
// ply_energy_ceiling gates the ply to positions where the opponent's energy
// is at or below the ceiling (99 = always, resolving the doc/ai_agents.md's A9 section's
// doc/ai_agents.md's A7 section conflict on gating as a calibratable default rather
// than an a-priori choice -- see doc/changelog.md for which value shipped).
// ply_beam_width, if nonzero, restricts the ply to the top-K candidates by
// one_ply score, skipping it for the rest (which then score as
// one_ply == two_ply, i.e. fall back to A7's own ranking). Both are
// analogous to A8's compute-budget dials: swept, not optimized, since more
// ply coverage is basically always at least as strong, just slower.
//
// Information hiding: same constraint as A7 -- may read
// gstate->hand[opponent].size, current_cash_balance[opponent],
// current_energy[opponent], and combat_zone[opponent] (all public), and
// must never read which cards are in gstate->hand[opponent].

#ifndef AI_STRAT_HBT2PLY_H
#define AI_STRAT_HBT2PLY_H

#include "../core/game_types.h"
#include "../core/game_context.h"
#include "ai_strat_hbt.h"

typedef struct
{ HBTParams base; // A7's 34 fields, frozen at hbt_get_default_params() --
  // this agent re-derives none of them, see doc/ai_agents.md's A9 section.

  // -- New to this agent (4 fields) --
  float   reply_trust;         // [0,1]; 0 recovers A7 exactly
  float   surrogate_pessimism; // [0,1]; how strong a blocking hand to assume
  uint8_t ply_energy_ceiling;  // run the ply only if opp_energy <= this; 99 = always
  uint8_t ply_beam_width;      // apply the ply to only the top-K one_ply candidates; 0 = all
} HBT2PlyParams;

void hbt2ply_attack_strategy(struct gamestate* gstate, GameContext* ctx);
void hbt2ply_defense_strategy(struct gamestate* gstate, GameContext* ctx);

HBT2PlyParams hbt2ply_get_default_params(void);

// Calibration-only override hook (see aicalibsrc/hbt2ply/), settable per
// player -- same pattern as hbt_set_params() and every other calibrated
// agent. Not part of the general strategy framework: normal play always
// uses the compiled defaults, since nothing else calls these.
void hbt2ply_set_params(PlayerID player, const HBT2PlyParams* params);
void hbt2ply_reset_params(void);

#endif // AI_STRAT_HBT2PLY_H
