// gui_setup.c -- see gui_setup.h.

#include <stdio.h>
#include <string.h>

#include "gui_setup.h"
#include "gui_card.h" // gui_draw_text()
#include "gui_dice.h" // gui_dice_draw(), reused for the rating dots and radio buttons
#include "../shared/localization.h"
#include "../shared/player_config.h"
#include "../../ai_strat/ai_strategy.h" // ai_strategy_is_implemented()

// Colours (same values as gui_render.c's, duplicated rather than shared).
#define SETUP_PANEL_BG (SDL_Color){ 0x0A, 0x1F, 0x22, 235 }
#define SETUP_FIELD_BG (SDL_Color){ 0x0F, 0x2B, 0x30, 255 }
#define SETUP_BUTTON_BG (SDL_Color){ 0x2E, 0x6B, 0x5E, 255 }
#define SETUP_BUTTON_IDLE (SDL_Color){ 0x1C, 0x3E, 0x44, 255 }
#define SETUP_TEXT (SDL_Color){ 255, 255, 255, 255 }
#define SETUP_MUTED (SDL_Color){ 0x9A, 0xB0, 0xB4, 255 }
#define SETUP_BOREALIS_BAND (SDL_Color){ 255, 255, 255, 28 }
#define SETUP_DOT_INK (SDL_Color){ 255, 255, 255, 255 }

#define SETUP_PANEL_MAX_W 1120.0f
#define SETUP_PAD 24.0f
#define SETUP_GAP 16.0f
#define SETUP_LABEL_W 190.0f
#define SETUP_TITLE_H 40.0f
#define SETUP_ROW_H 36.0f
#define SETUP_RADIO_H 30.0f
#define SETUP_LIST_LABEL_H 26.0f
#define SETUP_AGENT_ROW_H 30.0f
#define SETUP_HINT_H 24.0f
#define SETUP_START_W 160.0f
#define SETUP_START_H 44.0f
#define SETUP_FIELD_W 260.0f
#define SETUP_DOT_RADIUS 7.0f

typedef struct
{ SDL_FRect panel, title;
  float mode_y, name_y, radio_y, list_label_y, assign_y;
  SDL_FRect mode_btn[GUI_SETUP_MODE_COUNT];
  uint8_t name_count;               // 0 (AI vs AI), 1 (Human vs AI) or 2
  SDL_FRect name_field[2];          // seat i's field
  bool show_radio;
  SDL_FRect radio[2];
  int list_seat;                    // -1: no list
  SDL_FRect list;
  SDL_FRect assign_btn[3];
  SDL_FRect hint;
  SDL_FRect start;
} SetupLayout;

// ---------------------------------------------------------------- layout --

static uint8_t list_rows(const GuiSetupState* st)
{ return (uint8_t)((st->agent_count + 1) / 2);
} // list_rows

static float panel_height(const GuiSetupState* st, uint8_t names, bool radio, bool list)
{ float h = SETUP_PAD + SETUP_TITLE_H + SETUP_GAP + SETUP_ROW_H + SETUP_GAP;
  if(names > 0)
    h += SETUP_ROW_H + SETUP_GAP;
  if(radio)
    h += SETUP_RADIO_H + SETUP_GAP;
  if(list)
    h += SETUP_LIST_LABEL_H + list_rows(st) * SETUP_AGENT_ROW_H + SETUP_GAP;
  return h + SETUP_ROW_H + SETUP_HINT_H + SETUP_GAP + SETUP_START_H + SETUP_PAD;
} // panel_height

// `count` equal buttons from `x0` across `w`, each at most `max_w` wide.
static void button_row(SDL_FRect* out, uint8_t count, float x0, float y, float w, float max_w)
{ float bw = SDL_min(max_w, (w - (count - 1) * 10.0f) / count);
  for(uint8_t i = 0; i < count; i++)
    out[i] = (SDL_FRect)
  { x0 + i * (bw + 10.0f), y, bw, SETUP_ROW_H
  };
} // button_row

