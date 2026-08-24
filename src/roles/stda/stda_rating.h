// stda_rating.h
// MODE_STDA_RATING: round-robin Bradley-Terry rating benchmark. Registers
// every implemented AI agent (ai_strategy_is_implemented()), plays every
// unordered pair both seats-swapped, fits ratings via the batch MLE
// (src/rating/rating_batch.c), and prints the leaderboard -- this is
// where the project gets a real, reproducible rating table instead of
// the pairwise win rates scattered across doc/changelog.md.

#ifndef STDA_RATING_H
#define STDA_RATING_H

#include "../../core/game_types.h"

#define RATING_DEFAULT_GAMES_PER_ORIENTATION 2000

int run_mode_stda_rating(config_t* cfg);

#endif // STDA_RATING_H
