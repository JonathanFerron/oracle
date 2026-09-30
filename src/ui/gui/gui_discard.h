// gui_discard.h
// Which cards of a discard pile the GUI's mini-card grid shows (SDL-free, so it
// is unit-tested: testsrc/test_gui_discard.c).
//
// The grid has `capacity` cells. A pile that fits is shown whole, newest first.
// A pile that doesn't hides draw and cash cards before any champion: the newest
// champions are kept first, and only spare cells go to the newest draw/cash
// cards. Whatever is shown stays in newest-first order.

#ifndef GUI_DISCARD_H
#define GUI_DISCARD_H

#include <stdint.h>
#include "../../structures/card_collection.h" // Discard

#define GUI_DISCARD_MAX 40 // Discard's own capacity

// Fills `out` (GUI_DISCARD_MAX entries) with the card ids to show, newest first;
// returns how many (at most `capacity`).
uint8_t gui_discard_select(const Discard* d, uint8_t capacity, uint8_t out[GUI_DISCARD_MAX]);

#endif // GUI_DISCARD_H
