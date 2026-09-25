// stda_gui.c
// MODE_STDA_GUI entry point. This file is compiled into EVERY build
// (default/debug/release, not just `make gui`) -- see stda_gui.h's own
// comment. It only reaches into src/ui/gui/ (which needs SDL3 headers)
// behind #ifdef HAVE_SDL3, so a plain `make` links a working, if
// GUI-less, binary.

#include <stdio.h>
#include <stdlib.h>

#include "stda_gui.h"
#include "../../ui/shared/localization.h"

#ifdef HAVE_SDL3
  #include "../../ui/gui/gui_app.h"
  #include "../../ui/shared/player_config.h"
  #include "../../ui/shared/player_selection.h"
#endif

#ifdef HAVE_SDL3
// Plain stdio prompts in the launching terminal, run before the SDL window
// opens -- mirrors stda_tui.c's tui_setup_player_configuration() (same
// player_config.c/player_selection.c flow the CLI/TUI already use). Per the
// plan file's Step 7 note, an in-window setup screen is a later polish
// pass, not an M1 blocker.
static void gui_setup_player_configuration(config_t* cfg, PlayerConfig* pconfig)
{ init_player_config(pconfig);
  cfg->player_config = pconfig;

  display_player_selection_menu(cfg);
  int choice = get_player_type_choice(cfg);
  apply_player_selection(pconfig, cfg, choice);

  get_player_names(cfg, pconfig);
  get_ai_strategies(cfg, pconfig);
} // gui_setup_player_configuration
#endif

int run_mode_stda_gui(config_t* cfg)
{
  #ifdef HAVE_SDL3
  PlayerConfig pconfig;
  gui_setup_player_configuration(cfg, &pconfig);
  return gui_app_run(cfg, &pconfig);
  #else
  printf("%s\n", LOCALIZED_STRING(
           "Standalone GUI mode: this binary was built without SDL3 support. "
           "Rebuild with 'make gui' (needs libsdl3-dev/libsdl3-ttf-dev/libsdl3-image-dev) "
           "and run bin/oracle-gui instead.",
           "Mode graphique autonome : ce binaire a ete compile sans support SDL3. "
           "Recompilez avec 'make gui' (necessite libsdl3-dev/libsdl3-ttf-dev/libsdl3-image-dev) "
           "et lancez bin/oracle-gui a la place.",
           "Modo grafico independiente: este binario se compilo sin soporte SDL3. "
           "Recompile con 'make gui' (necesita libsdl3-dev/libsdl3-ttf-dev/libsdl3-image-dev) "
           "y ejecute bin/oracle-gui en su lugar."));
  return EXIT_SUCCESS;
  #endif
} // run_mode_stda_gui
