// gui_font_gallery.c -- see gui_font_gallery.h.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // strcasecmp

#include "gui_font_gallery.h"
#include "gui_card.h" // gui_draw_text()

#define GALLERY_WORD "Oracle"
#define GALLERY_BASE_PT 64.0f     // size every font is opened at, then scaled per cell
#define GALLERY_MARGIN 16.0f
#define GALLERY_LABEL_GAP 4.0f
#define GALLERY_WORD_W_FRAC 0.86f // "Oracle" may use this much of a cell's width...
#define GALLERY_WORD_H_FRAC 0.80f // ...and this much of the height left over by the label
#define GALLERY_CELL_ASPECT 1.6f  // preferred cell width / height when picking the column count

// ------------------------------------------------------------------ scan --

static int compare_fonts(const void* a, const void* b)
{ return strcasecmp(((const GuiGalleryFont*)a)->folder, ((const GuiGalleryFont*)b)->folder);
} // compare_fonts

// First file in `dir` matching `pattern` (case-insensitive), as a full path in `out`.
static bool glob_first(const char* dir, const char* pattern, char* out, size_t n)
{ int count = 0;
  char** hits = SDL_GlobDirectory(dir, pattern, SDL_GLOB_CASEINSENSITIVE, &count);
  bool found = hits && count > 0;
  if(found)
  { // SDL doesn't sort; take the alphabetically first so the pick is stable.
    const char* best = hits[0];
    for(int i = 1; i < count; i++)
      if(strcmp(hits[i], best) < 0)
        best = hits[i];
    snprintf(out, n, "%s/%s", dir, best);
  }
  SDL_free(hits);
  return found;
} // glob_first

// The file to show for one family folder (see gui_font_gallery_open()).
static bool pick_font_file(const char* family_dir, char* out, size_t n)
{ char static_dir[1024];
  snprintf(static_dir, sizeof(static_dir), "%s/static", family_dir);
  return glob_first(family_dir, "*-Regular.ttf", out, n)
         || glob_first(family_dir, "*-Regular.otf", out, n)
         || glob_first(static_dir, "*-Regular.ttf", out, n)
         || glob_first(family_dir, "*.ttf", out, n)
         || glob_first(family_dir, "*.otf", out, n);
} // pick_font_file

static void add_family(GuiFontGallery* g, const char* fonts_dir, const char* folder)
{ char dir[1024], file[1024];
  snprintf(dir, sizeof(dir), "%s/%s", fonts_dir, folder);
  SDL_PathInfo info;
  if(!SDL_GetPathInfo(dir, &info) || info.type != SDL_PATHTYPE_DIRECTORY
     || g->count >= GUI_FONT_GALLERY_MAX)
    return;

  GuiGalleryFont* f = &g->fonts[g->count++];
  memset(f, 0, sizeof(*f));
  snprintf(f->folder, sizeof(f->folder), "%s", folder);
  snprintf(f->name, sizeof(f->name), "%s", folder);
  for(char* c = f->name; *c; c++)
    if(*c == '_')
      *c = ' ';

  if(pick_font_file(dir, file, sizeof(file)))
  { f->font = TTF_OpenFont(file, GALLERY_BASE_PT);
    f->pt = GALLERY_BASE_PT;
  }
  if(!f->font)
    fprintf(stderr, "GUI font gallery: no usable font in %s\n", dir);
} // add_family

bool gui_font_gallery_open(GuiFontGallery* g, const char* fonts_dir)
{ memset(g, 0, sizeof(*g));
  int count = 0;
  char** entries = SDL_GlobDirectory(fonts_dir, "*", 0, &count);
  if(!entries)
  { fprintf(stderr, "GUI font gallery: cannot read %s: %s\n", fonts_dir, SDL_GetError());
    return false;
  }
  for(int i = 0; i < count; i++)
    if(!strchr(entries[i], '/')) // top-level entries only
      add_family(g, fonts_dir, entries[i]);
  SDL_free(entries);

  qsort(g->fonts, (size_t)g->count, sizeof(g->fonts[0]), compare_fonts);
  return true;
} // gui_font_gallery_open

void gui_font_gallery_close(GuiFontGallery* g)
{ for(int i = 0; i < g->count; i++)
    if(g->fonts[i].font)
      TTF_CloseFont(g->fonts[i].font);
  g->count = 0;
} // gui_font_gallery_close

// ------------------------------------------------------------------ draw --

