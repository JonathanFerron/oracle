// gui_energy.c -- see gui_energy.h.

#include <stdio.h>

#include "gui_energy.h"
#include "gui_card.h" // gui_draw_text()

#define ENERGY_SAT 0.60f   // picked from a preview sheet (2026-10-19)
#define ENERGY_LIGHT 0.60f
#define ENERGY_HUE_FULL 300.0f // magenta, at 99
#define ENERGY_HUE_LOW 0.0f    // red, at 1 (via blue, green, yellow, orange)
#define ENERGY_TRACK (SDL_Color){ 0x1E, 0x5C, 0x61, 255 } // the empty part: a darker teal
#define ENERGY_TEXT (SDL_Color){ 0x26, 0x26, 0x26, 255 }  // dark grey
#define ENERGY_TEXT_GAP 6.0f
#define ENERGY_TEXT_PT 0.85f // number point size / bar height

// HSL -> RGB, hue in degrees (any value; wrapped to 0..360).
static SDL_Color hsl_to_rgb(float hue, float sat, float light)
{ float c = (1.0f - SDL_fabsf(2.0f * light - 1.0f)) * sat;
  float h = SDL_fmodf(hue, 360.0f) / 60.0f;
  float x = c * (1.0f - SDL_fabsf(SDL_fmodf(h, 2.0f) - 1.0f));
  float m = light - c / 2.0f;
  float rgb[3] = { 0, 0, 0 };
  int sector = (int)h;
  rgb[0] = (sector == 0 || sector == 5) ? c : (sector == 1 || sector == 4) ? x : 0;
  rgb[1] = (sector == 1 || sector == 2) ? c : (sector == 0 || sector == 3) ? x : 0;
  rgb[2] = (sector == 3 || sector == 4) ? c : (sector == 2 || sector == 5) ? x : 0;
  return (SDL_Color)
  { (Uint8)((rgb[0] + m) * 255.0f + 0.5f), (Uint8)((rgb[1] + m) * 255.0f + 0.5f),
    (Uint8)((rgb[2] + m) * 255.0f + 0.5f), 255
  };
} // hsl_to_rgb

SDL_Color gui_energy_colour(uint8_t energy, float sat, float light)
{ float t = (float)(SDL_clamp(energy, 1, GUI_ENERGY_FULL) - 1) / (GUI_ENERGY_FULL - 1);
  return hsl_to_rgb(ENERGY_HUE_LOW + t * (ENERGY_HUE_FULL - ENERGY_HUE_LOW), sat, light);
} // gui_energy_colour

void gui_energy_draw(SDL_Renderer* r, TTF_Font* font, SDL_FRect rect, uint8_t energy)
{ SDL_FRect bar = { rect.x, rect.y, GUI_ENERGY_BAR_W, rect.h };
  SDL_SetRenderDrawColor(r, ENERGY_TRACK.r, ENERGY_TRACK.g, ENERGY_TRACK.b, 255);
  SDL_RenderFillRect(r, &bar);

  bar.w = GUI_ENERGY_BAR_W * (float)SDL_min(energy, GUI_ENERGY_FULL) / GUI_ENERGY_FULL;
  SDL_Color c = gui_energy_colour(energy, ENERGY_SAT, ENERGY_LIGHT);
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
  SDL_RenderFillRect(r, &bar);

  char text[8];
  snprintf(text, sizeof(text), "%u", energy);
  float old_pt = TTF_GetFontSize(font); // the font is shared with the status bar
  TTF_SetFontSize(font, rect.h * ENERGY_TEXT_PT);
  int w = 0, h = 0;
  TTF_GetStringSize(font, text, 0, &w, &h);
  gui_draw_text(r, font, text, rect.x + GUI_ENERGY_BAR_W + ENERGY_TEXT_GAP,
                rect.y + (rect.h - (float)h) / 2.0f, ENERGY_TEXT);
  TTF_SetFontSize(font, old_pt);
} // gui_energy_draw