static void layout_rows(const GuiSetupState* st, SetupLayout* L, float x0, float w)
{ float y = L->panel.y + SETUP_PAD + SETUP_TITLE_H + SETUP_GAP;
  L->mode_y = y;
  button_row(L->mode_btn, GUI_SETUP_MODE_COUNT, x0, y, w, 220.0f);
  y += SETUP_ROW_H + SETUP_GAP;

  if(L->name_count > 0)
  { L->name_y = y;
    float tag_w = L->name_count == 2 ? 22.0f : 0.0f; // room for the "1"/"2" tags
    for(uint8_t i = 0; i < L->name_count; i++)
      L->name_field[i] = (SDL_FRect)
    { x0 + tag_w + i * (SETUP_FIELD_W + 2.0f * tag_w), y,
      SETUP_FIELD_W, SETUP_ROW_H
    };
    y += SETUP_ROW_H + SETUP_GAP;
  }
  if(L->show_radio)
  { L->radio_y = y;
    for(int i = 0; i < 2; i++)
      L->radio[i] = (SDL_FRect)
    { x0 + i * (w / 2.0f), y, w / 2.0f - 10.0f, SETUP_RADIO_H
    };
    y += SETUP_RADIO_H + SETUP_GAP;
  }
  if(L->list_seat >= 0)
  { L->list_label_y = y;
    float lx = L->panel.x + SETUP_PAD;
    L->list = (SDL_FRect)
    { lx, y + SETUP_LIST_LABEL_H, L->panel.w - 2.0f * SETUP_PAD,
      list_rows(st) * SETUP_AGENT_ROW_H
    };
    y += SETUP_LIST_LABEL_H + L->list.h + SETUP_GAP;
  }
  L->assign_y = y;
  button_row(L->assign_btn, 3, x0, y, w, 220.0f);
  L->hint = (SDL_FRect)
  { x0, y + SETUP_ROW_H, w, SETUP_HINT_H
  };
} // layout_rows

static void compute_layout(const GuiSetupState* st, float win_w, float win_h, SetupLayout* L)
{ memset(L, 0, sizeof(*L));
  GuiSetupMode mode = gui_setup_get_mode(&st->pconfig);
  L->name_count = mode == GUI_SETUP_HUMAN_VS_HUMAN ? 2 : mode == GUI_SETUP_HUMAN_VS_AI ? 1 : 0;
  L->show_radio = mode == GUI_SETUP_AI_VS_AI;
  L->list_seat = gui_setup_list_seat(&st->pconfig, st->radio_seat);

  float pw = SDL_min(SETUP_PANEL_MAX_W, win_w - 80.0f);
  float ph = panel_height(st, L->name_count, L->show_radio, L->list_seat >= 0);
  L->panel = (SDL_FRect)
  { (win_w - pw) / 2.0f, SDL_max(10.0f, (win_h - ph) / 2.0f), pw, ph
  };
  L->title = (SDL_FRect)
  { L->panel.x + SETUP_PAD, L->panel.y + SETUP_PAD,
    pw - 2.0f * SETUP_PAD, SETUP_TITLE_H
  };

  float x0 = L->panel.x + SETUP_PAD + SETUP_LABEL_W;
  layout_rows(st, L, x0, pw - 2.0f * SETUP_PAD - SETUP_LABEL_W);
  L->start = (SDL_FRect)
  { L->panel.x + pw - SETUP_PAD - SETUP_START_W,
    L->panel.y + ph - SETUP_PAD - SETUP_START_H, SETUP_START_W, SETUP_START_H
  };
} // compute_layout

// Column-major: the first column holds the easiest half, top to bottom.
static SDL_FRect agent_cell(const SetupLayout* L, uint8_t rows, uint8_t i)
{ float col_w = (L->list.w - SETUP_GAP) / 2.0f;
  uint8_t col = (uint8_t)(i / rows), row = (uint8_t)(i % rows);
  return (SDL_FRect)
  { L->list.x + col * (col_w + SETUP_GAP), L->list.y + row * SETUP_AGENT_ROW_H,
    col_w, SETUP_AGENT_ROW_H
  };
} // agent_cell

static bool in_rect(SDL_FRect r, float x, float y)
{ return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
} // in_rect

// ------------------------------------------------------------------ open --

void gui_setup_open(GuiSetupState* st, const PlayerConfig* defaults)
{ memset(st, 0, sizeof(*st));
  st->pconfig = *defaults;
  PlayerConfig fresh;
  init_player_config(&fresh);
  memcpy(st->default_names, fresh.player_names, sizeof(st->default_names));
  st->radio_seat = PLAYER_A;
  st->focus_seat = -1;

  for(AIStrategyType t = 0; t < AI_STRATEGY_COUNT; t++)
    if(ai_strategy_is_implemented(t))
      st->agents[st->agent_count++] = (GuiSetupAgent)
    { t, get_ai_strategy_rating(t, NULL)
    };
  gui_setup_sort_agents(st->agents, st->agent_count);

  if(st->agent_count > 0)
  { st->min_rating = st->agents[0].rating;
    st->max_rating = st->agents[st->agent_count - 1].rating;
  }
} // gui_setup_open

