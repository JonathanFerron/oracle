// test_gui_config.c
// Unit tests for the GUI's [gui] INI reader (src/ui/gui/gui_config.c).
// SDL-free: gui_config.c depends on libc only.

#define _POSIX_C_SOURCE 200809L // fmemopen
#include "../src/ui/gui/gui_config.h"
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

// Parses `text` into a fresh default config.
static GuiConfig parse(const char* text)
{ GuiConfig cfg;
  gui_config_defaults(&cfg);
  FILE* f = fmemopen((void*)text, strlen(text), "r");
  gui_config_read(&cfg, f);
  fclose(f);
  return cfg;
} // parse

int main(void)
{ printf("=== gui_config tests ===\n");

  GuiConfig c;
  gui_config_defaults(&c);
  check("defaults: empty font_path, fractal off", !c.font_path[0] && !c.legacy_fractal);

  c = parse("[gui]\nfont_path = assets/fonts/Fredoka/Fredoka-Regular.ttf\nlegacy_fractal = true\n");
  check("both keys parsed", !strcmp(c.font_path, "assets/fonts/Fredoka/Fredoka-Regular.ttf")
        && c.legacy_fractal);

  c = parse("# comment\n; other comment\n\n[gui]\n  font_path=/a b/c.ttf  \n");
  check("comments/blank skipped, whitespace trimmed, spaces in path kept",
        !strcmp(c.font_path, "/a b/c.ttf"));

  c = parse("[general]\nfont_path = nope.ttf\nlegacy_fractal = true\n[mode]\ntype = stda.auto\n");
  check("keys outside [gui] ignored", !c.font_path[0] && !c.legacy_fractal);

  c = parse("[gui]\nlegacy_fractal = yes\n[ai]\nagent = x\n[gui]\nfont_path = late.ttf\n");
  check("[gui] may reappear after other sections", c.legacy_fractal && !strcmp(c.font_path, "late.ttf"));

  const char* truthy[] = { "true", "TRUE", "on", "1", "Yes" };
  bool all_true = true;
  for(int i = 0; i < 5; i++)
  { char buf[64];
    snprintf(buf, sizeof(buf), "[gui]\nlegacy_fractal = %s\n", truthy[i]);
    all_true &= parse(buf).legacy_fractal;
  }
  check("true/TRUE/on/1/Yes all true", all_true);

  c = parse("[gui]\nlegacy_fractal = true\nlegacy_fractal = off\n");
  check("later value wins (off)", !c.legacy_fractal);

  c = parse("[gui]\nlegacy_fractal = maybe\n");
  check("bad bool keeps default", !c.legacy_fractal);

  c = parse("[gui]\nbogus = 1\nnot a pair\nfont_path = ok.ttf\n");
  check("unknown key/malformed line skipped, later keys still read", !strcmp(c.font_path, "ok.ttf"));

  c = parse("[gui]\nlog_font_path = /x/mono.ttf\n");
  check("log_font_path parsed, default empty", !strcmp(c.log_font_path, "/x/mono.ttf"));

  c = parse("[gui]\n");
  check("combat_delay_seconds defaults to 2", c.combat_delay_seconds == 2);
  c = parse("[gui]\ncombat_delay_seconds = 5\n");
  check("combat_delay_seconds parsed", c.combat_delay_seconds == 5);
  c = parse("[gui]\ncombat_delay_seconds = 99\n");
  check("combat_delay_seconds clamped high", c.combat_delay_seconds == 10);
  c = parse("[gui]\ncombat_delay_seconds = -4\n");
  check("combat_delay_seconds clamped low", c.combat_delay_seconds == 0);
  c = parse("[gui]\ncombat_delay_seconds = soon\n");
  check("combat_delay_seconds junk keeps default", c.combat_delay_seconds == 2);

  c = parse("[gui]\r\nfont_path = crlf.ttf\r\nlegacy_fractal = true\r\n");
  check("CRLF line endings", !strcmp(c.font_path, "crlf.ttf") && c.legacy_fractal);

  char longpath[GUI_CONFIG_PATH_MAX + 64];
  memset(longpath, 'a', sizeof(longpath));
  memcpy(longpath, "[gui]\nfont_path = ", 18);
  longpath[sizeof(longpath) - 2] = '\n';
  longpath[sizeof(longpath) - 1] = '\0';
  c = parse(longpath);
  check("over-long font_path rejected, not truncated/overflowed", !c.font_path[0]);

  c = parse("[gui]\ncard_font_path = assets/fonts/Fredoka/static/Fredoka-SemiBold.ttf\nfont_path = a.ttf\n");
  check("card_font_path parsed independently of font_path",
        !strcmp(c.card_font_path, "assets/fonts/Fredoka/static/Fredoka-SemiBold.ttf")
        && !strcmp(c.font_path, "a.ttf"));
  gui_config_defaults(&c);
  check("card_font_path defaults to empty", !c.card_font_path[0]);

  gui_config_defaults(&c);
  check("missing file: load returns false, cfg untouched",
        !gui_config_load(&c, "/nonexistent/oracle_config.ini") && !c.font_path[0]);

  printf("\n%d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
} // main
