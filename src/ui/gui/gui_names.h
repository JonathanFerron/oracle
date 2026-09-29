// gui_names.h
// Display labels for the two players -- "Jonathan (Player A)" when a name was
// assigned, plain "Player A" when it wasn't (the untouched "Player1"/
// "Player2" defaults), "AI - Bean Counter (Player B)" for an AI seat (same
// "AI - <flavour>" text the CLI/TUI use, via format_player_label()). Built
// once at startup (the language doesn't change mid-game) and shared by the
// status bar, seat info and message log so they never disagree.

#ifndef GUI_NAMES_H
#define GUI_NAMES_H

#include <stddef.h>
#include "../../core/game_types.h" // ui_language_t, NUM_PLAYERS
#include "../shared/player_config.h" // PlayerConfig, MAX_PLAYER_LABEL_LEN

#define GUI_NAME_LEN (MAX_PLAYER_LABEL_LEN + 24)

typedef struct
{ char label[NUM_PLAYERS][GUI_NAME_LEN]; // indexed by PlayerID
} GuiNames;

// Builds both labels from `pconfig`. Pass NULL `pconfig` for bare
// "Player A"/"Player B" labels (used by tests and as a safe fallback).
void gui_names_init(GuiNames* names, PlayerConfig* pconfig, ui_language_t lang);

#endif // GUI_NAMES_H
