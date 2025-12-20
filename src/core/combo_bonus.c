// combo_bonus.c
// Champion card combo bonus calculator implementation

#include "combo_bonus.h"
#include "game_constants.h"
#include <stddef.h>

int calculate_combo_bonus(CombatCard *cards, int num_cards, DeckType deck_type)
{ if(num_cards < 2 || num_cards > 3) return 0;

  if(deck_type == DECK_RANDOM)
    return calc_random_bonus(cards, num_cards);
  else
    return calc_prebuilt_bonus(cards, num_cards);
}

int calc_random_bonus(CombatCard *cards, int num_cards)
{ int species_counts[SPECIES_COUNT] = {0};
  int order_counts[ORDER_COUNT] = {0};
  int color_counts[COLOR_COUNT] = {0};

  count_by_species(cards, num_cards, species_counts);
  count_by_order(cards, num_cards, order_counts);
  count_by_color(cards, num_cards, color_counts);

  int max_species = get_max_count(species_counts, SPECIES_COUNT);
  int max_order = get_max_count(order_counts, ORDER_COUNT);
  int max_color = get_max_count(color_counts, COLOR_COUNT);

  // Priority 1: Species matches
  if(max_species >= 2)
  { if(num_cards == 2) return 10;
    if(max_species == 3) return 16;
    if(third_matches_order_of_species_pair(cards, num_cards)) return 14;
    if(third_matches_color_of_species_pair(cards, num_cards)) return 13;
    return 10;
  }

  // Priority 2: Order matches (no species match)
  if(max_order >= 2)
  { if(num_cards == 2) return 7;
    if(max_order == 3) return 11;
    if(third_matches_color_of_order_pair(cards, num_cards)) return 9;
    return 7;
  }

  // Priority 3: Color matches (no species or order match)
  if(max_color >= 2)
  { if(num_cards == 2) return 5;
    if(max_color == 3) return 8;
    return 5;
  }

  return 0;
}

int calc_prebuilt_bonus(CombatCard *cards, int num_cards)
{ int species_counts[SPECIES_COUNT] = {0};
  int order_counts[ORDER_COUNT] = {0};

  count_by_species(cards, num_cards, species_counts);
  count_by_order(cards, num_cards, order_counts);

  int max_species = get_max_count(species_counts, SPECIES_COUNT);
  int max_order = get_max_count(order_counts, ORDER_COUNT);

  // Priority 1: Species matches
  if(max_species >= 2)
  { if(num_cards == 2) return 7;
    if(max_species == 3) return 12;
    if(third_matches_order_of_species_pair(cards, num_cards)) return 9;
    return 7;
  }

  // Priority 2: Order matches
  if(max_order >= 2)
  { if(num_cards == 2) return 4;
    if(max_order == 3) return 6;
    return 4;
  }

  return 0;
}

void count_by_species(CombatCard *cards, int n, int* counts)
{ for(int i = 0; i < n; i++)
  { if(cards[i].species < SPECIES_COUNT)
      counts[cards[i].species]++;
  }
}

void count_by_order(CombatCard *cards, int n, int* counts)
{ for(int i = 0; i < n; i++)
  { // OLD: ChampionOrder order = get_order_from_species(cards[i].species);
    // NEW: Direct access
    if(cards[i].order < ORDER_COUNT)
      counts[cards[i].order]++;
  }
}

void count_by_color(CombatCard *cards, int n, int* counts)
{ for(int i = 0; i < n; i++)
  { if(cards[i].color < COLOR_COUNT)
      counts[cards[i].color]++;
  }
}

int get_max_count(int* counts, int size)
{ int max = 0;
  for(int i = 0; i < size; i++)
  { if(counts[i] > max) max = counts[i];
  }
  return max;
}

int third_matches_order_of_species_pair(CombatCard *cards, int n)
{ if(n != 3) return 0;

  int species_counts[SPECIES_COUNT] = {0};
  count_by_species(cards, n, species_counts);

  ChampionSpecies paired = SPECIES_NOT_APPLICABLE;
  for(int i = 0; i < SPECIES_COUNT; i++)
  { if(species_counts[i] == 2)
    { paired = i;
      break;
    }
  }
  if(paired == SPECIES_NOT_APPLICABLE) return 0;

  ChampionOrder paired_order = ORDER_NOT_APPLICABLE;
  CombatCard *singleton = NULL;
  for(int i = 0; i < n; i++)
  { if(cards[i].species == paired)
      paired_order = cards[i].order;
    else
      singleton = &cards[i];
  }

  if(!singleton) return 0;

  return (singleton->order == paired_order &&
          singleton->species != paired);
}

int third_matches_color_of_species_pair(CombatCard *cards, int n)
{ if(n != 3) return 0;

  int species_counts[SPECIES_COUNT] = {0};
  count_by_species(cards, n, species_counts);

  ChampionSpecies paired = SPECIES_NOT_APPLICABLE;
  for(int i = 0; i < SPECIES_COUNT; i++)
  { if(species_counts[i] == 2)
    { paired = i;
      break;
    }
  }
  if(paired == SPECIES_NOT_APPLICABLE) return 0;

  ChampionColor paired_color = COLOR_NOT_APPLICABLE;
  CombatCard *singleton = NULL;
  for(int i = 0; i < n; i++)
  { if(cards[i].species == paired)
      paired_color = cards[i].color;
    else
      singleton = &cards[i];
  }

  return (singleton && singleton->color == paired_color);
}

int third_matches_color_of_order_pair(CombatCard *cards, int n)
{ if(n != 3) return 0;

  int order_counts[ORDER_COUNT] = {0};
  count_by_order(cards, n, order_counts);

  ChampionOrder paired_order = ORDER_NOT_APPLICABLE;
  for(int i = 0; i < ORDER_COUNT; i++)
  { if(order_counts[i] == 2)
    { paired_order = i;
      break;
    }
  }
  if(paired_order == ORDER_NOT_APPLICABLE) return 0;

  CombatCard *order_match_cards[2] = {NULL, NULL};
  CombatCard *singleton = NULL;
  int match_count = 0;

  for(int i = 0; i < n; i++)
  { if(cards[i].order == paired_order)
      order_match_cards[match_count++] = &cards[i];
    else
      singleton = &cards[i];
  }

  if(!singleton || match_count != 2) return 0;

  return (singleton->color == order_match_cards[0]->color ||
          singleton->color == order_match_cards[1]->color);
}
