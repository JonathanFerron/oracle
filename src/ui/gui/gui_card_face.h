// gui_card_face.h
// Champion card face, laid out like the printed cards (cartes champions
// SVG): thick colour border; a narrow left column of cells -- cost in a
// hexagon; attack die + base attack with a sword; species emblem with its
// order glyph; shield with the defense die -- the art area to its right (the
// fractal art when enabled, else blank), and a name bar along the bottom (the champion's name, champion_names.h).
// Text uses Fredoka Bold (the printed cards' Fredoka One, static instance).

#ifndef GUI_CARD_FACE_H
#define GUI_CARD_FACE_H

#include <stdbool.h>
#include <SDL3/SDL.h>
#include "../../core/game_types.h" // struct card

// Opens the face font from `font_path` (absolute). Non-fatal: without it
// gui_card_face_draw_champion() returns false and the caller falls back to
// the plain text card.
bool gui_card_face_init(const char* font_path);
void gui_card_face_shutdown(void);

// Draws champion `c` into `rect` in `border` colour. `highlighted` adds a
// white inner ring; the name bar shows the champion's name in `lang`. Returns false (drawing nothing) if the face font isn't
// available.
bool gui_card_face_draw_champion(SDL_Renderer* renderer, SDL_FRect rect, const struct card* c,
                                 SDL_Color border, bool highlighted, ui_language_t lang);

#endif // GUI_CARD_FACE_H
