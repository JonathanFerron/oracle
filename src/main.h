#ifndef MAIN_H
#define MAIN_H

#include <stdbool.h>

// extern
extern bool debug_enabled;

struct gamestate;
struct gamestats;
struct deck_stack;
struct HDCLList;

enum PlayerID ;

int main();

void ORACLE_0a_simulation(uint16_t, uint16_t, struct gamestats*);
void ORACLE_0b_play_game(uint16_t, struct gamestats*);
void ORACLE_1_setup_game(uint16_t, struct gamestate*);
void ORACLE_3_apply_mulligan();
void ORACLE_4a_play_turn(struct gamestats*, struct gamestate*);
void ORACLE_4b_draw1card(struct gamestate*, enum PlayerID );
void ORACLE_4c_shuffle_discard_and_form_deck(struct HDCLList*, struct deck_stack *);
double ORACLE_5_calculate_expected_value(uint8_t base, uint8_t dice_type);
void ORACLE_6_collect1luna(struct gamestate*);
void ORACLE_7a_discard_to_7_cards(struct gamestate*);
void ORACLE_7b_chg_current_player(struct gamestate*);
void ORACLE_8_record_final_stats(struct gamestats*, struct gamestate*);
void ORACLE_9_present_results(struct gamestats*);

#endif