// ------------------------------------------------------------------ draw --

static void fill_rect(SDL_Renderer* r, SDL_FRect rect, SDL_Color c)
{ SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  SDL_RenderFillRect(r, &rect);
} // fill_rect

static void text_in(SDL_Renderer* r, TTF_Font* font, const char* text, SDL_FRect rect,
                    float dx, bool centred, SDL_Color c)
{ int tw = 0, th = 0;
  TTF_GetStringSize(font, text, 0, &tw, &th);
  float x = centred ? rect.x + (rect.w - (float)tw) / 2.0f : rect.x + dx;
  gui_draw_text(r, font, text, SDL_roundf(x), SDL_roundf(rect.y + (rect.h - (float)th) / 2.0f), c);
} // text_in

static void row_label(SDL_Renderer* r, TTF_Font* font, const SetupLayout* L, float y, float h,
                      const char* text)
{ SDL_FRect rect = { L->panel.x + SETUP_PAD, y, SETUP_LABEL_W, h };
  text_in(r, font, text, rect, 0.0f, false, SETUP_MUTED);
} // row_label

static void choice_button(SDL_Renderer* r, TTF_Font* font, SDL_FRect rect, const char* label,
                          bool selected)
{ fill_rect(r, rect, selected ? SETUP_BUTTON_BG : SETUP_BUTTON_IDLE);
  SDL_SetRenderDrawColor(r, 255, 255, 255, selected ? 220 : 90);
  SDL_RenderRect(r, &rect);
  text_in(r, font, label, rect, 0.0f, true, selected ? SETUP_TEXT : SETUP_MUTED);
} // choice_button

static const char* mode_label(GuiSetupMode m, ui_language_t lang)
{ switch(m)
  { case GUI_SETUP_HUMAN_VS_HUMAN:
      return LOCALIZED_STRING_L(lang, "Human vs Human", "Humain vs Humain", "Humano vs Humano");
    case GUI_SETUP_AI_VS_AI:
      return LOCALIZED_STRING_L(lang, "AI vs AI", "IA vs IA", "IA vs IA");
    default:
      return LOCALIZED_STRING_L(lang, "Human vs AI", "Humain vs IA", "Humano vs IA");
  }
} // mode_label

static const char* assign_label(PlayerAssignmentMode m, ui_language_t lang)
{ switch(m)
  { case ASSIGN_INVERTED:
      return LOCALIZED_STRING_L(lang, "Inverted", "Inverse", "Invertido");
    case ASSIGN_RANDOM:
      return LOCALIZED_STRING_L(lang, "Random", "Aleatoire", "Aleatorio");
    default:
      return LOCALIZED_STRING_L(lang, "Direct", "Direct", "Directo");
  }
} // assign_label

static void draw_title(SDL_Renderer* r, TTF_Font* font, const SetupLayout* L, ui_language_t lang)
{ text_in(r, font, LOCALIZED_STRING_L(lang, "New game", "Nouvelle partie", "Nueva partida"),
          L->title, 0.0f, false, SETUP_TEXT);
  SDL_SetRenderDrawColor(r, 255, 255, 255, 60);
  SDL_RenderLine(r, L->title.x, L->title.y + L->title.h, L->title.x + L->title.w,
                 L->title.y + L->title.h);
} // draw_title

static void draw_mode_row(SDL_Renderer* r, TTF_Font* font, const GuiSetupState* st,
                          const SetupLayout* L, ui_language_t lang)
{ row_label(r, font, L, L->mode_y, SETUP_ROW_H,
            LOCALIZED_STRING_L(lang, "Game mode", "Mode de jeu", "Modo de juego"));
  GuiSetupMode cur = gui_setup_get_mode(&st->pconfig);
  for(GuiSetupMode m = 0; m < GUI_SETUP_MODE_COUNT; m++)
    choice_button(r, font, L->mode_btn[m], mode_label(m, lang), m == cur);
} // draw_mode_row

