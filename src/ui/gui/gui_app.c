// gui_app.c
// SDL3 main-callbacks GUI application (MODE_STDA_GUI). This is the M1
// "hello window" step -- proves the toolchain end to end (window, table
// background colour, the Oracle logo, the ModernRifgo wordmark) before any
// game state is rendered. See ideas/9 gui/gui_architecture_synthesis.md and
// the implementation plan (doc/oracle_roadmap.md's "SDL3 GUI" item) for
// where this goes next: VisibleGameState rendering, the session thread,
// staged input.
//
// Callbacks are named gui_sdl_* rather than the SDL_App* names SDL_main.h
// also declares -- those are the well-known names some platforms' "magic"
// main-shims look for when an app opts into SDL_MAIN_USE_CALLBACKS; this
// project keeps its own main() (main.c, shared with every other mode) and
// hands off explicitly via SDL_EnterAppMainCallbacks() instead (per that
// function's own doc: "your SDL app's C-style main()... its name doesn't
// matter"), so there is no reason to reuse those names.

#include <stdio.h>
#include <stdlib.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "gui_app.h"

#define GUI_WINDOW_TITLE "Oracle: The Champions of Arcadia"
#define GUI_LOGO_PATH "assets/logo/oracle_logo.png"
#define GUI_ICON_PATH "assets/currency/luna.png"
#define GUI_FONT_PATH "assets/fonts/ModernRifgoRegular-MAvdP.otf"
#define GUI_TITLE_FONT_PT 48.0f
#define GUI_LOGO_DISPLAY_SIZE 256.0f
#define GUI_LOGO_ALPHA 51 // 80% transparent (~20% opacity, 0.20 * 255)

// Table teal, HSL(181, 23%, 60%) -- the "table top in gui" entry in
// ../oracle outside git/oracle thematic colours.txt.
#define GUI_TABLE_R 130
#define GUI_TABLE_G 176
#define GUI_TABLE_B 176

// Title wordmark colour: #216778, the same colour used in
// ../oracle outside git/Text Logo.svg, at 50% opacity so it sits quietly
// on the table rather than competing with the logo.
#define GUI_TITLE_R 0x21
#define GUI_TITLE_G 0x67
#define GUI_TITLE_B 0x78
#define GUI_TITLE_A 128

typedef struct
{ SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Texture* logo;
  TTF_Font* font;
  TTF_TextEngine* text_engine;
  TTF_Text* title_text;
} GuiAppState;

// Builds "<executable dir>/../<relative>" into out -- asset paths resolve
// relative to the executable, not the working directory, so bin/oracle-gui
// works from any launch location (gui_architecture_synthesis.md section 9.9
// makes the same call for the later Android port).
static void gui_asset_path(char* out, size_t out_size, const char* relative)
{ const char* base = SDL_GetBasePath();
  snprintf(out, out_size, "%s../%s", base ? base : "./", relative);
} // gui_asset_path

// Non-fatal: a missing logo/font just means gui_draw_logo()/gui_draw_title()
// skip drawing it, so a broken asset path doesn't stop the window opening.
static bool gui_load_logo(GuiAppState* state)
{ char path[512];
  gui_asset_path(path, sizeof(path), GUI_LOGO_PATH);
  state->logo = IMG_LoadTexture(state->renderer, path);
  if(!state->logo)
  { fprintf(stderr, "GUI: could not load logo (%s): %s\n", path, SDL_GetError());
    return false;
  }

  SDL_SetTextureBlendMode(state->logo, SDL_BLENDMODE_BLEND);
  SDL_SetTextureAlphaMod(state->logo, GUI_LOGO_ALPHA);
  return true;
} // gui_load_logo

// Sets the window/taskbar icon to the Luna currency symbol (replacing
// SDL's own default) -- non-fatal, same as the logo/font: a missing icon
// just leaves the platform default in place. SDL_SetWindowIcon() copies
// the surface, so it's freed right after the call.
static void gui_load_window_icon(GuiAppState* state)
{ char path[512];
  gui_asset_path(path, sizeof(path), GUI_ICON_PATH);
  SDL_Surface* icon = IMG_Load(path);
  if(!icon)
  { fprintf(stderr, "GUI: could not load window icon (%s): %s\n", path, SDL_GetError());
    return;
  }

  SDL_SetWindowIcon(state->window, icon);
  SDL_DestroySurface(icon);
} // gui_load_window_icon

