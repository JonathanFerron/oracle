// ai_strat_a15_cards.h
// A15 Risk Threshold's combo-participation scorer and the victim-selection
// it drives -- shared by R9b (pressure-attack card choice), R1/R5
// (discard-to-7), and R2 (mulligan), replacing the "protect one best combo"
// list A7's ai_strat_hbt_cards.c uses with a continuous per-card score. See
// ai_strat_a15.h's "The shared combo-participation scorer" for the worked
// example this generalises.

#ifndef AI_STRAT_A15_CARDS_H
#define AI_STRAT_A15_CARDS_H

#include "../core/game_types.h"
#include "../core/game_context.h"

// Denominator floor for the attack/luna-cost tiebreak -- same role and
// value as A4/A6/A7's own cost floors (BR_COST_FLOOR/TAC_COST_FLOOR/
// HBT_COST_FLOOR), not itself a calibration dial.
#define A15_COST_FLOOR 1.3f

// Highest combo_bonus_for_selection() any 2- or 3-card subset of `player`'s
// hand champions achieves while including `card` -- 0 if card is not a
// champion currently in that hand, or is a champion with no combo partner
// at all. O(hand_size^2) per call (hand capped at 12); call once per
// decision per card needed, never inside an enumeration loop.
int a15_combo_participation(const struct gamestate* gstate, PlayerID player, uint8_t card);

// R1's tiebreak (expected_attack / (cost + A15_COST_FLOOR)), exposed so
// R9b's pressure-attack card choice (ai_strat_a15_attack.c) can share the
// exact same formula rather than a second copy.
float a15_attack_luna_ratio(uint8_t card_idx);

// The single card in player's hand this agent would give up next --
// lowest discard priority tier first (Cash cards, per R3's "let the Cash
// card leave via discard-to-7 rather than playing it"; then paid
// champions; then zero-cost champions, R1/R2/R5's protection; Draw/Recall
// cards last, R1/R5's "hold for a future opportunity"), then within a tier
// by ascending a15_combo_participation(), then by ascending
// expected_attack/(cost+A15_COST_FLOOR) (R1's stated tiebreak). Call
// repeatedly, removing the returned card from the hand between calls, for
// progressively-precise multi-card selection (a card's own participation
// can change once its combo partner leaves) -- this is what
// a15_discard_to_7()/a15_mulligan() both do. Returns UINT8_MAX if hand is
// empty.
uint8_t a15_pick_victim(const struct gamestate* gstate, PlayerID player);

// StrategySet mulligan_strategy[]/discard_strategy[] overrides -- R2 and
// R1/R3/R5 respectively. See ai_strat_a15.h for the full rule statements.
void a15_mulligan(struct gamestate* gstate, PlayerID player, GameContext* ctx);
void a15_discard_to_7(struct gamestate* gstate, PlayerID player, GameContext* ctx);

#endif // AI_STRAT_A15_CARDS_H
