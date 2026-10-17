// gui_app.c
// SDL3 main-callbacks GUI application (MODE_STDA_GUI). Owns the window/
// renderer lifecycle and the GameContext/SessionClient session; the M1
// hello-window splash (table colour, Oracle logo, ModernRifgo wordmark)
// opens with a New game button that leads to the player setup screen
// (gui_setup.c); once a game's first SessionUpdate arrives,
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
#include "gui_reveal.h"
#include "gui_setup.h"
#include "gui_card.h" // gui_draw_text()
#include "../shared/localization.h"
#include "../shared/player_config.h"
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
#define GUI_LOG_FONT_PT 13.0f
// Opening size: 1080p (Lenovo W530, the smallest screen this is played on),
// shrunk to the display's usable area when it doesn't fit there -- see
// gui_create_window(). On the 1440p desktop monitors it opens at exactly this.
#define GUI_DEFAULT_WIN_W 1920
#define GUI_DEFAULT_WIN_H 1080 // 3-row discard grids need >= ~1001
// Smallest the window can be resized to. Width is generous since hands and
// combat rows overlap cards sideways as needed; height is not, since every
// row (cards, dice, info lines) is fixed-height -- the two combat zones start
// colliding below ~940 (gui_layout.c). Fits inside the W530's usable area.
#define GUI_MIN_WIN_W 1280
#define GUI_MIN_WIN_H 960
// Room the window manager's title bar needs above the window (KWin/Breeze is
// ~30 px). Usable display bounds don't include it -- and with an auto-hidden
// panel they're the whole screen -- so a 1080p screen must count as too short
// for a 1080 px window.
#define GUI_TITLE_BAR_ALLOWANCE 40
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

// Splash "New game" button, centred under the wordmark.
#define GUI_SPLASH_BUTTON_W 200.0f
#define GUI_SPLASH_BUTTON_H 44.0f
#define GUI_SPLASH_BUTTON_BG (SDL_Color){ 0x2E, 0x6B, 0x5E, 255 } // gui_render.c's button green

// Which screen the window shows: the splash (logo + New game) at launch, the
// player setup panel (gui_setup.c), or the game itself.
typedef enum
{ GUI_SCREEN_SPLASH = 0,
  GUI_SCREEN_SETUP,
  GUI_SCREEN_GAME
} GuiScreen;

typedef struct
{ GuiScreen screen;
  GuiSetupState setup;   // setup panel state while screen == GUI_SCREEN_SETUP
  PlayerConfig pconfig;  // current players: the defaults until the first Start,
  // then the last setup's choices (Rematch replays them, Setup... edits them)
  uint32_t games_started; // the first game uses the configured seed, later ones a fresh one
  SDL_Window* window;
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
  GuiReveal reveal;       // paces combats + releases their log lines (gui_reveal.c)
  GuiLog log;             // message log (gui_log.c)
  bool log_open;          // message-log side panel shown (toggle: click/L)
  GuiNames names;         // "Jonathan (Player A)"-style labels (gui_names.h)
  GuiConfig config;       // [gui] section of oracle_config.ini (gui_config.h)
} GuiAppState;

// SDL_EnterAppMainCallbacks() offers no user-data slot besides argc/argv, so
// gui_app_run() stashes the caller's config here for gui_sdl_init() (the
// callback SDL actually invokes) to pick up. Only ever read between
// gui_app_run()'s call and the matching gui_sdl_quit() return, so this is a
// handoff, not shared mutable state.
static config_t* g_boot_cfg;

