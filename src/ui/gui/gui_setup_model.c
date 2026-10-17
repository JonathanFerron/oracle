// gui_setup_model.c
// SDL-free rules for the GUI player setup screen -- see gui_setup_model.h.

#include <math.h>
#include <string.h>
#include "gui_setup_model.h"

GuiSetupMode gui_setup_get_mode(const PlayerConfig* pconfig)
{ bool a_ai = pconfig->player_types[PLAYER_A] == AI_PLAYER;
  bool b_ai = pconfig->player_types[PLAYER_B] == AI_PLAYER;

  if(a_ai && b_ai)
    return GUI_SETUP_AI_VS_AI;
  if(!a_ai && !b_ai)
    return GUI_SETUP_HUMAN_VS_HUMAN;
  return GUI_SETUP_HUMAN_VS_AI;
} // gui_setup_get_mode

void gui_setup_set_mode(PlayerConfig* pconfig, GuiSetupMode mode)
{ switch(mode)
  { case GUI_SETUP_HUMAN_VS_HUMAN:
      pconfig->player_types[PLAYER_A] = INTERACTIVE_PLAYER;
      pconfig->player_types[PLAYER_B] = INTERACTIVE_PLAYER;
      break;
    case GUI_SETUP_AI_VS_AI:
      pconfig->player_types[PLAYER_A] = AI_PLAYER;
      pconfig->player_types[PLAYER_B] = AI_PLAYER;
      break;
    default:
      pconfig->player_types[PLAYER_A] = INTERACTIVE_PLAYER;
      pconfig->player_types[PLAYER_B] = AI_PLAYER;
      break;
  }
} // gui_setup_set_mode

int gui_setup_list_seat(const PlayerConfig* pconfig, PlayerID radio_seat)
{ switch(gui_setup_get_mode(pconfig))
  { case GUI_SETUP_AI_VS_AI:
      return radio_seat;
    case GUI_SETUP_HUMAN_VS_HUMAN:
      return -1;
    default:
      return pconfig->player_types[PLAYER_B] == AI_PLAYER ? PLAYER_B : PLAYER_A;
  }
} // gui_setup_list_seat

void gui_setup_apply_cmdline_agents(PlayerConfig* pconfig,
                                    const AIStrategyType agent[2],
                                    const bool agent_set[2])
{ for(int seat = 0; seat < NUM_PLAYERS; seat++)
    if(agent_set[seat])
      pconfig->ai_strategies[seat] = agent[seat];
} // gui_setup_apply_cmdline_agents

bool gui_setup_name_append(char name[MAX_PLAYER_NAME_LEN], const char* text)
{ size_t have = strlen(name);
  size_t add = strlen(text);

  if(add == 0 || have + add > MAX_PLAYER_NAME_LEN - 1)
    return false;

  memcpy(name + have, text, add + 1);
  return true;
} // gui_setup_name_append

void gui_setup_name_backspace(char name[MAX_PLAYER_NAME_LEN])
{ size_t len = strlen(name);

  // Step back over UTF-8 continuation bytes (10xxxxxx) to the lead byte.
  while(len > 0)
  { len--;
    if(((unsigned char)name[len] & 0xC0) != 0x80)
      break;
  }
  name[len] = '\0';
} // gui_setup_name_backspace

void gui_setup_sort_agents(GuiSetupAgent* agents, size_t n)
{ // Insertion sort: stable, and n is the roster size (25).
  for(size_t i = 1; i < n; i++)
  { GuiSetupAgent cur = agents[i];
    size_t j = i;
    while(j > 0 && agents[j - 1].rating > cur.rating)
    { agents[j] = agents[j - 1];
      j--;
    }
    agents[j] = cur;
  }
} // gui_setup_sort_agents

float gui_setup_rating_hue(int rating, int min_rating, int max_rating)
{ if(max_rating <= min_rating || rating <= min_rating)
    return GUI_SETUP_HUE_EASIEST;
  if(rating >= max_rating)
    return GUI_SETUP_HUE_HARDEST;

  float t = (float)(rating - min_rating) / (float)(max_rating - min_rating);
  return GUI_SETUP_HUE_EASIEST + t * (GUI_SETUP_HUE_HARDEST - GUI_SETUP_HUE_EASIEST);
} // gui_setup_rating_hue

static uint8_t unit_to_byte(float v)
{ if(v <= 0.0f) return 0;
  if(v >= 1.0f) return 255;
  return (uint8_t)lroundf(v * 255.0f);
} // unit_to_byte

void gui_setup_hsl_to_rgb(float h, float s, float l, uint8_t rgb[3])
{ h = fmodf(h, 360.0f);
  if(h < 0.0f) h += 360.0f;

  // Standard HSL: chroma, then the second-largest component x.
  float c = (1.0f - fabsf(2.0f * l - 1.0f)) * s;
  float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
  float m = l - c / 2.0f;
  float r = 0.0f, g = 0.0f, b = 0.0f;

  switch((int)(h / 60.0f))
  { case 0:
      r = c;
      g = x;
      break;
    case 1:
      r = x;
      g = c;
      break;
    case 2:
      g = c;
      b = x;
      break;
    case 3:
      g = x;
      b = c;
      break;
    case 4:
      r = x;
      b = c;
      break;
    default:
      r = c;
      b = x;
      break;
  }

  rgb[0] = unit_to_byte(r + m);
  rgb[1] = unit_to_byte(g + m);
  rgb[2] = unit_to_byte(b + m);
} // gui_setup_hsl_to_rgb
