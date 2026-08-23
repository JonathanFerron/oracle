// ai_strat_lib_heuristics.h
// Default power-based mulligan / discard-to-7 heuristics, shared by every AI
// agent that doesn't register its own override in ai_strategy.c's
// STRATEGY_REGISTRY[]. Extracted from card_actions.c/stda_auto.c so an agent
// that needs to protect specific cards (e.g. A3 Borealis's held combo
// pieces, see ideas/A3 ai agent greedy power (borealis)/
// greedy_power_borealis_handout.md Sec.7's 2026-08-22 note) can override
// just this decision without duplicating discard_to_7_cards()/
// apply_mulligan()'s surrounding plumbing. Matches the
// MulliganStrategyFunc/DiscardStrategyFunc signatures declared in
// ai_strategy.h.

#ifndef AI_STRAT_LIB_HEURISTICS_H
#define AI_STRAT_LIB_HEURISTICS_H

#include "../core/game_types.h"
#include "../core/game_context.h"

// Repeatedly discards player's lowest-`power` champion until their hand
// holds <= 7 cards. Caller (card_actions.c's discard_to_7_cards()) already
// guards the >7 check, so this assumes it needs to discard at least one.
void strat_lib_discard_to_7(struct gamestate* gstate, PlayerID player, GameContext* ctx);

// Mulligans up to 2 of player's below-AVERAGE_POWER_FOR_MULLIGAN cards,
// discarding lowest-power first and drawing replacements. Which player
// mulligans is the caller's decision (today always PLAYER_B, see
// stda_auto.c's apply_mulligan()); this function just acts on whichever
// player it's given.
void strat_lib_mulligan(struct gamestate* gstate, PlayerID player, GameContext* ctx);

#endif // AI_STRAT_LIB_HEURISTICS_H
