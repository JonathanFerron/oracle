#ifndef STDA_TUI_H
#define STDA_TUI_H

#include "../../core/game_types.h"

/* Main TUI mode entry point (Milestone 1: AI-vs-AI display skeleton --
   renders the ncurses layout and steps one turn per keypress; no human
   interaction yet). */
int run_mode_stda_tui(config_t* cfg);

#endif // STDA_TUI_H
