/* ============================================================
   stats_constants.h - Statistics and histogram constants
   ============================================================ */

#ifndef STATS_CONSTANTS_H
#define STATS_CONSTANTS_H

#include <stdint.h>

/* Histogram parameters for game turn distribution */
#define HISTOGRAM_NUM_BINS 27
#define HISTOGRAM_BIN_WIDTH 4
#define HISTOGRAM_MIN_VALUE 20

/* Histogram bin indices */
#define HISTOGRAM_UNDERFLOW_BIN 0
#define HISTOGRAM_OVERFLOW_BIN (HISTOGRAM_NUM_BINS + 1)
#define HISTOGRAM_TOTAL_BINS (HISTOGRAM_NUM_BINS + 2)

#endif /* STATS_CONSTANTS_H */
