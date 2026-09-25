// gui_input.c -- see gui_input.h.

#include <string.h>

#include "gui_input.h"
#include "gui_layout.h"
#include "gui_render.h" // gui_discard_champions()
#include "../../core/game_constants.h" // fullDeck[]

void gui_input_reset(GuiInputState* st, DecisionKind kind, PlayerID player)
{ memset(st, 0, sizeof(*st));
  st->kind = kind;
  st->player = player;
} // gui_input_reset

// ---- legality mirrors against SessionUpdate.legal[] (UX only -- see
// gui_input.h's file comment) ----

static bool set_contains(const uint8_t* set, uint8_t n, uint8_t val)
{ for(uint8_t i = 0; i < n; i++)
    if(set[i] == val) return true;
  return false;
} // set_contains

static bool sets_match(const uint8_t* a, uint8_t na, const uint8_t* b, uint8_t nb)
{ if(na != nb) return false;
  for(uint8_t i = 0; i < na; i++)
    if(!set_contains(b, nb, a[i])) return false;
  return true;
} // sets_match

static bool legal_has_type_card(const SessionUpdate* u, MoveType type, uint8_t card)
{ for(uint8_t i = 0; i < u->legal_count; i++)
    if(u->legal[i].type == type && u->legal[i].card == card) return true;
  return false;
} // legal_has_type_card

static bool champions_selection_is_legal(const SessionUpdate* u, const uint8_t* staged,
                                         uint8_t count)
{ for(uint8_t i = 0; i < u->legal_count; i++)
  { const GameMove* m = &u->legal[i];
    if(m->type == MOVE_CHAMPIONS && sets_match(m->cards, m->count, staged, count))
      return true;
  }
  return false;
} // champions_selection_is_legal

static bool cash_target_is_legal(const SessionUpdate* u, uint8_t cash_card, uint8_t target)
{ for(uint8_t i = 0; i < u->legal_count; i++)
  { const GameMove* m = &u->legal[i];
    if(m->type == MOVE_CASH && m->card == cash_card && m->cards[0] == target) return true;
  }
  return false;
} // cash_target_is_legal

static bool recall_selection_is_legal(const SessionUpdate* u, uint8_t recall_card,
                                      const uint8_t* staged, uint8_t count)
{ for(uint8_t i = 0; i < u->legal_count; i++)
  { const GameMove* m = &u->legal[i];
    if(m->type == MOVE_RECALL && m->card == recall_card
       && sets_match(m->recall, m->count, staged, count))
      return true;
  }
  return false;
} // recall_selection_is_legal

// Whether `card` is worth letting the player click at all (doc: "clicking
// cards that can't be part of any legal move does nothing") -- already
// staged is always clickable, so a toggle can always undo itself.
static bool champion_is_clickable(const SessionUpdate* u, const uint8_t* staged,
                                  uint8_t count, uint8_t card)
{ if(set_contains(staged, count, card)) return true;
  for(uint8_t i = 0; i < u->legal_count; i++)
  { const GameMove* m = &u->legal[i];
    if(m->type == MOVE_CHAMPIONS && set_contains(m->cards, m->count, card)) return true;
  }
  return false;
} // champion_is_clickable

// ---- button list (shared source of truth for gui_render.c's drawing and
// this file's own hit-testing) ----

uint8_t gui_input_active_buttons(const GuiInputState* st, const SessionUpdate* u,
                                 GuiButtonId out[GUI_MAX_BUTTONS], bool* out_confirm_enabled)
{ if(out_confirm_enabled)
    *out_confirm_enabled = false;

  switch(st->kind)
  { case DECISION_KIND_MULLIGAN:
      out[0] = GUI_BTN_CONFIRM;
      if(out_confirm_enabled)
        *out_confirm_enabled = true; // any 0..max_cards subset is legal
      return 1;

    case DECISION_KIND_DISCARD_TO_7:
      out[0] = GUI_BTN_CONFIRM;
      if(out_confirm_enabled)
        *out_confirm_enabled = (st->staged_count == u->min_cards);
      return 1;

    case DECISION_KIND_ATTACK:
      switch(st->attack_mode)
      { case ATTACK_INPUT_NONE:
          out[0] = GUI_BTN_PASS;
          out[1] = GUI_BTN_CONFIRM;
          if(out_confirm_enabled)
            *out_confirm_enabled = champions_selection_is_legal(u, st->staged, st->staged_count);
          return 2;
        case ATTACK_INPUT_SPECIAL:
        { uint8_t n = 0;
          if(legal_has_type_card(u, MOVE_DRAW, st->special_card)) out[n++] = GUI_BTN_DRAW;
          if(legal_has_type_card(u, MOVE_RECALL, st->special_card)) out[n++] = GUI_BTN_RECALL;
          out[n++] = GUI_BTN_CANCEL;
          return n;
        }
        case ATTACK_INPUT_CASH_TARGET:
          out[0] = GUI_BTN_CANCEL;
          out[1] = GUI_BTN_CONFIRM;
          if(out_confirm_enabled)
            *out_confirm_enabled = st->staged_count == 1
                                   && cash_target_is_legal(u, st->special_card, st->staged[0]);
          return 2;
        case ATTACK_INPUT_RECALL_PICK:
          out[0] = GUI_BTN_CANCEL;
          out[1] = GUI_BTN_CONFIRM;
          if(out_confirm_enabled)
            *out_confirm_enabled = recall_selection_is_legal(u, st->special_card,
                                                             st->recall_staged, st->recall_staged_count);
          return 2;
      }
      return 0;

    case DECISION_KIND_DEFENSE:
      out[0] = GUI_BTN_DECLINE;
      out[1] = GUI_BTN_CONFIRM;
      if(out_confirm_enabled)
        *out_confirm_enabled = champions_selection_is_legal(u, st->staged, st->staged_count);
      return 2;

    default:
      return 0;
  }
} // gui_input_active_buttons

