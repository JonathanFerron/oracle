// move_gen.h
// Legal-move enumeration for search-based agents (A8+). Attack phase
// enumerates the full move space (pass, champion subsets, draw, recall,
// cash); defense phase enumerates pass/champion-subsets only, matching how
// defense_phase()/a defense strategy actually decide (0-3 champions or
// decline -- see turn_logic.c). Reuses build_affordable_champions()
// (ai_strat_common.h) and collect_champions() (card_actions.h) rather than
// re-deriving hand filtering.

#ifndef MOVE_GEN_H
#define MOVE_GEN_H

#include "../core/game_types.h"
#include "game_move.h"

// Caps on the two branching-factor culprits flagged in
// doc/ai_agents.md's A10 section --
// recall's C(discard, choose_num) and cash's choice of exchange target.
// max_recall_variants == 0 disables recall entirely (Draw-N is always the
// alternative for the same card, so this is a safe degenerate case).
// max_cash_variants == 0 still emits exactly one variant per affordable
// cash card (the lowest-power exchange target, matching
// select_champion_for_cash_exchange()'s default) -- a cash card is always a
// legal move when the hand holds a champion, so 0 here means "don't search
// the exchange target," not "disable cash."
typedef struct
{ uint8_t max_recall_variants;
  uint8_t max_cash_variants;
} MoveGenLimits;

// Upper bound a caller can use to size `out`. Not a tight bound -- see
// move_gen.c's header comment for the derivation (doc/ai_agents.md's A8 section
// monte carlo (the soothsayer)/about.md's "max ~93 moves" is typical, not
// worst-case).
#define MOVE_GEN_MAX_MOVES 128

// Enumerates every legal move for `player` in gstate's *current* turn_phase,
// writing up to max_out entries to `out`. Returns the number written, which
// is never 0 -- MOVE_PASS is always legal in both phases.
uint8_t get_available_moves(const struct gamestate* gstate, PlayerID player,
                            const MoveGenLimits* limits,
                            GameMove* out, uint8_t max_out);

#endif // MOVE_GEN_H
