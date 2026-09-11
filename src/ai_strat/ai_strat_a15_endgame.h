// ai_strat_a15_endgame.h
// A15 Risk Threshold's R8 "endgame/kill-sequence" mode. See
// ai_strat_a15.h's header comment for R8's place in the overall priority
// chain (checked BEFORE R6, i.e. before anything else).
//
// ==== Stateless reformulation (a deliberate deviation from the design
// doc's own wording, flagged here rather than worked around silently) ====
// The design doc describes R8 as a STATEFUL commitment: trigger on turn i
// against a 4-attack threshold q1, then re-check on turns i+1/i+2/i+3
// against q2/q3/q4 as the window shrinks, aborting if any re-check fails.
// That requires remembering "which turn did I enter on" across calls.
// This agent has no such memory by design (ai_strat_a15.h's cost-
// discipline note; every other rule is a pure function of the CURRENT
// gstate, and per-player g_params[] is calibration config, not per-game
// state -- a real stateful counter would leak between games in a
// --stda.auto/--stda.rating run's shared process unless explicitly reset
// on every new game, and there is no "new game" hook in the strategy
// framework to reset it from).
//
// Every attack turn, P(finish within N) is computed DIRECTLY for all four
// horizons N=1-4 (find_triggering_horizon(), ai_strat_a15_endgame.c) and
// checked against that horizon's own q_N; R8 fires on the smallest N that
// clears. This reproduces every behavioural requirement without any stored
// state: "kicks off" when some q_N clears -- fires; "if it now falls below
// the threshold, abort immediately" -- for free, there is nothing to abort
// FROM, the next turn is just evaluated fresh; "defense stays active,
// unchanged, throughout" -- R8 only ever touches a15_attack_strategy().
//
// ==== Correction 2026-09-10: hand-size was NOT a good horizon proxy ====
// The first implementation derived N from hand size alone
// (ceil(champions-in-hand/3), the doc's own turn arithmetic) rather than
// checking P(finish) at every horizon directly. That measured badly: a
// diagnostic pass logging every P(finish) this function evaluates (200
// games) found over 90% of them below p=0.2 regardless of horizon --
// "my hand has thinned to roughly N attacks' worth" turns out to be almost
// completely decoupled from "the opponent is actually close to dying." A
// genuinely selective q (e.g. 0.5, "wait for a well-justified chance") on
// that hand-size-derived horizon fired essentially never (~0.5 times/game
// measured) because the pre-filter had already discarded nearly every real
// opportunity along with the bad ones -- not because real opportunities
// don't exist (p does reach 1.0 in the tail), just because hand size can't
// find them. Checking all four horizons directly, with no hand-size
// pre-filter at all, lets q1-q4 mean what they're supposed to mean: a
// genuine, calculated confidence bar on the actual computed probability,
// not a volume knob wearing a probability's clothing. See
// aicalibsrc/daredevil/README.md for the recalibration this required.

#ifndef AI_STRAT_A15_ENDGAME_H
#define AI_STRAT_A15_ENDGAME_H

#include "../core/game_types.h"
#include "../core/game_context.h"
#include "ai_strat_a15.h"

// If the horizon-appropriate threshold clears, plays the best available
// combo subset (any tier -- species/order/color, whichever scores highest;
// falls back to the highest-expected_attack affordable champions if no
// combo exists at all) and returns true. Returns false, touching nothing,
// otherwise -- the caller falls through to the normal R1-R9 chain.
bool a15_try_endgame_attack(struct gamestate* gstate, PlayerID player,
                            const A15Params* params, GameContext* ctx);

#endif // AI_STRAT_A15_ENDGAME_H
