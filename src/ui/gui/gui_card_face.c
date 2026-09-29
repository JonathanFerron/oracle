// gui_card_face.c -- see gui_card_face.h.

#include <stdio.h>

#include "gui_card_face.h"
#include "gui_card.h"  // gui_draw_text()
#include "gui_art.h"
#include "../../core/champion_names.h"
#include "../shared/localization.h"

// Proportions measured off the printed card (cartes champions pg1 - blank).
#define CF_BORDER 0.046f   // border thickness, fraction of card width
#define CF_COL_W 0.208f    // left column width, fraction of the inner width
#define CF_NAME_H 0.110f   // name bar height, fraction of the inner height
#define CF_TEXT_PX 0.072f  // text size, fraction of the inner height
#define CF_FACE (SDL_Color){ 0xF5, 0xF5, 0xF0, 255 }
#define CF_INK (SDL_Color){ 0x1A, 0x1A, 0x1A, 255 }

static TTF_Font* g_font;

bool gui_card_face_init(const char* font_path)
{ g_font = TTF_OpenFont(font_path, 16.0f);
  if(!g_font)
    fprintf(stderr, "GUI: could not load card face font (%s): %s\n", font_path, SDL_GetError());
  return g_font != NULL;
} // gui_card_face_init

void gui_card_face_shutdown(void)
{ if(g_font)
    TTF_CloseFont(g_font);
  g_font = NULL;
} // gui_card_face_shutdown

typedef struct
{ SDL_FRect inner, art, name_bar;
  SDL_FRect cell[4]; // cost, attack, species, shield
  float text_px;
} FaceGeom;

static void face_geometry(SDL_FRect rect, FaceGeom* g)
{ static const float CELL_FRAC[4] = { 0.153f, 0.373f, 0.214f, 0.260f };
  float bt = rect.w * CF_BORDER;
  g->inner = (SDL_FRect)
  { rect.x + bt, rect.y + bt, rect.w - 2 * bt, rect.h - 2 * bt
  };
  float col_w = g->inner.w * CF_COL_W;
  float name_h = g->inner.h * CF_NAME_H;
  float col_h = g->inner.h - name_h;

  g->art = (SDL_FRect)
  { g->inner.x + col_w, g->inner.y, g->inner.w - col_w, col_h
  };
  g->name_bar = (SDL_FRect)
  { g->inner.x, g->inner.y + col_h, g->inner.w, name_h
  };
  float y = g->inner.y;
  for(int i = 0; i < 4; i++)
  { g->cell[i] = (SDL_FRect)
    { g->inner.x, y, col_w, col_h * CELL_FRAC[i]
    };
    y += col_h * CELL_FRAC[i];
  }
  g->text_px = g->inner.h * CF_TEXT_PX;
} // face_geometry

// Draws `text` centred on (cx, cy) at `px` size in the face font. Size and
// position are snapped to whole pixels: a fractional blit position makes the
// renderer bilinear-resample the glyph texture, which reads as fuzzy text.
static void draw_centered(SDL_Renderer* r, const char* text, float cx, float cy, float px)
{ int w = 0, h = 0;
  TTF_SetFontSize(g_font, SDL_roundf(px));
  TTF_GetStringSize(g_font, text, 0, &w, &h);
  gui_draw_text(r, g_font, text, SDL_roundf(cx - (float)w / 2.0f), SDL_roundf(cy - (float)h / 2.0f),
                CF_INK);
} // draw_centered

// Draws `tex` aspect-fitted and centred inside `box`.
static void draw_icon_fit(SDL_Renderer* r, SDL_Texture* tex, SDL_FRect box)
{ if(!tex)
    return;
  float scale = SDL_min(box.w / (float)tex->w, box.h / (float)tex->h);
  SDL_FRect dst = { 0, 0, (float)tex->w * scale, (float)tex->h * scale };
  dst.x = box.x + (box.w - dst.w) / 2.0f;
  dst.y = box.y + (box.h - dst.h) / 2.0f;
  SDL_RenderTexture(r, tex, NULL, &dst);
} // draw_icon_fit

// A sub-rectangle of `cell`, given as fractions of its own size.
static SDL_FRect sub(SDL_FRect cell, float fx, float fy, float fw, float fh)
{ return (SDL_FRect)
  { cell.x + cell.w * fx, cell.y + cell.h * fy, cell.w * fw, cell.h * fh
  };
} // sub

