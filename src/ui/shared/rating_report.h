// rating_report.h
// Localized leaderboard/detail rendering for a RatingSystem -- kept in
// src/ui/ (not src/rating/) so the rating library itself stays UI-free
// and every user-facing string can go through LOCALIZED_STRING_L (see
// CLAUDE.md's trilingual-UI rule).

#ifndef RATING_REPORT_H
#define RATING_REPORT_H

#include "../../rating/rating.h"

// Sorted by rating, descending. Prints a "(no entrants)" line instead of
// an empty table -- unlike the v2 spec's rating_print_leaderboard(),
// which underflows an unsigned loop bound on zero entrants.
void rating_print_leaderboard(const RatingSystem* rs, ui_language_t lang);

// One entrant's rating, strength, record, and 95% confidence interval.
void rating_print_entrant_details(const RatingSystem* rs, uint32_t id, ui_language_t lang);

#endif // RATING_REPORT_H
