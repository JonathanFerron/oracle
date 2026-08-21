# GUI Main-Loop / Engine Integration Notes

**Scope**: `oracle_sdl3_gui_plan.md` (this folder) covers SDL3 setup, card rendering,
fonts/assets, input-key-mapping, and platform specifics in depth, but doesn't address a
different question: **how does an event-driven GUI main loop actually poll and drive
the shared game engine** — submitting actions, letting AI turns resolve automatically,
staying responsive every frame regardless of whose turn it is? This file collects the
sketches that answer that question. It was consolidated (2026-08-20) from four
now-deleted files in `ideas/2 engine and action system design/`
(`gamegui_main_loop.txt`, `unified_gui_interface.txt`, `stda_game_gui.txt`,
`stda_game_impl.txt`) plus the GUI example from that folder's `mode_usage_examples.txt`.

**Dependency**: all of this assumes the pollable state machine
(`core/game_engine.c`/`.h`: `GameEngine`, `GamePhase`, `engine_step()`,
`engine_submit_action()`) sketched in `ideas/2 …/unified_state_machine.txt` and
`game_engine_impl.txt` actually exists. None of it has been built — this is design
exploration, not current architecture. See `doc/oracle_design.md` §11 for how this fits
the project's overall planned-architecture picture.

---

## The core idea: UI owns the loop, engine never blocks

CLI/TUI can block on input because a human reading text has nothing else to wait for.
A GUI can't: blocking would freeze rendering, animations, hover effects, and window
events. So GUI mode must **own its main loop** and poll the engine non-blockingly,
rather than the engine owning a blocking loop the way `stda_game_loop_cli()` does.

```
Blocking (CLI/TUI, game owns loop):        Event-driven (GUI, UI owns loop):
  while (!game_over) {                       while (running) {
    display();                                 handle_events();        // never blocks
    action = wait_for_input();  // BLOCKS       engine_step(engine);    // non-blocking
    process(action);                            update_animations(dt); // always runs
  }                                              render();              // always runs
                                               }
```

The same `engine_step(GameEngine*, GameContext*)` — returns `true` if it advanced,
`false` if it's now blocked waiting for input — serves both: blocking modes call it in
a `while` loop (`engine_run_until_input()`), event-driven modes call it once per frame.

---

## Standalone role: a non-blocking wrapper around the engine

For local (non-networked) GUI play, `roles/stda/` needs a `_gui`-suffixed sibling to its
existing blocking CLI/TUI entry points — a thin, poll-based wrapper the GUI's frame loop
calls into every tick instead of blocking:

```c
// roles/stda/stda_game.h
typedef struct
{ GameState* game;
  bool running;
  bool waiting_for_input;
  GamePhase current_phase;
  int active_player;
} GameLoopContext;

GameLoopContext* stda_init_game_gui(GameConfig* cfg);
void stda_update_game_gui(GameLoopContext* ctx, float dt);      // non-blocking
bool stda_submit_action_gui(GameLoopContext* ctx, Action* action);
bool stda_is_waiting_for_input(GameLoopContext* ctx, int player);
GameState* stda_get_game_state(GameLoopContext* ctx);           // for rendering
void stda_cleanup_game_gui(GameLoopContext* ctx);

// Existing blocking CLI/TUI entry points are unaffected — this is additive:
void stda_game_loop_cli(GameConfig* cfg);
void stda_game_loop_tui(GameConfig* cfg);
```

```c
// roles/stda/stda_game.c (sketch)
void stda_update_game_gui(GameLoopContext* ctx, float dt)
{ if(!ctx->running) return;
  if(is_game_over(ctx->game)) { ctx->running = false; return; }
  if(ctx->waiting_for_input) return;  // do nothing until the GUI submits an action

  if(is_ai_player(ctx->game, ctx->game->active_player))
  { Action* ai_action = get_ai_action(ctx->game, ctx->game->active_player);
    if(ai_action) { process_action(ctx->game, ai_action); free_action(ai_action); }
  }
  else
  { ctx->waiting_for_input = true;  // human's turn — wait for stda_submit_action_gui()
  }
}

bool stda_submit_action_gui(GameLoopContext* ctx, Action* action)
{ if(!ctx->waiting_for_input) return false;
  if(!validate_action(action, ctx->game)) return false;
  process_action(ctx->game, action);
  ctx->waiting_for_input = false;
  return true;
}
```