// Builds a StrategySet from pconfig and starts the session -- the session
// thread copies the StrategySet by value (stda_session.h), so the local one
// is freed right after session_local_start() returns. `ctx` is NOT copied:
// the session thread owns it exclusively from this call onward, and
// gui_sdl_quit() destroys it only after session_client_close() has joined
// that thread. The seat assignment (Direct/Inverted/Random) is applied to a
// per-game copy of `pconfig`, never to `pconfig` itself, so every game starts
// from the same as-entered config and Random re-rolls each time; the seat
// labels are rebuilt from that copy so they follow any swap.
static bool gui_start_session(GuiAppState* state, config_t* cfg, PlayerConfig* pconfig)
{ state->ctx = create_game_context(cfg);
  if(!state->ctx)
    return false;

  PlayerConfig game_pconfig = *pconfig;
  apply_player_assignment(&game_pconfig, cfg, state->ctx);
  gui_names_init(&state->names, &game_pconfig, cfg->language);

  StrategySet* strategies = create_strategy_set();
  set_player_strategy_by_type(strategies, PLAYER_A, game_pconfig.ai_strategies[PLAYER_A]);
  set_player_strategy_by_type(strategies, PLAYER_B, game_pconfig.ai_strategies[PLAYER_B]);

  state->session = session_local_start(game_pconfig.player_types, strategies,
                                       INITIAL_CASH_DEFAULT, state->ctx);
  free_strategy_set(strategies);

  if(!state->session)
  { destroy_game_context(state->ctx);
    state->ctx = NULL;
    return false;
  }
  return true;
} // gui_start_session

// Joins the current session (if any) and frees its GameContext.
static void gui_end_session(GuiAppState* state)
{ if(state->session)
    session_client_close(state->session);
  state->session = NULL;
  if(state->ctx)
    destroy_game_context(state->ctx);
  state->ctx = NULL;
} // gui_end_session

// Starts a game with state->pconfig, ending any previous one. The first game
// uses the configured seed (so `-p` still reproduces it); every later one gets
// a fresh random seed -- replaying a fixed seed would just repeat the same
// game. The seed shown in the status bar follows (g_boot_cfg->prng_seed).
static void gui_begin_game(GuiAppState* state)
{ gui_end_session(state);
  if(state->games_started > 0)
    g_boot_cfg->prng_seed = generate_random_seed();

  if(!gui_start_session(state, g_boot_cfg, &state->pconfig))
  { fprintf(stderr, "GUI: could not start a new game\n");
    SDL_Event quit = { .type = SDL_EVENT_QUIT };
    SDL_PushEvent(&quit);
    return;
  }
  state->games_started++;
  gui_log_init(&state->log);
  gui_reveal_init(&state->reveal, state->config.combat_delay_seconds);
  state->have_update = false;
  state->input_submitted = true; // staging starts fresh with the first update
  state->screen = GUI_SCREEN_GAME;
} // gui_begin_game

// [gui] default_agent as an agent. An unknown or unplayable code falls back
// to GUI_DEFAULT_AGENT_DEFAULT (gui_config.h) with a warning.
static AIStrategyType gui_default_agent(const GuiConfig* config)
{ AIStrategyType t = parse_ai_strategy_shorthand(config->default_agent);
  if(t != AI_STRATEGY_COUNT && ai_strategy_is_implemented(t))
    return t;

  fprintf(stderr, "GUI config: default_agent '%s' is not a playable agent; using %s\n",
          config->default_agent, GUI_DEFAULT_AGENT_DEFAULT);
  return parse_ai_strategy_shorthand(GUI_DEFAULT_AGENT_DEFAULT);
} // gui_default_agent

// The setup screen's first defaults: init_player_config(), the [gui]
// default_agent on both seats, then -Aa/-Ab on top for this session.
static void gui_default_pconfig(const GuiAppState* state, PlayerConfig* out)
{ init_player_config(out);
  AIStrategyType agent = gui_default_agent(&state->config);
  out->ai_strategies[PLAYER_A] = agent;
  out->ai_strategies[PLAYER_B] = agent;
  gui_setup_apply_cmdline_agents(out, g_boot_cfg->agent, g_boot_cfg->agent_set);
} // gui_default_pconfig

static void gui_open_setup(GuiAppState* state)
{ gui_setup_open(&state->setup, &state->pconfig);
  state->screen = GUI_SCREEN_SETUP;
} // gui_open_setup

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

