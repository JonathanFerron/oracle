// gui_app.c
// SDL3 main-callbacks GUI application (MODE_STDA_GUI). Owns the window/
// renderer lifecycle and the GameContext/SessionClient session; the M1
// hello-window splash (table colour, Oracle logo, ModernRifgo wordmark)
// shows only until the session's first SessionUpdate arrives, then
// gui_render_frame() (gui_render.c) takes over every frame. See
// ideas/9 gui/gui_architecture_synthesis.md and the implementation plan
// (doc/oracle_roadmap.md's "SDL3 GUI" item) for what's still missing:
// staged input (gui_input.c) and the message log (gui_log.c).
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
#include "gui_render.h"
#include "gui_input.h"
#include "../../core/game_constants.h" // INITIAL_CASH_DEFAULT
#include "../../core/game_context.h"
#include "../../ai_strat/ai_strategy.h"
#include "../../roles/stda/stda_session.h"
#include "gui_config.h"
#include "gui_art.h"
#include "gui_card_face.h"
#include "gui_names.h"
#include "gui_layout.h"
#include "../../util/prng_seed.h" // generate_random_seed()

#define GUI_WINDOW_TITLE "Oracle: The Champions of Arcadia"
#define GUI_LOGO_PATH "assets/logo/oracle_logo.png"
#define GUI_ICON_PATH "assets/currency/luna.png"
#define GUI_TITLE_FONT_PATH "assets/fonts/Modern_Rifgo/ModernRifgoRegular-MAvdP.otf"
#define GUI_STATUS_FONT_PATH "assets/fonts/Comic_Neue/ComicNeue-Regular.ttf"
#define GUI_CARD_FONT_PATH "assets/fonts/Patrick_Hand/PatrickHand-Regular.ttf"
#define GUI_CONFIG_FILE "oracle_config.ini" // next to the executable
#define GUI_ASSETS_DIR "assets"
#define GUI_FACE_FONT_PATH "assets/fonts/Fredoka/static/Fredoka-Medium.ttf"
#define GUI_TITLE_FONT_PT 48.0f
#define GUI_STATUS_FONT_PT 16.0f
#define GUI_CARD_FONT_PT 16.0f
// Source Code Pro Regular (a static instance cut from the variable font, whose
// own default weight is ExtraLight). Also bundled but not wired in: Anonymous
// Pro, Fira Mono. System monospace fonts work too via [gui] log_font_path.
#define GUI_LOG_FONT_PATH "assets/fonts/Source_Code_Pro/SourceCodePro-Regular.ttf"
#define GUI_LOG_FONT_PT 16.0f
// Sized for a 1080p laptop (Lenovo W530, the smallest screen this is played on)
// with the log side panel open; resizable from there.
#define GUI_DEFAULT_WIN_W 1600
#define GUI_DEFAULT_WIN_H 1000 // two 56 px dice rows need >= ~960
#define GUI_LOGO_DISPLAY_SIZE 256.0f
#define GUI_LOGO_ALPHA 51 // 80% transparent (~20% opacity, 0.20 * 255)

// Table teal, HSL(181, 23%, 60%) -- the "table top in gui" entry in
// /mnt/nougatoctet/oracle/oracle thematic colours.txt.
#define GUI_TABLE_R 130
#define GUI_TABLE_G 176
#define GUI_TABLE_B 176

// Title wordmark colour: #216778, the same colour used in
// /mnt/nougatoctet/oracle/Text Logo.svg, at 50% opacity so it sits quietly
// on the table rather than competing with the logo.
#define GUI_TITLE_R 0x21
#define GUI_TITLE_G 0x67
#define GUI_TITLE_B 0x78
#define GUI_TITLE_A 128

