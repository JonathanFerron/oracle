// gui_energy.h
// A seat's energy as a colour-coded bar with the number in dark grey to its
// right. The coloured part is energy/99 of the bar's width; its colour runs
// from magenta at 99 to red at 1 (HSL hue 300 down to 0, through blue,
// green, yellow and orange, at a fixed saturation and lightness). The number
// always sits at the same place, just past the full bar's right end.

#ifndef GUI_ENERGY_H
#define GUI_ENERGY_H

#include <stdint.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#define GUI_ENERGY_BAR_H 25.0f                    // nearly all the gap beside the coin flowers
#define GUI_ENERGY_BAR_W (7.0f * GUI_ENERGY_BAR_H) // 5-8x the height
#define GUI_ENERGY_FULL 99                        // the starting energy: a full bar

// The bar colour for `energy` (clamped to 1..99) at HSL saturation `sat` and
// lightness `light` (both 0..1). Public so a preview can compare shades;
// gui_energy_draw() uses its own fixed pair.
SDL_Color gui_energy_colour(uint8_t energy, float sat, float light);

// Draws the bar at the left of `rect` (gui_layout.c's GuiSeatLayout.energy)
// and the number after it in `font`, whose size is changed and restored.
void gui_energy_draw(SDL_Renderer* r, TTF_Font* font, SDL_FRect rect, uint8_t energy);

#endif // GUI_ENERGY_H
