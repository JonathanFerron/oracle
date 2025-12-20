// game_state.h
// Game state initialization and management

#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "game_types.h"
#include "strategy.h"

// Game initialization and management
void setup_game(uint16_t initial_cash, struct gamestate* gstate, GameContext* ctx);
void collect_1_luna(struct gamestate* gstate);
void change_current_player(struct gamestate* gstate);

#endif // GAME_STATE_H