typedef struct
{ SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Texture* logo;
  TTF_Font* font;         // M1 splash title (ModernRifgo)
  TTF_Font* status_font;  // status bar + seat energy/cash/name (ComicNeue-Regular)
  TTF_Font* log_font;     // message log (monospaced; [gui] log_font_path)
  TTF_Font* card_font;    // card text + deck/discard count badges (PatrickHand-Regular)
  TTF_TextEngine* text_engine;
  TTF_Text* title_text;

  GameContext* ctx;
  SessionClient* session;
  SessionUpdate update;  // latest polled state
  bool have_update;      // false until the first poll -- keeps the M1 hello-window
  // splash on screen during the brief startup window before it
  GuiInputState input;   // viewer's own staging state (gui_input.c)
  uint16_t input_turn;   // turn `input` was staged in (staging resets when it moves on)
  bool input_submitted;  // a command was sent; the next update starts staging fresh
  GuiLog log;             // message log (gui_log.c)
  bool log_open;          // message-log side panel shown (toggle: click/L)
  GuiNames names;         // "Jonathan (Player A)"-style labels (gui_names.h)
  GuiConfig config;       // [gui] section of oracle_config.ini (gui_config.h)
} GuiAppState;

// SDL_EnterAppMainCallbacks() offers no user-data slot besides argc/argv, so
// gui_app_run() stashes the caller's config here for gui_sdl_init() (the
// callback SDL actually invokes) to pick up. Both pointers are only ever
// read between gui_app_run()'s call and the matching gui_sdl_quit() return,
// so this is a handoff, not shared mutable state.
static config_t* g_boot_cfg;
static PlayerConfig* g_boot_pconfig;

// Builds a StrategySet from pconfig and starts the session -- the session
// thread copies the StrategySet by value (stda_session.h), so the local one
// is freed right after session_local_start() returns. `ctx` is NOT copied:
// the session thread owns it exclusively from this call onward, and
// gui_sdl_quit() destroys it only after session_client_close() has joined
// that thread.
static bool gui_start_session(GuiAppState* state, config_t* cfg, PlayerConfig* pconfig)
{ state->ctx = create_game_context(cfg);
  if(!state->ctx)
    return false;

  StrategySet* strategies = create_strategy_set();
  set_player_strategy_by_type(strategies, PLAYER_A, pconfig->ai_strategies[PLAYER_A]);
  set_player_strategy_by_type(strategies, PLAYER_B, pconfig->ai_strategies[PLAYER_B]);

  state->session = session_local_start(pconfig->player_types, strategies,
                                       INITIAL_CASH_DEFAULT, state->ctx);
  free_strategy_set(strategies);

  if(!state->session)
  { destroy_game_context(state->ctx);
    state->ctx = NULL;
    return false;
  }
  return true;
} // gui_start_session

// "New game" after game over: joins the finished session, then starts another
// with the same player types/names/AI choices (g_boot_pconfig) and a fresh
// random seed -- replaying a `-p` fixed seed would just repeat the same game.
// The seed shown in the status bar follows (g_boot_cfg->prng_seed).
static void gui_new_game(GuiAppState* state)
{ session_client_close(state->session);
  state->session = NULL;
  destroy_game_context(state->ctx);
  state->ctx = NULL;

  g_boot_cfg->prng_seed = generate_random_seed();
  if(!gui_start_session(state, g_boot_cfg, g_boot_pconfig))
  { fprintf(stderr, "GUI: could not start a new game\n");
    SDL_Event quit = { .type = SDL_EVENT_QUIT };
    SDL_PushEvent(&quit);
    return;
  }
  gui_log_init(&state->log);
  state->have_update = false;
  state->input_submitted = true; // staging starts fresh with the first update
} // gui_new_game

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
  gui_asset_path(path, sizeof(path), GUI_TITLE_FONT_PATH);
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

// Opens an in-game text font: the [gui] font_path override if set and
// loadable (absolute, or relative to the project root like every other
// asset), else the bundled `default_rel`. Non-fatal like the other assets:
// gui_draw_text() just draws nothing if font is NULL.
static void gui_resolve_path(char* out, size_t n, const char* configured)
{ if(configured[0] == '/')
    snprintf(out, n, "%s", configured);
  else
    gui_asset_path(out, n, configured);
} // gui_resolve_path

