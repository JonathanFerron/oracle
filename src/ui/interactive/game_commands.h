// game_commands.h
// UI-agnostic interactive command grammar and rules, shared between the CLI
// and TUI: attack/defense command dispatch (cham/draw/cash/pass/exit), card
// selection parsing/validation, recall (draw/recall cards), and cash
// exchange. Talks to the caller only through a UiIO* (src/ui/shared/ui_io.h)
// -- no stdio/ncurses here. Board/state rendering and any UI-only
// diagnostic commands (e.g. the CLI's gmst/shod/help) are NOT part of this
// module; callers handle those themselves before/after delegating here.

#ifndef GAME_COMMANDS_H
#define GAME_COMMANDS_H

#include "../../core/game_types.h"
#include "../../core/game_context.h"
#include "../shared/ui_io.h"

/* Card selection input helpers */
int parse_champion_indices(char* input, uint8_t* indices, int max_count,
                           int hand_size, config_t* cfg, UiIO* io);

int parse_card_indices_with_validation(char* input, uint8_t* indices,
                                       int max_count, int hand_size,
                                       config_t* cfg, UiIO* io);

/* Champion play validation/execution */
int validate_and_play_champions(struct gamestate* gstate, PlayerID player,
                                uint8_t* indices, int count, GameContext* ctx,
                                config_t* cfg, UiIO* io);

/* Recall functionality (draw/recall cards) */
char prompt_draw_or_recall(config_t* cfg, UiIO* io);

int handle_recall_choice(struct gamestate* gstate, PlayerID player,
                         uint8_t card_idx, GameContext* ctx, config_t* cfg,
                         UiIO* io);

int validate_and_recall_champions(struct gamestate* gstate, PlayerID player,
                                  uint8_t draw_card_idx, uint8_t* indices,
                                  int count, GameContext* ctx, config_t* cfg,
                                  UiIO* io);

int handle_draw_command(struct gamestate* gstate, PlayerID player,
                        char* input, GameContext* ctx, config_t* cfg,
                        UiIO* io);

/* Cash exchange functionality (cash cards) */
int prompt_champion_exchange(Hand* hand, config_t* cfg, UiIO* io);

int handle_cash_command(struct gamestate* gstate, PlayerID player,
                        char* input, GameContext* ctx, config_t* cfg,
                        UiIO* io);

/* Discard-then-optionally-draw helper (pure state mutation, no I/O) --
   used by mulligan and discard-to-7. */
void discard_and_draw_cards(struct gamestate* gstate, PlayerID player,
                            uint8_t* indices, int count,
                            bool draw_replacements, GameContext* ctx);

/* Shared turn-action grammar: cham <indices> / draw <index> / cash <index> /
   pass / exit, plus the unknown-command fallback. Callers intercept any
   UI-only commands (CLI: gmst/shod/help) before falling through to these. */
int game_process_attack_command(char* input_buffer, struct gamestate* gstate,
                                PlayerID player, GameContext* ctx,
                                config_t* cfg, UiIO* io);

int game_process_defense_command(char* input_buffer, struct gamestate* gstate,
                                 PlayerID player, GameContext* ctx,
                                 config_t* cfg, UiIO* io);

/* Mulligan (Player B, game start) and discard-to-7 (end of every turn)
   grammar: mull <indices>/pass, and disc <indices>, respectively. Both
   return 1 when done, 0 to keep prompting. Callers intercept the CLI-only
   "help" command (full hand redisplay) before falling through to these. */
int game_process_mulligan_command(char* input_buffer, struct gamestate* gstate,
                                  GameContext* ctx, config_t* cfg, UiIO* io);

int game_process_discard_command(char* input_buffer, struct gamestate* gstate,
                                 int cards_to_discard, GameContext* ctx,
                                 config_t* cfg, UiIO* io);

#endif // GAME_COMMANDS_H
