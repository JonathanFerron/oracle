#ifndef CLI_INPUT_H
#define CLI_INPUT_H

#include "../../core/game_types.h"
#include "../../core/game_context.h"

/* Input parsing and validation */
int parse_champion_indices(char* input, uint8_t* indices, int max_count,
                           int hand_size, config_t* cfg);

int validate_and_play_champions(struct gamestate* gstate, PlayerID player,
                                uint8_t* indices, int count, GameContext* ctx,
                                config_t* cfg);

/* Card action handlers */
int handle_draw_command(struct gamestate* gstate, PlayerID player,
                        char* input, GameContext* ctx, config_t* cfg);

int handle_cash_command(struct gamestate* gstate, PlayerID player,
                        char* input, GameContext* ctx, config_t* cfg);

/* Command processing */
int process_attack_command(char* input_buffer, struct gamestate* gstate,
                          PlayerID player, GameContext* ctx, config_t* cfg);

int process_defense_command(char* input_buffer, struct gamestate* gstate,
                           PlayerID player, GameContext* ctx, config_t* cfg);
                           
// Card selection input helpers
int parse_card_indices_with_validation(char* input, uint8_t* indices,
                                       int max_count, int hand_size,
                                       config_t* cfg);

void discard_and_draw_cards(struct gamestate* gstate, PlayerID player,
                            uint8_t* indices, int count,
                            bool draw_replacements, GameContext* ctx);                           

#endif // CLI_INPUT_H