static TTF_Font* gui_open_text_font(const GuiAppState* state, const char* default_rel,
                                    float pt, const char* what)
{ char path[512];
  if(state->config.font_path[0])
  { gui_resolve_path(path, sizeof(path), state->config.font_path);
    TTF_Font* font = TTF_OpenFont(path, pt);
    if(font)
      return font;
    fprintf(stderr, "GUI: could not load configured font_path (%s): %s -- using default\n",
            path, SDL_GetError());
  }
  gui_asset_path(path, sizeof(path), default_rel);
  TTF_Font* font = TTF_OpenFont(path, pt);
  if(!font)
    fprintf(stderr, "GUI: could not load %s font (%s): %s\n", what, path, SDL_GetError());
  return font;
} // gui_open_text_font

// Status bar + per-seat energy/cash/name text -- ComicNeue-Regular by
// default, the closer Comic-Sans lookalike (Jonathan's call, 2026-09-25).
static bool gui_load_status_font(GuiAppState* state)
{ state->status_font = gui_open_text_font(state, GUI_STATUS_FONT_PATH, GUI_STATUS_FONT_PT,
                                          "status");
  return state->status_font != NULL;
} // gui_load_status_font

// Card text (species/cost/dice/base-attack, draw/recall/cash labels) +
// deck/discard count badges -- PatrickHand-Regular by default, the rougher
// handwriting-style Comic-Sans alternative (Jonathan's call, 2026-09-25).
static bool gui_load_card_font(GuiAppState* state)
{ state->card_font = gui_open_text_font(state, GUI_CARD_FONT_PATH, GUI_CARD_FONT_PT, "card");
  return state->card_font != NULL;
} // gui_load_card_font

// Message-log font: [gui] log_font_path if set and loadable, else the bundled
// monospace one. May stay NULL; the renderer then falls back to the status font.
static void gui_load_log_font(GuiAppState* state)
{ char path[512];
  if(state->config.log_font_path[0])
  { gui_resolve_path(path, sizeof(path), state->config.log_font_path);
    state->log_font = TTF_OpenFont(path, GUI_LOG_FONT_PT);
    if(state->log_font)
      return;
    fprintf(stderr, "GUI: could not load configured log_font_path (%s): %s -- using default\n",
            path, SDL_GetError());
  }
  gui_asset_path(path, sizeof(path), GUI_LOG_FONT_PATH);
  state->log_font = TTF_OpenFont(path, GUI_LOG_FONT_PT);
  if(!state->log_font)
    fprintf(stderr, "GUI: could not load log font (%s): %s\n", path, SDL_GetError());
} // gui_load_log_font

