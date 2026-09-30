// gui_art.c -- see gui_art.h.

#include <stdio.h>
#include <string.h>
#include <SDL3_image/SDL_image.h>

#include "gui_art.h"

#define GUI_ART_FRACTAL_MAX_SIDE 160 // art area is ~130 px tall; keep 2x headroom
#define GUI_ART_ICON_MAX_SIDE 32     // card-face cells are ~25 px
#define GUI_ART_ICON_BOLD_PX 3       // species/order line weight added, in pixels of a 512 px source
#define GUI_ART_MAX_ID 102 // fullDeck has 102 champions, champion_id 1-102

static const char* const SPECIES_FILES[] =
{ "human", "elf", "dwarf", "orc", "goblin", "dragon", "hobbit", "centaur",
  "minotaur", "aven", "cyclops", "faun", "fairy", "koatl", "lycan"
};

static struct
{ SDL_Renderer* renderer;               // NULL = art disabled
  char dir[512];                        // the assets/ folder
  bool fractal;
  SDL_Texture* tex[GUI_ART_MAX_ID + 1]; // fractals, indexed by champion_id (0 unused)
  bool tried[GUI_ART_MAX_ID + 1];       // load attempted (success or failure)
  SDL_Texture* icon[GUI_ICON_COUNT];
  bool icon_tried[GUI_ICON_COUNT];
} g_art;

void gui_art_init(SDL_Renderer* renderer, const char* assets_dir)
{ gui_art_shutdown();
  g_art.renderer = renderer;
  snprintf(g_art.dir, sizeof(g_art.dir), "%s", assets_dir);
} // gui_art_init

void gui_art_set_fractal(bool enabled)
{ g_art.fractal = enabled;
} // gui_art_set_fractal

// Thickens black line art: every pixel takes the largest alpha within `radius`
// (square window, two passes) and goes black. Done on the full-size source,
// before the shrink, so the strokes stay crisp after it. Returns the surface to
// use (a converted RGBA32 copy; `surf` is freed), or `surf` unchanged on failure.
static SDL_Surface* embolden(SDL_Surface* surf, int radius)
{ SDL_Surface* src = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
  SDL_Surface* tmp = src ? SDL_CreateSurface(src->w, src->h, SDL_PIXELFORMAT_RGBA32) : NULL;
  if(!tmp)
  { SDL_DestroySurface(src);
    return surf;
  }
  SDL_DestroySurface(surf);

  for(int pass = 0; pass < 2; pass++)
  { SDL_Surface* from = pass == 0 ? src : tmp;
    SDL_Surface* to = pass == 0 ? tmp : src;
    for(int y = 0; y < src->h; y++)
    { uint8_t* out = (uint8_t*)to->pixels + (size_t)y * to->pitch;
      for(int x = 0; x < src->w; x++)
      { uint8_t best = 0;
        for(int d = -radius; d <= radius; d++)
        { int xx = pass == 0 ? x + d : x, yy = pass == 0 ? y : y + d;
          if(xx < 0 || xx >= src->w || yy < 0 || yy >= src->h)
            continue;
          const uint8_t* px = (const uint8_t*)from->pixels + (size_t)yy * from->pitch + 4 * xx;
          best = SDL_max(best, px[3]);
        }
        out[4 * x] = out[4 * x + 1] = out[4 * x + 2] = 0;
        out[4 * x + 3] = best;
      }
    }
  }
  SDL_DestroySurface(tmp);
  return src;
} // embolden