static void draw_name_field(SDL_Renderer* r, TTF_Font* font, const GuiSetupState* st,
                            SDL_FRect f, int seat)
{ bool focused = st->focus_seat == seat;
  fill_rect(r, f, SETUP_FIELD_BG);
  SDL_SetRenderDrawColor(r, 255, 255, 255, focused ? 220 : 90);
  SDL_RenderRect(r, &f);

  const char* name = st->pconfig.player_names[seat];
  bool placeholder = focused && st->name_fresh;
  text_in(r, font, name, f, 10.0f, false, placeholder ? SETUP_MUTED : SETUP_TEXT);
  if(!focused)
    return;

  int tw = 0, th = 0;
  TTF_GetStringSize(font, placeholder ? "" : name, 0, &tw, &th);
  float cx = f.x + 10.0f + (float)tw + 1.0f;
  SDL_SetRenderDrawColor(r, 255, 255, 255, 230);
  SDL_RenderLine(r, cx, f.y + 7.0f, cx, f.y + f.h - 7.0f);
} // draw_name_field

static void draw_names_row(SDL_Renderer* r, TTF_Font* font, const GuiSetupState* st,
                           const SetupLayout* L, ui_language_t lang)
{ if(L->name_count == 0)
    return;
  row_label(r, font, L, L->name_y, SETUP_ROW_H,
            L->name_count == 1 ? LOCALIZED_STRING_L(lang, "Your name", "Votre nom", "Tu nombre")
            : LOCALIZED_STRING_L(lang, "Player names", "Noms des joueurs",
                                 "Nombres de jugadores"));
  for(uint8_t i = 0; i < L->name_count; i++)
  { char num[4];
    snprintf(num, sizeof(num), "%u", i + 1);
    SDL_FRect tag = { L->name_field[i].x - 22.0f, L->name_field[i].y, 22.0f, SETUP_ROW_H };
    if(L->name_count == 2)
      text_in(r, font, num, tag, 0.0f, false, SETUP_MUTED);
    draw_name_field(r, font, st, L->name_field[i], i);
  }
} // draw_names_row

static void draw_radio_row(SDL_Renderer* r, TTF_Font* font, const GuiSetupState* st,
                           const SetupLayout* L, ui_language_t lang)
{ if(!L->show_radio)
    return;
  row_label(r, font, L, L->radio_y, SETUP_RADIO_H,
            LOCALIZED_STRING_L(lang, "Choosing for", "Choix pour", "Eligiendo para"));
  for(int i = 0; i < 2; i++)
  { SDL_FRect b = L->radio[i];
    bool on = st->radio_seat == i;
    gui_dice_draw(r, NULL, b.x + 9.0f, b.y + b.h / 2.0f, 8.0f, 20, SETUP_TEXT, on ? 0 : -1);

    char text[96];
    snprintf(text, sizeof(text), "%s %d: %s",
             LOCALIZED_STRING_L(lang, "Player", "Joueur", "Jugador"), i + 1,
             get_strategy_display_name(st->pconfig.ai_strategies[i], lang));
    text_in(r, font, text, b, 26.0f, false, on ? SETUP_TEXT : SETUP_MUTED);
  }
} // draw_radio_row

static void draw_agent_row(SDL_Renderer* r, TTF_Font* font, const GuiSetupState* st,
                           SDL_FRect cell, const GuiSetupAgent* a, bool selected,
                           ui_language_t lang)
{ if(selected)
    fill_rect(r, cell, SETUP_BUTTON_BG);
  else if(a->type == AI_STRATEGY_BOREALIS)
    fill_rect(r, cell, SETUP_BOREALIS_BAND);

  uint8_t rgb[3];
  float hue = gui_setup_rating_hue(a->rating, st->min_rating, st->max_rating);
  gui_setup_hsl_to_rgb(hue, 1.0f, 0.5f, rgb);
  gui_dice_draw(r, NULL, cell.x + 14.0f, cell.y + cell.h / 2.0f, SETUP_DOT_RADIUS, 20,
                (SDL_Color)
  { rgb[0], rgb[1], rgb[2], 255
  }, 0);

  char name[96], text[128], rating[8];
  format_strategy_menu_name(a->type, lang, name, sizeof(name));
  snprintf(text, sizeof(text), "%s _ %s", name, get_ai_strategy_shorthand(a->type));
  text_in(r, font, text, cell, 30.0f, false, selected ? SETUP_TEXT : SETUP_MUTED);

  snprintf(rating, sizeof(rating), "%d", a->rating);
  int tw = 0, th = 0;
  TTF_GetStringSize(font, rating, 0, &tw, &th);
  text_in(r, font, rating, cell, cell.w - 10.0f - (float)tw, false, SETUP_TEXT);
} // draw_agent_row

