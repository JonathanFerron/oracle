// gui_art.h
// Lazy texture cache for GUI art: the fractal card art (assets/fractals/
// fractale_<ID>.png, ID = the champion's 3-digit zero-padded fullDeck
// champion_id, 001-102 -- see assets/about.md), shown when `legacy_fractal =
// true` in the [gui] config, plus the small icons the champion card face uses
// (sword, shield, cost hexagon, species emblems, order glyphs). Module-level
// state (one renderer, one cache) rather than a passed-around handle: the GUI
// has exactly one renderer and draws from one thread, and this keeps
// gui_card_draw()'s signature (7 call sites) unchanged.

#ifndef GUI_ART_H
#define GUI_ART_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL3/SDL.h>
#include "../../core/game_types.h" // ChampionSpecies, ChampionOrder

typedef enum
{ GUI_ICON_SWORD,
  GUI_ICON_SHIELD,
  GUI_ICON_COST_HEX,
  GUI_ICON_ORDER_FIRST,                                  // + ChampionOrder (5)
  GUI_ICON_SPECIES_FIRST = GUI_ICON_ORDER_FIRST + 5,     // + ChampionSpecies (15)
  GUI_ICON_COUNT = GUI_ICON_SPECIES_FIRST + 15
} GuiIcon;

// Enables the art cache, loading from `assets_dir` (the project's assets/
// folder). Call once after the renderer exists; until then (or if never
// called) every lookup returns NULL. Fractal art starts disabled.
void gui_art_init(SDL_Renderer* renderer, const char* assets_dir);

// Turns fractal card art on/off (the `legacy_fractal` setting).
void gui_art_set_fractal(bool enabled);

// Texture for champion `champion_id` (1-102), loaded on first request and
// cached. NULL if fractal art is disabled, the ID is out of range, or the
// file is missing/unreadable (a failure is remembered -- one stderr warning,
// not one per frame). The caller must not destroy the texture.
SDL_Texture* gui_art_fractal(uint8_t champion_id);

// Same contract for a card-face icon; NULL if unavailable.
SDL_Texture* gui_art_icon(GuiIcon icon);
SDL_Texture* gui_art_order_icon(ChampionOrder order);
SDL_Texture* gui_art_species_icon(ChampionSpecies species);

// Destroys all cached textures and disables art. Safe if never initialised.
// Call before the renderer is destroyed.
void gui_art_shutdown(void);

#endif // GUI_ART_H
