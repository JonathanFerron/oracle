// ai_strat_common.h
// Turn-strategy helpers shared between AI agents (distinct from strat_lib,
// which is scoped to non-turn heuristics like mulligan/discard-to-7/cash
// exchange -- see ideas/G1 AI agent general info/strat_lib_refactor_handout.md).
//
// Written for A1 Value Based; intended to be reused as-is by later agents
// (A2 Combo Threshold, A3 Borealis) rather than re-derived per agent, so
// draw-card behaviour in particular stays comparable across head-to-head runs.

#ifndef AI_STRAT_COMMON_H
#define AI_STRAT_COMMON_H

#include "../core/game_types.h"
#include "../core/game_context.h"

// Filters gstate->hand[player] down to CHAMPION_CARD entries affordable
// within `budget` lunas, writing their fullDeck[] indices to `out` (caller
// must size it at gstate->hand[player].size -- the hand is capped at 12).
// Returns the number of affordable champions written.
uint8_t build_affordable_champions(const struct gamestate* gstate, PlayerID player,
                                   uint16_t budget, uint8_t* out);

// Expected attack `defender` is currently facing: the sum of
// fullDeck[].expected_attack over the *attacker's* combat zone (an attacker
// commits their champions before the defender acts, so this is public
// information -- reading it is not a combo-blindness violation; that
// constraint is about an agent's own card selection, not about what it may
// observe), plus the combo bonus that zone would score. Matches combat.c's
// unconditional DECK_RANDOM argument to calculate_combo_bonus() so this
// stays consistent with how combat is actually resolved. Call this during
// the defense phase, when gstate->combat_zone[1 - defender] holds the
// attacker's committed champions.
float expected_incoming_attack(const struct gamestate* gstate, PlayerID defender);

// Combo bonus a selection of up to 3 fullDeck[] indices would score if
// played together, via calculate_combo_bonus() with the same DECK_RANDOM
// argument combat.c always passes -- so this matches how combat actually
// resolves. n must be 0-3; returns 0 outside 2-3 (calculate_combo_bonus()'s
// own range check), same as calling it directly.
int combo_bonus_for_selection(const uint8_t* card_indices, uint8_t n);

// Borealis's placeholder draw heuristic (greedy_power_borealis_handout.md
// Sec.9): if hand size < min_hand_size and opponent energy > opp_energy_floor
// and an affordable DRAW_CARD is held, play the cheapest one and return true.
// Otherwise returns false without touching game state.
bool try_play_draw_card(struct gamestate* gstate, PlayerID player,
                        uint8_t min_hand_size, uint8_t opp_energy_floor,
                        GameContext* ctx);

#endif // AI_STRAT_COMMON_H
