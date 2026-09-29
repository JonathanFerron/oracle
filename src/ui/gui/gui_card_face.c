// gui_card_face.c -- see gui_card_face.h.

#include <stdio.h>
#include <ctype.h>

#include "gui_card_face.h"
#include "gui_card.h"  // gui_draw_text()
#include "gui_art.h"
#include "../../core/game_constants.h" // CHAMPION_SPECIES_NAMES

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

// Draws `text` centred on (cx, cy) at `px` size in the face font.
static void draw_centered(SDL_Renderer* r, const char* text, float cx, float cy, float px)
{ int w = 0, h = 0;
  TTF_SetFontSize(g_font, px);
  TTF_GetStringSize(g_font, text, 0, &w, &h);
  gui_draw_text(r, g_font, text, cx - (float)w / 2.0f, cy - (float)h / 2.0f, CF_INK);
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

// "HUMAN" -> "Human" (species names are stored upper-case).
static void title_case(const char* in, char* out, size_t n)
{ size_t i = 0;
  for(; in[i] && i + 1 < n; i++)
    out[i] = (char)(i ? tolower((unsigned char)in[i]) : toupper((unsigned char)in[i]));
  out[i] = '\0';
} // title_case

bool gui_card_face_draw_champion(SDL_Renderer* r, SDL_FRect rect, const struct card* c,
                                 SDL_Color border, bool highlighted)
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

  char name[24];
  title_case(CHAMPION_SPECIES_NAMES[c->species], name, sizeof(name));
  draw_centered(r, name, g.name_bar.x + g.name_bar.w / 2, g.name_bar.y + g.name_bar.h / 2,
                g.text_px * 1.1f);

  if(highlighted)
  { SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    for(float i = 0; i < 2.0f; i++)
    { SDL_FRect ring = { g.inner.x + i, g.inner.y + i, g.inner.w - 2 * i, g.inner.h - 2 * i };
      SDL_RenderRect(r, &ring);
    }
  }
  return true;
} // gui_card_face_draw_champion
