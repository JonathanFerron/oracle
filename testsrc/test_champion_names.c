// test_champion_names.c -- checks the generated name table (src/core/champion_names.c)
// against known cards, and its consistency with fullDeck.

#include "../src/core/champion_names.h"
#include "../src/core/game_constants.h"
#include <stdio.h>
#include <string.h>

static int failed = 0;

static void check(const char* name, bool ok)
{ printf("  %s: %s\n", ok ? "\033[32m✓ PASS\033[0m" : "\033[31m✗ FAIL\033[0m", name);
  if(!ok)
    failed++;
} // check

int main(void)
{ printf("=== champion_names tests ===\n");

  // Ground truth: the first card of each group on the printed cards page 1.
  check("id 1 = Furial", !strcmp(champion_name(1, LANG_EN), "Furial"));
  check("id 35 = Fury / Furie / Furia",
        !strcmp(champion_name(35, LANG_EN), "Fury") && !strcmp(champion_name(35, LANG_FR), "Furie")
        && !strcmp(champion_name(35, LANG_ES), "Furia"));
  check("id 69 = Lof", !strcmp(champion_name(69, LANG_FR), "Lof"));
  check("id 3 = Demata / Démata (accent in FR)",
        !strcmp(champion_name(3, LANG_EN), "Demata") && !strcmp(champion_name(3, LANG_FR), "Démata"));
  check("id 102 = Wolfly / Casilou", !strcmp(champion_name(102, LANG_EN), "Wolfly")
        && !strcmp(champion_name(102, LANG_FR), "Casilou"));

  bool all_named = true;
  for(int i = 0; i < FULL_DECK_SIZE; i++)
  { const struct card* c = &fullDeck[i];
    if(c->card_type == CHAMPION_CARD)
    { for(int l = LANG_EN; l <= LANG_ES; l++)
        all_named &= champion_name(c->champion_id, (ui_language_t)l)[0] != '\0';
    }
  }
  check("every fullDeck champion has a non-empty name in all 3 languages", all_named);

  check("id 0 (non-champion) and 103 return empty string",
        !champion_name(0, LANG_EN)[0] && !champion_name(103, LANG_EN)[0]);
  check("out-of-range language returns empty string", !champion_name(1, (ui_language_t)7)[0]);

  printf("\n%s\n", failed ? "FAILED" : "all passed");
  return failed ? 1 : 0;
} // main
