#ifndef STDA_GUI_H
#define STDA_GUI_H

#include "../../core/game_types.h"

/* Main GUI mode entry point (MODE_STDA_GUI). Always built -- unlike
   src/ui/gui/, this file never #includes SDL3 headers unless HAVE_SDL3 is
   defined, so the default/debug/release build stays SDL3-free (see
   makefile's SOURCES comment and doc/oracle_roadmap.md's "SDL3 GUI" item).
   Build with `make gui`/`make gui-debug` for the real implementation. */
int run_mode_stda_gui(config_t* cfg);

#endif // STDA_GUI_H
