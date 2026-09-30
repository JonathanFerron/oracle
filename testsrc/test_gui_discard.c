// test_gui_discard.c
// Unit tests for the GUI discard-grid card selection (src/ui/gui/gui_discard.c).

#include "../src/ui/gui/gui_discard.h"
#include "../src/core/game_constants.h"
#include <stdio.h>

static int passed = 0;
static int failed = 0;

static void check(const char* name, bool ok)
{ printf("  %s: %s\n", ok ? "\033[32m✓ PASS\033[0m" : "\033[31m✗ FAIL\033[0m", name);
  if(ok)
    passed++;
  else
    failed++;
} // check

// First `n` champion / non-champion card ids of fullDeck.
static void find_cards(uint8_t* champs, int nc, uint8_t* others, int no)
{ int c = 0, o = 0;
  for(int i = 0; i < FULL_DECK_SIZE; i++)
  { if(fullDeck[i].card_type == CHAMPION_CARD && c < nc)
      champs[c++] = (uint8_t)i;
    else if(fullDeck[i].card_type != CHAMPION_CARD && o < no)
      others[o++] = (uint8_t)i;
  }
} // find_cards

int main(void)
{ uint8_t ch[12], ot[12], out[GUI_DISCARD_MAX];
  find_cards(ch, 12, ot, 12);
  Discard d;

  printf("fits: shown whole, newest first\n");
  Discard_init(&d);
  Discard_add(&d, ch[0]);
  Discard_add(&d, ot[0]);
  Discard_add(&d, ch[1]);
  check("3 of 7", gui_discard_select(&d, 7, out) == 3);
  check("newest first", out[0] == ch[1] && out[1] == ot[0] && out[2] == ch[0]);

  printf("overflow: draw/cash hidden before champions\n");
  Discard_init(&d);
  Discard_add(&d, ot[0]);
  for(int i = 0; i < 6; i++)
  { Discard_add(&d, ch[i]);
    Discard_add(&d, ot[i + 1]);
  }
  check("7 shown of 13", gui_discard_select(&d, 7, out) == 7);
  int champs = 0;
  for(int i = 0; i < 7; i++)
    champs += fullDeck[out[i]].card_type == CHAMPION_CARD;
  bool all_champs = champs == 6;
  check("all 6 champions kept", all_champs);
  check("the spare cell goes to the newest non-champion", out[0] == ot[6]);

  printf("more champions than cells: newest champions win\n");
  Discard_init(&d);
  for(int i = 0; i < 10; i++)
    Discard_add(&d, ch[i]);
  check("7 shown", gui_discard_select(&d, 7, out) == 7);
  check("newest kept, oldest dropped", out[0] == ch[9] && out[6] == ch[3]);

  printf("empty and zero capacity\n");
  Discard_init(&d);
  check("empty pile", gui_discard_select(&d, 7, out) == 0);
  Discard_add(&d, ch[0]);
  check("zero capacity", gui_discard_select(&d, 0, out) == 0);

  printf("\n%d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
