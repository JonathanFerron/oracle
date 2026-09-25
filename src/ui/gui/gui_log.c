// gui_log.c -- see gui_log.h.

#include <stdio.h>
#include <string.h>

#include "gui_log.h"
#include "gui_card.h" // gui_draw_text()
#include "../../core/game_constants.h" // fullDeck[], CHAMPION_SPECIES_NAMES
#include "../shared/localization.h"

#define GUI_LOG_BG (SDL_Color){ 0x0A, 0x1F, 0x22, 220 }
#define GUI_LOG_TEXT (SDL_Color){ 0xE0, 0xE0, 0xE0, 255 }

void gui_log_init(GuiLog* log)
{ memset(log, 0, sizeof(*log));
} // gui_log_init

static void push_line(GuiLog* log, const char* text)
{ snprintf(log->lines[log->next], GUI_LOG_LINE_LEN, "%s", text);
  log->next = (uint16_t)((log->next + 1) % GUI_LOG_CAPACITY);
  if(log->count < GUI_LOG_CAPACITY) log->count++;
} // push_line

static char player_letter(PlayerID p)
{ return p == PLAYER_A ? 'A' : 'B';
} // player_letter

// Species name for a champion, or a generic label for a draw/cash card --
// species names stay English/unlocalized everywhere in this codebase (see
// game_constants.c's own comment on CHAMPION_SPECIES_NAMES).
static void card_label(uint8_t card, char* buf, size_t n, ui_language_t lang)
{ const struct card* c = &fullDeck[card];
  switch(c->card_type)
  { case CHAMPION_CARD:
      snprintf(buf, n, "%s", CHAMPION_SPECIES_NAMES[c->species]);
      return;
    case DRAW_CARD:
      snprintf(buf, n, "%s %u", LOCALIZED_STRING_L(lang, "Draw", "Pige", "Roba"), c->draw_num);
      return;
    case CASH_CARD:
      snprintf(buf, n, "%s", LOCALIZED_STRING_L(lang, "Cash", "Argent", "Dinero"));
      return;
  }
} // card_label

// Joins up to 3 card labels with ", " into buf -- used for champion moves/
// recalls/mulligan/discard-to-7 lines.
static void card_list(const uint8_t* cards, uint8_t count, char* buf, size_t n,
                      ui_language_t lang)
{ buf[0] = '\0';
  for(uint8_t i = 0; i < count; i++)
  { char one[32];
    card_label(cards[i], one, sizeof(one), lang);
    size_t used = strlen(buf);
    snprintf(buf + used, n - used, "%s%s", i > 0 ? ", " : "", one);
  }
} // card_list

static void format_move_played(const GameEvent* e, char* text, size_t n, ui_language_t lang)
{ char letter = player_letter(e->player);
  const GameMove* m = &e->u.move;
  char list[128];

  switch(m->type)
  { case MOVE_PASS:
      snprintf(text, n, "%s %c %s.", LOCALIZED_STRING_L(lang, "Player", "Joueur", "Jugador"),
               letter, LOCALIZED_STRING_L(lang, "passed", "a passe", "paso"));
      return;
    case MOVE_CHAMPIONS:
      card_list(m->cards, m->count, list, sizeof(list), lang);
      snprintf(text, n, "%s %c %s: %s", LOCALIZED_STRING_L(lang, "Player", "Joueur", "Jugador"),
               letter, LOCALIZED_STRING_L(lang, "played", "a joue", "jugo"), list);
      return;
    case MOVE_DRAW:
      snprintf(text, n, "%s %c %s %u %s.",
               LOCALIZED_STRING_L(lang, "Player", "Joueur", "Jugador"), letter,
               LOCALIZED_STRING_L(lang, "drew", "a pige", "robo"),
               fullDeck[m->card].draw_num, LOCALIZED_STRING_L(lang, "cards", "cartes", "cartas"));
      return;
    case MOVE_RECALL:
      card_list(m->recall, m->count, list, sizeof(list), lang);
      snprintf(text, n, "%s %c %s: %s", LOCALIZED_STRING_L(lang, "Player", "Joueur", "Jugador"),
               letter, LOCALIZED_STRING_L(lang, "recalled", "a rappele", "recordo"), list);
      return;
    case MOVE_CASH:
      card_list(m->cards, 1, list, sizeof(list), lang);
      snprintf(text, n, "%s %c %s %s %s (+%u).",
               LOCALIZED_STRING_L(lang, "Player", "Joueur", "Jugador"), letter,
               LOCALIZED_STRING_L(lang, "exchanged", "a echange", "cambio"), list,
               LOCALIZED_STRING_L(lang, "for cash", "contre argent", "por dinero"),
               fullDeck[m->card].exchange_cash);
      return;
  }
} // format_move_played

static void format_combat_resolved(const GameEvent* e, char* text, size_t n)
{ const CombatDetails* c = &e->u.combat;
  PlayerID defender = e->player == PLAYER_A ? PLAYER_B : PLAYER_A;
  snprintf(text, n, "Combat: attack %d vs defense %d -- Player %c takes %d damage (%u -> %u).",
           c->total_attack, c->total_defense, player_letter(defender), c->damage,
           c->defender_energy_before, c->defender_energy_after);
} // format_combat_resolved

