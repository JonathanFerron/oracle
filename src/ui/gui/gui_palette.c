// gui_palette.c
// See gui_palette.h. RGB values are HSL(h,s,l) from
// "oracle thematic colours.txt" converted once (colorsys), then confirmed
// against a pixel sample of the rasterized printed-card sheets:
//   orange   HSL(26,95,40)  -> #C75905
//   red      HSL(350,80,40) -> #B81430
//   indigo   HSL(244,80,30) -> #170F8A
//   draw-3 purple HSL(274,53,42) -> #7332A4 (matches cartes pige3 et
//     monnaie.svg's card border fill exactly)
//   draw-2 green  HSL(162,35,35) -> #3A7866 (matches cartes pige2.svg)
//   cash/card-text grey HSL(0,0,30) -> #4C4C4C (matches the cash card's
//     border in cartes pige3 et monnaie.svg, #4d4d4d, off by rounding)

#include "gui_palette.h"

static SDL_Color champion_colour(ChampionColor c)
{ switch(c)
  { case COLOR_ORANGE:
      return (SDL_Color)
      { 0xC7, 0x59, 0x05, 255
      };
    case COLOR_RED:
      return (SDL_Color)
      { 0xB8, 0x14, 0x30, 255
      };
    case COLOR_INDIGO:
      return (SDL_Color)
      { 0x17, 0x0F, 0x8A, 255
      };
    default:
      return (SDL_Color)
      { 0x4C, 0x4C, 0x4C, 255
      };
  }
} // champion_colour

static SDL_Color draw_card_colour(uint8_t draw_num)
{ if(draw_num >= 3)
    return (SDL_Color)
  { 0x73, 0x32, 0xA4, 255
  };   // draw-3 purple
  return (SDL_Color)
  { 0x3A, 0x78, 0x66, 255
  };     // draw-2 green
} // draw_card_colour

SDL_Color gui_card_border_colour(const struct card* c)
{ switch(c->card_type)
  { case CHAMPION_CARD:
      return champion_colour(c->color);
    case DRAW_CARD:
      return draw_card_colour(c->draw_num);
    case CASH_CARD:
    default:
      return (SDL_Color)
      { 0x4C, 0x4C, 0x4C, 255
      };
  }
} // gui_card_border_colour
