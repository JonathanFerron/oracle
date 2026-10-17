// gui_setup_model.h
// Rules behind the GUI's in-window player setup screen (SDL-free, so it is
// unit-tested: testsrc/test_gui_setup.c). Drawing and input live in
// gui_setup.c.
//
// The screen edits a PlayerConfig directly (player_config.h): game mode is
// derived from player_types[], one name field per human seat, one agent
// list (two columns, easiest first) for the AI seat -- or, in AI vs AI, for
// whichever seat the Player A / Player B radio selects -- and the assignment
// choice (applied per game by gui_app.c, so Random re-rolls each game).
//
// Nothing here calls into player_config.c or the agent registry: callers
// pass ratings/availability in, which keeps the test binary from linking
// every AI agent.

#ifndef GUI_SETUP_MODEL_H
#define GUI_SETUP_MODEL_H

#include <stddef.h>
#include <stdint.h>
#include "../shared/player_config.h"

typedef enum
{ GUI_SETUP_HUMAN_VS_AI = 0,
  GUI_SETUP_HUMAN_VS_HUMAN,
  GUI_SETUP_AI_VS_AI,
  GUI_SETUP_MODE_COUNT
} GuiSetupMode;

// One row of the agent list.
typedef struct
{ AIStrategyType type;
  int8_t rating;
} GuiSetupAgent;

// Hue (degrees) of the easiest and hardest agent's rating icon; ratings in
// between are linear. Fully saturated, 50% lightness.
#define GUI_SETUP_HUE_EASIEST 310.0f
#define GUI_SETUP_HUE_HARDEST 0.0f

// Game mode as implied by pconfig->player_types[]. AI vs Human (AI at seat
// A) can't come from this screen and reads as Human vs AI.
GuiSetupMode gui_setup_get_mode(const PlayerConfig* pconfig);

// Sets player_types[] for `mode`: Human vs AI puts the human at seat A,
// matching the CLI's apply_player_selection().
void gui_setup_set_mode(PlayerConfig* pconfig, GuiSetupMode mode);

// The seat whose agent the list edits: seat B in Human vs AI, `radio_seat`
// in AI vs AI, -1 in Human vs Human (no list shown).
int gui_setup_list_seat(const PlayerConfig* pconfig, PlayerID radio_seat);

// Copies -Aa/-Ab (config_t's agent[]/agent_set[]) into the defaults.
void gui_setup_apply_cmdline_agents(PlayerConfig* pconfig,
                                    const AIStrategyType agent[2],
                                    const bool agent_set[2]);

// Appends UTF-8 `text` (one SDL text-input event) to a name, dropping it
// whole if it doesn't fit in MAX_PLAYER_NAME_LEN - 1 bytes. Returns whether
// anything was appended.
bool gui_setup_name_append(char name[MAX_PLAYER_NAME_LEN], const char* text);

// Removes the last UTF-8 character of `name`, not just its last byte.
void gui_setup_name_backspace(char name[MAX_PLAYER_NAME_LEN]);

// Sorts agents easiest first (rating ascending); equal ratings keep their
// incoming order.
void gui_setup_sort_agents(GuiSetupAgent* agents, size_t n);

// Icon hue for `rating` on the [min_rating, max_rating] range in use:
// GUI_SETUP_HUE_EASIEST at min, GUI_SETUP_HUE_HARDEST at max, clamped
// outside it. A zero-width range gives the easiest hue.
float gui_setup_rating_hue(int rating, int min_rating, int max_rating);

// HSL -> 8-bit RGB. h in degrees (any value, wrapped), s and l in [0, 1].
void gui_setup_hsl_to_rgb(float h, float s, float l, uint8_t rgb[3]);

#endif // GUI_SETUP_MODEL_H
