// combat.c
// Combat resolution implementation

#include "combat.h"
#include "combo_bonus.h"
#include "game_constants.h"
#include "rnd.h"
#include <stdio.h>

extern bool debug_enabled;

#define max(a,b) ({ typeof (a) _a = (a); typeof (b) _b = (b); _a > _b ? _a : _b; })
#define min(a,b) ({ typeof (a) _a = (a); typeof (b) _b = (b); _a < _b ? _a : _b; })

void resolve_combat(struct gamestate* gstate) {
    PlayerID attacker = gstate->current_player;
    PlayerID defender = 1 - gstate->current_player;
    
    // Calculate attack and defense
    int16_t total_attack = calculate_total_attack(gstate, attacker);
    int16_t total_defense = calculate_total_defense(gstate, defender);
    
    // Apply damage
    apply_combat_damage(gstate, total_attack, total_defense);
    
    // Clear combat zones
    clear_combat_zones(gstate);
}

int16_t calculate_total_attack(struct gamestate* gstate, PlayerID player) {
    int16_t total = 0;
    struct LLNode* current = gstate->combat_zone[player].head;
    
    // Build CombatCard array for combo calculation
    uint8_t num_cards = gstate->combat_zone[player].size;
    CombatCard combat_cards[3];
    
    for (uint8_t i = 0; i < num_cards; i++) {
        uint8_t card_idx = current->data;
        
        // Add base attack + dice roll
        total += fullDeck[card_idx].attack_base + 
                 RND_dn(fullDeck[card_idx].defense_dice);
        
        // Store for combo calculation
        combat_cards[i].species = fullDeck[card_idx].species;
        combat_cards[i].color = fullDeck[card_idx].color;
        
        if (debug_enabled) {
            printf(" Attack card %u: D%u+%u, cost %u\n", 
                   card_idx, 
                   fullDeck[card_idx].defense_dice,
                   fullDeck[card_idx].attack_base,
                   fullDeck[card_idx].cost);
        }
        
        current = current->next;
    }
    
    // Add combo bonus (assuming DECK_RANDOM for now)
    int bonus = calculate_combo_bonus(combat_cards, num_cards, DECK_RANDOM);
    total += bonus;
    
    if (debug_enabled && bonus > 0) {
        printf(" Combo bonus: +%d\n", bonus);
    }
    
    if (debug_enabled) {
        printf(" Total attack: %d\n", total);
    }
    
    return total;
}

int16_t calculate_total_defense(struct gamestate* gstate, PlayerID player) {
    int16_t total = 0;
    struct LLNode* current = gstate->combat_zone[player].head;
    
    // Build CombatCard array for combo calculation
    uint8_t num_cards = gstate->combat_zone[player].size;
    CombatCard combat_cards[3];
    
    for (uint8_t i = 0; i < num_cards; i++) {
        uint8_t card_idx = current->data;
        
        // Add dice roll only (no base for defense)
        total += RND_dn(fullDeck[card_idx].defense_dice);
        
        // Store for combo calculation
        combat_cards[i].species = fullDeck[card_idx].species;
        combat_cards[i].color = fullDeck[card_idx].color;
        
        if (debug_enabled) {
            printf(" Defense card %u: D%u, cost %u\n",
                   card_idx,
                   fullDeck[card_idx].defense_dice,
                   fullDeck[card_idx].cost);
        }
        
        current = current->next;
    }
    
    // Add combo bonus (assuming DECK_RANDOM for now)
    int bonus = calculate_combo_bonus(combat_cards, num_cards, DECK_RANDOM);
    total += bonus;
    
    if (debug_enabled && bonus > 0) {
        printf(" Combo bonus: +%d\n", bonus);
    }
    
    if (debug_enabled) {
        printf(" Total defense: %d\n", total);
    }
    
    return total;
}

void apply_combat_damage(struct gamestate* gstate, int16_t total_attack, 
                         int16_t total_defense) {
    PlayerID defender = 1 - gstate->current_player;
    
    int16_t total_damage = max(total_attack - total_defense, 0);
    
    if (debug_enabled) {
        printf(" Defender energy before: %u\n", gstate->current_energy[defender]);
    }
    
    gstate->current_energy[defender] -= min((uint8_t)total_damage, 
                                            gstate->current_energy[defender]);
    
    if (debug_enabled) {
        printf(" Damage dealt: %d\n", total_damage);
        printf(" Defender energy after: %u\n", gstate->current_energy[defender]);
    }
    
    // Check for game end
    if (gstate->current_energy[defender] == 0) {
        gstate->someone_has_zero_energy = true;
        gstate->game_state = (gstate->current_player == PLAYER_A) ? 
                             PLAYER_A_WINS : PLAYER_B_WINS;
    }
}

void clear_combat_zones(struct gamestate* gstate) {
    PlayerID attacker = gstate->current_player;
    PlayerID defender = 1 - gstate->current_player;
    
    // Move attacker's cards to discard
    while (gstate->combat_zone[attacker].size > 0) {
        uint8_t card_idx = HDCLL_removeNodeFromBeginning(&gstate->combat_zone[attacker]);
        HDCLL_insertNodeAtBeginning(&gstate->discard[attacker], card_idx);
    }
    
    // Move defender's cards to discard
    while (gstate->combat_zone[defender].size > 0) {
        uint8_t card_idx = HDCLL_removeNodeFromBeginning(&gstate->combat_zone[defender]);
        HDCLL_insertNodeAtBeginning(&gstate->discard[defender], card_idx);
    }
}