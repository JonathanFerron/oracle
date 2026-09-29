// gui_config.h
// Minimal INI reader for the GUI's `[gui]` section of oracle_config.ini,
// read once at startup (no runtime settings UI -- design settled 2026-09-25,
// see the plan file's "Next up: rounding out GUI M1"). Format follows
// ideas/7 config file/'s precedent: `[section]`, `key = value`, `#`/`;`
// comments. Only `[gui]` is interpreted; every other section is skipped, so
// the eventual general config (`[general]`/`[mode]`/`[ai]`/`[output]`) can
// share the same file. Deliberately SDL-free (libc only) so it is unit-
// testable without a display -- see testsrc/test_gui_config.c.
//
// Keys:
//   font_path      = <path>   text font for the status bar, seat labels, log
//                              and draw/cash card text;
//                              absolute, or relative to the project root
//                              (e.g. assets/fonts/Fredoka/static/Fredoka-Regular.ttf)
//   card_font_path = <path>   font for the numbers/name on champion cards
//                              (default assets/fonts/Fredoka/static/Fredoka-Medium.ttf)
//   legacy_fractal = <bool>   draw fractal art (assets/fractals/) on
//                              champion cards; true/false/yes/no/on/off/1/0

#ifndef GUI_CONFIG_H
#define GUI_CONFIG_H

#include <stdbool.h>
#include <stdio.h>

#define GUI_CONFIG_PATH_MAX 512

typedef struct
{ char font_path[GUI_CONFIG_PATH_MAX]; // "" = use the bundled default fonts
  char card_font_path[GUI_CONFIG_PATH_MAX]; // "" = use the bundled card-face font
  bool legacy_fractal;                 // default false: no fractal art on cards
} GuiConfig;

// Fills `cfg` with the defaults (empty font_path, legacy_fractal off).
void gui_config_defaults(GuiConfig* cfg);

// Reads `path` into `cfg` (which should already hold defaults). Returns
// false if the file could not be opened -- not an error for the caller, a
// missing config file just means "all defaults". Malformed lines and
// unknown keys in [gui] are skipped with a warning on stderr.
bool gui_config_load(GuiConfig* cfg, const char* path);

// Same as gui_config_load() but from an already-open stream (used by tests).
void gui_config_read(GuiConfig* cfg, FILE* in);

#endif // GUI_CONFIG_H