static void draw_dividers(SDL_Renderer* r, const FaceGeom* g, SDL_Color c)
{ float t = SDL_max(1.0f, g->inner.w * 0.008f);
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
  SDL_FRect v = { g->art.x - t / 2, g->inner.y, t, g->art.h };
  SDL_RenderFillRect(r, &v);
  for(int i = 1; i < 4; i++)
  { SDL_FRect h = { g->inner.x, g->cell[i].y - t / 2, g->cell[i].w, t };
    SDL_RenderFillRect(r, &h);
  }
  SDL_FRect nb = { g->inner.x, g->name_bar.y - t / 2, g->inner.w, t };
  SDL_RenderFillRect(r, &nb);
} // draw_dividers

static void draw_cost_cell(SDL_Renderer* r, const FaceGeom* g, const struct card* c)
{ SDL_FRect cell = g->cell[0];
  char buf[8];
  draw_icon_fit(r, gui_art_icon(GUI_ICON_COST_HEX), sub(cell, 0.06f, 0.10f, 0.88f, 0.80f));
  snprintf(buf, sizeof(buf), "%u", c->cost);
  draw_centered(r, buf, cell.x + cell.w / 2, cell.y + cell.h / 2, g->text_px * 1.15f);
} // draw_cost_cell

static void draw_attack_cell(SDL_Renderer* r, const FaceGeom* g, const struct card* c)
{ SDL_FRect cell = g->cell[1];
  float cx = cell.x + cell.w / 2;
  char buf[8];
  snprintf(buf, sizeof(buf), "d%u", c->defense_dice);
  draw_centered(r, buf, cx, cell.y + cell.h * 0.13f, g->text_px);
  draw_centered(r, "+", cx, cell.y + cell.h * 0.34f, g->text_px);
  snprintf(buf, sizeof(buf), "%u", c->attack_base);
  draw_centered(r, buf, cx, cell.y + cell.h * 0.53f, g->text_px);
  draw_icon_fit(r, gui_art_icon(GUI_ICON_SWORD), sub(cell, 0.10f, 0.64f, 0.80f, 0.34f));
} // draw_attack_cell

static void draw_species_cell(SDL_Renderer* r, const FaceGeom* g, const struct card* c)
{ SDL_FRect cell = g->cell[2];
  draw_icon_fit(r, gui_art_species_icon(c->species), sub(cell, 0.04f, 0.04f, 0.92f, 0.92f));
  draw_icon_fit(r, gui_art_order_icon(c->order), sub(cell, 0.66f, 0.66f, 0.30f, 0.30f));
} // draw_species_cell

static void draw_shield_cell(SDL_Renderer* r, const FaceGeom* g, const struct card* c)
{ SDL_FRect cell = g->cell[3];
  char buf[8];
  draw_icon_fit(r, gui_art_icon(GUI_ICON_SHIELD), sub(cell, 0.10f, 0.04f, 0.80f, 0.58f));
  snprintf(buf, sizeof(buf), "d%u", c->defense_dice);
  draw_centered(r, buf, cell.x + cell.w / 2, cell.y + cell.h * 0.80f, g->text_px);
} // draw_shield_cell

// Draws `text` centred on (cx, cy), shrunk from `px` if wider than `max_w`
// (the longest Spanish champion names, e.g. "Espuma de las Montanas", need it).
static void draw_centered_fit(SDL_Renderer* r, const char* text, float cx, float cy,
                              float px, float max_w)
{ int w = 0, h = 0;
  TTF_SetFontSize(g_font, SDL_roundf(px));
  TTF_GetStringSize(g_font, text, 0, &w, &h);
  if((float)w > max_w)
    px *= max_w / (float)w;
  draw_centered(r, text, cx, cy, px);
} // draw_centered_fit

static void draw_name_bar(SDL_Renderer* r, const FaceGeom* g, const char* name)
{ draw_centered_fit(r, name, g->name_bar.x + g->name_bar.w / 2, g->name_bar.y + g->name_bar.h / 2,
                    g->text_px * 1.1f, g->name_bar.w - 8.0f);
} // draw_name_bar

// Fills `rect` with `border`, the face colour inside it; returns the inner rect.
static SDL_FRect draw_frame(SDL_Renderer* r, SDL_FRect rect, SDL_Color border)
{ float bt = rect.w * CF_BORDER;
  SDL_FRect inner = { rect.x + bt, rect.y + bt, rect.w - 2 * bt, rect.h - 2 * bt };
  SDL_SetRenderDrawColor(r, border.r, border.g, border.b, 255);
  SDL_RenderFillRect(r, &rect);
  SDL_SetRenderDrawColor(r, CF_FACE.r, CF_FACE.g, CF_FACE.b, 255);
  SDL_RenderFillRect(r, &inner);
  return inner;
} // draw_frame

// Staged-card ring, `inset` px inside the face and `thick` px wide.
static void draw_ring(SDL_Renderer* r, SDL_FRect inner, SDL_Color c, float inset, float thick)
{ SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
  for(float i = inset; i < inset + thick; i++)
  { SDL_FRect ring = { inner.x + i, inner.y + i, inner.w - 2 * i, inner.h - 2 * i };
    SDL_RenderRect(r, &ring);
  }
} // draw_ring