static void draw_agent_list(SDL_Renderer* r, TTF_Font* font, const GuiSetupState* st,
                            const SetupLayout* L, ui_language_t lang)
{ if(L->list_seat < 0)
    return;
  char label[96];
  if(L->show_radio)
    snprintf(label, sizeof(label), "%s %d", LOCALIZED_STRING_L(lang, "AI agent for Player",
                                                               "Agent IA du Joueur", "Agente IA del Jugador"), L->list_seat + 1);
  else
    snprintf(label, sizeof(label), "%s", LOCALIZED_STRING_L(lang, "AI opponent",
                                                            "Adversaire IA", "Oponente IA"));
  SDL_FRect lr = { L->list.x, L->list_label_y, L->list.w, SETUP_LIST_LABEL_H };
  text_in(r, font, label, lr, 0.0f, false, SETUP_MUTED);

  AIStrategyType chosen = st->pconfig.ai_strategies[L->list_seat];
  for(uint8_t i = 0; i < st->agent_count; i++)
    draw_agent_row(r, font, st, agent_cell(L, list_rows(st), i), &st->agents[i],
                   st->agents[i].type == chosen, lang);
} // draw_agent_list

// "Player1 plays A (starts), The Gambler plays B" for the chosen assignment.
static void draw_assign_hint(SDL_Renderer* r, TTF_Font* font, const GuiSetupState* st,
                             const SetupLayout* L, ui_language_t lang)
{ const PlayerConfig* pc = &st->pconfig;
  const char* who[2];
  for(int i = 0; i < 2; i++)
    who[i] = pc->player_types[i] == AI_PLAYER
             ? get_strategy_display_name(pc->ai_strategies[i], lang) : pc->player_names[i];

  char hint[192];
  if(pc->assignment_mode == ASSIGN_RANDOM)
    snprintf(hint, sizeof(hint), "%s", LOCALIZED_STRING_L(lang,
                                                          "Seats A and B drawn at random each game (A starts)",
                                                          "Places A et B tirees au sort a chaque partie (A commence)",
                                                          "Puestos A y B sorteados en cada partida (A empieza)"));
  else
  { bool inv = pc->assignment_mode == ASSIGN_INVERTED;
    snprintf(hint, sizeof(hint), LOCALIZED_STRING_L(lang, "%s plays A (starts), %s plays B",
                                                    "%s joue A (commence), %s joue B", "%s juega A (empieza), %s juega B"),
             who[inv ? 1 : 0], who[inv ? 0 : 1]);
  }
  text_in(r, font, hint, L->hint, 0.0f, false, SETUP_MUTED);
} // draw_assign_hint

static void draw_assign_row(SDL_Renderer* r, TTF_Font* font, const GuiSetupState* st,
                            const SetupLayout* L, ui_language_t lang)
{ row_label(r, font, L, L->assign_y, SETUP_ROW_H,
            LOCALIZED_STRING_L(lang, "Seats", "Places", "Puestos"));
  for(PlayerAssignmentMode m = ASSIGN_DIRECT; m <= ASSIGN_RANDOM; m++)
    choice_button(r, font, L->assign_btn[m], assign_label(m, lang),
                  m == st->pconfig.assignment_mode);
  draw_assign_hint(r, font, st, L, lang);
} // draw_assign_row

void gui_setup_draw(SDL_Renderer* r, TTF_Font* font, const GuiSetupState* st,
                    float win_w, float win_h, ui_language_t lang)
{ SetupLayout L;
  compute_layout(st, win_w, win_h, &L);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  fill_rect(r, L.panel, SETUP_PANEL_BG);
  SDL_SetRenderDrawColor(r, 255, 255, 255, 120);
  SDL_RenderRect(r, &L.panel);

  draw_title(r, font, &L, lang);
  draw_mode_row(r, font, st, &L, lang);
  draw_names_row(r, font, st, &L, lang);
  draw_radio_row(r, font, st, &L, lang);
  draw_agent_list(r, font, st, &L, lang);
  draw_assign_row(r, font, st, &L, lang);
  choice_button(r, font, L.start, LOCALIZED_STRING_L(lang, "Start", "Commencer", "Empezar"),
                true);
} // gui_setup_draw

// ----------------------------------------------------------------- input --