// Loads <assets>/<rel>, pre-shrunk by repeated halving until its longest side
// is under 2x `max_side`. The line-art icons are hundreds of pixels wide and
// get drawn ~20x smaller; a single linear-filtered minification aliases badly
// (no mipmaps in SDL_Renderer), while halving steps act as a box filter.
// A failure is warned about once by the caller's cache and remembered.
static SDL_Texture* load_texture(const char* rel, int max_side, int bold_px)
{ char path[640];
  snprintf(path, sizeof(path), "%s/%s", g_art.dir, rel);
  SDL_Surface* surf = IMG_Load(path);
  if(!surf)
  { fprintf(stderr, "GUI: could not load art (%s): %s\n", path, SDL_GetError());
    return NULL;
  }

  if(bold_px > 0)
    surf = embolden(surf, bold_px * SDL_max(surf->w, surf->h) / 512);
  while(SDL_max(surf->w, surf->h) >= 2 * max_side)
  { SDL_Surface* half = SDL_ScaleSurface(surf, surf->w / 2, surf->h / 2, SDL_SCALEMODE_LINEAR);
    if(!half)
      break;
    SDL_DestroySurface(surf);
    surf = half;
  }
  SDL_Texture* tex = SDL_CreateTextureFromSurface(g_art.renderer, surf);
  SDL_DestroySurface(surf);
  if(!tex)
    fprintf(stderr, "GUI: could not upload art (%s): %s\n", path, SDL_GetError());
  return tex;
} // load_texture

SDL_Texture* gui_art_fractal(uint8_t champion_id)
{ if(!g_art.renderer || !g_art.fractal || champion_id < 1 || champion_id > GUI_ART_MAX_ID)
    return NULL;
  if(!g_art.tried[champion_id])
  { char rel[64];
    snprintf(rel, sizeof(rel), "fractals/fractale_%03u.png", champion_id);
    g_art.tried[champion_id] = true;
    g_art.tex[champion_id] = load_texture(rel, GUI_ART_FRACTAL_MAX_SIDE, 0);
  }
  return g_art.tex[champion_id];
} // gui_art_fractal

static void icon_path(GuiIcon icon, char* rel, size_t n)
{ if(icon >= GUI_ICON_SPECIES_FIRST)
    snprintf(rel, n, "species/%s.png", SPECIES_FILES[icon - GUI_ICON_SPECIES_FIRST]);
  else if(icon >= GUI_ICON_ORDER_FIRST)
    snprintf(rel, n, "orders/order_%c.png", 'a' + (icon - GUI_ICON_ORDER_FIRST));
  else
    snprintf(rel, n, "icons/%s.png",
             icon == GUI_ICON_SWORD ? "sword" : icon == GUI_ICON_SHIELD ? "shield" : "cost_hex");
} // icon_path

SDL_Texture* gui_art_icon(GuiIcon icon)
{ if(!g_art.renderer || icon < 0 || icon >= GUI_ICON_COUNT)
    return NULL;
  if(!g_art.icon_tried[icon])
  { char rel[64];
    icon_path(icon, rel, sizeof(rel));
    g_art.icon_tried[icon] = true;
    g_art.icon[icon] = load_texture(rel, GUI_ART_ICON_MAX_SIDE,
                                    icon >= GUI_ICON_ORDER_FIRST ? GUI_ART_ICON_BOLD_PX : 0);
  }
  return g_art.icon[icon];
} // gui_art_icon

SDL_Texture* gui_art_order_icon(ChampionOrder order)
{ return order < 5 ? gui_art_icon(GUI_ICON_ORDER_FIRST + order) : NULL;
} // gui_art_order_icon

SDL_Texture* gui_art_species_icon(ChampionSpecies species)
{ return species < 15 ? gui_art_icon(GUI_ICON_SPECIES_FIRST + species) : NULL;
} // gui_art_species_icon

void gui_art_shutdown(void)
{ for(int i = 0; i <= GUI_ART_MAX_ID; i++)
  { if(g_art.tex[i])
      SDL_DestroyTexture(g_art.tex[i]);
  }
  for(int i = 0; i < GUI_ICON_COUNT; i++)
  { if(g_art.icon[i])
      SDL_DestroyTexture(g_art.icon[i]);
  }
  g_art = (typeof(g_art))
  { 0
  };
} // gui_art_shutdown
