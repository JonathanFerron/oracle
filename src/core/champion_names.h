// champion_names.h
// The 102 champions' individual names ("Furial", "Mimosa Cotton", ...) in
// English/French/Spanish, compiled in (like CHAMPION_SPECIES_NAMES) rather
// than loaded from a file: they're stable, and every UI -- including the
// SDL-free CLI/TUI/stda.auto builds -- can use them without file I/O or
// asset-path resolution. champion_names.c is generated from the source
// spreadsheet by tools/assets/gen_champion_names.py.

#ifndef CHAMPION_NAMES_H
#define CHAMPION_NAMES_H

#include <stdint.h>
#include "game_types.h" // ui_language_t

#define CHAMPION_COUNT 102

// Name of the champion with `champion_id` (1-102, i.e. struct card's
// champion_id) in `lang`. Returns "" for an out-of-range id or language
// (e.g. a draw/cash card, whose champion_id is 0).
const char* champion_name(uint8_t champion_id, ui_language_t lang);

#endif // CHAMPION_NAMES_H
