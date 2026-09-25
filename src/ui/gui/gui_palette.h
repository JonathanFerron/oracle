// gui_palette.h
// Card border/background colours for the procedural (no-art-yet) card
// rendering -- see the plan file's "Asset folder structure" section.
// Converted once from the HSL values in "oracle thematic colours.txt"
// (Inkscape), cross-checked against the printed card sheets
// (cartes/*.svg -- cartes pige2.svg for draw-2 green, cartes pige3 et
// monnaie.svg for draw-3 purple and the cash card's grey) by rasterizing
// and sampling actual pixels, not just recalled from the HSL table.

#ifndef GUI_PALETTE_H
#define GUI_PALETTE_H

#include <SDL3/SDL.h>
#include "../../core/game_types.h"

// CHAMPION_CARD: by c->color. DRAW_CARD: by c->draw_num (2 -> green, 3 ->
// purple). CASH_CARD: grey (the same "card text grey" hue as the printed
// cash-exchange card's border).
SDL_Color gui_card_border_colour(const struct card* c);

#endif // GUI_PALETTE_H