static void focus_name(GuiSetupState* st, int seat)
{ st->focus_seat = seat;
  st->name_fresh = seat >= 0 &&
                   strcmp(st->pconfig.player_names[seat], st->default_names[seat]) == 0;
} // focus_name

// An emptied name falls back to its default.
static void restore_empty_names(GuiSetupState* st)
{ for(int i = 0; i < 2; i++)
    if(st->pconfig.player_names[i][0] == '\0')
      memcpy(st->pconfig.player_names[i], st->default_names[i], MAX_PLAYER_NAME_LEN);
} // restore_empty_names

static void leave_field(GuiSetupState* st)
{ focus_name(st, -1);
  restore_empty_names(st);
} // leave_field

static GuiSetupResult start(GuiSetupState* st)
{ leave_field(st);
  return GUI_SETUP_START;
} // start

static bool click_agent(GuiSetupState* st, const SetupLayout* L, float x, float y)
{ if(L->list_seat < 0 || !in_rect(L->list, x, y))
    return false;
  for(uint8_t i = 0; i < st->agent_count; i++)
    if(in_rect(agent_cell(L, list_rows(st), i), x, y))
    { st->pconfig.ai_strategies[L->list_seat] = st->agents[i].type;
      return true;
    }
  return true;
} // click_agent

static bool click_buttons(GuiSetupState* st, const SetupLayout* L, float x, float y)
{ for(GuiSetupMode m = 0; m < GUI_SETUP_MODE_COUNT; m++)
    if(in_rect(L->mode_btn[m], x, y))
    { gui_setup_set_mode(&st->pconfig, m);
      return true;
    }
  for(PlayerAssignmentMode m = ASSIGN_DIRECT; m <= ASSIGN_RANDOM; m++)
    if(in_rect(L->assign_btn[m], x, y))
    { st->pconfig.assignment_mode = m;
      return true;
    }
  for(int i = 0; L->show_radio && i < 2; i++)
    if(in_rect(L->radio[i], x, y))
    { st->radio_seat = (PlayerID)i;
      return true;
    }
  return false;
} // click_buttons

GuiSetupResult gui_setup_click(GuiSetupState* st, float win_w, float win_h, float x, float y)
{ SetupLayout L;
  compute_layout(st, win_w, win_h, &L);

  for(uint8_t i = 0; i < L.name_count; i++)
    if(in_rect(L.name_field[i], x, y))
    { if(st->focus_seat != i)
      { leave_field(st);
        focus_name(st, i);
      }
      return GUI_SETUP_CONTINUE;
    }
  leave_field(st);

  if(in_rect(L.start, x, y))
    return start(st);
  if(!click_buttons(st, &L, x, y))
    click_agent(st, &L, x, y);
  return GUI_SETUP_CONTINUE;
} // gui_setup_click

static void name_backspace(GuiSetupState* st)
{ char* name = st->pconfig.player_names[st->focus_seat];
  if(st->name_fresh)
    name[0] = '\0';
  else
    gui_setup_name_backspace(name);
  st->name_fresh = false;
} // name_backspace

// Tab: the other human's field, if there is one.
static void next_field(GuiSetupState* st)
{ int other = 1 - st->focus_seat;
  bool both_human = gui_setup_get_mode(&st->pconfig) == GUI_SETUP_HUMAN_VS_HUMAN;
  leave_field(st);
  focus_name(st, both_human ? other : -1);
} // next_field

GuiSetupResult gui_setup_key(GuiSetupState* st, SDL_Keycode key)
{ if(st->focus_seat < 0)
    return (key == SDLK_RETURN || key == SDLK_KP_ENTER) ? start(st) : GUI_SETUP_CONTINUE;

  switch(key)
  { case SDLK_BACKSPACE:
      name_backspace(st);
      break;
    case SDLK_TAB:
      next_field(st);
      break;
    case SDLK_ESCAPE:
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
      leave_field(st);
      break;
    default:
      break;
  }
  return GUI_SETUP_CONTINUE;
} // gui_setup_key

void gui_setup_text(GuiSetupState* st, const char* text)
{ if(st->focus_seat < 0)
    return;
  char* name = st->pconfig.player_names[st->focus_seat];
  if(st->name_fresh)
    name[0] = '\0';
  st->name_fresh = false;
  gui_setup_name_append(name, text);
} // gui_setup_text

bool gui_setup_wants_text(const GuiSetupState* st)
{ return st->focus_seat >= 0;
} // gui_setup_wants_text
