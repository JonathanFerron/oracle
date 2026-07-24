// tui_render_io.c
// Message log, input predicates, command-line drawing, and combat-details
// rendering -- split out of tui_render.c to keep both files under the
// project's file-size guideline. Everything here is interaction-support
// rendering (as opposed to the persistent board drawn by tui_render.c /
// tui_render_playarea.c), added for TUI Milestone 2.

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "tui_render.h"
#include "../../core/game_constants.h"
#include "../shared/localization.h"

/* Same ncurses.h/COLOR_RED discipline as tui_render.c -- see that file's
   top-of-file comment for why. */
#include <ncurses.h>
#undef COLOR_RED

static void tui_push_message(TuiScreen* screen, int color_pair, const char* text)
{ if(screen->message_count >= TUI_MAX_MESSAGES)
  { free(screen->messages[0]);
    memmove(screen->messages, screen->messages + 1,
            (TUI_MAX_MESSAGES - 1) * sizeof(char*));
    memmove(screen->message_colors, screen->message_colors + 1,
            (TUI_MAX_MESSAGES - 1) * sizeof(int));
    screen->message_count--;
  }

  screen->messages[screen->message_count] = strdup(text);
  screen->message_colors[screen->message_count] = color_pair;
  screen->message_count++;
}

void tui_add_message(TuiScreen* screen, const char* format, ...)
{ char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  tui_push_message(screen, TUI_MSG_COLOR_DEFAULT, buffer);
}

void tui_add_message_colored(TuiScreen* screen, int color_pair, const char* format, ...)
{ char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  tui_push_message(screen, color_pair, buffer);
}

int tui_get_input(void)
{ return getch();
}

bool tui_input_is_quit(int ch)
{ return ch == 'q' || ch == 'Q';
}

bool tui_input_is_resize(int ch)
{ return ch == KEY_RESIZE;
}

bool tui_input_is_tab(int ch)
{ return ch == '\t';
}

bool tui_input_is_enter(int ch)
{ return ch == '\n' || ch == '\r' || ch == KEY_ENTER;
}

bool tui_input_is_escape(int ch)
{ return ch == 27;
}

bool tui_input_is_backspace(int ch)
{ return ch == KEY_BACKSPACE || ch == 127 || ch == 8;
}

bool tui_input_is_printable(int ch)
{ return ch >= 32 && ch < 127;
}

void tui_draw_command_line(TuiScreen* screen, const char* text, bool show_cursor)
{ if(screen->too_small) return;

  werase(screen->win_command);
  mvwprintw(screen->win_command, 0, 0, "%.*s", getmaxx(screen->win_command) - 1, text);
  curs_set(show_cursor ? 1 : 0);
  wrefresh(screen->win_command);
}

/* ========================================================================
   Combat Results (message-log rendering)
   ======================================================================== */

static void tui_show_combat_side(TuiScreen* screen, int count,
                                 const ChampionSpecies* species,
                                 const uint8_t* dice, const uint8_t* rolls,
                                 const uint8_t* base, const int16_t* totals,
                                 int show_base)
{ for(int i = 0; i < count; i++)
  { char line[64];
    int pos = snprintf(line, sizeof(line), "  - %s: D%d [%d]",
                       CHAMPION_SPECIES_NAMES[species[i]], dice[i], rolls[i]);
    if(show_base && pos > 0 && (size_t)pos < sizeof(line))
      snprintf(line + pos, sizeof(line) - pos, " + %d = %d", base[i], totals[i]);
    tui_add_message(screen, "%s", line);
  }
}

void tui_show_combat_details(TuiScreen* screen, struct gamestate* gstate,
                             CombatDetails* details, config_t* cfg)
{ PlayerID attacker = gstate->current_player;
  PlayerID defender = 1 - attacker;

  tui_add_message(screen, "=== %s ===",
                  LOCALIZED_STRING("Combat Resolution", "Resolution du combat",
                                   "Resolucion del combate"));

  tui_add_message(screen, "%s (%s):", PLAYER_NAMES[attacker],
                  LOCALIZED_STRING("Attacker", "Attaquant", "Atacante"));
  tui_show_combat_side(screen, details->num_attackers, details->attacker_species,
                       details->attacker_dice, details->attacker_rolls,
                       details->attacker_base, details->attacker_total, 1);
  if(details->attack_combo > 0)
    tui_add_message_colored(screen, TUI_MSG_COLOR_SUCCESS, "  %s: +%d",
                            LOCALIZED_STRING("Combo bonus", "Bonus combo", "Bono combo"),
                            details->attack_combo);
  tui_add_message(screen, "  %s: %d",
                  LOCALIZED_STRING("Total attack", "Attaque totale", "Ataque total"),
                  details->total_attack);

  tui_add_message(screen, "%s (%s):", PLAYER_NAMES[defender],
                  LOCALIZED_STRING("Defender", "Defenseur", "Defensor"));
  if(details->num_defenders == 0)
    tui_add_message(screen, "  %s",
                    LOCALIZED_STRING("No defense", "Aucune defense", "Sin defensa"));
  else
  { tui_show_combat_side(screen, details->num_defenders, details->defender_species,
                         details->defender_dice, details->defender_rolls,
                         NULL, details->defender_total, 0);
    if(details->defense_combo > 0)
      tui_add_message_colored(screen, TUI_MSG_COLOR_SUCCESS, "  %s: +%d",
                              LOCALIZED_STRING("Combo bonus", "Bonus combo", "Bono combo"),
                              details->defense_combo);
    tui_add_message(screen, "  %s: %d",
                    LOCALIZED_STRING("Total defense", "Defense totale", "Defensa total"),
                    details->total_defense);
  }

  if(details->damage > 0)
    tui_add_message_colored(screen, TUI_MSG_COLOR_ERROR, "%s: %d (%d - %d)",
                            LOCALIZED_STRING("Damage", "Degats", "Dano"),
                            details->damage, details->total_attack, details->total_defense);
  else
    tui_add_message_colored(screen, TUI_MSG_COLOR_SUCCESS, "%s",
                            LOCALIZED_STRING("Attack blocked! No damage.",
                                             "Attaque bloquee! Aucun degat.",
                                             "Ataque bloqueado! Sin dano."));

  tui_add_message(screen, "%s: %d -> %d", PLAYER_NAMES[defender],
                  details->defender_energy_before, details->defender_energy_after);
}
