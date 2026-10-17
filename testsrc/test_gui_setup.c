// test_gui_setup.c
// Unit tests for the GUI player setup rules (src/ui/gui/gui_setup_model.c).

#include "../src/ui/gui/gui_setup_model.h"
#include <stdio.h>
#include <string.h>

static int passed = 0;
static int failed = 0;

static void check(const char* name, bool ok)
{ printf("  %s: %s\n", ok ? "\033[32m✓ PASS\033[0m" : "\033[31m✗ FAIL\033[0m", name);
  if(ok)
    passed++;
  else
    failed++;
} // check

static bool rgb_is(const uint8_t rgb[3], uint8_t r, uint8_t g, uint8_t b)
{ return rgb[0] == r && rgb[1] == g && rgb[2] == b;
} // rgb_is

static void test_modes(void)
{ PlayerConfig pc = {0};
  printf("game mode <-> player types\n");

  for(GuiSetupMode m = 0; m < GUI_SETUP_MODE_COUNT; m++)
  { gui_setup_set_mode(&pc, m);
    check("round trip", gui_setup_get_mode(&pc) == m);
  }

  gui_setup_set_mode(&pc, GUI_SETUP_HUMAN_VS_AI);
  check("HvAI: human at A", pc.player_types[PLAYER_A] == INTERACTIVE_PLAYER &&
        pc.player_types[PLAYER_B] == AI_PLAYER);
  check("HvAI: list edits B", gui_setup_list_seat(&pc, PLAYER_A) == PLAYER_B);

  gui_setup_set_mode(&pc, GUI_SETUP_AI_VS_AI);
  check("AIvAI: list follows radio A", gui_setup_list_seat(&pc, PLAYER_A) == PLAYER_A);
  check("AIvAI: list follows radio B", gui_setup_list_seat(&pc, PLAYER_B) == PLAYER_B);

  gui_setup_set_mode(&pc, GUI_SETUP_HUMAN_VS_HUMAN);
  check("HvH: no list", gui_setup_list_seat(&pc, PLAYER_B) == -1);
} // test_modes

static void test_cmdline_agents(void)
{ PlayerConfig pc = {0};
  pc.ai_strategies[0] = pc.ai_strategies[1] = AI_STRATEGY_RANDOM;
  AIStrategyType agent[2] = { AI_STRATEGY_BOREALIS, AI_STRATEGY_ISMCTS_PUCT };
  bool set[2] = { false, true };
  printf("-Aa/-Ab defaults\n");

  gui_setup_apply_cmdline_agents(&pc, agent, set);
  check("unset seat untouched", pc.ai_strategies[0] == AI_STRATEGY_RANDOM);
  check("set seat copied", pc.ai_strategies[1] == AI_STRATEGY_ISMCTS_PUCT);
} // test_cmdline_agents

static void test_names(void)
{ char name[MAX_PLAYER_NAME_LEN] = "";
  printf("name editing\n");

  check("append ascii", gui_setup_name_append(name, "Jo") && strcmp(name, "Jo") == 0);
  check("append utf-8", gui_setup_name_append(name, "\xC3\xA9") &&
        strcmp(name, "Jo\xC3\xA9") == 0);
  gui_setup_name_backspace(name);
  check("backspace drops whole e-acute", strcmp(name, "Jo") == 0);
  gui_setup_name_backspace(name);
  gui_setup_name_backspace(name);
  gui_setup_name_backspace(name);
  check("backspace on empty is safe", name[0] == '\0');

  memset(name, 'x', MAX_PLAYER_NAME_LEN - 2);
  name[MAX_PLAYER_NAME_LEN - 2] = '\0';
  check("last free byte", gui_setup_name_append(name, "y"));
  check("full: refused", !gui_setup_name_append(name, "z"));
  name[MAX_PLAYER_NAME_LEN - 2] = '\0';
  check("multi-byte that doesn't fit: refused whole",
        !gui_setup_name_append(name, "\xC3\xA9") &&
        strlen(name) == MAX_PLAYER_NAME_LEN - 2);
} // test_names

static void test_sort(void)
{ GuiSetupAgent a[] =
  { { AI_STRATEGY_RANDOM, 2 }, { AI_STRATEGY_ISMCTS_PUCT, 76 },
    { AI_STRATEGY_HYBRID_HBT, 65 }, { AI_STRATEGY_JUNIOR, 6 },
    { AI_STRATEGY_CARTOGRAPHER, 65 }, { AI_STRATEGY_BOREALIS, 50 },
  };
  size_t n = sizeof(a) / sizeof(a[0]);
  printf("agent order\n");

  gui_setup_sort_agents(a, n);
  bool ascending = true;
  for(size_t i = 1; i < n; i++)
    ascending = ascending && a[i - 1].rating <= a[i].rating;
  check("easiest first", ascending && a[0].type == AI_STRATEGY_RANDOM &&
        a[n - 1].type == AI_STRATEGY_ISMCTS_PUCT);
  check("ties keep incoming order", a[3].type == AI_STRATEGY_HYBRID_HBT &&
        a[4].type == AI_STRATEGY_CARTOGRAPHER);
} // test_sort

static void test_hue(void)
{ uint8_t rgb[3];
  printf("rating hue\n");

  check("min -> 310", gui_setup_rating_hue(2, 2, 76) == 310.0f);
  check("max -> 0", gui_setup_rating_hue(76, 2, 76) == 0.0f);
  check("midpoint -> 155", gui_setup_rating_hue(39, 2, 76) == 155.0f);
  check("clamped below", gui_setup_rating_hue(-5, 2, 76) == 310.0f);
  check("clamped above", gui_setup_rating_hue(90, 2, 76) == 0.0f);
  check("zero-width range", gui_setup_rating_hue(5, 5, 5) == 310.0f);

  gui_setup_hsl_to_rgb(0.0f, 1.0f, 0.5f, rgb);
  check("hsl red", rgb_is(rgb, 255, 0, 0));
  gui_setup_hsl_to_rgb(120.0f, 1.0f, 0.5f, rgb);
  check("hsl green", rgb_is(rgb, 0, 255, 0));
  gui_setup_hsl_to_rgb(240.0f, 1.0f, 0.5f, rgb);
  check("hsl blue", rgb_is(rgb, 0, 0, 255));
  gui_setup_hsl_to_rgb(310.0f, 1.0f, 0.5f, rgb);
  check("hsl 310 = magenta-pink (255,0,213)", rgb_is(rgb, 255, 0, 213));
  gui_setup_hsl_to_rgb(360.0f, 1.0f, 0.5f, rgb);
  check("hsl 360 wraps to red", rgb_is(rgb, 255, 0, 0));
} // test_hue

int main(void)
{ test_modes();
  test_cmdline_agents();
  test_names();
  test_sort();
  test_hue();

  printf("\n%d/%d passed\n", passed, passed + failed);
  return failed == 0 ? 0 : 1;
} // main