// Columns that make cells closest to GALLERY_CELL_ASPECT for `n` cells.
static int pick_columns(int n, float w, float h)
{ int best = 1;
  float best_err = 1e9f;
  for(int cols = 1; cols <= n; cols++)
  { int rows = (n + cols - 1) / cols;
    float aspect = (w / cols) / (h / rows);
    float err = aspect > GALLERY_CELL_ASPECT ? aspect / GALLERY_CELL_ASPECT
                : GALLERY_CELL_ASPECT / aspect;
    if(err < best_err)
    { best_err = err;
      best = cols;
    }
  }
  return best;
} // pick_columns

// Sets f's size so "Oracle" fits within max_w x max_h (measured at the base size).
static void fit_font(GuiGalleryFont* f, float max_w, float max_h)
{ TTF_SetFontSize(f->font, GALLERY_BASE_PT);
  int w = 0, h = 0;
  TTF_GetStringSize(f->font, GALLERY_WORD, 0, &w, &h);
  float pt = GALLERY_BASE_PT;
  if(w > 0 && h > 0)
    pt = SDL_min(GALLERY_BASE_PT * max_w / (float)w, GALLERY_BASE_PT * max_h / (float)h);
  pt = SDL_floorf(pt);
  TTF_SetFontSize(f->font, pt);
  f->pt = pt;
} // fit_font

static void draw_centred(SDL_Renderer* r, TTF_Font* font, const char* text, float cx, float y,
                         SDL_Color c)
{ int w = 0, h = 0;
  TTF_GetStringSize(font, text, 0, &w, &h);
  gui_draw_text(r, font, text, SDL_roundf(cx - (float)w / 2.0f), SDL_roundf(y), c);
} // draw_centred

static void draw_cell(SDL_Renderer* r, GuiGalleryFont* f, TTF_Font* label_font, SDL_FRect cell,
                      int number, SDL_Color ink, bool is_current)
{ int lw = 0, lh = 0;
  TTF_GetStringSize(label_font, "Ag", 0, &lw, &lh);
  float word_h_max = (cell.h - (float)lh - GALLERY_LABEL_GAP) * GALLERY_WORD_H_FRAC;
  float cx = cell.x + cell.w / 2.0f;

  char label[96];
  snprintf(label, sizeof(label), "%d  %s%s", number, f->name, is_current ? "  (current)" : "");
  SDL_Color label_ink = { ink.r, ink.g, ink.b, 255 };

  if(!f->font)
  { draw_centred(r, label_font, label, cx, cell.y + cell.h / 2.0f, label_ink);
    return;
  }
  // A cell's fitted size only changes with the window; refit only then.
  float max_w = cell.w * GALLERY_WORD_W_FRAC;
  int w = 0, h = 0;
  TTF_GetStringSize(f->font, GALLERY_WORD, 0, &w, &h);
  if(w > max_w || h > word_h_max || (w < max_w * 0.97f && h < word_h_max * 0.97f))
    fit_font(f, max_w, word_h_max);
  TTF_GetStringSize(f->font, GALLERY_WORD, 0, &w, &h);

  // The word is centred in a fixed box and the label sits at a fixed line under
  // it, so labels line up across a row whatever each font's height.
  float box_h = (cell.h - (float)lh - GALLERY_LABEL_GAP) * 0.9f;
  float box_y = cell.y + (cell.h - box_h - GALLERY_LABEL_GAP - (float)lh) / 2.0f;
  draw_centred(r, f->font, GALLERY_WORD, cx, box_y + (box_h - (float)h) / 2.0f, ink);
  draw_centred(r, label_font, label, cx, box_y + box_h + GALLERY_LABEL_GAP, label_ink);
} // draw_cell

void gui_font_gallery_draw(SDL_Renderer* r, GuiFontGallery* g, TTF_Font* label_font,
                           float win_w, float win_h, SDL_Color ink, const char* current)
{ if(g->count == 0)
    return;
  float area_w = win_w - 2.0f * GALLERY_MARGIN, area_h = win_h - 2.0f * GALLERY_MARGIN;
  int cols = pick_columns(g->count, area_w, area_h);
  int rows = (g->count + cols - 1) / cols;
  float cw = area_w / cols, ch = area_h / rows;

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for(int i = 0; i < g->count; i++)
  { SDL_FRect cell = { GALLERY_MARGIN + (float)(i % cols) * cw, GALLERY_MARGIN + (float)(i / cols) * ch, cw, ch };
    bool is_current = current && strcmp(current, g->fonts[i].folder) == 0;
    draw_cell(r, &g->fonts[i], label_font, cell, i + 1, ink, is_current);
  }
} // gui_font_gallery_draw
