#ifndef CLI_GAME_H
#define CLI_GAME_H

#include "game_types.h"
#include "game_context.h"
#include "strategy.h"

#define EXIT_SIGNAL -1

/* Game phase handlers */
int handle_interactive_attack(struct gamestate* gstate,
                              PlayerID player, GameContext* ctx, config_t* cfg);

int handle_interactive_defense(struct gamestate* gstate,
                               PlayerID player, GameContext* ctx, config_t* cfg);

/* Game turn execution */
int execute_game_turn(struct gamestate* gstate, StrategySet* strategies,
                     GameContext* ctx, config_t* cfg);

/* Game initialization and cleanup */
struct gamestate* initialize_cli_game(uint16_t initial_cash,
                                      StrategySet** strategies_out,
                                      config_t* cfg,
                                      GameContext* ctx);

void cleanup_cli_game(struct gamestate* gstate, StrategySet* strategies,
                     GameContext* ctx);

#endif // CLI_GAME_H