// Reads <executable dir>/oracle_config.ini's [gui] section (gui_config.h).
// A missing file just means all defaults. Also brings up the art cache and
// card-face font, which need the config's fractal setting.
static void gui_load_config(GuiAppState* state)
{ char path[512];
  const char* base = SDL_GetBasePath();
  snprintf(path, sizeof(path), "%s%s", base ? base : "./", GUI_CONFIG_FILE);
  gui_config_defaults(&state->config);
  gui_config_load(&state->config, path);

  gui_asset_path(path, sizeof(path), GUI_ASSETS_DIR);
  gui_art_init(state->renderer, path);
  gui_art_set_fractal(state->config.legacy_fractal);

  // Card-face font: the configured card_font_path if it loads, else the bundled one.
  if(state->config.card_font_path[0])
  { gui_resolve_path(path, sizeof(path), state->config.card_font_path);
    if(gui_card_face_init(path))
      return;
    fprintf(stderr, "GUI: using the default card font instead\n");
  }
  gui_asset_path(path, sizeof(path), GUI_FACE_FONT_PATH);
  gui_card_face_init(path);
} // gui_load_config

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

  // Taller than a plain "hello window" default -- leaves the message log
  // (gui_log.c, drawn in whatever's left between the two combat zones)
  // genuinely useful-sized out of the box instead of a sliver.
  if(!SDL_CreateWindowAndRenderer(GUI_WINDOW_TITLE, GUI_DEFAULT_WIN_W, GUI_DEFAULT_WIN_H, SDL_WINDOW_RESIZABLE,
                                  &state->window, &state->renderer))
  { fprintf(stderr, "SDL_CreateWindowAndRenderer failed: %s\n", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  // Caps gui_sdl_iterate() to the display's refresh rate. Without this,
  // SDL_RenderPresent() doesn't block at all on some platforms/compositors
  // -- the immediate-mode redraw (recreating every card/text texture from
  // scratch each frame) then runs completely unthrottled, pegging a full
  // CPU core even at idle (confirmed 2026-09-25: ~100% in this sandbox, no
  // compositor throttling; reported as ~7% on Jonathan's real desktop,
  // where the Wayland compositor happens to pace present calls anyway --
  // this makes that pacing explicit and portable instead of incidental).
  // Non-fatal like the other setup steps: a renderer that can't vsync still
  // works, just uncapped.
  if(!SDL_SetRenderVSync(state->renderer, 1))
    fprintf(stderr, "GUI: SDL_SetRenderVSync failed: %s\n", SDL_GetError());

  gui_load_config(state);
  gui_load_window_icon(state);
  gui_load_logo(state);
  gui_load_title_text(state);
  gui_load_status_font(state);
  gui_load_card_font(state);
  gui_load_log_font(state);
  gui_log_init(&state->log);
  state->log_open = true;
  gui_names_init(&state->names, g_boot_pconfig, g_boot_cfg->language);

  if(!gui_start_session(state, g_boot_cfg, g_boot_pconfig))
  { fprintf(stderr, "GUI: could not start game session\n");
    return SDL_APP_FAILURE;
  }

  return SDL_APP_CONTINUE;
} // gui_sdl_init

// Debug trace line, independent of on-screen rendering -- cheap, and useful
// on its own for following a game headless (e.g. under valgrind).
static void gui_trace_update(const SessionUpdate* u)
{ fprintf(stderr, "GUI: turn %u, pending %s (player %u)%s\n",
          u->view.turn, gui_decision_kind_debug_name(u->pending.kind),
          u->pending.player, u->rejected ? " [rejected]" : "");
} // gui_trace_update

// A defense with nothing to choose (no champion in hand, so the only legal
// move is to decline) needs no click: submit the decline ourselves. The
// combat still shows up in the log. Runs once per update: input_submitted
// blocks a repeat until the next update has been polled.
static void gui_auto_decline_defense(GuiAppState* state)
{ const SessionUpdate* u = &state->update;
  if(!state->have_update || state->input_submitted || u->rejected
     || u->pending.kind != DECISION_KIND_DEFENSE || u->pending.player != u->view.viewer
     || u->legal_count != 1 || u->legal[0].type != MOVE_PASS)
    return;

  SessionCommand cmd = { .type = CMD_SUBMIT };
  cmd.decision = (PlayerDecision)
  { .kind = DECISION_KIND_DEFENSE, .move = { .type = MOVE_PASS }
  };
  session_client_send(state->session, &cmd);
  state->input_submitted = true;
} // gui_auto_decline_defense

static SDL_AppResult gui_sdl_iterate(void* appstate)
{ GuiAppState* state = (GuiAppState*)appstate;

  if(session_client_poll(state->session, &state->update))
  { state->have_update = true;
    gui_trace_update(&state->update);
    gui_log_append_events(&state->log, &state->update, &state->names, g_boot_cfg->language);

    // Start staging fresh whenever this is a different decision than the one
    // the staged cards were picked for: kind/player changed, a rejected
    // submit, we just submitted something, or the turn moved on. Kind/player
    // alone is not enough -- when the opponent passes their attack the viewer
    // gets no defense decision, so two consecutive attack decisions look
    // identical and the previous turn's staged cards would carry over (which
    // silently blocks staging once 3 stale cards fill the selection).
    // Same pending decision republished for another reason (e.g. an AI move
    // elsewhere while it's still not our turn) leaves staging alone.
    const PendingDecision* pending = &state->update.pending;
    if(state->update.rejected || state->input_submitted
       || state->update.view.turn != state->input_turn
       || pending->kind != state->input.kind || pending->player != state->input.player)
    { gui_input_reset(&state->input, pending->kind, pending->player);
      state->input_turn = state->update.view.turn;
      state->input_submitted = false;
    }
  }

  gui_auto_decline_defense(state);

  SDL_SetRenderDrawColor(state->renderer, GUI_TABLE_R, GUI_TABLE_G, GUI_TABLE_B, 255);
  SDL_RenderClear(state->renderer);

  if(state->have_update)
  { GuiFonts fonts = { .title_font = state->font, .status_font = state->status_font,
                       .card_font = state->card_font, .log_font = state->log_font
                     };
    gui_render_frame(state->renderer, &fonts, &state->update, &state->input,
                     &state->log, &state->names, state->log_open, g_boot_cfg->prng_seed,
                     g_boot_cfg->language);
  }
  else
  { gui_draw_logo(state);
    gui_draw_title(state);
  }

  SDL_RenderPresent(state->renderer);
  return SDL_APP_CONTINUE;
} // gui_sdl_iterate

// Log panel toggle: the `L` key, or a click on the header button/corner tab.
static bool gui_log_toggle_hit(const GuiAppState* state, float x, float y)
{ int win_w, win_h;
  SDL_GetRenderOutputSize(state->renderer, &win_w, &win_h);
  GuiLayout layout;
  gui_layout_compute((float)win_w, (float)win_h, state->log_open, &layout);
  SDL_FPoint p = { x, y };
  return SDL_PointInRectFloat(&p, &layout.log_toggle);
} // gui_log_toggle_hit

static SDL_AppResult gui_sdl_event(void* appstate, SDL_Event* event)
{ GuiAppState* state = (GuiAppState*)appstate;
  if(event->type == SDL_EVENT_QUIT)
    return SDL_APP_SUCCESS;

  if(event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_L && !event->key.repeat)
    state->log_open = !state->log_open;

  if(event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT)
  { if(gui_log_toggle_hit(state, event->button.x, event->button.y))
      state->log_open = !state->log_open;
    else if(state->have_update)
    { int win_w, win_h;
      SDL_GetRenderOutputSize(state->renderer, &win_w, &win_h);

      if(state->update.pending.kind == DECISION_KIND_NONE)
      { GuiLayout layout;
        gui_layout_compute((float)win_w, (float)win_h, state->log_open, &layout);
        SDL_FRect btn = gui_layout_button_rect(layout.action_bar, 0, 1);
        SDL_FPoint p = { event->button.x, event->button.y };
        if(SDL_PointInRectFloat(&p, &btn))
          gui_new_game(state);
        return SDL_APP_CONTINUE;
      }

      SessionCommand cmd;
      if(gui_input_handle_click(&state->input, &state->update, event->button.x, event->button.y,
                                (float)win_w, (float)win_h, state->log_open, &cmd))
      { session_client_send(state->session, &cmd);
        state->input_submitted = true;
      }
    }
  }

  return SDL_APP_CONTINUE;
} // gui_sdl_event

static void gui_sdl_quit(void* appstate, SDL_AppResult result)
{ (void)result;
  GuiAppState* state = (GuiAppState*)appstate;
  if(!state)
    return;

  // Joins the session thread before ctx is destroyed -- session_client_close()
  // guarantees the thread is no longer touching ctx once it returns.
  if(state->session)
    session_client_close(state->session);
  if(state->ctx)
    destroy_game_context(state->ctx);

  if(state->title_text)
    TTF_DestroyText(state->title_text);
  if(state->text_engine)
    TTF_DestroyRendererTextEngine(state->text_engine);
  if(state->card_font)
    TTF_CloseFont(state->card_font);
  if(state->log_font)
    TTF_CloseFont(state->log_font);
  if(state->status_font)
    TTF_CloseFont(state->status_font);
  if(state->font)
    TTF_CloseFont(state->font);
  if(state->logo)
    SDL_DestroyTexture(state->logo);
  gui_card_face_shutdown();
  gui_art_shutdown();
  if(state->renderer)
    SDL_DestroyRenderer(state->renderer);
  if(state->window)
    SDL_DestroyWindow(state->window);
  SDL_free(state);

  TTF_Quit();
  SDL_Quit();
} // gui_sdl_quit

int gui_app_run(config_t* cfg, PlayerConfig* pconfig)
{ g_boot_cfg = cfg;
  g_boot_pconfig = pconfig;
  int result = SDL_EnterAppMainCallbacks(0, NULL, gui_sdl_init, gui_sdl_iterate,
                                         gui_sdl_event, gui_sdl_quit);
  return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
} // gui_app_run
