// gui_combat_panel.c -- see gui_combat_panel.h.

#include <stdio.h>

#include "gui_combat_panel.h"
#include "gui_card.h"
#include "gui_dice.h"
#include "gui_palette.h"
#include "gui_card_face.h"
#include "../../core/game_constants.h" // fullDeck[]
#include "../shared/localization.h"

#define GUI_PANEL_INK (SDL_Color){ 0x0F, 0x2B, 0x30, 255 }
#define GUI_PANEL_DIE_RADIUS 24.0f
#define GUI_PANEL_TEXT_PT 20.0f
#define GUI_PANEL_BIG_PT 24.0f

// One side of the combat: what to draw for it.
typedef struct
{ int count;
  const uint8_t* cards;
  const uint8_t* dice;
  const uint8_t* rolls;
  const uint8_t* base; // NULL for the defender (no base attack)
  const int16_t* totals;
  const ChampionColor* colors;
  bool revealed;       // dice values shown (else empty n-gons)
} PanelSide;

static void draw_text_px(SDL_Renderer* r, TTF_Font* font, float px, const char* text,
                         float x, float y)
{ TTF_SetFontSize(font, px);
  gui_draw_text(r, font, text, SDL_roundf(x), SDL_roundf(y), GUI_PANEL_INK);
} // draw_text_px

// Card `i` in the combat zone and its die (+ "+base = total") in the dice row.
static void draw_champion(SDL_Renderer* r, TTF_Font* card_font, TTF_Font* text_font,
                          const GuiSeatRects* rects, const PanelSide* s, int i, ui_language_t lang)
{ SDL_FRect slot = gui_layout_card_slot(rects->zone, (uint8_t)i, (uint8_t)s->count);
  gui_card_draw(r, card_font, slot, s->cards[i], false, lang);

  float cy = rects->dice.y + rects->dice.h / 2.0f;
  float cx = slot.x + GUI_PANEL_DIE_RADIUS + 6.0f;
  int value = s->revealed ? s->rolls[i] : -1;
  gui_dice_draw(r, gui_card_face_font(), cx, cy, GUI_PANEL_DIE_RADIUS, s->dice[i],
                gui_champion_colour(s->colors[i]), value);

  if(s->base && s->revealed)
  { char buf[24];
    snprintf(buf, sizeof(buf), "+%u = %d", s->base[i], s->totals[i]);
    int w = 0, h = 0;
    TTF_SetFontSize(text_font, GUI_PANEL_TEXT_PT);
    TTF_GetStringSize(text_font, buf, 0, &w, &h);
    draw_text_px(r, text_font, GUI_PANEL_TEXT_PT, buf, cx + GUI_PANEL_DIE_RADIUS + 8.0f,
                 cy - (float)h / 2.0f);
  }
} // draw_champion

// Stacks lines top-down in the side info rect; returns the y after the last.
static float info_line(SDL_Renderer* r, TTF_Font* font, const GuiSeatRects* rects, float y,
                       float px, const char* text)
{ draw_text_px(r, font, px, text, rects->info.x, y);
  return y + (float)TTF_GetFontHeight(font) + 2.0f;
} // info_line

static void draw_total(SDL_Renderer* r, TTF_Font* font, const GuiSeatRects* rects,
                       const char* label, int combo, int total, ui_language_t lang, float* y)
{ char buf[48];
  snprintf(buf, sizeof(buf), "%s %+d", LOCALIZED_STRING_L(lang, "Combo", "Combo", "Combo"), combo);
  *y = info_line(r, font, rects, *y, GUI_PANEL_TEXT_PT, buf);
  snprintf(buf, sizeof(buf), "%s %d", label, total);
  *y = info_line(r, font, rects, *y, GUI_PANEL_BIG_PT, buf);
} // draw_total

static void draw_outcome(SDL_Renderer* r, TTF_Font* font, const GuiSeatRects* rects,
                         const CombatDetails* d, ui_language_t lang, float* y)
{ char buf[64];
  snprintf(buf, sizeof(buf), "%s %d", LOCALIZED_STRING_L(lang, "Damage", "Degats", "Dano"),
           d->damage);
  *y = info_line(r, font, rects, *y, GUI_PANEL_BIG_PT, buf);
  snprintf(buf, sizeof(buf), "%s %u -> %u",
           LOCALIZED_STRING_L(lang, "Energy", "Energie", "Energia"),
           d->defender_energy_before, d->defender_energy_after);
  *y = info_line(r, font, rects, *y, GUI_PANEL_TEXT_PT, buf);
} // draw_outcome

static void draw_side(SDL_Renderer* r, TTF_Font* card_font, TTF_Font* text_font,
                      const GuiSeatRects* rects, const PanelSide* s, ui_language_t lang)
{ for(int i = 0; i < s->count; i++)
    draw_champion(r, card_font, text_font, rects, s, i, lang);
} // draw_side

void gui_combat_panel_draw(SDL_Renderer* r, TTF_Font* card_font, TTF_Font* text_font,
                           const GuiLayout* layout, const CombatDetails* d, PlayerID attacker,
                           PlayerID viewer, RevealStage stage, ui_language_t lang)
{ float old_pt = TTF_GetFontSize(text_font);
  PlayerID defender = (PlayerID)((attacker + 1) % NUM_PLAYERS);
  GuiSeatRects a_rects = gui_layout_seat_rects(layout, gui_layout_seat_for_player(attacker, viewer));
  GuiSeatRects d_rects = gui_layout_seat_rects(layout, gui_layout_seat_for_player(defender, viewer));
  bool full = stage == REVEAL_FULL;

  PanelSide att = { d->num_attackers, d->attacker_card, d->attacker_dice, d->attacker_rolls,
                    d->attacker_base, d->attacker_total, d->attacker_color, true
                  };
  PanelSide def = { d->num_defenders, d->defender_card, d->defender_dice, d->defender_rolls,
                    NULL, d->defender_total, d->defender_color, full
                  };
  draw_side(r, card_font, text_font, &a_rects, &att, lang);
  draw_side(r, card_font, text_font, &d_rects, &def, lang);

  float y = a_rects.info.y;
  draw_total(r, text_font, &a_rects, LOCALIZED_STRING_L(lang, "Attack", "Attaque", "Ataque"),
             d->attack_combo, d->total_attack, lang, &y);
  if(full)
  { y = d_rects.info.y;
    draw_total(r, text_font, &d_rects, LOCALIZED_STRING_L(lang, "Defense", "Defense", "Defensa"),
               d->defense_combo, d->total_defense, lang, &y);
    draw_outcome(r, text_font, &d_rects, d, lang, &y);
  }
  TTF_SetFontSize(text_font, old_pt);
} // gui_combat_panel_draw
