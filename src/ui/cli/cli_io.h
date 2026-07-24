// cli_io.h
// CLI (stdio) implementation of the UiIO seam (src/ui/shared/ui_io.h), so
// the shared interactive command logic in src/ui/interactive/ can run
// against the terminal exactly as it did when it called printf/fgets
// directly.

#ifndef CLI_IO_H
#define CLI_IO_H

#include "../shared/ui_io.h"
#include "../../core/game_types.h"

// Builds a UiIO backed by stdio. cfg is stored in io.ctx (needed for
// LOCALIZED_STRING at call sites and for display_card_with_power()).
UiIO cli_io_create(config_t* cfg);

#endif // CLI_IO_H
