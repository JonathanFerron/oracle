// gui_coins.h
// A player's coins (VisibleGameState.coins, see core/coin_wallet.h) drawn as
// three piles stacked in the seat's coins column: Lunas on top, Aureus in the
// middle, Opals at the bottom. Each pile is a flower of up to 7 flat-top
// hexagons, one per coin: the centre one (carrying the currency symbol) first,
// then clockwise from the top, the bottom one last. Past 7 coins the bottom
// hexagon reads "+1", "+2", ... An empty pile shows its centre hexagon and
// symbol faded, so the three piles never move.

#ifndef GUI_COINS_H
#define GUI_COINS_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "../../core/coin_wallet.h" // CoinSet

// Draws `coins` in `column` (gui_layout.c's GuiSeatLayout.coins). `font`
// labels the "+n" overflow; its size is changed and restored.
void gui_coins_draw(SDL_Renderer* r, TTF_Font* font, SDL_FRect column, const CoinSet* coins);

#endif // GUI_COINS_H
