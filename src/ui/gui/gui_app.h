#ifndef GUI_APP_H
#define GUI_APP_H

#include "../../core/game_types.h"

// Runs the SDL3 GUI to completion (blocks until the window is closed).
// Deliberately takes no SDL3 types in its signature so callers (stda_gui.c)
// can declare/call it without pulling in SDL3 headers themselves.
// Returns EXIT_SUCCESS/EXIT_FAILURE.
int gui_app_run(config_t* cfg);

#endif // GUI_APP_H
