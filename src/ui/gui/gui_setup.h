// gui_setup.h
// In-window player setup screen: game mode, human names, AI agent (two-column
// list, easiest first, colour dot per rating, faint band behind Borealis),
// a Player A / Player B radio in AI vs AI, seat assignment, and Start. The
// rules live in gui_setup_model.c (SDL-free, unit-tested); this file draws
// the panel and maps clicks/keys/text onto it. One layout function feeds
// both drawing and hit-testing, so they always agree.

#ifndef GUI_SETUP_H
#define GUI_SETUP_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "gui_setup_model.h"

typedef struct
{ PlayerConfig pconfig;              // the choices being edited
  char default_names[2][MAX_PLAYER_NAME_LEN]; // restored if a field is left empty
  PlayerID radio_seat;               // AI vs AI: the seat the agent list edits
  int focus_seat;                    // name field with keyboard focus, -1 = none
  bool name_fresh;                   // focused field still shows its default name:
  // the first keystroke replaces it rather than appending
  GuiSetupAgent agents[AI_STRATEGY_COUNT]; // implemented agents, easiest first
  uint8_t agent_count;
  int8_t min_rating, max_rating;     // range the rating colours are spread over
} GuiSetupState;

typedef enum
{ GUI_SETUP_CONTINUE = 0, // still on the setup screen
  GUI_SETUP_START         // Start pressed: st->pconfig holds the final choices
} GuiSetupResult;

// Opens the screen with `defaults` preselected (first launch: init_player_config()
// plus -Aa/-Ab; "change setup": the previous game's choices).
void gui_setup_open(GuiSetupState* st, const PlayerConfig* defaults);

void gui_setup_draw(SDL_Renderer* renderer, TTF_Font* font, const GuiSetupState* st,
                    float win_w, float win_h, ui_language_t lang);

GuiSetupResult gui_setup_click(GuiSetupState* st, float win_w, float win_h, float x, float y);

// Backspace edits, Tab moves to the other name field, Escape/Return leave the
// field; Return outside a field starts the game.
GuiSetupResult gui_setup_key(GuiSetupState* st, SDL_Keycode key);

// One SDL_EVENT_TEXT_INPUT's UTF-8 text, for the focused name field.
void gui_setup_text(GuiSetupState* st, const char* text);

// Whether a name field has focus -- gui_app.c starts/stops SDL text input on it.
bool gui_setup_wants_text(const GuiSetupState* st);

#endif // GUI_SETUP_H
