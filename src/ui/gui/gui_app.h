#ifndef GUI_APP_H
#define GUI_APP_H

#include "../../core/game_types.h"
#include "../shared/player_config.h" // PlayerConfig -- SDL3-free header, safe here

// Runs the SDL3 GUI to completion (blocks until the window is closed).
// Deliberately takes no SDL3 types in its signature so callers (stda_gui.c)
// can declare/call it without pulling in SDL3 headers themselves. `pconfig`
// must outlive the call (it isn't copied); the game session itself is
// started from inside gui_sdl_init() once the window/renderer exist.
// Returns EXIT_SUCCESS/EXIT_FAILURE.
int gui_app_run(config_t* cfg, PlayerConfig* pconfig);

#endif // GUI_APP_H
