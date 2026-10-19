// gui_coins.c -- see gui_coins.h.

#include <stdio.h>

#include "gui_coins.h"
#include "gui_art.h"
#include "gui_card.h" // gui_draw_text()

#define COIN_TYPES 3
#define COIN_FLOWER_MAX 7
#define COIN_HEX_GAP 0.88f     // drawn radius / spacing radius: a thin gap between hexagons
#define COIN_SYMBOL_H 0.70f    // symbol height / hexagon height
#define COIN_LABEL_PT 0.85f    // "+n" point size / hexagon height
#define COIN_EMPTY_ALPHA 0.30f // an empty pile's faded centre hexagon and symbol
#define SQRT3 1.7320508f

typedef struct
{ SDL_Color fill;   // darker shade: the hexagons
  SDL_Color symbol; // lighter shade: the currency symbol and "+n"
  GuiIcon icon;
} CoinStyle;

// Luna keeps luna.png's own silver grey (172) between its two shades; Aureus
// is golden, Opal a blue with a hint of green.
static const CoinStyle COIN_STYLES[COIN_TYPES] =
{ { { 0x5E, 0x5E, 0x62, 255 }, { 0xDC, 0xDC, 0xDC, 255 }, GUI_ICON_COIN_LUNA },
  { { 0x96, 0x69, 0x0F, 255 }, { 0xFF, 0xD8, 0x62, 255 }, GUI_ICON_COIN_AUREUS },
  { { 0x13, 0x5A, 0x82, 255 }, { 0x7C, 0xD7, 0xE6, 255 }, GUI_ICON_COIN_OPAL },
};

// Flat-top hexagon centres, in units of the spacing radius: the centre, then
// clockwise from the top, the bottom one last (it only appears at 7 coins,
// exactly when it may need the "+n" label).
static const SDL_FPoint FLOWER[COIN_FLOWER_MAX] =
{ { 0.0f, 0.0f }, { 0.0f, -SQRT3 }, { 1.5f, -SQRT3 / 2 }, { 1.5f, SQRT3 / 2 },
  { -1.5f, SQRT3 / 2 }, { -1.5f, -SQRT3 / 2 }, { 0.0f, SQRT3 },
};

// One filled flat-top hexagon of circumradius `rad` centred on (cx, cy).
static void fill_hexagon(SDL_Renderer* r, float cx, float cy, float rad, SDL_Color c, float alpha)
{ SDL_Vertex v[7];
  SDL_FColor fc = { c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, alpha };
  v[0] = (SDL_Vertex)
  { { cx, cy }, fc, { 0, 0 }
  };
  for(int k = 0; k < 6; k++)
  { float a = (float)k * SDL_PI_F / 3.0f;
    v[k + 1] = (SDL_Vertex)
    { { cx + rad * SDL_cosf(a), cy + rad * SDL_sinf(a) }, fc, { 0, 0 }
    };
  }
  static const int IDX[18] = { 0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5, 0, 5, 6, 0, 6, 1 };
  SDL_RenderGeometry(r, NULL, v, 7, IDX, 18);
} // fill_hexagon

// The tinted currency symbol, fitted to the centre hexagon.
static void draw_symbol(SDL_Renderer* r, const CoinStyle* s, float cx, float cy, float hex_h,
                        float alpha)
{ SDL_Texture* tex = gui_art_icon(s->icon);
  float tw = 0, th = 0;
  if(!tex || !SDL_GetTextureSize(tex, &tw, &th) || th <= 0)
    return;
  float h = hex_h * COIN_SYMBOL_H, w = h * tw / th;
  SDL_FRect dst = { cx - w / 2, cy - h / 2, w, h };
  SDL_SetTextureColorMod(tex, s->symbol.r, s->symbol.g, s->symbol.b);
  SDL_SetTextureAlphaModFloat(tex, alpha);
  SDL_RenderTexture(r, tex, NULL, &dst);
  SDL_SetTextureAlphaModFloat(tex, 1.0f);
} // draw_symbol

// "+n" centred on (cx, cy).
static void draw_overflow(SDL_Renderer* r, TTF_Font* font, const CoinStyle* s, unsigned extra,
                          float cx, float cy, float hex_h)
{ char text[8];
  snprintf(text, sizeof(text), "+%u", extra);
  float old_pt = TTF_GetFontSize(font);
  TTF_SetFontSize(font, hex_h * COIN_LABEL_PT);
  int w = 0, h = 0;
  TTF_GetStringSize(font, text, 0, &w, &h);
  gui_draw_text(r, font, text, cx - (float)w / 2, cy - (float)h / 2, s->symbol);
  TTF_SetFontSize(font, old_pt);
} // draw_overflow

// One pile of `count` coins centred on (cx, cy), spacing radius `rad`.
static void draw_pile(SDL_Renderer* r, TTF_Font* font, const CoinStyle* s, unsigned count,
                      float cx, float cy, float rad)
{ float alpha = count == 0 ? COIN_EMPTY_ALPHA : 1.0f;
  unsigned shown = count == 0 ? 1 : SDL_min(count, COIN_FLOWER_MAX);
  for(unsigned i = 0; i < shown; i++)
    fill_hexagon(r, cx + FLOWER[i].x * rad, cy + FLOWER[i].y * rad, rad * COIN_HEX_GAP,
                 s->fill, alpha);

  float hex_h = SQRT3 * rad * COIN_HEX_GAP;
  draw_symbol(r, s, cx, cy, hex_h, alpha);
  if(count > COIN_FLOWER_MAX)
    draw_overflow(r, font, s, count - COIN_FLOWER_MAX, cx + FLOWER[6].x * rad,
                  cy + FLOWER[6].y * rad, hex_h);
} // draw_pile

void gui_coins_draw(SDL_Renderer* r, TTF_Font* font, SDL_FRect column, const CoinSet* coins)
{ const unsigned counts[COIN_TYPES] = { coins->luna, coins->aureus, coins->opal };
  float band = column.h / COIN_TYPES;
  // A flower is 5 radii wide and 3*sqrt(3) radii tall.
  float rad = SDL_min(column.w / 5.0f, band / (3.0f * SQRT3));

  SDL_BlendMode old_mode;
  SDL_GetRenderDrawBlendMode(r, &old_mode);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for(int t = 0; t < COIN_TYPES; t++)
    draw_pile(r, font, &COIN_STYLES[t], counts[t], column.x + column.w / 2,
              column.y + band * ((float)t + 0.5f), rad);
  SDL_SetRenderDrawBlendMode(r, old_mode);
} // gui_coins_draw
