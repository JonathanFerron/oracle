// gui_render.c -- see gui_render.h.

#include <stdio.h>

#include "gui_render.h"
#include "gui_layout.h"
#include "gui_card.h"
#include "../../core/game_constants.h" // fullDeck[]
#include "../shared/localization.h"

#define GUI_STATUS_BG (SDL_Color){ 0x0F, 0x2B, 0x30, 255 }
#define GUI_STATUS_TEXT (SDL_Color){ 255, 255, 255, 255 }
#define GUI_INFO_TEXT (SDL_Color){ 0xE0, 0xE0, 0xE0, 255 }
#define GUI_BADGE_TEXT (SDL_Color){ 255, 255, 255, 255 }
#define GUI_ACTION_BG (SDL_Color){ 0x14, 0x3A, 0x40, 255 }
#define GUI_BUTTON_BG (SDL_Color){ 0x2E, 0x6B, 0x5E, 255 }
#define GUI_BUTTON_BG_DISABLED (SDL_Color){ 0x55, 0x55, 0x55, 255 }
#define GUI_BUTTON_TEXT (SDL_Color){ 255, 255, 255, 255 }
#define GUI_THINKING_TEXT (SDL_Color){ 0xC0, 0xC0, 0xC0, 255 }
#define GUI_OVERLAY_BG (SDL_Color){ 0x0A, 0x1F, 0x22, 235 }

static bool arr_contains(const uint8_t* arr, uint8_t n, uint8_t val)
{ for(uint8_t i = 0; i < n; i++)
    if(arr[i] == val) return true;
  return false;
} // arr_contains

uint8_t gui_discard_champions(const Discard* d, uint8_t out[40])
{ uint8_t n = 0;
  for(uint8_t i = 0; i < d->size; i++)
  { uint8_t c = Discard_get(d, i);
    if(fullDeck[c].card_type == CHAMPION_CARD) out[n++] = c;
  }
  return n;
} // gui_discard_champions

const char* gui_decision_kind_debug_name(DecisionKind kind)
{ switch(kind)
  { case DECISION_KIND_NONE:
      return "NONE (game over)";
    case DECISION_KIND_MULLIGAN:
      return "MULLIGAN";
    case DECISION_KIND_ATTACK:
      return "ATTACK";
    case DECISION_KIND_DEFENSE:
      return "DEFENSE";
    case DECISION_KIND_DISCARD_TO_7:
      return "DISCARD_TO_7";
    default:
      return "?";
  }
} // gui_decision_kind_debug_name

static const char* localized_decision_kind(DecisionKind kind, ui_language_t lang)
{ switch(kind)
  { case DECISION_KIND_MULLIGAN:
      return LOCALIZED_STRING_L(lang, "Mulligan", "Mulligan", "Mulligan");
    case DECISION_KIND_ATTACK:
      return LOCALIZED_STRING_L(lang, "Attack", "Attaque", "Ataque");
    case DECISION_KIND_DEFENSE:
      return LOCALIZED_STRING_L(lang, "Defense", "Defense", "Defensa");
    case DECISION_KIND_DISCARD_TO_7:
      return LOCALIZED_STRING_L(lang, "Discard to 7", "Defausser a 7", "Descartar a 7");
    default:
      return "";
  }
} // localized_decision_kind

static const char* button_label(GuiButtonId btn, ui_language_t lang)
{ switch(btn)
  { case GUI_BTN_CONFIRM:
      return LOCALIZED_STRING_L(lang, "Confirm", "Confirmer", "Confirmar");
    case GUI_BTN_PASS:
      return LOCALIZED_STRING_L(lang, "Pass", "Passer", "Pasar");
    case GUI_BTN_DECLINE:
      return LOCALIZED_STRING_L(lang, "Decline", "Refuser", "Rechazar");
    case GUI_BTN_DRAW:
      return LOCALIZED_STRING_L(lang, "Draw", "Piger", "Robar");
    case GUI_BTN_RECALL:
      return LOCALIZED_STRING_L(lang, "Recall", "Rappeler", "Recordar");
    case GUI_BTN_CANCEL:
      return LOCALIZED_STRING_L(lang, "Cancel", "Annuler", "Cancelar");
    default:
      return "";
  }
} // button_label

