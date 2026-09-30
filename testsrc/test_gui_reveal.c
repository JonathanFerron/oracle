// test_gui_reveal.c
// Unit tests for the GUI's combat-reveal queue (src/ui/gui/gui_reveal.c),
// driven with a fake clock. SDL-free.

#include "../src/ui/gui/gui_reveal.h"
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

static GameEvent combat(PlayerID attacker, uint8_t energy_before, int16_t damage)
{ GameEvent e = { .type = EVT_COMBAT_RESOLVED, .player = attacker };
  e.u.combat.damage = damage;
  e.u.combat.defender_energy_before = energy_before;
  e.u.combat.defender_energy_after = (uint8_t)(energy_before - damage);
  return e;
} // combat

static GameEvent plain(GameEventType t)
{ GameEvent e = { .type = t };
  return e;
} // plain

static EventBuf* buf_of(EventBuf* b, const GameEvent* evs, int n)
{ memset(b, 0, sizeof(*b));
  for(int i = 0; i < n; i++)
    b->ev[b->count++] = evs[i];
  return b;
} // buf_of

static GuiReveal rv;
static EventBuf eb;
static GameEvent out;

static void test_plain_events(void)
{ printf("plain events\n");
  gui_reveal_init(&rv, 2);
  GameEvent evs[] = { plain(EVT_TURN_BEGAN), plain(EVT_LUNA_COLLECTED) };
  gui_reveal_push(&rv, buf_of(&eb, evs, 2));
  check("busy after push", gui_reveal_busy(&rv));
  check("first released at once", gui_reveal_pop(&rv, 0, &out) && out.type == EVT_TURN_BEGAN);
  check("second released at once", gui_reveal_pop(&rv, 0, &out) && out.type == EVT_LUNA_COLLECTED);
  check("empty afterwards", !gui_reveal_pop(&rv, 0, &out) && !gui_reveal_busy(&rv));
} // test_plain_events

static void test_combat_two_steps(void)
{ printf("one combat, 2 s delay\n");
  gui_reveal_init(&rv, 2);
  GameEvent evs[] = { plain(EVT_MOVE_PLAYED), combat(PLAYER_A, 50, 7), plain(EVT_LUNA_COLLECTED) };
  gui_reveal_push(&rv, buf_of(&eb, evs, 3));
  const CombatDetails* d;
  PlayerID att;
  RevealStage st;
  check("nothing shown yet", !gui_reveal_current(&rv, &d, &att, &st));
  check("move logged first", gui_reveal_pop(&rv, 1000, &out) && out.type == EVT_MOVE_PLAYED);
  check("combat held back at start", !gui_reveal_pop(&rv, 1000, &out));
  check("attack half on show", gui_reveal_current(&rv, &d, &att, &st) && st == REVEAL_ATTACK
        && att == PLAYER_A && d->damage == 7);
  uint8_t en = 0;
  check("defender energy shown pre-damage", gui_reveal_energy_override(&rv, PLAYER_B, &en) && en == 50);
  check("attacker energy not overridden", !gui_reveal_energy_override(&rv, PLAYER_A, &en));
  check("still held at 1 s", !gui_reveal_pop(&rv, 2999, &out));
  check("released after delay", gui_reveal_pop(&rv, 3000, &out) && out.type == EVT_COMBAT_RESOLVED);
  check("now fully shown", gui_reveal_current(&rv, &d, &att, &st) && st == REVEAL_FULL);
  check("energy override gone", !gui_reveal_energy_override(&rv, PLAYER_B, &en));
  check("luna follows immediately", gui_reveal_pop(&rv, 3000, &out) && out.type == EVT_LUNA_COLLECTED);
  check("panel stays up when idle", gui_reveal_current(&rv, &d, &att, &st) && st == REVEAL_FULL
        && !gui_reveal_busy(&rv));
} // test_combat_two_steps

