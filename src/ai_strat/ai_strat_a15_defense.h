// ai_strat_a15_defense.h
// A15 Risk Threshold's R7 defense-subset selection. R4 (whether to defend
// at all) is decided by the caller (ai_strat_a15.c) via
// a15_p_death_undefended() directly -- this module only answers "given that
// I must defend, which champions do I commit?" See ai_strat_a15.h for R4/R7's
// full statements.

#ifndef AI_STRAT_A15_DEFENSE_H
#define AI_STRAT_A15_DEFENSE_H

#include "../core/game_types.h"

// Picks the defense subset per R7's priority order: among affordable
// subsets that push P(death) below `threshold`, prefer (in order) not
// using a card from the best combo currently held in hand, fewest cards,
// most attack_base==0 cards, highest combo bonus, lowest attack_base sum.
// If no affordable subset clears the threshold, falls back to whichever
// minimizes P(death) outright. Writes 0-3 fullDeck[] indices to `out`
// (already sized for CombatZone's 3-card bound) and returns the count.
uint8_t a15_choose_defense_subset(const struct gamestate* gstate, PlayerID defender,
                                  float threshold, uint8_t* out);

#endif // AI_STRAT_A15_DEFENSE_H
