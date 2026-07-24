// stda_tui_interactive.h
// TUI Milestone 2 human-turn handlers: mulligan, discard-to-7, attack,
// defense, and the phase-by-phase per-turn orchestrator. Split out of
// stda_tui.c (setup/loop) to keep both files under the project's
// file-size guideline.

#ifndef STDA_TUI_INTERACTIVE_H
#define STDA_TUI_INTERACTIVE_H

#include "../../core/game_types.h"
#include "../../core/game_context.h"
#include "../../ai_strat/ai_strategy.h"
#include "../../ui/shared/player_config.h"
#include "../../ui/tui/tui_render.h"

void tui_handle_interactive_mulligan(TuiScreen* screen, struct gamestate* gstate,
                                     GameContext* ctx, config_t* cfg);

void tui_handle_interactive_discard_to_7(TuiScreen* screen, struct gamestate* gstate,
                                         GameContext* ctx, config_t* cfg);

// Runs one full turn (begin_of_turn through end_of_turn), branching to a
// human handler or the plain AI strategy call per phase per player's
// configured type. Returns false if the player quit ('q') mid-turn.
bool tui_play_turn_with_humans(TuiScreen* screen, struct gamestate* gstate,
                               StrategySet* strategies, GameContext* ctx,
                               config_t* cfg, PlayerConfig* pconfig);

#endif // STDA_TUI_INTERACTIVE_H