static SDL_FRect gui_splash_button_rect(const GuiAppState* state)
{ int win_w, win_h;
  SDL_GetRenderOutputSize(state->renderer, &win_w, &win_h);
  return (SDL_FRect)
  { (win_w - GUI_SPLASH_BUTTON_W) / 2.0f, win_h / 2.0f + GUI_LOGO_DISPLAY_SIZE / 2.0f + 60.0f,
    GUI_SPLASH_BUTTON_W, GUI_SPLASH_BUTTON_H
  };
} // gui_splash_button_rect

static void gui_draw_splash_button(GuiAppState* state)
{ SDL_FRect b = gui_splash_button_rect(state);
  SDL_Color bg = GUI_SPLASH_BUTTON_BG;
  SDL_SetRenderDrawColor(state->renderer, bg.r, bg.g, bg.b, bg.a);
  SDL_RenderFillRect(state->renderer, &b);
  SDL_SetRenderDrawColor(state->renderer, 255, 255, 255, 220);
  SDL_RenderRect(state->renderer, &b);

  const char* label = LOCALIZED_STRING_L(g_boot_cfg->language, "New game", "Nouvelle partie",
                                         "Nueva partida");
  int tw = 0, th = 0;
  TTF_GetStringSize(state->status_font, label, 0, &tw, &th);
  gui_draw_text(state->renderer, state->status_font, label, SDL_roundf(b.x + (b.w - tw) / 2.0f),
                SDL_roundf(b.y + (b.h - th) / 2.0f), (SDL_Color)
  { 255, 255, 255, 255
  });
} // gui_draw_splash_button

// Opens the window at GUI_DEFAULT_WIN_W x GUI_DEFAULT_WIN_H, or -- when that
// plus its title bar doesn't fit the primary display's usable area (the
// screen minus any non-auto-hidden panels) -- maximized, so the window
// manager fits it exactly, title bar included (clamping the size alone would
// still overflow by the title bar). The un-maximized size is then the usable
// area itself. Also sets the GUI_MIN_WIN_W x GUI_MIN_WIN_H resize floor.
static bool gui_create_window(GuiAppState* state)
{ int w = GUI_DEFAULT_WIN_W, h = GUI_DEFAULT_WIN_H;
  SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
  SDL_Rect usable;
  if(SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable)
     && (usable.w < w || usable.h < h + GUI_TITLE_BAR_ALLOWANCE))
  { w = SDL_min(w, usable.w);
    h = SDL_min(h, usable.h);
    flags |= SDL_WINDOW_MAXIMIZED;
  }

  if(!SDL_CreateWindowAndRenderer(GUI_WINDOW_TITLE, w, h, flags, &state->window, &state->renderer))
  { fprintf(stderr, "SDL_CreateWindowAndRenderer failed: %s\n", SDL_GetError());
    return false;
  }
  if(!SDL_SetWindowMinimumSize(state->window, GUI_MIN_WIN_W, GUI_MIN_WIN_H))
    fprintf(stderr, "GUI: SDL_SetWindowMinimumSize failed: %s\n", SDL_GetError());
  return true;
} // gui_create_window

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

  if(!gui_create_window(state))
    return SDL_APP_FAILURE;

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
  gui_reveal_init(&state->reveal, state->config.combat_delay_seconds);
  gui_load_window_icon(state);
  gui_load_logo(state);
  gui_load_title_text(state);
  gui_load_status_font(state);
  gui_load_card_font(state);
  gui_load_log_font(state);
  gui_log_init(&state->log);
  state->log_open = true;

  // No game yet: the splash's New game button opens the setup screen.
  gui_default_pconfig(state, &state->pconfig);
  state->screen = GUI_SCREEN_SPLASH;
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

// Moves every event gui_reveal.c now allows (everything but a combat still
// being revealed) into the message log.
static void gui_release_log_events(GuiAppState* state)
{ GameEvent ev;
  while(gui_reveal_pop(&state->reveal, (int64_t)SDL_GetTicks(), &ev))
    gui_log_append_event(&state->log, &ev, &state->update.view, &state->names,
                         g_boot_cfg->language);
} // gui_release_log_events