static bool format_event(const GameEvent* e, const VisibleGameState* view, char* text, size_t n,
                         ui_language_t lang)
{ char letter = player_letter(e->player);
  char list[128];

  switch(e->type)
  { case EVT_GAME_STARTED:
      snprintf(text, n, "%s", LOCALIZED_STRING_L(lang, "Game started.", "Partie commencee.",
                                                 "Partida iniciada."));
      return true;
    case EVT_MULLIGAN_DONE:
      snprintf(text, n, "%s %c %s %u %s.", LOCALIZED_STRING_L(lang, "Player", "Joueur", "Jugador"),
               letter, LOCALIZED_STRING_L(lang, "mulliganed", "a fait un mulligan de",
                                          "hizo mulligan de"),
               e->u.cards.size, LOCALIZED_STRING_L(lang, "card(s)", "carte(s)", "carta(s)"));
      return true;
    case EVT_TURN_BEGAN:
      snprintf(text, n, "-- %s %u: %s %c --", LOCALIZED_STRING_L(lang, "Turn", "Tour", "Turno"),
               e->u.turn, LOCALIZED_STRING_L(lang, "Player", "Joueur", "Jugador"), letter);
      return true;
    case EVT_CARD_DRAWN:
      if(e->u.card == EVT_CARD_REDACTED)
        snprintf(text, n, "%s %c %s.", LOCALIZED_STRING_L(lang, "Player", "Joueur", "Jugador"),
                 letter, LOCALIZED_STRING_L(lang, "drew a card", "a pige une carte",
                                            "robo una carta"));
      else
      { char one[32];
        card_label(e->u.card, one, sizeof(one), lang);
        snprintf(text, n, "%s %c %s %s.", LOCALIZED_STRING_L(lang, "Player", "Joueur", "Jugador"),
                 letter, LOCALIZED_STRING_L(lang, "drew", "a pige", "robo"), one);
      }
      return true;
    case EVT_DECK_RESHUFFLED:
      snprintf(text, n, "%s %c %s.", LOCALIZED_STRING_L(lang, "Player", "Joueur", "Jugador"),
               letter, LOCALIZED_STRING_L(lang, "reshuffled their discard into their deck",
                                          "a remelange sa defausse dans son paquet",
                                          "barajo su descarte en su mazo"));
      return true;
    case EVT_MOVE_PLAYED:
      format_move_played(e, text, n, lang);
      return true;
    case EVT_COMBAT_RESOLVED:
      format_combat_resolved(e, text, n);
      return true;
    case EVT_LUNA_COLLECTED:
      snprintf(text, n, "%s %c %s.", LOCALIZED_STRING_L(lang, "Player", "Joueur", "Jugador"),
               letter, LOCALIZED_STRING_L(lang, "collected 1 luna", "a recolte 1 luna",
                                          "recolecto 1 luna"));
      return true;
    case EVT_DISCARDED_TO_7:
      card_list(e->u.cards.cards, e->u.cards.size, list, sizeof(list), lang);
      snprintf(text, n, "%s %c %s: %s", LOCALIZED_STRING_L(lang, "Player", "Joueur", "Jugador"),
               letter, LOCALIZED_STRING_L(lang, "discarded", "a defausse", "descarto"), list);
      return true;
    case EVT_GAME_OVER:
    { const char* outcome =
        view->game_state == PLAYER_A_WINS ?
        LOCALIZED_STRING_L(lang, "Player A wins!", "Le joueur A gagne !", "Gana el jugador A!") :
        view->game_state == PLAYER_B_WINS ?
        LOCALIZED_STRING_L(lang, "Player B wins!", "Le joueur B gagne !", "Gana el jugador B!") :
        LOCALIZED_STRING_L(lang, "Draw!", "Match nul !", "Empate!");
      snprintf(text, n, "%s %s", LOCALIZED_STRING_L(lang, "Game over --", "Partie terminee --",
                                                    "Partida terminada --"), outcome);
      return true;
    }
    default:
      return false;
  }
} // format_event

void gui_log_append_events(GuiLog* log, const SessionUpdate* u, ui_language_t lang)
{ for(uint16_t i = 0; i < u->events.count; i++)
  { char text[GUI_LOG_LINE_LEN];
    if(format_event(&u->events.ev[i], &u->view, text, sizeof(text), lang))
      push_line(log, text);
  }
} // gui_log_append_events

void gui_log_draw(SDL_Renderer* r, TTF_Font* font, SDL_FRect area, const GuiLog* log)
{ SDL_SetRenderDrawColor(r, GUI_LOG_BG.r, GUI_LOG_BG.g, GUI_LOG_BG.b, GUI_LOG_BG.a);
  SDL_RenderFillRect(r, &area);
  SDL_SetRenderDrawColor(r, 255, 255, 255, 120);
  SDL_RenderRect(r, &area);

  int line_h = TTF_GetFontHeight(font) + 2;
  if(line_h <= 0 || log->count == 0) return;

  int max_lines = (int)(area.h / (float)line_h);
  if(max_lines <= 0) return;

  int n = log->count < (uint16_t)max_lines ? log->count : max_lines;
  int newest = (log->next - 1 + GUI_LOG_CAPACITY) % GUI_LOG_CAPACITY;
  int start = (newest - (n - 1) + 2 * GUI_LOG_CAPACITY) % GUI_LOG_CAPACITY;

  float y = area.y + 4.0f;
  for(int i = 0; i < n; i++)
  { int idx = (start + i) % GUI_LOG_CAPACITY;
    gui_draw_text(r, font, log->lines[idx], area.x + 8.0f, y, GUI_LOG_TEXT);
    y += (float)line_h;
  }
} // gui_log_draw
