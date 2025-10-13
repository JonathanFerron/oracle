// card_actions.c
// Implementation of card playing and game actions

#include "card_actions.h"
#include "game_constants.h"
#include "rnd.h"
#include <stdio.h>
#include <stdlib.h>

extern bool debug_enabled;

int has_champion_in_hand(struct HDCLList* hand) {
    struct LLNode* current = hand->head;
    for (uint8_t i = 0; i < hand->size; i++) {
        if (fullDeck[current->data].card_type == CHAMPION_CARD) {
            return 1;
        }
        current = current->next;
    }
    return 0;
}

uint8_t select_champion_for_cash_exchange(struct HDCLList* hand) {
    struct LLNode* current = hand->head;
    float min_power = 100.0;
    uint8_t champion_to_exchange = 0;
    
    for (uint8_t i = 0; i < hand->size; i++) {
        if (fullDeck[current->data].card_type == CHAMPION_CARD) {
            if (fullDeck[current->data].power < min_power) {
                min_power = fullDeck[current->data].power;
                champion_to_exchange = current->data;
            }
        }
        current = current->next;
    }
    
    return champion_to_exchange;
}

void play_card(struct gamestate* gstate, PlayerID player, uint8_t card_idx) {
    CardType type = fullDeck[card_idx].card_type;
    
    if (type == CHAMPION_CARD) {
        play_champion(gstate, player, card_idx);
    } else if (type == DRAW_CARD) {
        play_draw_card(gstate, player, card_idx);
    } else if (type == CASH_CARD) {
        play_cash_card(gstate, player, card_idx);
    }
}

void play_champion(struct gamestate* gstate, PlayerID player, uint8_t card_idx) {
    // Add to combat zone
    HDCLL_insertNodeAtBeginning(&gstate->combat_zone[player], card_idx);
    
    // Remove from hand
    HDCLL_removeNodeByValue(&gstate->hand[player], card_idx);
    
    // Pay cost
    gstate->current_cash_balance[player] -= fullDeck[card_idx].cost;
    
    if (debug_enabled) {
        printf(" Played champion card index %u\n", card_idx);
    }
}

void play_draw_card(struct gamestate* gstate, PlayerID player, uint8_t card_idx) {
    // Remove from hand
    HDCLL_removeNodeByValue(&gstate->hand[player], card_idx);
    
    // Pay cost
    gstate->current_cash_balance[player] -= fullDeck[card_idx].cost;
    
    // Draw cards
    uint8_t n = fullDeck[card_idx].draw_num;
    if (debug_enabled) {
        printf(" Playing draw card %u, drawing %u cards\n", card_idx, n);
    }
    
    for (uint8_t i = 0; i < n; i++) {
        draw_1_card(gstate, player);
    }
    
    // Move to discard
    HDCLL_insertNodeAtBeginning(&gstate->discard[player], card_idx);
}

  
void play_cash_card(struct gamestate* gstate, PlayerID player, uint8_t card_idx) {
    // Remove cash card from hand
    HDCLL_removeNodeByValue(&gstate->hand[player], card_idx);
    
    // Pay cost (0 for cash cards)
    gstate->current_cash_balance[player] -= fullDeck[card_idx].cost;
    
    // Select champion to exchange
    uint8_t champion_to_exchange = select_champion_for_cash_exchange(&gstate->hand[player]);
    
    if (champion_to_exchange != 0) {
        // Remove champion from hand and place in discard
        HDCLL_removeNodeByValue(&gstate->hand[player], champion_to_exchange);
        HDCLL_insertNodeAtBeginning(&gstate->discard[player], champion_to_exchange);
        
        // Collect cash
        uint8_t cash_received = fullDeck[card_idx].exchange_cash;
        gstate->current_cash_balance[player] += cash_received;
        
        if (debug_enabled) {
            printf(" Exchanged champion card %u for %u lunas\n", 
                   champion_to_exchange, cash_received);
        }
    }
    
    // Move cash card to discard
    HDCLL_insertNodeAtBeginning(&gstate->discard[player], card_idx);
}

void draw_1_card(struct gamestate* gstate, PlayerID player) {
    if (DeckStk_isEmpty(&gstate->deck[player])) {
        shuffle_discard_and_form_deck(&gstate->discard[player], &gstate->deck[player]);
        if (debug_enabled) {
            printf(" Reshuffled deck for player %u\n", player);
        }
    }
    
    uint8_t cardindex = DeckStk_pop(&gstate->deck[player]);
    HDCLL_insertNodeAtBeginning(&gstate->hand[player], cardindex);
    
    if (debug_enabled) {
        printf(" Drew card index %u from player %u deck\n", cardindex, player);
    }
}

void shuffle_discard_and_form_deck(struct HDCLList* discard, struct deck_stack* deck) {
    uint8_t* A = HDCLL_toArray(discard);
    uint8_t n = discard->size;
    
    if (debug_enabled) {
        printf(" Discard size: %u\n", n);
    }
    
    // Shuffle the card indices
    RND_partial_shuffle(A, n, n);
    
    // Push to deck
    for (uint8_t i = 0; i < n; i++) {
        DeckStk_push(deck, A[i]);
    }
    
    // Free heap memory
    free(A);
    
    // Empty the discard
    for (uint8_t i = 0; i < n; i++) {
        HDCLL_removeNodeFromBeginning(discard);
    }
}

void collect_1_luna(struct gamestate* gstate) {
    gstate->current_cash_balance[gstate->current_player]++;
}

void discard_to_7_cards(struct gamestate* gstate) {
    if (gstate->hand[gstate->current_player].size <= 7) return;
    
    float minpower;
    uint8_t card_with_lowest_power;
    
    while (gstate->hand[gstate->current_player].size > 7) {
        // Find card with lowest power
        minpower = 100.0;
        card_with_lowest_power = 0;
        struct LLNode* current = gstate->hand[gstate->current_player].head;
        
        for (uint8_t i = 0; i < gstate->hand[gstate->current_player].size; i++) {
            if (fullDeck[current->data].power < minpower) {
                minpower = fullDeck[current->data].power;
                card_with_lowest_power = current->data;
            }
            current = current->next;
        }
        
        // Discard it
        HDCLL_removeNodeByValue(&gstate->hand[gstate->current_player], 
                                card_with_lowest_power);
        HDCLL_insertNodeAtBeginning(&gstate->discard[gstate->current_player], 
                                     card_with_lowest_power);
    }
}

void change_current_player(struct gamestate* gstate) {
    gstate->current_player = 1 - gstate->current_player;
}
