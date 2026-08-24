// rating_internal.h
// Private constants shared between src/rating/*.c only -- not part of the
// public API in rating.h.

#ifndef RATING_INTERNAL_H
#define RATING_INTERNAL_H

// Strength floor: keeps bt_strength strictly positive after a multiplicative
// update or a batch-fit step, mirroring the v2 spec's own positivity guard
// (oracle_rating_system.c's rating_update_single_game()/rating_batch_compute()).
#define RATING_STRENGTH_FLOOR 1e-10

#endif // RATING_INTERNAL_H