// ---- staging mutators ----

static void toggle_card(uint8_t* arr, uint8_t* count, uint8_t max, uint8_t card)
{ for(uint8_t i = 0; i < *count; i++)
    if(arr[i] == card)
    { for(uint8_t j = i; j + 1 < *count; j++) arr[j] = arr[j + 1];
      (*count)--;
      return;
    }
  if(*count < max) arr[(*count)++] = card;
} // toggle_card

static void reset_attack_substate(GuiInputState* st)
{ st->attack_mode = ATTACK_INPUT_NONE;
  st->special_card = 0;
  st->staged_count = 0;
  st->recall_staged_count = 0;
} // reset_attack_substate

// ---- command builders ----

static void submit_move(SessionCommand* out, DecisionKind kind, GameMove move)
{ out->type = CMD_SUBMIT;
  out->decision = (PlayerDecision)
  { .kind = kind, .move = move
  };
} // submit_move

static void submit_cards(SessionCommand* out, DecisionKind kind, const uint8_t* cards,
                         uint8_t count)
{ out->type = CMD_SUBMIT;
  PlayerDecision d = { .kind = kind, .count = count };
  memcpy(d.cards, cards, count);
  out->decision = d;
} // submit_cards

// ---- hit-testing ----

static int hit_test_row(SDL_FRect row, uint8_t count, float x, float y)
{ SDL_FPoint p = { x, y };
  for(uint8_t i = 0; i < count; i++)
  { SDL_FRect slot = gui_layout_card_slot(row, i, count);
    if(SDL_PointInRectFloat(&p, &slot)) return i;
  }
  return -1;
} // hit_test_row

static int hit_test_grid(SDL_FRect area, uint8_t count, float x, float y)
{ SDL_FPoint p = { x, y };
  for(uint8_t i = 0; i < count; i++)
  { SDL_FRect slot = gui_layout_grid_slot(area, i, count);
    if(SDL_PointInRectFloat(&p, &slot)) return i;
  }
  return -1;
} // hit_test_grid

// Returns the button hit (or GUI_BTN_NONE), given the same button list
// gui_render.c drew this frame.
static GuiButtonId hit_test_buttons(const GuiInputState* st, const SessionUpdate* u,
                                    SDL_FRect action_bar, float x, float y,
                                    bool* confirm_enabled)
{ GuiButtonId buttons[GUI_MAX_BUTTONS];
  uint8_t n = gui_input_active_buttons(st, u, buttons, confirm_enabled);
  SDL_FPoint p = { x, y };
  for(uint8_t i = 0; i < n; i++)
  { SDL_FRect r = gui_layout_button_rect(action_bar, i, n);
    if(SDL_PointInRectFloat(&p, &r)) return buttons[i];
  }
  return GUI_BTN_NONE;
} // hit_test_buttons

// Builds the MOVE_CHAMPIONS move for whichever of ATTACK/DEFENSE's own
// 1-3-champion toggle staged[]/staged_count currently holds.
static GameMove champions_move(const GuiInputState* st)
{ GameMove m = { .type = MOVE_CHAMPIONS, .count = st->staged_count };
  memcpy(m.cards, st->staged, st->staged_count);
  return m;
} // champions_move

