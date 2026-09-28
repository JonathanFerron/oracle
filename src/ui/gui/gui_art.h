// gui_art.h
// Lazy texture cache for card art. Currently only the legacy fractal art
// (assets/fractals/fractale_<ID>.png, ID = the champion's 3-digit
// zero-padded fullDeck champion_id, 001-102 -- see assets/about.md), shown
// when `legacy_fractal = true` in the [gui] config. Module-level state (one
// renderer, one cache) rather than a passed-around handle: the GUI has
// exactly one renderer and draws from one thread, and this keeps
// gui_card_draw()'s signature (7 call sites) unchanged.

#ifndef GUI_ART_H
#define GUI_ART_H

#include <stdint.h>
#include <SDL3/SDL.h>

// Enables fractal art, loading from `fractal_dir` (a directory holding
// fractale_NNN.png). Call once after the renderer exists; until then (or
// if never called) gui_art_fractal() returns NULL for every card.
void gui_art_init(SDL_Renderer* renderer, const char* fractal_dir);

// Texture for champion `champion_id` (1-102), loaded on first request and
// cached. NULL if art is disabled, the ID is out of range, or the file is
// missing/unreadable (a failure is remembered -- one stderr warning, not one
// per frame). The caller must not destroy the texture.
SDL_Texture* gui_art_fractal(uint8_t champion_id);

// Destroys all cached textures and disables art. Safe if never initialised.
// Call before the renderer is destroyed.
void gui_art_shutdown(void);

#endif // GUI_ART_H