static void draw_button(SDL_Renderer* r, TTF_Font* font, SDL_FRect rect,
                        const char* label, bool enabled)
{ SDL_Color bg = enabled ? GUI_BUTTON_BG : GUI_BUTTON_BG_DISABLED;
  SDL_SetRenderDrawColor(r, bg.r, bg.g, bg.b, 255);
  SDL_RenderFillRect(r, &rect);
  SDL_SetRenderDrawColor(r, 255, 255, 255, 180);
  SDL_RenderRect(r, &rect);

  int tw, th;
  TTF_GetStringSize(font, label, 0, &tw, &th);
  float tx = rect.x + (rect.w - (float)tw) / 2.0f;
  float ty = rect.y + (rect.h - (float)th) / 2.0f;
  gui_draw_text(r, font, label, tx, ty, GUI_BUTTON_TEXT);
} // draw_button

// The action bar shows nothing once the game is over (the status bar
// already says so), an "opponent is thinking" label when it isn't the
// viewer's move (synthesis doc section 9.7), or the current buttons
// (gui_input_active_buttons() -- the same list gui_input.c hit-tests).
static void draw_action_bar(SDL_Renderer* r, TTF_Font* font, SDL_FRect bar,
                            const SessionUpdate* u, const GuiInputState* input,
                            ui_language_t lang)
{ SDL_SetRenderDrawColor(r, GUI_ACTION_BG.r, GUI_ACTION_BG.g, GUI_ACTION_BG.b, 255);
  SDL_RenderFillRect(r, &bar);

  if(!input || u->pending.kind == DECISION_KIND_NONE)
    return;

  if(u->pending.player != u->view.viewer)
  { const char* label = LOCALIZED_STRING_L(lang, "Opponent is thinking...",
                                           "L'adversaire reflechit...",
                                           "El oponente esta pensando...");
    float y = bar.y + (bar.h - (float)TTF_GetFontHeight(font)) / 2.0f;
    gui_draw_text(r, font, label, bar.x + 10.0f, y, GUI_THINKING_TEXT);
    return;
  }

  GuiButtonId buttons[GUI_MAX_BUTTONS];
  bool confirm_enabled;
  uint8_t n = gui_input_active_buttons(input, u, buttons, &confirm_enabled);
  for(uint8_t i = 0; i < n; i++)
  { SDL_FRect rect = gui_layout_button_rect(bar, i, n);
    bool enabled = (buttons[i] != GUI_BTN_CONFIRM) || confirm_enabled;
    draw_button(r, font, rect, button_label(buttons[i], lang), enabled);
  }
} // draw_action_bar

// Highlighting mirrors gui_input.c's own staging fields -- see
// GuiInputState's comment for what each mode's staged[]/special_card means.
static bool card_is_highlighted(const GuiInputState* input, uint8_t card)
{ if(!input) return false;

  if(input->kind == DECISION_KIND_ATTACK)
  { if(input->attack_mode == ATTACK_INPUT_NONE)
      return arr_contains(input->staged, input->staged_count, card);
    if(card == input->special_card) return true;
    return input->attack_mode == ATTACK_INPUT_CASH_TARGET
           && input->staged_count == 1 && input->staged[0] == card;
  }
  return arr_contains(input->staged, input->staged_count, card);
} // card_is_highlighted

