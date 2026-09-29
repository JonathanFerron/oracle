// gui_dice.c -- see gui_dice.h.

#include <math.h>
#include <stdio.h>

#include "gui_dice.h"
#include "gui_card.h" // gui_draw_text()

#define GUI_PI 3.14159265358979f
#define GUI_DICE_MAX_SIDES 20
#define GUI_DICE_INK (SDL_Color){ 0x1A, 0x1A, 0x1A, 255 }
#define GUI_DICE_EMPTY_ALPHA 110 // an unrolled die: same shape, faded fill

int gui_dice_polygon_sides(uint8_t faces)
{ switch(faces)
  { case 4:
    case 6:
    case 8:
    case 12:
    case 20:
      return faces;
    default:
      return 6;
  }
} // gui_dice_polygon_sides

static SDL_Vertex vertex(float x, float y, SDL_FColor c)
{ return (SDL_Vertex)
  { .position = { x, y }, .color = c
  };
} // vertex

static SDL_FColor to_fcolor(SDL_Color c)
{ return (SDL_FColor)
  { c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f
  };
} // to_fcolor

// Outline between the `inner` and `outer` circumradii, drawn as quads so a
// translucent body under it isn't darkened by an overlapping outline.
static void ring_polygon(SDL_Renderer* r, float cx, float cy, float inner, float outer, int n,
                         SDL_Color c)
{ SDL_Vertex v[GUI_DICE_MAX_SIDES * 2];
  int idx[GUI_DICE_MAX_SIDES * 6];
  SDL_FColor fc = to_fcolor(c);
  float a0 = -GUI_PI / 2 + GUI_PI / (float)n;

  for(int i = 0; i < n; i++)
  { float a = a0 + 2.0f * GUI_PI * (float)i / (float)n;
    v[i * 2] = vertex(cx + inner * cosf(a), cy + inner * sinf(a), fc);
    v[i * 2 + 1] = vertex(cx + outer * cosf(a), cy + outer * sinf(a), fc);
    int j = (i + 1) % n;
    int q[6] = { i * 2, i * 2 + 1, j * 2 + 1, i * 2, j * 2 + 1, j * 2 };
    for(int k = 0; k < 6; k++)
      idx[i * 6 + k] = q[k];
  }
  SDL_RenderGeometry(r, NULL, v, n * 2, idx, n * 6);
} // ring_polygon

// Fills the regular n-gon (flat bottom edge) as a triangle fan.
static void fill_polygon(SDL_Renderer* r, float cx, float cy, float radius, int n, SDL_Color c)
{ SDL_Vertex v[GUI_DICE_MAX_SIDES + 1];
  int idx[GUI_DICE_MAX_SIDES * 3];
  SDL_FColor fc = to_fcolor(c);
  float a0 = -GUI_PI / 2 + GUI_PI / (float)n;

  v[0] = vertex(cx, cy, fc);
  for(int i = 0; i < n; i++)
  { float a = a0 + 2.0f * GUI_PI * (float)i / (float)n;
    v[i + 1] = vertex(cx + radius * cosf(a), cy + radius * sinf(a), fc);
    idx[i * 3] = 0;
    idx[i * 3 + 1] = i + 1;
    idx[i * 3 + 2] = (i + 1) % n + 1;
  }
  SDL_RenderGeometry(r, NULL, v, n + 1, idx, n * 3);
} // fill_polygon

static void draw_value(SDL_Renderer* r, TTF_Font* font, float cx, float cy, float radius, int value)
{ if(!font || value < 0)
    return;
  char buf[16];
  int w = 0, h = 0;
  snprintf(buf, sizeof(buf), "%d", value);
  TTF_SetFontSize(font, SDL_roundf(radius * 1.15f));
  TTF_GetStringSize(font, buf, 0, &w, &h);
  gui_draw_text(r, font, buf, SDL_roundf(cx - (float)w / 2.0f), SDL_roundf(cy - (float)h / 2.0f),
                (SDL_Color)
  { 255, 255, 255, 255
  });
} // draw_value

void gui_dice_draw(SDL_Renderer* r, TTF_Font* font, float cx, float cy, float radius,
                   uint8_t faces, SDL_Color fill, int value)
{ int n = gui_dice_polygon_sides(faces);
  // A square's circumradius looks small next to a 20-gon's, so widen it a bit.
  float rad = n == 4 ? radius * 1.12f : radius;
  float ring = SDL_max(1.5f, rad * 0.09f);

  SDL_Color edge = GUI_DICE_INK;
  SDL_Color body = fill;
  if(value < 0)
    body.a = GUI_DICE_EMPTY_ALPHA;
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  fill_polygon(r, cx, cy, rad - ring, n, body);
  ring_polygon(r, cx, cy, rad - ring, rad, n, edge);
  draw_value(r, font, cx, cy, rad, value);
} // gui_dice_draw
