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

// Hard ceiling on the mulligan card count -- sizes every fixed-size buffer
// that holds mulligan indices (game_commands.c's interactive handler in
// particular). mulligan_get_max_cards() must never exceed this. Not the
// same thing as the shipped default (2, see mulligan_get_max_cards()) --
// this is just the largest value the plumbing can safely carry, generously
// above it so a calibration override has room to explore.
#define MULLIGAN_HARD_CAP 4

// Repeatedly discards player's lowest-`power` champion until their hand
// holds <= 7 cards. Caller (card_actions.c's discard_to_7_cards()) already
// guards the >7 check, so this assumes it needs to discard at least one.
void strat_lib_discard_to_7(struct gamestate* gstate, PlayerID player, GameContext* ctx);

// Mulligans up to mulligan_get_max_cards() of player's
// below-AVERAGE_POWER_FOR_MULLIGAN cards, discarding lowest-power first and
// drawing replacements. Which player mulligans is the caller's decision
// (today always PLAYER_B, see stda_auto.c's apply_mulligan()); this
// function just acts on whichever player it's given.
void strat_lib_mulligan(struct gamestate* gstate, PlayerID player, GameContext* ctx);

// The mulligan card-count cap, shared by every mulligan implementation
// (strat_lib_mulligan() above, A3 Borealis's borealis_mulligan(), A7/A9's
// hbt_mulligan(), and the interactive human path in game_commands.c) --
// consolidated 2026-08-28 (previously a local `= 2` duplicated in each of
// those files) so the seat-advantage investigation
// (aicalibsrc/mulligan/) has one place to override rather than four.
// Shipped default is 2 (doc/game_rules_doc.md's "up to 2 cards"); this is a
// shared game-rule parameter, not a per-agent personality dial, so it's one
// global value rather than per-player. A10 IS-MCTS is the one agent that
// does NOT read this -- its mulligan search
// (ai_strat_ismcts_flat.c's enumerate_mulligan_candidates()) is fixed at
// exactly 2 via explicit enumerate_singles()/enumerate_pairs() calls, not a
// variable bound; extending it needs real search-space work
// (an enumerate_triples() for a cap of 3+), out of scope here.
uint8_t mulligan_get_max_cards(void);

// Calibration-only override (see aicalibsrc/mulligan/) -- value must be
// <= MULLIGAN_HARD_CAP. Normal play never calls these; nothing else does.
void mulligan_set_max_cards(uint8_t max_cards);
void mulligan_reset_max_cards(void);

#endif // AI_STRAT_LIB_HEURISTICS_H
