// game_state.c
// Game state initialization and management implementation

#include "game_state.h"
#include "turn_logic.h"
#include "card_actions.h"
#include "game_constants.h"
#include "rnd.h"
#include "deckstack.h"
#include <stdio.h>
#include <string.h>

extern bool debug_enabled;

#define min(a,b) ({ typeof (a) _a = (a); typeof (b) _b = (b); _a < _b ? _a : _b; })
#define max(a,b) ({ typeof (a) _a = (a); typeof (b) _b = (b); _a > _b ? _a : _b; })

void setup_game(uint16_t initial_cash, struct gamestate* gstate) {
    // Initialize game state
    gstate->current_player = PLAYER_A;
    gstate->current_cash_balance[PLAYER_A] = initial_cash;
    gstate->current_cash_balance[PLAYER_B] = initial_cash;
    gstate->current_energy[PLAYER_A] = 99;
    gstate->current_energy[PLAYER_B] = 99;
    gstate->someone_has_zero_energy = false;
    gstate->game_state = ACTIVE;
    
    // Initialize decks
    gstate->deck[PLAYER_A].top = -1;
    gstate->deck[PLAYER_B].top = -1;
    
    // Randomly distribute cards
    uint8_t rndCardIndex[FULL_DECK_SIZE];
    for (uint8_t i = 0; i < FULL_DECK_SIZE; i++) {
        rndCardIndex[i] = i;
    }
    RND_partial_shuffle(rndCardIndex, FULL_DECK_SIZE, 2*MAX_DECK_STACK_SIZE);
    
    // Push cards to decks alternately
    uint8_t i = 0;
    while (i < 2*MAX_DECK_STACK_SIZE) {
        DeckStk_push(&gstate->deck[PLAYER_A], rndCardIndex[i++]);
        DeckStk_push(&gstate->deck[PLAYER_B], rndCardIndex[i++]);
    }
    
    // Initialize hands, combat zones, and discards
    HDCLL_initialize(&gstate->hand[PLAYER_A]);
    HDCLL_initialize(&gstate->hand[PLAYER_B]);
    HDCLL_initialize(&gstate->discard[PLAYER_A]);
    HDCLL_initialize(&gstate->discard[PLAYER_B]);
    HDCLL_initialize(&gstate->combat_zone[PLAYER_A]);
    HDCLL_initialize(&gstate->combat_zone[PLAYER_B]);
    
    // Draw initial hands (6 cards each)
    for (i = 0; i < 6; i++) {
        uint8_t cardindex = DeckStk_pop(&gstate->deck[PLAYER_A]);
        HDCLL_insertNodeAtBeginning(&gstate->hand[PLAYER_A], cardindex);
        cardindex = DeckStk_pop(&gstate->deck[PLAYER_B]);
        HDCLL_insertNodeAtBeginning(&gstate->hand[PLAYER_B], cardindex);
    }
    
    // Apply mulligan for player B
    apply_mulligan(gstate);
}

void apply_mulligan(struct gamestate* gstate) {
    uint8_t max_nbr_cards_to_mulligan = 2;
    
    // Count cards to mulligan
    struct LLNode* current = gstate->hand[PLAYER_B].head;
    uint8_t nbr_cards_to_mulligan = 0;
    
    for (uint8_t i = 0; (i < gstate->hand[PLAYER_B].size) && 
         (nbr_cards_to_mulligan < max_nbr_cards_to_mulligan); i++) {
        if (fullDeck[current->data].power < AVERAGE_POWER_FOR_MULLIGAN) {
            nbr_cards_to_mulligan++;
        }
        current = current->next;
    }
    
    if (debug_enabled) {
        printf("Number of cards to mulligan: %u\n", nbr_cards_to_mulligan);
    }
    
    // Discard lowest power cards
    float minpower;
    uint8_t card_with_lowest_power;
    uint8_t nbr_cards_left_to_mulligan = nbr_cards_to_mulligan;
    
    while (nbr_cards_left_to_mulligan > 0) {
        minpower = 100.0;
        card_with_lowest_power = 0;
        current = gstate->hand[PLAYER_B].head;
        
        for (uint8_t i = 0; i < gstate->hand[PLAYER_B].size; i++) {
            if (fullDeck[current->data].power < minpower) {
                minpower = fullDeck[current->data].power;
                card_with_lowest_power = current->data;
            }
            current = current->next;
        }
        
        HDCLL_removeNodeByValue(&gstate->hand[PLAYER_B], card_with_lowest_power);
        HDCLL_insertNodeAtBeginning(&gstate->discard[PLAYER_B], card_with_lowest_power);
        nbr_cards_left_to_mulligan--;
    }
    
    // Draw replacement cards
    for (uint8_t i = 0; i < nbr_cards_to_mulligan; i++) {
        draw_1_card(gstate, PLAYER_B);
    }
}