static void test_two_combats(void)
{ printf("two combats in one update\n");
  gui_reveal_init(&rv, 2);
  GameEvent evs[] = { combat(PLAYER_A, 50, 5), combat(PLAYER_B, 40, 3) };
  gui_reveal_push(&rv, buf_of(&eb, evs, 2));
  check("first waits", !gui_reveal_pop(&rv, 0, &out));
  check("first released", gui_reveal_pop(&rv, 2000, &out));
  check("second waits its own beat", !gui_reveal_pop(&rv, 2001, &out));
  const CombatDetails* d;
  PlayerID att;
  RevealStage st;
  check("first still on show during the hold", !gui_reveal_pop(&rv, 3999, &out)
        && gui_reveal_current(&rv, &d, &att, &st) && st == REVEAL_FULL && att == PLAYER_A);
  check("second starts after hold", !gui_reveal_pop(&rv, 4000, &out)
        && gui_reveal_current(&rv, &d, &att, &st) && st == REVEAL_ATTACK && att == PLAYER_B);
  uint8_t en = 0;
  check("its defender is A", !gui_reveal_energy_override(&rv, PLAYER_B, &en)
        && gui_reveal_energy_override(&rv, PLAYER_A, &en) && en == 40);
  check("second released after delay", gui_reveal_pop(&rv, 6000, &out) && !gui_reveal_busy(&rv));
} // test_two_combats

static void test_skip_and_zero(void)
{ printf("skip and zero delay\n");
  gui_reveal_init(&rv, 5);
  GameEvent evs[] = { combat(PLAYER_A, 10, 1), combat(PLAYER_A, 9, 1) };
  gui_reveal_push(&rv, buf_of(&eb, evs, 2));
  check("held", !gui_reveal_pop(&rv, 0, &out));
  gui_reveal_skip(&rv, 100);
  check("skip releases the attack wait", gui_reveal_pop(&rv, 100, &out));
  check("second held by the beat", !gui_reveal_pop(&rv, 101, &out));
  gui_reveal_skip(&rv, 102);
  check("skip ends the hold, second starts", !gui_reveal_pop(&rv, 102, &out));
  gui_reveal_skip(&rv, 103);
  check("skip again releases it", gui_reveal_pop(&rv, 103, &out));

  gui_reveal_init(&rv, 0);
  gui_reveal_push(&rv, buf_of(&eb, evs, 2));
  check("delay 0: both at once", gui_reveal_pop(&rv, 0, &out) && gui_reveal_pop(&rv, 0, &out)
        && !gui_reveal_busy(&rv));
  gui_reveal_init(&rv, 99);
  check("delay clamped to 10 s", rv.delay_ms == 10000);
  gui_reveal_init(&rv, -3);
  check("negative delay clamped to 0", rv.delay_ms == 0);
} // test_skip_and_zero

static void test_ack_and_game_over(void)
{ printf("acknowledge + held game over\n");
  gui_reveal_init(&rv, 1);
  GameEvent evs[] = { combat(PLAYER_A, 10, 10), plain(EVT_GAME_OVER) };
  gui_reveal_push(&rv, buf_of(&eb, evs, 2));
  check("game over is pending behind the combat", gui_reveal_game_over_pending(&rv));
  check("no ack needed before a combat finishes", !gui_reveal_needs_ack(&rv));
  gui_reveal_pop(&rv, 0, &out);
  check("combat released after delay", gui_reveal_pop(&rv, 1000, &out));
  check("game over still queued right after", gui_reveal_game_over_pending(&rv));
  check("game over released next", gui_reveal_pop(&rv, 1000, &out) && out.type == EVT_GAME_OVER);
  check("no longer pending", !gui_reveal_game_over_pending(&rv));
  check("needs ack once drained", gui_reveal_needs_ack(&rv));
  gui_reveal_ack(&rv);
  check("ack clears it", !gui_reveal_needs_ack(&rv));

  GameEvent one[] = { combat(PLAYER_A, 10, 1) };
  gui_reveal_push(&rv, buf_of(&eb, one, 1));
  gui_reveal_pop(&rv, 5000, &out);
  gui_reveal_pop(&rv, 6000, &out);
  check("explicit ack clears it", gui_reveal_needs_ack(&rv) && (gui_reveal_ack(&rv), !gui_reveal_needs_ack(&rv)));
} // test_ack_and_game_over

static void test_wraparound(void)
{ printf("ring buffer wraparound\n");
  gui_reveal_init(&rv, 0);
  GameEvent e = plain(EVT_LUNA_COLLECTED);
  int ok = 1;
  for(int round = 0; round < 3; round++)
  { memset(&eb, 0, sizeof(eb));
    for(int i = 0; i < 400; i++)
      eb.ev[eb.count++] = e;
    gui_reveal_push(&rv, &eb);
    int n = 0;
    while(gui_reveal_pop(&rv, 0, &out))
      n++;
    ok = ok && n == 400;
  }
  check("3 x 400 events pass through a 512 ring", ok);
} // test_wraparound

int main(void)
{ test_plain_events();
  test_combat_two_steps();
  test_two_combats();
  test_skip_and_zero();
  test_ack_and_game_over();
  test_wraparound();
  printf("\n%d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
} // main