static void draw_recall_overlay(SDL_Renderer* r, TTF_Font* font, SDL_FRect area,
                                const Discard* d, const GuiInputState* input,
                                ui_language_t lang)
{ SDL_SetRenderDrawColor(r, GUI_OVERLAY_BG.r, GUI_OVERLAY_BG.g, GUI_OVERLAY_BG.b,
                         GUI_OVERLAY_BG.a);
  SDL_RenderFillRect(r, &area);
  SDL_SetRenderDrawColor(r, 255, 255, 255, 200);
  SDL_RenderRect(r, &area);

  char title[96];
  snprintf(title, sizeof(title), "%s %u",
           LOCALIZED_STRING_L(lang, "Recall", "Rappelle", "Recuerda"),
           fullDeck[input->special_card].choose_num);
  gui_draw_text(r, font, title, area.x + 10.0f, area.y + 6.0f, GUI_BUTTON_TEXT);

  uint8_t champs[40];
  uint8_t n = gui_discard_champions(d, champs);
  SDL_FRect grid = { area.x + 10.0f, area.y + 36.0f, area.w - 20.0f, area.h - 46.0f };
  for(uint8_t i = 0; i < n; i++)
  { SDL_FRect slot = gui_layout_grid_slot(grid, i, n);
    bool hl = arr_contains(input->recall_staged, input->recall_staged_count, champs[i]);
    gui_card_draw(r, font, slot, champs[i], hl, lang);
  }
} // draw_recall_overlay

static void draw_status_bar(SDL_Renderer* r, TTF_Font* font, SDL_FRect bar,
                            const SessionUpdate* u, ui_language_t lang)
{ SDL_SetRenderDrawColor(r, GUI_STATUS_BG.r, GUI_STATUS_BG.g, GUI_STATUS_BG.b, 255);
  SDL_RenderFillRect(r, &bar);

  char text[160];
  if(u->pending.kind == DECISION_KIND_NONE)
  { const char* outcome =
      u->view.game_state == PLAYER_A_WINS ?
      LOCALIZED_STRING_L(lang, "Player A wins", "Le joueur A gagne", "Gana el jugador A") :
      u->view.game_state == PLAYER_B_WINS ?
      LOCALIZED_STRING_L(lang, "Player B wins", "Le joueur B gagne", "Gana el jugador B") :
      LOCALIZED_STRING_L(lang, "Draw", "Match nul", "Empate");
    snprintf(text, sizeof(text), "%s -- %s",
             LOCALIZED_STRING_L(lang, "Game over", "Partie terminee", "Partida terminada"),
             outcome);
  }
  else
  { char letter = u->pending.player == PLAYER_A ? 'A' : 'B';
    snprintf(text, sizeof(text), "%s %u -- %s %c: %s",
             LOCALIZED_STRING_L(lang, "Turn", "Tour", "Turno"), u->view.turn,
             LOCALIZED_STRING_L(lang, "Player", "Joueur", "Jugador"), letter,
             localized_decision_kind(u->pending.kind, lang));
  }

  float text_y = bar.y + (bar.h - (float)TTF_GetFontHeight(font)) / 2.0f;
  gui_draw_text(r, font, text, bar.x + 10.0f, text_y, GUI_STATUS_TEXT);
} // draw_status_bar

static void draw_seat_info(SDL_Renderer* r, TTF_Font* font, SDL_FRect rect,
                           PlayerID p, const VisibleGameState* v, ui_language_t lang)
{ char text[96];
  char letter = p == PLAYER_A ? 'A' : 'B';
  snprintf(text, sizeof(text), "%s %c   %s %u   %s %u",
           LOCALIZED_STRING_L(lang, "Player", "Joueur", "Jugador"), letter,
           LOCALIZED_STRING_L(lang, "Energy", "Energie", "Energia"), v->energy[p],
           LOCALIZED_STRING_L(lang, "Cash", "Argent", "Dinero"), v->cash[p]);
  gui_draw_text(r, font, text, rect.x, rect.y, GUI_INFO_TEXT);
} // draw_seat_info