AI turns resolve automatically inside `stda_update_game_gui()` without the GUI having
to do anything special — the frame loop just keeps calling it, and it only actually
stalls (sets `waiting_for_input`) when a human needs to act.

---

## SDL3 main loop: standalone mode

```c
// ui/gui/gui_display.c (sketch)
typedef struct
{ SDL_Window* window;
  SDL_Renderer* renderer;
  FontManager* fonts;
  TextureCache* textures;
  GameLoopContext* game_ctx;
  InputState input_state;
  bool running;
} GUIContext;

GUIContext* init_gamegui(GameConfig* cfg)
{ GUIContext* gui = malloc(sizeof(GUIContext));
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
  gui->window = SDL_CreateWindow("Oracle", 1280, 720, 0);
  gui->renderer = SDL_CreateRenderer(gui->window, NULL);
  gui->fonts = init_font_manager(1.0f);
  gui->textures = create_texture_cache(gui->renderer, 256);
  gui->game_ctx = stda_init_game_gui(cfg);
  init_input_state(&gui->input_state);
  gui->running = true;
  return gui;
}

void run_gamegui_loop(GUIContext* gui)
{ uint64_t last_time = SDL_GetTicks();
  while(gui->running)
  { uint64_t now = SDL_GetTicks();
    float dt = (now - last_time) / 1000.0f;
    last_time = now;

    SDL_Event event;
    while(SDL_PollEvent(&event))
      if(!handle_gui_event(&event, gui)) gui->running = false;

    stda_update_game_gui(gui->game_ctx, dt);   // non-blocking
    update_gui_state(gui, dt);                 // animations, tooltips
    render_game_gui(gui);                      // always renders

    SDL_Delay(16); // ~60 FPS
  }
}

// Mouse click → translate to Action*, submit if it's our turn
bool handle_mouse_click(SDL_Event* e, GUIContext* gui, GameState* gs)
{ if(!stda_is_waiting_for_input(gui->game_ctx, PLAYER_A)) return true; // not our turn

  ClickTarget target = get_click_target((int)e->button.x, (int)e->button.y, gs);
  if(target.type == TARGET_CARD_IN_HAND)
    stda_submit_action_gui(gui->game_ctx, create_select_card_action(target.index));
  else if(target.type == TARGET_PLAY_BUTTON)
    stda_submit_action_gui(gui->game_ctx, create_play_cards_action());
  else if(target.type == TARGET_PASS_BUTTON)
    stda_submit_action_gui(gui->game_ctx, create_pass_turn_action());
  return true;
}
```

---

## Same GUI code, two roles: standalone vs. networked client

The GUI rendering/input code shouldn't need to know whether it's driving a local game
or a networked one — only the *source of truth* for game state differs (a local
`GameState*` vs. a `VisibleGameState` streamed from a server; see
`ideas/8 client server/game_loop_and_client_api_notes.md` for the client-side loop this
plugs into). One way to keep that role-switch thin:

```c
typedef enum { GUI_MODE_STANDALONE, GUI_MODE_CLIENT } GUIMode;

typedef struct
{ SDL_Window* window;
  SDL_Renderer* renderer;
  FontManager* fonts;
  TextureCache* textures;
  InputState input_state;
  bool running;
  GUIMode mode;
  union
  { GameLoopContext* stda_ctx;    // standalone
    ClientGameContext* client_ctx; // networked — see ideas/8 …
  };
} GUIContext;

// Role-agnostic call sites, dispatching on gui->mode internally:
bool submit_action_gui(GUIContext* gui, Action* action);
bool is_waiting_for_my_input(GUIContext* gui);
void* get_game_state_for_render(GUIContext* gui); // GameState* or VisibleGameState*

void run_gamegui_loop(GUIContext* gui)
{ /* same shape as above, but the "update game" step branches on gui->mode:
     GUI_MODE_STANDALONE → stda_update_game_gui(gui->stda_ctx, dt)
     GUI_MODE_CLIENT      → client_update_game_gui(gui->client_ctx, dt) */
}
```

Everything downstream (rendering, click handling, animation) only ever touches the
role-agnostic accessor functions, never `stda_ctx`/`client_ctx` directly — so
`ui/gui/*` code is written once and works for both `roles/stda/` and `roles/client/`.