static bool handle_button(GuiInputState* st, GuiButtonId btn, bool confirm_enabled,
                          SessionCommand* out_cmd)
{ switch(btn)
  { case GUI_BTN_PASS:
      submit_move(out_cmd, DECISION_KIND_ATTACK, (GameMove)
      { .type = MOVE_PASS
      });
      return true;
    case GUI_BTN_DECLINE:
      submit_move(out_cmd, DECISION_KIND_DEFENSE, (GameMove)
      { .type = MOVE_PASS
      });
      return true;
    case GUI_BTN_DRAW:
      submit_move(out_cmd, DECISION_KIND_ATTACK,
                  (GameMove)
      { .type = MOVE_DRAW, .card = st->special_card
      });
      return true;
    case GUI_BTN_RECALL:
      st->attack_mode = ATTACK_INPUT_RECALL_PICK;
      st->recall_staged_count = 0;
      return false;
    case GUI_BTN_CANCEL:
      reset_attack_substate(st);
      return false;
    case GUI_BTN_CONFIRM:
      if(!confirm_enabled) return false;
      switch(st->kind)
      { case DECISION_KIND_MULLIGAN:
        case DECISION_KIND_DISCARD_TO_7:
          submit_cards(out_cmd, st->kind, st->staged, st->staged_count);
          return true;
        case DECISION_KIND_DEFENSE:
          submit_move(out_cmd, DECISION_KIND_DEFENSE, champions_move(st));
          return true;
        case DECISION_KIND_ATTACK:
          if(st->attack_mode == ATTACK_INPUT_CASH_TARGET)
          { GameMove m = { .type = MOVE_CASH, .card = st->special_card, .count = 1,
                           .cards = { st->staged[0] }
                         };
            submit_move(out_cmd, DECISION_KIND_ATTACK, m);
          }
          else if(st->attack_mode == ATTACK_INPUT_RECALL_PICK)
          { GameMove m = { .type = MOVE_RECALL, .card = st->special_card,
                           .count = st->recall_staged_count
                         };
            memcpy(m.recall, st->recall_staged, st->recall_staged_count);
            submit_move(out_cmd, DECISION_KIND_ATTACK, m);
          }
          else
            submit_move(out_cmd, DECISION_KIND_ATTACK, champions_move(st));
          return true;
        default:
          return false;
      }
    default:
      return false;
  }
} // handle_button

static void handle_hand_click(GuiInputState* st, const SessionUpdate* u, uint8_t card)
{ switch(st->kind)
  { case DECISION_KIND_MULLIGAN:
      toggle_card(st->staged, &st->staged_count, u->max_cards, card);
      return;
    case DECISION_KIND_DISCARD_TO_7:
      toggle_card(st->staged, &st->staged_count, u->max_cards, card);
      return;
    case DECISION_KIND_DEFENSE:
      if(champion_is_clickable(u, st->staged, st->staged_count, card))
        toggle_card(st->staged, &st->staged_count, 3, card);
      return;
    case DECISION_KIND_ATTACK:
      break;
    default:
      return;
  }

  // ATTACK, mode NONE: dispatch on what kind of card was clicked.
  if(st->attack_mode != ATTACK_INPUT_NONE) return;

  switch(fullDeck[card].card_type)
  { case CHAMPION_CARD:
      if(champion_is_clickable(u, st->staged, st->staged_count, card))
        toggle_card(st->staged, &st->staged_count, 3, card);
      return;
    case DRAW_CARD:
      if(legal_has_type_card(u, MOVE_DRAW, card) || legal_has_type_card(u, MOVE_RECALL, card))
      { st->attack_mode = ATTACK_INPUT_SPECIAL;
        st->special_card = card;
      }
      return;
    case CASH_CARD:
      if(legal_has_type_card(u, MOVE_CASH, card))
      { st->attack_mode = ATTACK_INPUT_CASH_TARGET;
        st->special_card = card;
        st->staged_count = 0;
      }
      return;
  }
} // handle_hand_click

bool gui_input_handle_click(GuiInputState* st, const SessionUpdate* u,
                            float x, float y, float win_w, float win_h,
                            SessionCommand* out_cmd)
{ if(u->pending.player != u->view.viewer || u->pending.kind != st->kind
     || u->pending.player != st->player)
    return false; // not our move, or staging is stale for a decision that's already gone

  GuiLayout layout;
  gui_layout_compute(win_w, win_h, &layout);

  bool confirm_enabled = false;
  GuiButtonId btn = hit_test_buttons(st, u, layout.action_bar, x, y, &confirm_enabled);
  if(btn != GUI_BTN_NONE)
    return handle_button(st, btn, confirm_enabled, out_cmd);

  if(st->kind == DECISION_KIND_ATTACK && st->attack_mode == ATTACK_INPUT_RECALL_PICK)
  { uint8_t champs[40];
    uint8_t n = gui_discard_champions(&u->view.discard[st->player], champs);
    SDL_FRect area = gui_layout_overlay_area(win_w, win_h);
    int idx = hit_test_grid(area, n, x, y);
    if(idx >= 0)
      toggle_card(st->recall_staged, &st->recall_staged_count,
                  fullDeck[st->special_card].choose_num, champs[idx]);
    return false;
  }

  SDL_FRect hand_row = layout.seat[0].hand;
  uint8_t hand_count = u->view.my_hand.size;
  int idx = hit_test_row(hand_row, hand_count, x, y);
  if(idx < 0) return false;

  uint8_t card = Hand_get(&u->view.my_hand, (uint8_t)idx);
  if(st->kind == DECISION_KIND_ATTACK && st->attack_mode == ATTACK_INPUT_CASH_TARGET)
  { if(card != st->special_card)
    { st->staged[0] = card;
      st->staged_count = 1;
    }
    return false;
  }

  handle_hand_click(st, u, card);
  return false;
} // gui_input_handle_click
