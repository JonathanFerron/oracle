// gui_card.c -- see gui_card.h.

#include <stdio.h>

#include "gui_card.h"
#include "gui_palette.h"
#include "../../core/game_constants.h" // fullDeck[], CHAMPION_SPECIES_NAMES
#include "../shared/localization.h"

#define GUI_CARD_BORDER_PX 3.0f
#define GUI_CARD_BORDER_HIGHLIGHT_PX 6.0f
#define GUI_CARD_BACK_R 0x21
#define GUI_CARD_BACK_G 0x67
#define GUI_CARD_BACK_B 0x78
#define GUI_CARD_FACE_R 0xF5
#define GUI_CARD_FACE_G 0xF5
#define GUI_CARD_FACE_B 0xF0
#define GUI_CARD_TEXT_LINE_GAP 4.0f

void gui_draw_text(SDL_Renderer* renderer, TTF_Font* font, const char* text,
                   float x, float y, SDL_Color colour)
{ if(!font || !text || !text[0])
    return;

  SDL_Surface* surf = TTF_RenderText_Blended(font, text, 0, colour);
  if(!surf)
    return;

  SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
  SDL_DestroySurface(surf);
  if(!tex)
    return;

  SDL_FRect dst = { x, y, (float)tex->w, (float)tex->h };
  SDL_RenderTexture(renderer, tex, NULL, &dst);
  SDL_DestroyTexture(tex);
} // gui_draw_text

void gui_card_draw_back(SDL_Renderer* renderer, SDL_FRect rect)
{ SDL_SetRenderDrawColor(renderer, GUI_CARD_BACK_R, GUI_CARD_BACK_G, GUI_CARD_BACK_B, 255);
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
  SDL_RenderRect(renderer, &rect);
} // gui_card_draw_back

// Text lines drawn inside a face-up card, top to bottom -- everything a
// human needs to decide with, no art. `lines` is filled in by
// champion_card_lines()/draw_card_lines()/cash_card_lines() below;
// `*count` is how many of MAX_CARD_LINES it actually used.
#define MAX_CARD_LINES 4

static void champion_card_lines(const struct card* c, char lines[][40], uint8_t* count)
{ snprintf(lines[0], 40, "%s", CHAMPION_SPECIES_NAMES[c->species]);
  snprintf(lines[1], 40, "Cost %u", c->cost);
  snprintf(lines[2], 40, "d%u  base %u", c->defense_dice, c->attack_base);
  *count = 3;
} // champion_card_lines

static void draw_card_lines(const struct card* c, char lines[][40], uint8_t* count,
                            ui_language_t lang)
{ snprintf(lines[0], 40, "%s %u",
           LOCALIZED_STRING_L(lang, "Draw", "Pige", "Roba"), c->draw_num);
  snprintf(lines[1], 40, "%s %s %u",
           LOCALIZED_STRING_L(lang, "or", "ou", "o"),
           LOCALIZED_STRING_L(lang, "Recall", "Rappelle", "Recuerda"), c->choose_num);
  *count = 2;
} // draw_card_lines

static void cash_card_lines(const struct card* c, char lines[][40], uint8_t* count,
                            ui_language_t lang)
{ snprintf(lines[0], 40, "%s", LOCALIZED_STRING_L(lang, "Cash", "Argent", "Dinero"));
  snprintf(lines[1], 40, "+%u", c->exchange_cash);
  *count = 2;
} // cash_card_lines

void gui_card_draw(SDL_Renderer* renderer, TTF_Font* font, SDL_FRect rect,
                   uint8_t card_index, bool highlighted, ui_language_t lang)
{ const struct card* c = &fullDeck[card_index];
  SDL_Color border = gui_card_border_colour(c);

  SDL_SetRenderDrawColor(renderer, GUI_CARD_FACE_R, GUI_CARD_FACE_G, GUI_CARD_FACE_B, 255);
  SDL_RenderFillRect(renderer, &rect);

  float bw = highlighted ? GUI_CARD_BORDER_HIGHLIGHT_PX : GUI_CARD_BORDER_PX;
  SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, 255);
  for(float i = 0; i < bw; i++)
  { SDL_FRect ring = { rect.x + i, rect.y + i, rect.w - 2 * i, rect.h - 2 * i };
    SDL_RenderRect(renderer, &ring);
  }

  char lines[MAX_CARD_LINES][40];
  uint8_t count = 0;
  switch(c->card_type)
  { case CHAMPION_CARD:
      champion_card_lines(c, lines, &count);
      break;
    case DRAW_CARD:
      draw_card_lines(c, lines, &count, lang);
      break;
    case CASH_CARD:
      cash_card_lines(c, lines, &count, lang);
      break;
  }

  SDL_Color text_colour = { 0x4C, 0x4C, 0x4C, 255 }; // card text grey, HSL(0,0,30)
  float text_y = rect.y + bw + 6.0f;
  int line_h = TTF_GetFontHeight(font);
  for(uint8_t i = 0; i < count; i++)
  { gui_draw_text(renderer, font, lines[i], rect.x + bw + 6.0f, text_y, text_colour);
    text_y += (float)line_h + GUI_CARD_TEXT_LINE_GAP;
  }
} // gui_card_draw
