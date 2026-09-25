// gui_card.h
// Procedural (no-art-yet) card rendering: coloured border + name/cost/dice/
// base-attack text, per the plan file's "three changes agreed" note (art is
// a later wiring pass, not an M1 blocker). Also hosts gui_draw_text(), a
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

// Card back (opponent's hand, deck) -- a plain filled+outlined rect, no
// text or art.
void gui_card_draw_back(SDL_Renderer* renderer, SDL_FRect rect);

// A face-up card (fullDeck[card_index]): coloured border (gui_palette.h),
// name/cost/dice/base-attack text, no art yet. `highlighted` draws a
// thicker border -- unused until gui_input.c raises staged cards, but
// threaded through now so that file won't need to touch this signature.
// `lang` localizes the draw/recall/cash-exchange label text (champion
// species names stay English/unlocalized, matching every other UI surface
// -- see game_constants.c's own comment on CHAMPION_SPECIES_NAMES).
void gui_card_draw(SDL_Renderer* renderer, TTF_Font* font, SDL_FRect rect,
                   uint8_t card_index, bool highlighted, ui_language_t lang);

#endif // GUI_CARD_H
