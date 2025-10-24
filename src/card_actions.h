// card_actions.h
// Functions for playing cards and managing game actions

#ifndef CARD_ACTIONS_H
#define CARD_ACTIONS_H

#include "game_types.h"
#include "game_context.h"

// Card playing functions
void play_card(struct gamestate* gstate, PlayerID player, uint8_t card_idx, GameContext* ctx);
void play_champion(struct gamestate* gstate, PlayerID player, uint8_t card_idx, GameContext* ctx);
void play_draw_card(struct gamestate* gstate, PlayerID player, uint8_t card_idx, GameContext* ctx);
void play_cash_card(struct gamestate* gstate, PlayerID player, uint8_t card_idx, GameContext* ctx);

// Helper functions for card management
int has_champion_in_hand(struct HDCLList* hand);
uint8_t select_champion_for_cash_exchange(struct HDCLList* hand);

// Game action functions
void draw_1_card(struct gamestate* gstate, PlayerID player, GameContext* ctx);
void shuffle_discard_and_form_deck(struct HDCLList* discard, struct deck_stack* deck, GameContext* ctx);
void discard_to_7_cards(struct gamestate* gstate, GameContext* ctx);

#endif // CARD_ACTIONS_H