static bool gui_load_title_text(GuiAppState* state)
{ char path[512];
  gui_asset_path(path, sizeof(path), GUI_FONT_PATH);
  state->font = TTF_OpenFont(path, GUI_TITLE_FONT_PT);
  if(!state->font)
  { fprintf(stderr, "GUI: could not load font (%s): %s\n", path, SDL_GetError());
    return false;
  }

  state->text_engine = TTF_CreateRendererTextEngine(state->renderer);
  if(!state->text_engine)
    return false;

  state->title_text = TTF_CreateText(state->text_engine, state->font, GUI_WINDOW_TITLE, 0);
  if(!state->title_text)
    return false;

  TTF_SetTextColor(state->title_text, GUI_TITLE_R, GUI_TITLE_G, GUI_TITLE_B, GUI_TITLE_A);
  return true;
} // gui_load_title_text

static void gui_draw_logo(GuiAppState* state)
{ if(!state->logo)
    return;

  int win_w, win_h;
  SDL_GetRenderOutputSize(state->renderer, &win_w, &win_h);

  SDL_FRect dst = { (win_w - GUI_LOGO_DISPLAY_SIZE) / 2.0f,
                    (win_h - GUI_LOGO_DISPLAY_SIZE) / 2.0f - 40.0f,
                    GUI_LOGO_DISPLAY_SIZE, GUI_LOGO_DISPLAY_SIZE
                  };
  SDL_RenderTexture(state->renderer, state->logo, NULL, &dst);
} // gui_draw_logo

static void gui_draw_title(GuiAppState* state)
{ if(!state->title_text)
    return;

  int win_w, win_h, text_w, text_h;
  SDL_GetRenderOutputSize(state->renderer, &win_w, &win_h);
  TTF_GetTextSize(state->title_text, &text_w, &text_h);

  float x = (win_w - text_w) / 2.0f;
  float y = (win_h / 2.0f) + (GUI_LOGO_DISPLAY_SIZE / 2.0f) - 20.0f;
  TTF_DrawRendererText(state->title_text, x, y);
} // gui_draw_title

static SDL_AppResult gui_sdl_init(void** appstate, int argc, char** argv)
{ (void)argc;
  (void)argv;

  if(!SDL_Init(SDL_INIT_VIDEO))
  { fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  if(!TTF_Init())
  { fprintf(stderr, "TTF_Init failed: %s\n", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  GuiAppState* state = SDL_calloc(1, sizeof(GuiAppState));
  if(!state)
    return SDL_APP_FAILURE;
  *appstate = state;

  if(!SDL_CreateWindowAndRenderer(GUI_WINDOW_TITLE, 1280, 800, SDL_WINDOW_RESIZABLE,
                                  &state->window, &state->renderer))
  { fprintf(stderr, "SDL_CreateWindowAndRenderer failed: %s\n", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  gui_load_window_icon(state);
  gui_load_logo(state);
  gui_load_title_text(state);
  return SDL_APP_CONTINUE;
} // gui_sdl_init

static SDL_AppResult gui_sdl_iterate(void* appstate)
{ GuiAppState* state = (GuiAppState*)appstate;

  SDL_SetRenderDrawColor(state->renderer, GUI_TABLE_R, GUI_TABLE_G, GUI_TABLE_B, 255);
  SDL_RenderClear(state->renderer);

  gui_draw_logo(state);
  gui_draw_title(state);

  SDL_RenderPresent(state->renderer);
  return SDL_APP_CONTINUE;
} // gui_sdl_iterate

static SDL_AppResult gui_sdl_event(void* appstate, SDL_Event* event)
{ (void)appstate;
  if(event->type == SDL_EVENT_QUIT)
    return SDL_APP_SUCCESS;
  return SDL_APP_CONTINUE;
} // gui_sdl_event

static void gui_sdl_quit(void* appstate, SDL_AppResult result)
{ (void)result;
  GuiAppState* state = (GuiAppState*)appstate;
  if(!state)
    return;

  if(state->title_text)
    TTF_DestroyText(state->title_text);
  if(state->text_engine)
    TTF_DestroyRendererTextEngine(state->text_engine);
  if(state->font)
    TTF_CloseFont(state->font);
  if(state->logo)
    SDL_DestroyTexture(state->logo);
  if(state->renderer)
    SDL_DestroyRenderer(state->renderer);
  if(state->window)
    SDL_DestroyWindow(state->window);
  SDL_free(state);

  TTF_Quit();
  SDL_Quit();
} // gui_sdl_quit

int gui_app_run(config_t* cfg)
{ (void)cfg; // not yet needed -- wired in once localization/player setup lands (M1 step 7)
  int result = SDL_EnterAppMainCallbacks(0, NULL, gui_sdl_init, gui_sdl_iterate,
                                         gui_sdl_event, gui_sdl_quit);
  return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
} // gui_app_run
