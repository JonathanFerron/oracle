// gui_font_gallery.h
// Font gallery "easter egg" screen: "Oracle" written in every font family
// bundled under assets/fonts/, numbered in alphabetical order with the family
// name underneath, on the table colour in the splash wordmark's colour -- built
// 2026-10-16 to pick a replacement for the wordmark font. Families are found at
// run time (one subfolder each), so a newly bundled font shows up on its own.
// Not reachable from the UI yet; the plan is a hidden hotkey later.

#ifndef GUI_FONT_GALLERY_H
#define GUI_FONT_GALLERY_H

#include <stdbool.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#define GUI_FONT_GALLERY_MAX 96

typedef struct
{ char folder[64];         // its assets/fonts/ subfolder, e.g. "Comic_Neue"
  char name[64];           // display name: folder name with '_' -> ' '
  TTF_Font* font;          // NULL if no usable file was found in the folder
  float pt;                // size the font is currently set to
  int number;              // 1-based position in the full alphabetical list (kept by _keep())
} GuiGalleryFont;

typedef struct
{ GuiGalleryFont fonts[GUI_FONT_GALLERY_MAX];
  int count;
} GuiFontGallery;

// Scans `fonts_dir` (assets/fonts): one entry per subfolder, sorted by name
// (case-insensitive). Each folder's font is its *-Regular.ttf/.otf, else
// static/*-Regular.*, else its first .ttf/.otf. Returns false if the folder
// can't be read.
bool gui_font_gallery_open(GuiFontGallery* g, const char* fonts_dir);

// Draws the gallery over the whole window: a grid sized to fit, "Oracle" in
// each font scaled to its cell, and "<n>  <name>" in `label_font` below it.
// `ink` is the wordmark colour (alpha included); the label uses it opaque.
// `current` (may be NULL) is the folder name of the font in use, marked as such.
void gui_font_gallery_draw(SDL_Renderer* renderer, GuiFontGallery* g, TTF_Font* label_font,
                           float win_w, float win_h, SDL_Color ink, const char* current);

// Keeps only the fonts whose number (as shown in the full gallery) is in
// `numbers`, closing the rest; the kept ones keep their original numbers, so a
// shortlist render can be compared against the full one.
void gui_font_gallery_keep(GuiFontGallery* g, const int* numbers, int n);

void gui_font_gallery_close(GuiFontGallery* g);

#endif // GUI_FONT_GALLERY_H
