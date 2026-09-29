// gui_dice.h
// A die result as the number inside an n-gon for its die type, filled in the
// champion's colour (design settled 2026-09-28): d4 square, d6 hexagon, d8
// octagon, d12 12-gon, d20 20-gon. `value` < 0 draws the empty die (staged
// champion, roll not revealed yet).

#ifndef GUI_DICE_H
#define GUI_DICE_H

#include <stdint.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

// Number of sides of the polygon drawn for a die with `faces` faces (4, 6, 8,
// 12, 20; anything else falls back to 6).
int gui_dice_polygon_sides(uint8_t faces);

// Draws the die centred at (cx, cy) with circumradius `radius` px. `font` is
// used for the number (its size is set here); NULL draws no number.
void gui_dice_draw(SDL_Renderer* renderer, TTF_Font* font, float cx, float cy, float radius,
                   uint8_t faces, SDL_Color fill, int value);

#endif // GUI_DICE_H
