// gui_names.c -- see gui_names.h.

#include <stdio.h>
#include <string.h>

#include "gui_names.h"
#include "../shared/localization.h"

// True for the untouched defaults init_player_config() installs.
static bool is_default_name(const char* name, PlayerID p)
{ return !name[0] || !strcmp(name, p == PLAYER_A ? "Player1" : "Player2");
} // is_default_name

void gui_names_init(GuiNames* names, PlayerConfig* pconfig, ui_language_t lang)
{ for(uint8_t i = 0; i < NUM_PLAYERS; i++)
  { PlayerID p = (PlayerID)i;
    char who[MAX_PLAYER_LABEL_LEN] = "";
    if(pconfig
       && (pconfig->player_types[p] != INTERACTIVE_PLAYER
           || !is_default_name(pconfig->player_names[p], p)))
      format_player_label(p, pconfig, lang, who, sizeof(who));

    const char* word = LOCALIZED_STRING_L(lang, "Player", "Joueur", "Jugador");
    char letter = p == PLAYER_A ? 'A' : 'B';
    if(who[0])
      snprintf(names->label[i], GUI_NAME_LEN, "%s (%s %c)", who, word, letter);
    else
      snprintf(names->label[i], GUI_NAME_LEN, "%s %c", word, letter);
  }
} // gui_names_init
