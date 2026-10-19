// game_state.h
// Game state initialization and management

#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "game_types.h"
#include "../ai_strat/ai_strategy.h"

// Game initialization and management
void setup_game(uint16_t initial_cash, struct gamestate* gstate, GameContext* ctx);
void collect_1_luna(struct gamestate* gstate);
// Pays `player`'s unsettled spend (coin_value(wallet) - current_cash_balance)
// out of their wallet using coin_select_payment(), and returns the overpay
// (> 0 means the bank gave change). No-op returning 0 when nothing is owed.
uint8_t settle_coin_payment(struct gamestate* gstate, PlayerID player);
void change_current_player(struct gamestate* gstate);

#endif // GAME_STATE_H