static void draw_count_badge(SDL_Renderer* r, TTF_Font* font, SDL_FRect slot, uint8_t count)
{ char text[8];
  snprintf(text, sizeof(text), "x%u", count);
  float y = slot.y + slot.h - (float)TTF_GetFontHeight(font) - 4.0f;
  gui_draw_text(r, font, text, slot.x + 4.0f, y, GUI_BADGE_TEXT);
} // draw_count_badge

static void draw_hand(SDL_Renderer* r, TTF_Font* font, SDL_FRect row, PlayerID p,
                      PlayerID viewer, const VisibleGameState* v,
                      const GuiInputState* input, ui_language_t lang)
{ bool face_up = (p == viewer);
  uint8_t count = face_up ? v->my_hand.size : v->hand_count[p];

  for(uint8_t i = 0; i < count; i++)
  { SDL_FRect slot = gui_layout_card_slot(row, i, count);
    if(face_up)
    { uint8_t card = Hand_get(&v->my_hand, i);
      gui_card_draw(r, font, slot, card, card_is_highlighted(input, card), lang);
    }
    else
      gui_card_draw_back(r, slot);
  }
} // draw_hand

static void draw_deck(SDL_Renderer* r, TTF_Font* font, SDL_FRect slot, uint8_t count)
{ if(count == 0)
    return;
  gui_card_draw_back(r, slot);
  draw_count_badge(r, font, slot, count);
} // draw_deck

static void draw_discard(SDL_Renderer* r, TTF_Font* font, SDL_FRect slot,
                         const Discard* d, ui_language_t lang)
{ if(d->size == 0)
    return;
  gui_card_draw(r, font, slot, Discard_get(d, d->size - 1), false, lang);
  draw_count_badge(r, font, slot, d->size);
} // draw_discard

static void draw_combat_zone(SDL_Renderer* r, TTF_Font* font, SDL_FRect row,
                             const CombatZone* z, ui_language_t lang)
{ for(uint8_t i = 0; i < z->size; i++)
  { SDL_FRect slot = gui_layout_card_slot(row, i, z->size);
    gui_card_draw(r, font, slot, CombatZone_get(z, i), false, lang);
  }
} // draw_combat_zone

void gui_render_frame(SDL_Renderer* renderer, const GuiFonts* fonts,
                      const SessionUpdate* u, const GuiInputState* input,
                      const GuiLog* log, ui_language_t lang)
{ int win_w, win_h;
  SDL_GetRenderOutputSize(renderer, &win_w, &win_h);

  GuiLayout layout;
  gui_layout_compute((float)win_w, (float)win_h, &layout);

  draw_status_bar(renderer, fonts->status_font, layout.status_bar, u, lang);
  draw_action_bar(renderer, fonts->status_font, layout.action_bar, u, input, lang);
  gui_log_draw(renderer, fonts->status_font, layout.log_panel, log);

  for(uint8_t p = 0; p < NUM_PLAYERS; p++)
  { uint8_t seat = gui_layout_seat_for_player((PlayerID)p, u->view.viewer);
    GuiSeatLayout* sl = &layout.seat[seat];

    draw_seat_info(renderer, fonts->status_font, sl->info, (PlayerID)p, &u->view, lang);
    draw_hand(renderer, fonts->card_font, sl->hand, (PlayerID)p, u->view.viewer, &u->view,
              input, lang);
    draw_deck(renderer, fonts->card_font, sl->deck, u->view.deck_count[p]);
    draw_discard(renderer, fonts->card_font, sl->discard, &u->view.discard[p], lang);
    draw_combat_zone(renderer, fonts->card_font, layout.combat_zone[seat],
                     &u->view.combat_zone[p], lang);
  }

  if(input && input->kind == DECISION_KIND_ATTACK
     && input->attack_mode == ATTACK_INPUT_RECALL_PICK)
  { SDL_FRect area = gui_layout_overlay_area((float)win_w, (float)win_h);
    draw_recall_overlay(renderer, fonts->card_font, area, &u->view.discard[input->player],
                        input, lang);
  }
} // gui_render_frame