void play_game(uint16_t initial_cash, struct gamestats* gstats, 
               StrategySet* strategies) {
    struct gamestate gstate;
    setup_game(initial_cash, &gstate);
    
    if (debug_enabled) {
        printf("Game started with %d A, %d B cash; %d A, %d B energy\n",
               gstate.current_cash_balance[PLAYER_A],
               gstate.current_cash_balance[PLAYER_B],
               gstate.current_energy[PLAYER_A],
               gstate.current_energy[PLAYER_B]);
    }
    
    gstate.turn = 0;
    
    do {
        play_turn(gstats, &gstate, strategies);
    } while (gstate.turn < MAX_NUMBER_OF_TURNS && !gstate.someone_has_zero_energy);
    
    if (!gstate.someone_has_zero_energy) {
        gstate.game_state = DRAW;
    }
    
    if (debug_enabled) {
        printf("Game ended at round %.4u, turn %.4u, winner is %s\n",
               (uint16_t)((gstate.turn-1) * 0.5)+1,
               gstate.turn,
               GAME_STATE_NAMES[gstate.game_state]);
    }
    /* printf("Game ended at round %.4u, turn %.4u, winner is %s\n",
               (uint16_t)((gstate.turn-1) * 0.5)+1,
               gstate.turn,
               GAME_STATE_NAMES[gstate.game_state]);
    */
    
    record_final_stats(gstats, &gstate);
    
    // Free heap memory
    DeckStk_emptyOut(&gstate.deck[PLAYER_A]);
    DeckStk_emptyOut(&gstate.deck[PLAYER_B]);
    HDCLL_emptyOut(&gstate.combat_zone[PLAYER_A]);
    HDCLL_emptyOut(&gstate.combat_zone[PLAYER_B]);
    HDCLL_emptyOut(&gstate.hand[PLAYER_A]);
    HDCLL_emptyOut(&gstate.hand[PLAYER_B]);
    HDCLL_emptyOut(&gstate.discard[PLAYER_A]);
    HDCLL_emptyOut(&gstate.discard[PLAYER_B]);
}

void run_simulation(uint16_t numsim, uint16_t initial_cash,
                    struct gamestats* gstats, StrategySet* strategies) {
    for (gstats->simnum = 0; gstats->simnum < numsim; gstats->simnum++) {
        if (debug_enabled) {
            printf("Begin game %.4u\n", gstats->simnum);
        }
        play_game(initial_cash, gstats, strategies);
        if (debug_enabled) {
            printf("End game %.4u\n\n", gstats->simnum);
        }
    }
}

void record_final_stats(struct gamestats* gstats, struct gamestate* gstate) {
    switch (gstate->game_state) {
        case PLAYER_A_WINS:
            ++gstats->cumul_player_wins[PLAYER_A];
            break;
        case PLAYER_B_WINS:
            ++gstats->cumul_player_wins[PLAYER_B];
            break;
        case DRAW:
            ++gstats->cumul_number_of_draws;
            break;
        case ACTIVE:
            break;
    }
    
    gstats->game_end_turn_number[gstats->simnum] = gstate->turn;
}

void present_results(struct gamestats* gstats) {
    printf("Number of wins for player A: %u\n", gstats->cumul_player_wins[PLAYER_A]);
    printf("Number of wins for player B: %u\n", gstats->cumul_player_wins[PLAYER_B]);
    printf("Number of draws: %u\n", gstats->cumul_number_of_draws);
    
    uint16_t minNbrTurn = MAX_NUMBER_OF_TURNS;
    uint16_t maxNbrTurn = 0;
    uint32_t totalNbrTurn = 0;
    
    for (uint16_t s = 0; s < gstats->simnum; s++) {
        minNbrTurn = min(minNbrTurn, gstats->game_end_turn_number[s]);
        maxNbrTurn = max(maxNbrTurn, gstats->game_end_turn_number[s]);
        totalNbrTurn += gstats->game_end_turn_number[s];
    }
    
    printf("\nAverage = %.1f, Minimum = %u, Maximum = %d number of turns per game\n",
           (float)totalNbrTurn / (float)gstats->simnum, minNbrTurn, maxNbrTurn);
}
