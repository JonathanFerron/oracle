// combo_bonus.h
// Champion card combo bonus calculator

#ifndef COMBO_BONUS_H
#define COMBO_BONUS_H

#include "game_types.h"

// Simplified combat card structure for combo calculations
typedef struct
{ ChampionSpecies species;
  ChampionColor color;
  ChampionOrder order;
} CombatCard;

// Main calculation function
int calculate_combo_bonus(CombatCard *cards, int num_cards, DeckType deck_type);

// Mode-specific calculators
int calc_random_bonus(CombatCard *cards, int num_cards);
int calc_prebuilt_bonus(CombatCard *cards, int num_cards);

// Helper functions
void count_by_species(CombatCard *cards, int num_cards, int* counts);
void count_by_order(CombatCard *cards, int num_cards, int* counts);
void count_by_color(CombatCard *cards, int num_cards, int* counts);
int get_max_count(int* counts, int size);

// Matching helpers
int third_matches_order_of_species_pair(CombatCard *cards, int num_cards);
int third_matches_color_of_species_pair(CombatCard *cards, int num_cards);
int third_matches_color_of_order_pair(CombatCard *cards, int num_cards);

#endif // COMBO_BONUS_H
