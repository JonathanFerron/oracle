// ai_strat_hbt2ply_reply.h
// A9 HBT 2-Ply's opponent-reply modelling: the public-information-only
// surrogate hand that stands in for the opponent's real (hidden) hand when
// scoring the second ply -- see ai_strat_hbt2ply.h for the full spec and
// why this exists. Internal to the A9 agent: nothing outside
// ai_strat_hbt2ply.c should include this.

#ifndef AI_STRAT_HBT2PLY_REPLY_H
#define AI_STRAT_HBT2PLY_REPLY_H

#include "../core/game_types.h"
#include "../structures/card_collection.h"
#include "ai_strat_hbt2ply.h"
#include "ai_strat_hbt_enum.h"

// Builds a deterministic stand-in for (1 - pov)'s hidden hand, for use only
// as input to hbt_best_defense_move() when estimating that player's best
// reply. Never reads or reveals which cards the opponent actually holds --
// inherits A7's own constraint verbatim (ai_strat_hbt.h: "must never read
// which cards are in gstate->hand[opponent]"). Composition:
//
//   unseen = fullDeck[FULL_DECK_SIZE] - hand[pov] - both discards
//                                      - both combat zones
//
// (via ai_strat_common.h's strat_common_unseen_pool() -- the same pool
// definition ai_strat_playout.c's mc_determinize() uses, promoted there once
// a third caller, A13, needed it too; see ai_strat_common.h).
// The surrogate is sized to (1 - pov)'s real (public) hand size, and holds
// the same champion/non-champion proportion as the unseen pool. Which
// champions: the unseen champions sorted descending by expected_defense,
// windowed by `surrogate_pessimism` -- 1.0 selects the strongest available
// blockers (window at the top of the sort), 0.0 selects a hand of roughly
// median blocking strength (window centered on the pool), linearly
// interpolated and monotonic between the two. No RNG: same gstate and
// pessimism always produce the same surrogate, keeping A9 fully
// deterministic like A7.
//
// Safe to call with `out == &gstate->hand[1 - pov]` (building the surrogate
// directly into a gamestate clone's own opponent-hand slot, avoiding a
// separate copy) -- the target size is read before `out` is touched, so
// this aliasing cannot corrupt the result.
void build_surrogate_hand(const struct gamestate* gstate, PlayerID pov,
                          float surrogate_pessimism, Hand* out);

// A9's own opponent-reply oracle -- NOT A7's hbt_best_defense_move()
// verbatim, despite the original design intent (see ai_strat_hbt2ply.h's
// header comment for the full story of why). Identical to A7's function in
// every respect -- same enumeration, same variance_aware_incoming(), same
// evaluate_defense_subset() (both reused verbatim from ai_strat_hbt_enum.h)
// -- except the PASS/decline baseline correctly charges the incoming
// attack against own_energy (own_energy - incoming, clamped >= 0) instead
// of scoring PASS at the undamaged own_energy. That one difference matters:
// A7's own baseline makes PASS mathematically dominate every blocking
// option under the shipped HBTParams (see ai_strat_hbt_enum.h's
// hbt_best_defense_move() comment), which would make this agent's entire
// second ply a no-op if it reused that function as its reply model. A7
// itself is deliberately left untouched -- fixing its own shipped, measured
// formula is a separate task, not part of A9.
HBTBestMove hbt2ply_reply_defense_move(const struct gamestate* gstate, PlayerID defender,
                                       const HBTParams* params, const HBTState* state);

// Scores one candidate attack champion-subset move under A9's two-ply
// model. Mirrors A7's own evaluate_attack_subset() gating exactly: returns
// false (the subset should not be considered at all) if it isn't affordable
// or is a held-back combo (A3's is_held_combo() rule, ported via A7) --
// same exclusions A7's own enumeration applies before scoring. On success,
// writes the blended score to *out_score:
//
//   one_ply = A7's own hbt_advantage() for the undefended post-move
//             position -- bit-for-bit what A7 itself would compute for
//             this exact subset.
//   two_ply = the same advantage after cloning the position, committing
//             the subset, replacing the opponent's hand with the public
//             surrogate (build_surrogate_hand(), above), and asking THIS
//             FILE's own hbt2ply_reply_defense_move() (above -- NOT A7's
//             hbt_best_defense_move()) for their best reply from their
//             side -- then scoring the NET DAMAGE of that reply instead of
//             the undefended one. Deliberately NOT crediting the reply's
//             own hand/cash cost (opp_hand/opp_cash stay exactly as in
//             one_ply) -- see this file's .c for why: an earlier version
//             did, and a controlled test showed that over-crediting
//             "forced them to spend a resource" (weighted ~5x higher than
//             the damage term) made the ply actively harmful even when its
//             block prediction was accurate.
//   *out_score = (1 - reply_trust) * one_ply + reply_trust * two_ply
//
// Skips the two_ply simulation entirely (two_ply left equal to one_ply)
// when reply_trust == 0 or the opponent's energy is above
// ply_energy_ceiling -- both a compute-budget saving and the mechanism
// that makes reply_trust == 0 recover A7's decision exactly (see
// ai_strat_hbt2ply.h and testsrc/test_moves.c's regression test).
// `my_state` must be the CALLER's own HBTState (hbt_evaluate_state(gstate,
// player, ...)) -- the opponent's state for scoring their reply is derived
// internally, since hbt_advantage() is not sign-symmetric (ai_strat_hbt_
// enum.h's header comment).
bool hbt2ply_score_attack_subset(const struct gamestate* gstate, PlayerID player,
                                 const uint8_t* cards, uint8_t count,
                                 const HBT2PlyParams* params, const HBTState* my_state,
                                 float* out_score);

// A9's full attack-move enumeration: A7's own shape (pass + every
// affordable 1-3 champion subset + every affordable draw card + every
// affordable cash card) via hbt_advantage()/build_affordable_champions() --
// but every champion subset is scored via hbt2ply_score_attack_subset()
// above instead of the undefended one-ply formula. Draw/cash/pass never get
// a ply (nothing is committed for a defender to reply to), so those three
// always score exactly as A7 would score them.
//
// `ply_beam_width` (0 = no restriction, A9's default) caps how many
// candidates get the (more expensive) full two-ply treatment: when
// nonzero, every subset is first ranked by its cheap one-ply-only score,
// then only the top `ply_beam_width` are re-scored with the real
// `reply_trust`; the rest keep their one-ply score. This is a pure
// compute-budget dial -- it can only ever narrow which candidates get the
// ply, never change what the ply computes for a candidate that gets it.
HBTBestMove hbt2ply_best_attack_move(struct gamestate* gstate, PlayerID player,
                                     const HBT2PlyParams* params, const HBTState* my_state);

#endif // AI_STRAT_HBT2PLY_REPLY_H