// Polls the session and moves revealed events into the log (game screen only).
static void gui_poll_session(GuiAppState* state)
{ if(state->screen != GUI_SCREEN_GAME || !state->session)
    return;

  if(session_client_poll(state->session, &state->update))
  { state->have_update = true;
    gui_trace_update(&state->update);
    gui_reveal_push(&state->reveal, &state->update.events);

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

  gui_release_log_events(state);
  gui_auto_decline_defense(state);
} // gui_poll_session

static SDL_AppResult gui_sdl_iterate(void* appstate)
{ GuiAppState* state = (GuiAppState*)appstate;
  gui_poll_session(state);

  SDL_SetRenderDrawColor(state->renderer, GUI_TABLE_R, GUI_TABLE_G, GUI_TABLE_B, 255);
  SDL_RenderClear(state->renderer);

  if(state->screen == GUI_SCREEN_SETUP)
  { int win_w, win_h;
    SDL_GetRenderOutputSize(state->renderer, &win_w, &win_h);
    gui_setup_draw(state->renderer, state->status_font, &state->setup, (float)win_w,
                   (float)win_h, g_boot_cfg->language);
  }
  else if(state->screen == GUI_SCREEN_GAME && state->have_update)
  { GuiFonts fonts = { .title_font = state->font, .status_font = state->status_font,
                       .card_font = state->card_font, .log_font = state->log_font
                     };
    gui_render_frame(state->renderer, &fonts, &state->update, &state->input,
                     &state->log, &state->names, state->log_open, g_boot_cfg->prng_seed,
                     &state->reveal, g_boot_cfg->language);
  }
  else
  { gui_draw_logo(state);
    gui_draw_title(state);
    if(state->screen == GUI_SCREEN_SPLASH)
      gui_draw_splash_button(state);
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

// While a revealed combat waits to be taken in, the only live control is the
// action bar's "Continue" button.
static void gui_continue_click(GuiAppState* state, float x, float y)
{ int win_w, win_h;
  SDL_GetRenderOutputSize(state->renderer, &win_w, &win_h);
  GuiLayout layout;
  gui_layout_compute((float)win_w, (float)win_h, state->log_open, &layout);
  SDL_FRect btn = gui_layout_button_rect(layout.buttons, 0, 1);
  SDL_FPoint p = { x, y };
  if(SDL_PointInRectFloat(&p, &btn))
    gui_reveal_ack(&state->reveal);
} // gui_continue_click

// Game over: the action bar's Rematch (same players, fresh seed) and Setup...
// (back to the setup screen, previous choices preselected) buttons --
// drawn in this order by gui_render.c's draw_action_bar().
static void gui_game_over_click(GuiAppState* state, float x, float y, float win_w, float win_h)
{ GuiLayout layout;
  gui_layout_compute(win_w, win_h, state->log_open, &layout);
  SDL_FPoint p = { x, y };
  SDL_FRect rematch = gui_layout_button_rect(layout.buttons, 0, 2);
  SDL_FRect setup = gui_layout_button_rect(layout.buttons, 1, 2);
  if(SDL_PointInRectFloat(&p, &rematch))
    gui_begin_game(state);
  else if(SDL_PointInRectFloat(&p, &setup))
    gui_open_setup(state);
} // gui_game_over_click

static void gui_game_click(GuiAppState* state, float x, float y)
{ if(gui_log_toggle_hit(state, x, y))
    state->log_open = !state->log_open;
  else if(gui_reveal_busy(&state->reveal))
    gui_reveal_skip(&state->reveal, (int64_t)SDL_GetTicks()); // input is locked meanwhile
  else if(gui_reveal_needs_ack(&state->reveal))
    gui_continue_click(state, x, y);
  else if(state->have_update)
  { int win_w, win_h;
    SDL_GetRenderOutputSize(state->renderer, &win_w, &win_h);

    if(state->update.pending.kind == DECISION_KIND_NONE)
    { gui_game_over_click(state, x, y, (float)win_w, (float)win_h);
      return;
    }

    SessionCommand cmd;
    if(gui_input_handle_click(&state->input, &state->update, x, y, (float)win_w, (float)win_h,
                              state->log_open, &cmd))
    { session_client_send(state->session, &cmd);
      state->input_submitted = true;
    }
  }
} // gui_game_click

static void gui_game_event(GuiAppState* state, const SDL_Event* event)
{ if(event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_L && !event->key.repeat)
    state->log_open = !state->log_open;
  else if(event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat)
  { gui_reveal_skip(&state->reveal, (int64_t)SDL_GetTicks()); // any other key ends a reveal wait
    gui_reveal_ack(&state->reveal);                            // ...or dismisses "Continue"
  }

  if(event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT)
    gui_game_click(state, event->button.x, event->button.y);
} // gui_game_event

static void gui_splash_event(GuiAppState* state, const SDL_Event* event)
{ bool enter = event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat
               && (event->key.key == SDLK_RETURN || event->key.key == SDLK_KP_ENTER);
  bool click = false;
  if(event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT)
  { SDL_FRect b = gui_splash_button_rect(state);
    SDL_FPoint p = { event->button.x, event->button.y };
    click = SDL_PointInRectFloat(&p, &b);
  }
  if(enter || click)
    gui_open_setup(state);
} // gui_splash_event

static void gui_setup_event(GuiAppState* state, const SDL_Event* event)
{ GuiSetupResult res = GUI_SETUP_CONTINUE;
  if(event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT)
  { int win_w, win_h;
    SDL_GetRenderOutputSize(state->renderer, &win_w, &win_h);
    res = gui_setup_click(&state->setup, (float)win_w, (float)win_h, event->button.x,
                          event->button.y);
  }
  else if(event->type == SDL_EVENT_KEY_DOWN) // repeats too, so a held Backspace keeps erasing
    res = gui_setup_key(&state->setup, event->key.key);
  else if(event->type == SDL_EVENT_TEXT_INPUT)
    gui_setup_text(&state->setup, event->text.text);

  if(res == GUI_SETUP_START)
  { state->pconfig = state->setup.pconfig;
    gui_begin_game(state);
  }
} // gui_setup_event

// SDL text input (SDL_EVENT_TEXT_INPUT, and the on-screen keyboard on touch
// devices) is on exactly while a setup name field has focus.
static void gui_sync_text_input(GuiAppState* state)
{ bool want = state->screen == GUI_SCREEN_SETUP && gui_setup_wants_text(&state->setup);
  if(want == SDL_TextInputActive(state->window))
    return;
  if(want)
    SDL_StartTextInput(state->window);
  else
    SDL_StopTextInput(state->window);
} // gui_sync_text_input

static SDL_AppResult gui_sdl_event(void* appstate, SDL_Event* event)
{ GuiAppState* state = (GuiAppState*)appstate;
  if(event->type == SDL_EVENT_QUIT)
    return SDL_APP_SUCCESS;

  switch(state->screen)
  { case GUI_SCREEN_SPLASH:
      gui_splash_event(state, event);
      break;
    case GUI_SCREEN_SETUP:
      gui_setup_event(state, event);
      break;
    default:
      gui_game_event(state, event);
      break;
  }
  gui_sync_text_input(state);
  return SDL_APP_CONTINUE;
} // gui_sdl_event

static void gui_sdl_quit(void* appstate, SDL_AppResult result)
{ (void)result;
  GuiAppState* state = (GuiAppState*)appstate;
  if(!state)
    return;

  // Joins the session thread before ctx is destroyed -- session_client_close()
  // guarantees the thread is no longer touching ctx once it returns.
  gui_end_session(state);

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

int gui_app_run(config_t* cfg)
{ g_boot_cfg = cfg;
  int result = SDL_EnterAppMainCallbacks(0, NULL, gui_sdl_init, gui_sdl_iterate,
                                         gui_sdl_event, gui_sdl_quit);
  return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
} // gui_app_run
