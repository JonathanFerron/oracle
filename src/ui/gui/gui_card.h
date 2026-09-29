// gui_card.h
// Card rendering: procedural (coloured border + name/cost/dice/
// base-attack text) with optional fractal art behind it on champions (gui_art.h;
// off unless legacy_fractal is set). Also hosts gui_draw_text(), a
// small one-shot text helper every gui_*.c file needs (status labels, card
// text, panel headers) -- see its own comment for why it isn't cached.

#ifndef GUI_CARD_H
#define GUI_CARD_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "../../core/game_types.h" // ui_language_t

// Renders `text` with `font` and draws it left-aligned at (x,y), one shot
// (surface -> texture -> draw -> destroy, every call). Simpler than a
// glyph/texture cache and fine at this scale (a few dozen short strings a
// frame) -- see CLAUDE.md's "no premature optimization" -- revisit only if
// profiling ever shows text rendering as a real cost.
void gui_draw_text(SDL_Renderer* renderer, TTF_Font* font, const char* text,
                   float x, float y, SDL_Color colour);

// Word-wrapped variant: wraps at `wrap_w` pixels, returns the drawn height
// (0 if nothing was drawn). gui_text_height_wrapped() measures the same text
// without drawing, so a caller can lay out from the bottom up.
int gui_draw_text_wrapped(SDL_Renderer* renderer, TTF_Font* font, const char* text,
                          float x, float y, int wrap_w, SDL_Color colour);
int gui_text_height_wrapped(TTF_Font* font, const char* text, int wrap_w);

// Card back (opponent's hand, deck) -- a plain filled+outlined rect, no
// text or art.
void gui_card_draw_back(SDL_Renderer* renderer, SDL_FRect rect);

// A face-up card (fullDeck[card_index]): coloured border (gui_palette.h),
// name/cost/dice/base-attack text, plus fractal art on champions when
// enabled (gui_art.h). `highlighted` draws a thicker border -- unused until gui_input.c raises staged cards, but
// threaded through now so that file won't need to touch this signature.
// `lang` localizes the draw/recall/cash-exchange label text (champion
// species names stay English/unlocalized, matching every other UI surface
// -- see game_constants.c's own comment on CHAMPION_SPECIES_NAMES).
void gui_card_draw(SDL_Renderer* renderer, TTF_Font* font, SDL_FRect rect,
                   uint8_t card_index, bool highlighted, ui_language_t lang);

#endif // GUI_CARD_H
