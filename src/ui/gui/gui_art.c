// gui_art.c -- see gui_art.h.

#include <stdio.h>
#include <SDL3_image/SDL_image.h>

#include "gui_art.h"

#define GUI_ART_MAX_ID 102 // fullDeck has 102 champions, champion_id 1-102

static struct
{ SDL_Renderer* renderer;               // NULL = art disabled
  char dir[512];
  SDL_Texture* tex[GUI_ART_MAX_ID + 1]; // indexed by champion_id (0 unused)
  bool tried[GUI_ART_MAX_ID + 1];       // load attempted (success or failure)
} g_art;

void gui_art_init(SDL_Renderer* renderer, const char* fractal_dir)
{ gui_art_shutdown();
  g_art.renderer = renderer;
  snprintf(g_art.dir, sizeof(g_art.dir), "%s", fractal_dir);
} // gui_art_init

SDL_Texture* gui_art_fractal(uint8_t champion_id)
{ if(!g_art.renderer || champion_id < 1 || champion_id > GUI_ART_MAX_ID)
    return NULL;
  if(g_art.tried[champion_id])
    return g_art.tex[champion_id];

  g_art.tried[champion_id] = true;
  char path[600];
  snprintf(path, sizeof(path), "%s/fractale_%03u.png", g_art.dir, champion_id);
  g_art.tex[champion_id] = IMG_LoadTexture(g_art.renderer, path);
  if(!g_art.tex[champion_id])
    fprintf(stderr, "GUI: could not load fractal art (%s): %s\n", path, SDL_GetError());
  return g_art.tex[champion_id];
} // gui_art_fractal

void gui_art_shutdown(void)
{ for(int i = 0; i <= GUI_ART_MAX_ID; i++)
  { if(g_art.tex[i])
      SDL_DestroyTexture(g_art.tex[i]);
  }
  g_art = (typeof(g_art))
  { 0
  };
} // gui_art_shutdown
