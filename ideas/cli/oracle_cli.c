/* Oracle: Les Champions d'Arcadie - CLI Version
   Compile: gcc -o oracle oracle_cli.c -std=c99 -Wall
   Run: ./oracle
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#define MAX_DECK_SIZE 40
#define MAX_HAND_SIZE 12
#define INITIAL_ENERGY 99
#define INITIAL_LUNAS 30
#define MAX_COMMAND_LEN 256
#define HAND_LIMIT 7

// Card types
typedef enum
{ CARD_CHAMPION,
  CARD_DRAW2,     // Pige 2 ou rappelle 1
  CARD_DRAW3,     // Pige 3 ou rappelle 2
  CARD_EXCHANGE   // Échange champion pour 5 lunas
} CardType;

// Champion colors
typedef enum
{ COLOR_INDIGO,
  COLOR_ORANGE,
  COLOR_ROUGE,
  COLOR_NONE
} Color;

// Champion species/orders
typedef enum
{ SPECIES_CENTAUR, SPECIES_DWARF, SPECIES_FAIRY, SPECIES_MINOTAUR, SPECIES_LYCAN,
  SPECIES_DRAGON, SPECIES_HOBBIT, SPECIES_HUMAN, SPECIES_ORC, SPECIES_AVEN,
  SPECIES_CYCLOPS, SPECIES_ELF, SPECIES_FAUN, SPECIES_GOBLIN, SPECIES_KOATL,
  SPECIES_NONE
} Species;

// Rank (A-E)
typedef enum { RANK_A, RANK_B, RANK_C, RANK_D, RANK_E, RANK_NONE } Rank;

// Card structure
typedef struct
{ CardType type;
  Color color;
  Species species;
  Rank rank;
  int attack;
  int defense;
  int cost;
  char name[64];
  int id;
} Card;

// Player structure
typedef struct
{ char name[32];
  int energy;
  int lunas;
  Card deck[MAX_DECK_SIZE];
  int deck_count;
  Card hand[MAX_HAND_SIZE];
  int hand_count;
  Card discard[MAX_DECK_SIZE * 3];
  int discard_count;
  Card field[3]; // Champions on field (max 3)
  int field_count;
} Player;

// Game state
typedef struct
{ Player players[2];
  int current_player;
  int turn_number;
  int game_mode; // 0=random, 1=monochrome, 2=custom
  int first_turn; // Track if first turn
} GameState;

// Global game state
GameState game;

// Function prototypes
void init_game(void);
void setup_random_decks(void);
void setup_monochrome_decks(void);
void shuffle_deck(Card *deck, int count);
void draw_card(Player *player);
void deal_initial_hands(void);
void apply_mulligan(void);
void execute_command(char* command);
void display_help(void);
void display_game_state(void);
void display_hand(Player *player);
void play_turn(void);
int parse_card_indices(char* str, int* indices, int max);
void play_champions(Player *player, int* indices, int count);
void play_draw_card(Player *player, int hand_idx);
void play_exchange_card(Player *player, int hand_idx);
void combat_phase(void);
int calculate_combo_bonus(Card *cards, int count);
void discard_to_limit(Player *player);
void collect_luna(Player *player);
void reshuffle_if_needed(Player *player);
void end_game(int winner);

// Initialize a champion card
Card create_champion(Color col, Species sp, Rank rk, int atk, int def, int cost, const char* name, int id)
{ Card c = {CARD_CHAMPION, col, sp, rk, atk, def, cost, "", id};
  strncpy(c.name, name, 63);
  c.name[63] = '\0';
  return c;
}

// Create special cards
Card create_draw2(void)
{ Card c = {CARD_DRAW2, COLOR_NONE, SPECIES_NONE, RANK_NONE, 0, 0, 1, "Draw 2 / Recall 1", 0};
  return c;
}

Card create_draw3(void)
{ Card c = {CARD_DRAW3, COLOR_NONE, SPECIES_NONE, RANK_NONE, 0, 0, 2, "Draw 3 / Recall 2", 0};
  return c;
}

Card create_exchange(void)
{ Card c = {CARD_EXCHANGE, COLOR_NONE, SPECIES_NONE, RANK_NONE, 0, 0, 0, "Exchange for 5 Lunas", 0};
  return c;
}

// Shuffle deck using Fisher-Yates
void shuffle_deck(Card *deck, int count)
{ for(int i = count - 1; i > 0; i--)
  { int j = rand() % (i + 1);
    Card temp = deck[i];
    deck[i] = deck[j];
    deck[j] = temp;
  }
}

// Draw a card from deck to hand
void draw_card(Player *player)
{ if(player->deck_count == 0)
  { reshuffle_if_needed(player);
    if(player->deck_count == 0)
    { printf("%s has no cards to draw!\n", player->name);
      return;
    }
  }
  if(player->hand_count >= MAX_HAND_SIZE)
  { printf("%s's hand is full!\n", player->name);
    return;
  }
  player->hand[player->hand_count++] = player->deck[--player->deck_count];
}

// Reshuffle discard into deck when empty
void reshuffle_if_needed(Player *player)
{ if(player->deck_count == 0 && player->discard_count > 0)
  { printf("Reshuffling %s's discard pile into deck...\n", player->name);
    memcpy(player->deck, player->discard, player->discard_count * sizeof(Card));
    player->deck_count = player->discard_count;
    player->discard_count = 0;
    shuffle_deck(player->deck, player->deck_count);
  }
}

// Deal initial 6 cards to each player
void deal_initial_hands(void)
{ for(int p = 0; p < 2; p++)
  { for(int i = 0; i < 6; i++)
      draw_card(&game.players[p]);
  }
}

// Second player mulligan (discard up to 2, draw same amount)
void apply_mulligan(void)
{ Player *p2 = &game.players[1];
  printf("\n%s (Player 2), you may mulligan up to 2 cards.\n", p2->name);
  display_hand(p2);
  printf("Enter card indices to discard (e.g., '0 1'), or press Enter to skip: ");

  char input[MAX_COMMAND_LEN];
  if(fgets(input, sizeof(input), stdin) != NULL)
  { input[strcspn(input, "\n")] = 0;
    if(strlen(input) == 0) return;

    int indices[2];
    int count = parse_card_indices(input, indices, 2);

    // Discard and draw
    for(int i = count - 1; i >= 0; i--)
    { if(indices[i] >= 0 && indices[i] < p2->hand_count)
      { p2->discard[p2->discard_count++] = p2->hand[indices[i]];
        // Shift hand
        for(int j = indices[i]; j < p2->hand_count - 1; j++)
          p2->hand[j] = p2->hand[j + 1];
        p2->hand_count--;
      }
    }

    for(int i = 0; i < count; i++)
      draw_card(p2);
    printf("Mulligan complete. Drew %d card(s).\n", count);
  }
}

// Parse space-separated card indices
int parse_card_indices(char* str, int* indices, int max)
{ int count = 0;
  char* token = strtok(str, " ");
  while(token != NULL && count < max)
  { indices[count++] = atoi(token);
    token = strtok(NULL, " ");
  }
  return count;
}

// Setup random distribution
void setup_random_decks(void)
{ Card all_cards[120];
  int card_count = 0;

  // Create simplified champion set (102 champions + 18 special cards)
  // For demo: create 40 random champions per color with varied stats
  const char* colors[] = {"Indigo", "Orange", "Rouge"};
  Color color_vals[] = {COLOR_INDIGO, COLOR_ORANGE, COLOR_ROUGE};

  for(int c = 0; c < 3; c++)
  { for(int i = 0; i < 34; i++)
    { char name[64];
      snprintf(name, 64, "%s Champion %d", colors[c], i + 1);
      int atk = 5 + (rand() % 15);
      int def = 5 + (rand() % 15);
      int cost = (rand() % 4); // 0-3 lunas
      Species sp = (Species)(rand() % 5);
      Rank rk = (Rank)(rand() % 5);
      all_cards[card_count++] = create_champion(color_vals[c], sp, rk, atk, def, cost, name, card_count);
    }
  }

  // Add special cards
  for(int i = 0; i < 9; i++) all_cards[card_count++] = create_draw2();
  for(int i = 0; i < 6; i++) all_cards[card_count++] = create_draw3();
  for(int i = 0; i < 3; i++) all_cards[card_count++] = create_exchange();

  // Shuffle and deal 40 to each player
  shuffle_deck(all_cards, card_count);

  for(int i = 0; i < 40; i++)
  { game.players[0].deck[i] = all_cards[i];
    game.players[1].deck[i] = all_cards[40 + i];
  }
  game.players[0].deck_count = 40;
  game.players[1].deck_count = 40;
}

// Setup monochrome decks (34 champions + 5 draw + 1 exchange)
void setup_monochrome_decks(void)
{ Color colors[] = {COLOR_INDIGO, COLOR_ORANGE};
  const char* color_names[] = {"Indigo", "Orange"};

  for(int p = 0; p < 2; p++)
  { int idx = 0;
    // 34 champions
    for(int i = 0; i < 34; i++)
    { char name[64];
      snprintf(name, 64, "%s Champion %d", color_names[p], i + 1);
      int atk = 5 + (rand() % 15);
      int def = 5 + (rand() % 15);
      int cost = (rand() % 3);
      Species sp = (Species)(rand() % 5);
      Rank rk = (Rank)(rand() % 5);
      game.players[p].deck[idx++] = create_champion(colors[p], sp, rk, atk, def, cost, name, idx);
    }
    // 3 draw2, 2 draw3, 1 exchange
    for(int i = 0; i < 3; i++) game.players[p].deck[idx++] = create_draw2();
    for(int i = 0; i < 2; i++) game.players[p].deck[idx++] = create_draw3();
    game.players[p].deck[idx++] = create_exchange();

    game.players[p].deck_count = 40;
    shuffle_deck(game.players[p].deck, 40);
  }
}

// Initialize game
void init_game(void)
{ srand(time(NULL));

  printf("=== Oracle: Les Champions d'Arcadie ===\n\n");
  printf("Select game mode:\n");
  printf("  1. Random distribution\n");
  printf("  2. Monochrome teams\n");
  printf("Choice (1-2): ");

  char choice[10];
  fgets(choice, sizeof(choice), stdin);
  game.game_mode = (choice[0] == '2') ? 1 : 0;

  // Initialize players
  strcpy(game.players[0].name, "Player 1");
  strcpy(game.players[1].name, "Player 2");

  for(int i = 0; i < 2; i++)
  { game.players[i].energy = INITIAL_ENERGY;
    game.players[i].lunas = INITIAL_LUNAS;
    game.players[i].deck_count = 0;
    game.players[i].hand_count = 0;
    game.players[i].discard_count = 0;
    game.players[i].field_count = 0;
  }

  // Setup decks
  if(game.game_mode == 1)
    setup_monochrome_decks();
  else
    setup_random_decks();

  // Deal initial hands
  deal_initial_hands();

  // Determine first player
  game.current_player = rand() % 2;
  printf("\n%s goes first!\n", game.players[game.current_player].name);

  // Apply mulligan for second player
  apply_mulligan();

  game.turn_number = 1;
  game.first_turn = 1;
}

// Display help
void display_help(void)
{ printf("\n=== Commands ===\n");
  printf("  status      - Show game state\n");
  printf("  hand        - Show your hand\n");
  printf("  attack X Y  - Attack with champions at indices X Y (1-3 cards)\n");
  printf("  defend X Y  - Defend with champions at indices X Y (0-3 cards)\n");
  printf("  draw X      - Play draw/recall card at index X\n");
  printf("  exchange X  - Play exchange card at index X\n");
  printf("  pass        - Pass turn and collect 1 luna\n");
  printf("  help        - Show this help\n");
  printf("  exit        - Quit game\n\n");
}

// Display game state
void display_game_state(void)
{ printf("\n=== Game State (Turn %d) ===\n", game.turn_number);
  for(int i = 0; i < 2; i++)
  { Player *p = &game.players[i];
    printf("%s: Energy=%d, Lunas=%d, Hand=%d, Deck=%d, Discard=%d\n",
           p->name, p->energy, p->lunas, p->hand_count, p->deck_count, p->discard_count);
  }
  printf("Current player: %s\n", game.players[game.current_player].name);
}

// Display player's hand
void display_hand(Player *player)
{ printf("\n%s's Hand:\n", player->name);
  for(int i = 0; i < player->hand_count; i++)
  { Card *c = &player->hand[i];
    if(c->type == CARD_CHAMPION)
      printf("  [%d] %s (ATK:%d DEF:%d Cost:%d)\n", i, c->name, c->attack, c->defense, c->cost);
    else
      printf("  [%d] %s (Cost:%d)\n", i, c->name, c->cost);
  }
}

// Calculate combo bonus based on game mode and cards played
int calculate_combo_bonus(Card *cards, int count)
{ if(count < 2) return 0;

  int bonus = 0;
  int same_species_count = 0;
  int same_rank_count = 0;
  int same_color_count = 0;

  // Check for same species
  for(int i = 1; i < count; i++)
  { if(cards[i].species == cards[0].species && cards[i].species != SPECIES_NONE)
      same_species_count++;
  }

  // Check for same rank
  for(int i = 1; i < count; i++)
  { if(cards[i].rank == cards[0].rank && cards[i].rank != RANK_NONE)
      same_rank_count++;
  }

  // Check for same color
  for(int i = 1; i < count; i++)
  { if(cards[i].color == cards[0].color && cards[i].color != COLOR_NONE)
      same_color_count++;
  }

  // Apply bonuses based on game mode
  if(game.game_mode == 1)    // Monochrome
  { if(same_species_count == count - 1)
      bonus = (count == 2) ? 7 : 12;
    else if(same_species_count == 1 && same_rank_count == count - 1)
      bonus = 9;
    else if(same_rank_count == count - 1)
      bonus = (count == 2) ? 4 : 6;
  }
  else     // Random distribution
  { if(same_species_count >= 1)
    { bonus = 10;
      if(count == 3 && same_species_count == 2) bonus += 6;
      else if(count == 3 && same_rank_count == 2) bonus += 4;
      else if(count == 3 && same_color_count == 2) bonus += 3;
    }
    else if(same_rank_count >= 1)
    { bonus = 7;
      if(count == 3 && same_rank_count == 2) bonus += 4;
      else if(count == 3 && same_color_count >= 1) bonus += 2;
    }
    else if(same_color_count >= 1)
      bonus = (count == 2) ? 5 : 8;
  }

  return bonus;
}

// Main game loop command processor
void execute_command(char* command)
{ Player *current = &game.players[game.current_player];

  if(strcmp(command, "help") == 0)
    display_help();
  else if(strcmp(command, "status") == 0)
    display_game_state();
  else if(strcmp(command, "hand") == 0)
    display_hand(current);
  else if(strncmp(command, "attack ", 7) == 0)
  { int indices[3];
    int count = parse_card_indices(command + 7, indices, 3);
    if(count >= 1 && count <= 3)
    { play_champions(current, indices, count);
      combat_phase();
      collect_luna(current);
      discard_to_limit(current);
      game.current_player = 1 - game.current_player;
      game.turn_number++;
      game.first_turn = 0;
    }
    else
      printf("Invalid attack command. Use: attack X [Y] [Z]\n");
  }
  else if(strncmp(command, "draw ", 5) == 0)
  { int idx = atoi(command + 5);
    play_draw_card(current, idx);
    collect_luna(current);
    discard_to_limit(current);
    game.current_player = 1 - game.current_player;
    game.turn_number++;
    game.first_turn = 0;
  }
  else if(strncmp(command, "exchange ", 9) == 0)
  { int idx = atoi(command + 9);
    play_exchange_card(current, idx);
    collect_luna(current);
    discard_to_limit(current);
    game.current_player = 1 - game.current_player;
    game.turn_number++;
    game.first_turn = 0;
  }
  else if(strncmp(command, "pass", 4) == 0)
  { printf("%s passes their turn.\n", current->name);
    collect_luna(current);
    discard_to_limit(current);
    game.current_player = 1 - game.current_player;
    game.turn_number++;
    game.first_turn = 0;
  }
  else if(strcmp(command, "exit") == 0)
  { printf("Exiting game. Goodbye!\n");
    exit(0);
  }
  else
    printf("Unknown command: %s (type 'help' for commands)\n", command);
}

// Play draw/recall card
void play_draw_card(Player *player, int hand_idx)
{ if(hand_idx < 0 || hand_idx >= player->hand_count)
  { printf("Invalid card index.\n");
    return;
  }

  Card *card = &player->hand[hand_idx];
  if(card->type != CARD_DRAW2 && card->type != CARD_DRAW3)
  { printf("That's not a draw/recall card!\n");
    return;
  }

  if(card->cost > player->lunas)
  { printf("Not enough lunas! Cost: %d, Have: %d\n", card->cost, player->lunas);
    return;
  }

  int draw_count = (card->type == CARD_DRAW2) ? 2 : 3;
  int recall_max = (card->type == CARD_DRAW2) ? 1 : 2;

  printf("\n%s plays '%s' (Cost: %d lunas)\n", player->name, card->name, card->cost);
  printf("Choose: (1) Draw %d cards, or (2) Recall up to %d champion(s): ",
         draw_count, recall_max);

  char choice[10];
  fgets(choice, sizeof(choice), stdin);

  // Pay cost
  player->lunas -= card->cost;

  if(choice[0] == '1')
  { // Draw cards
    for(int i = 0; i < draw_count; i++)
      draw_card(player);
    printf("Drew %d card(s).\n", draw_count);
  }
  else if(choice[0] == '2')
  { // Recall champions from discard
    printf("\nChampions in discard:\n");
    int champion_count = 0;
    for(int i = 0; i < player->discard_count; i++)
    { if(player->discard[i].type == CARD_CHAMPION)
      { printf("  [%d] %s (ATK:%d DEF:%d)\n", i,
               player->discard[i].name,
               player->discard[i].attack,
               player->discard[i].defense);
        champion_count++;
      }
    }

    if(champion_count == 0)
      printf("No champions in discard to recall.\n");
    else
    { printf("Enter up to %d indices to recall (space-separated): ", recall_max);
      char input[MAX_COMMAND_LEN];
      if(fgets(input, sizeof(input), stdin) != NULL)
      { input[strcspn(input, "\n")] = 0;
        int indices[2];
        int count = parse_card_indices(input, indices, recall_max);

        // Recall cards in reverse order
        for(int i = count - 1; i >= 0; i--)
        { int idx = indices[i];
          if(idx >= 0 && idx < player->discard_count)
          { if(player->discard[idx].type == CARD_CHAMPION)
            { player->hand[player->hand_count++] = player->discard[idx];
              // Remove from discard
              for(int j = idx; j < player->discard_count - 1; j++)
                player->discard[j] = player->discard[j + 1];
              player->discard_count--;
            }
          }
        }
        printf("Recalled %d champion(s).\n", count);
      }
    }
  }

  // Discard the draw/recall card
  player->discard[player->discard_count++] = *card;
  for(int j = hand_idx; j < player->hand_count - 1; j++)
    player->hand[j] = player->hand[j + 1];
  player->hand_count--;
}

// Play exchange card
void play_exchange_card(Player *player, int hand_idx)
{ if(hand_idx < 0 || hand_idx >= player->hand_count)
  { printf("Invalid card index.\n");
    return;
  }

  Card *card = &player->hand[hand_idx];
  if(card->type != CARD_EXCHANGE)
  { printf("That's not an exchange card!\n");
    return;
  }

  printf("\n%s plays '%s'\n", player->name, card->name);
  printf("Select a champion from your hand to exchange for 5 lunas.\n");
  display_hand(player);
  printf("Enter champion index: ");

  char input[10];
  if(fgets(input, sizeof(input), stdin) != NULL)
  { int champ_idx = atoi(input);

    if(champ_idx >= 0 && champ_idx < player->hand_count &&
       champ_idx != hand_idx)
    { if(player->hand[champ_idx].type == CARD_CHAMPION)
      { // Exchange champion for lunas
        player->lunas += 5;
        printf("Exchanged %s for 5 lunas (Total: %d)\n",
               player->hand[champ_idx].name, player->lunas);

        // Discard both cards
        player->discard[player->discard_count++] = player->hand[champ_idx];
        player->discard[player->discard_count++] = *card;

        // Remove cards from hand (higher index first)
        int high_idx = (champ_idx > hand_idx) ? champ_idx : hand_idx;
        int low_idx = (champ_idx < hand_idx) ? champ_idx : hand_idx;

        for(int j = high_idx; j < player->hand_count - 1; j++)
          player->hand[j] = player->hand[j + 1];
        player->hand_count--;

        for(int j = low_idx; j < player->hand_count - 1; j++)
          player->hand[j] = player->hand[j + 1];
        player->hand_count--;
      }
      else
        printf("That's not a champion card!\n");
    }
    else
      printf("Invalid champion index.\n");
  }
}
void play_champions(Player *player, int* indices, int count)
{ Card attack_cards[3];
  int total_cost = 0;

  // Validate and collect cards
  for(int i = 0; i < count; i++)
  { if(indices[i] < 0 || indices[i] >= player->hand_count)
    { printf("Invalid card index: %d\n", indices[i]);
      return;
    }
    attack_cards[i] = player->hand[indices[i]];
    if(attack_cards[i].type != CARD_CHAMPION)
    { printf("Card %d is not a champion!\n", indices[i]);
      return;
    }
    total_cost += attack_cards[i].cost;
  }

  // Check cost
  if(total_cost > player->lunas)
  { printf("Not enough lunas! Cost: %d, Have: %d\n", total_cost, player->lunas);
    return;
  }

  // Pay cost
  player->lunas -= total_cost;

  // Calculate attack
  int base_attack = 0;
  for(int i = 0; i < count; i++)
    base_attack += attack_cards[i].attack;
  int bonus = calculate_combo_bonus(attack_cards, count);
  int total_attack = base_attack + bonus;

  printf("\n%s attacks with %d champion(s)!\n", player->name, count);
  for(int i = 0; i < count; i++)
    printf("  - %s (ATK:%d)\n", attack_cards[i].name, attack_cards[i].attack);
  printf("Base attack: %d, Combo bonus: +%d, Total: %d\n", base_attack, bonus, total_attack);

  // Store on field
  player->field_count = count;
  for(int i = 0; i < count; i++)
    player->field[i] = attack_cards[i];

  // Remove from hand (in reverse order to avoid index issues)
  for(int i = count - 1; i >= 0; i--)
  { int idx = indices[i];
    for(int j = idx; j < player->hand_count - 1; j++)
      player->hand[j] = player->hand[j + 1];
    player->hand_count--;
  }
}

// Combat phase with defender response
void combat_phase(void)
{ Player *attacker = &game.players[game.current_player];
  Player *defender = &game.players[1 - game.current_player];

  // Calculate attack value
  int attack_value = 0;
  for(int i = 0; i < attacker->field_count; i++)
    attack_value += attacker->field[i].attack;
  attack_value += calculate_combo_bonus(attacker->field, attacker->field_count);

  // Defender chooses to defend or not
  printf("\n%s, you are being attacked for %d damage!\n", defender->name, attack_value);
  display_hand(defender);
  printf("Defend with champions? Enter indices (e.g., '0 1'), or press Enter to take damage: ");

  char input[MAX_COMMAND_LEN];
  int defense_value = 0;

  if(fgets(input, sizeof(input), stdin) != NULL)
  { input[strcspn(input, "\n")] = 0;

    if(strlen(input) > 0)
    { int indices[3];
      int count = parse_card_indices(input, indices, 3);

      Card defense_cards[3];
      int total_cost = 0;

      // Validate defense
      int valid = 1;
      for(int i = 0; i < count; i++)
      { if(indices[i] < 0 || indices[i] >= defender->hand_count)
        { valid = 0;
          break;
        }
        defense_cards[i] = defender->hand[indices[i]];
        if(defense_cards[i].type != CARD_CHAMPION)
        { valid = 0;
          break;
        }
        total_cost += defense_cards[i].cost;
      }

      if(valid && total_cost <= defender->lunas)
      { defender->lunas -= total_cost;

        for(int i = 0; i < count; i++)
          defense_value += defense_cards[i].defense;
        defense_value += calculate_combo_bonus(defense_cards, count);

        printf("%s defends with %d champion(s) for %d defense!\n",
               defender->name, count, defense_value);

        // Remove defense cards from hand
        for(int i = count - 1; i >= 0; i--)
        { int idx = indices[i];
          defender->discard[defender->discard_count++] = defender->hand[idx];
          for(int j = idx; j < defender->hand_count - 1; j++)
            defender->hand[j] = defender->hand[j + 1];
          defender->hand_count--;
        }
      }
    }
  }

  // Calculate damage
  int damage = (attack_value > defense_value) ? (attack_value - defense_value) : 0;
  defender->energy -= damage;

  printf("Damage dealt: %d (Attack: %d - Defense: %d)\n", damage, attack_value, defense_value);
  printf("%s's energy: %d\n", defender->name, defender->energy);

  // Discard attacker's champions
  for(int i = 0; i < attacker->field_count; i++)
    attacker->discard[attacker->discard_count++] = attacker->field[i];
  attacker->field_count = 0;

  // Check win condition
  if(defender->energy <= 0)
    end_game(game.current_player);
}

// Collect 1 luna at end of turn
void collect_luna(Player *player)
{ player->lunas++;
  printf("%s collects 1 luna (Total: %d)\n", player->name, player->lunas);
}

// Discard down to hand limit
void discard_to_limit(Player *player)
{ while(player->hand_count > HAND_LIMIT)
  { printf("\n%s must discard to %d cards.\n", player->name, HAND_LIMIT);
    display_hand(player);
    printf("Enter card index to discard: ");

    char input[MAX_COMMAND_LEN];
    if(fgets(input, sizeof(input), stdin) != NULL)
    { int idx = atoi(input);
      if(idx >= 0 && idx < player->hand_count)
      { player->discard[player->discard_count++] = player->hand[idx];
        for(int j = idx; j < player->hand_count - 1; j++)
          player->hand[j] = player->hand[j + 1];
        player->hand_count--;
      }
    }
  }
}

// End game
void end_game(int winner)
{ printf("\n=== GAME OVER ===\n");
  printf("%s wins!\n", game.players[winner].name);
  printf("Final scores:\n");
  for(int i = 0; i < 2; i++)
    printf("  %s: Energy=%d\n", game.players[i].name, game.players[i].energy);
  exit(0);
}

// Main function
int main(void)
{ char input_buffer[MAX_COMMAND_LEN];

  init_game();
  display_help();

  printf("\n=== Game Start ===\n");

  while(1)
  { Player *current = &game.players[game.current_player];

    // Draw card (except first player on first turn)
    if(!game.first_turn || game.current_player != 0)
    { draw_card(current);
      printf("\n%s draws a card.\n", current->name);
    }

    display_game_state();
    display_hand(current);

    printf("\n%s> ", current->name);

    if(fgets(input_buffer, sizeof(input_buffer), stdin) != NULL)
    { input_buffer[strcspn(input_buffer, "\n")] = 0;
      execute_command(input_buffer);
    }
    else
    { printf("Error reading input.\n");;
    }
  }

  return 0;
}
