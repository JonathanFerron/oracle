// move_apply.h
// Applies a GameMove (game_move.h) to a game state -- the counterpart to
// move_gen.h's enumeration. A single dispatch point over MoveType,
// delegating to the existing card_actions.c primitives (play_champion(),
// play_draw_card(), play_cash_card_interactive(), and the new
// play_recall_card()) rather than re-deriving any of them.

#ifndef MOVE_APPLY_H
#define MOVE_APPLY_H

#include "../core/game_types.h"
#include "../core/game_context.h"
#include "game_move.h"

// Applies `move` (as returned by get_available_moves()) to gstate on behalf
// of `player`. MOVE_PASS is a no-op. Every other type must have come from
// get_available_moves() for the same gstate/player -- this performs no
// legality re-check (affordability, hand membership, etc.), matching how
// play_champion()/play_draw_card()/play_recall_card() already trust their
// callers.
void apply_move(struct gamestate* gstate, PlayerID player, const GameMove* move,
                GameContext* ctx);

#endif // MOVE_APPLY_H