// Draw/cash cards are cream inside their own coloured border, so a white ring
// would vanish: ring them in the border colour instead, set in from the border.
static void draw_highlight(SDL_Renderer* r, SDL_FRect inner, SDL_Color border)
{ draw_ring(r, inner, border, 3.0f, 3.0f);
} // draw_highlight

// A face-down "?" card as printed on the draw cards: white, thin ink outline.
static void draw_mystery_card(SDL_Renderer* r, SDL_FRect box, float px)
{ SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
  SDL_RenderFillRect(r, &box);
  SDL_SetRenderDrawColor(r, CF_INK.r, CF_INK.g, CF_INK.b, 255);
  SDL_RenderRect(r, &box);
  draw_centered(r, "?", box.x + box.w / 2, box.y + box.h / 2, px);
} // draw_mystery_card

// Cost hexagon in the top-left corner, as on the champion cards.
static void draw_corner_cost(SDL_Renderer* r, SDL_FRect inner, uint8_t cost, float px)
{ SDL_FRect box = sub(inner, 0.08f, 0.05f, 0.19f, 0.11f);
  char buf[8];
  draw_icon_fit(r, gui_art_icon(GUI_ICON_COST_HEX), box);
  snprintf(buf, sizeof(buf), "%u", cost);
  draw_centered(r, buf, box.x + box.w / 2, box.y + box.h / 2, px);
} // draw_corner_cost

// The "?" cards: two side by side, or three (two over one) for a draw 3.
static void draw_mystery_cards(SDL_Renderer* r, SDL_FRect inner, uint8_t n, float px)
{ static const float POS2[2][2] = { { 0.20f, 0.13f }, { 0.56f, 0.13f } };
  static const float POS3[3][2] = { { 0.13f, 0.13f }, { 0.63f, 0.13f }, { 0.38f, 0.30f } };
  for(uint8_t i = 0; i < n; i++)
  { const float* p = n >= 3 ? POS3[i] : POS2[i];
    draw_mystery_card(r, sub(inner, p[0], p[1], 0.25f, 0.23f), px * 1.8f);
  }
} // draw_mystery_cards

static void draw_recall_shields(SDL_Renderer* r, SDL_FRect inner, uint8_t n, float y)
{ SDL_Texture* shield = gui_art_icon(GUI_ICON_SHIELD);
  for(uint8_t i = 0; i < n; i++)
  { float x = n == 1 ? 0.39f : 0.22f + 0.34f * (float)i;
    draw_icon_fit(r, shield, sub(inner, x, y, 0.22f, 0.19f));
  }
} // draw_recall_shields

// "Draw N cards or / Recall / M champion(s)" (3 lines) under the "?" cards.
static void draw_draw_text(SDL_Renderer* r, SDL_FRect inner, const struct card* c, float px,
                           float y, ui_language_t lang)
{ char l1[48], l3[48];
  snprintf(l1, sizeof(l1), LOCALIZED_STRING_L(lang, "Draw %u cards or", "Pige %u cartes ou",
                                              "Roba %u cartas o"), c->draw_num);
  snprintf(l3, sizeof(l3), "%u %s", c->choose_num,
           c->choose_num == 1 ? LOCALIZED_STRING_L(lang, "champion", "champion", "campeon")
           : LOCALIZED_STRING_L(lang, "champions", "champions", "campeones"));
  float cx = inner.x + inner.w / 2, max_w = inner.w - 6.0f, step = inner.h * 0.078f;
  draw_centered_fit(r, l1, cx, inner.y + inner.h * y, px, max_w);
  draw_centered_fit(r, LOCALIZED_STRING_L(lang, "Recall", "Rappelle", "Recuerda"), cx,
                    inner.y + inner.h * y + step, px, max_w);
  draw_centered_fit(r, l3, cx, inner.y + inner.h * y + 2 * step, px, max_w);
} // draw_draw_text

bool gui_card_face_draw_draw(SDL_Renderer* r, SDL_FRect rect, const struct card* c,
                             SDL_Color border, bool highlighted, ui_language_t lang)
{ if(!g_font)
    return false;
  SDL_FRect inner = draw_frame(r, rect, border);
  float px = inner.h * CF_TEXT_PX;
  bool three = c->draw_num >= 3;
  draw_corner_cost(r, inner, c->cost, px);
  draw_mystery_cards(r, inner, c->draw_num, px);
  draw_draw_text(r, inner, c, px, three ? 0.575f : 0.47f, lang);
  draw_recall_shields(r, inner, c->choose_num, three ? 0.81f : 0.70f);
  if(highlighted)
    draw_highlight(r, inner, border);
  return true;
} // gui_card_face_draw_draw

