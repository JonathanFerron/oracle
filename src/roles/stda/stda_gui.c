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
#endif

int run_mode_stda_gui(config_t* cfg)
{
  #ifdef HAVE_SDL3
  return gui_app_run(cfg);
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