// Six flat-top hexagons in a honeycomb (three columns; the middle one is a row
// higher) with the luna amount on the bottom-middle one, as on the printed cash
// card. Sized in pixels so the hexagons keep their own aspect ratio.
static void draw_hex_cluster(SDL_Renderer* r, SDL_FRect area, uint8_t amount, float px)
{ static const float HEX_ASPECT = 256.0f / 293.0f;
  static const float POS[6][2] = { { 0.0f, 0.5f }, { 0.0f, 1.5f }, { 0.75f, 0.0f },
    { 0.75f, 1.0f }, { 1.5f, 0.5f }, { 1.5f, 1.5f }
  };
  float hw = SDL_min(area.w / 2.5f, area.h / (2.5f * HEX_ASPECT)), hh = hw * HEX_ASPECT;
  float x0 = area.x + (area.w - 2.5f * hw) / 2.0f, y0 = area.y + (area.h - 2.5f * hh) / 2.0f;
  SDL_Texture* hex = gui_art_icon(GUI_ICON_COST_HEX);
  for(int i = 0; i < 6; i++)
  { SDL_FRect box = { x0 + POS[i][0] * hw, y0 + POS[i][1] * hh, hw, hh };
    draw_icon_fit(r, hex, box);
  }
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", amount);
  draw_centered(r, buf, x0 + 1.25f * hw, y0 + 1.5f * hh, px * 1.3f);
} // draw_hex_cluster

// Down arrow between the sword and the hexagons, drawn as thick lines.
static void draw_arrow(SDL_Renderer* r, SDL_FRect box)
{ float cx = box.x + box.w / 2, t = SDL_max(2.0f, box.w * 0.10f), tip = box.y + box.h;
  SDL_SetRenderDrawColor(r, CF_INK.r, CF_INK.g, CF_INK.b, 255);
  SDL_FRect shaft = { cx - t / 2, box.y, t, box.h * 0.85f };
  SDL_RenderFillRect(r, &shaft);
  for(float d = -t / 2; d <= t / 2; d += 1.0f)
  { SDL_RenderLine(r, cx - box.w * 0.30f + d, box.y + box.h * 0.55f, cx + d, tip);
    SDL_RenderLine(r, cx + box.w * 0.30f + d, box.y + box.h * 0.55f, cx + d, tip);
  }
} // draw_arrow

bool gui_card_face_draw_cash(SDL_Renderer* r, SDL_FRect rect, const struct card* c,
                             SDL_Color border, bool highlighted)
{ if(!g_font)
    return false;
  SDL_FRect inner = draw_frame(r, rect, border);
  draw_icon_fit(r, gui_art_icon(GUI_ICON_SHIELD), sub(inner, 0.08f, 0.05f, 0.20f, 0.18f));
  draw_icon_fit(r, gui_art_icon(GUI_ICON_SWORD), sub(inner, 0.25f, 0.04f, 0.55f, 0.32f));
  draw_arrow(r, sub(inner, 0.36f, 0.40f, 0.28f, 0.16f));
  draw_hex_cluster(r, sub(inner, 0.16f, 0.60f, 0.68f, 0.36f), c->exchange_cash,
                   inner.h * CF_TEXT_PX);
  if(highlighted)
    draw_highlight(r, inner, border);
  return true;
} // gui_card_face_draw_cash

bool gui_card_face_draw_champion(SDL_Renderer* r, SDL_FRect rect, const struct card* c,
                                 SDL_Color border, bool highlighted, ui_language_t lang)
{ if(!g_font)
    return false;

  FaceGeom g;
  face_geometry(rect, &g);
  SDL_SetRenderDrawColor(r, border.r, border.g, border.b, 255);
  SDL_RenderFillRect(r, &rect);
  SDL_SetRenderDrawColor(r, CF_FACE.r, CF_FACE.g, CF_FACE.b, 255);
  SDL_RenderFillRect(r, &g.inner);

  SDL_Texture* art = gui_art_fractal(c->champion_id);
  if(art)
    SDL_RenderTexture(r, art, NULL, &g.art);

  draw_dividers(r, &g, border);
  draw_cost_cell(r, &g, c);
  draw_attack_cell(r, &g, c);
  draw_species_cell(r, &g, c);
  draw_shield_cell(r, &g, c);

  draw_name_bar(r, &g, champion_name(c->champion_id, lang));

  if(highlighted)
    draw_ring(r, g.inner, (SDL_Color)
  { 255, 255, 255, 255
  }, 0.0f, 2.0f);
  return true;
} // gui_card_face_draw_champion
