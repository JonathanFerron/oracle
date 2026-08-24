// rating_csv.c
// CSV persistence for a RatingSystem -- the first file I/O anywhere in
// src/ (main.c's -o/--output only redirects stdout via freopen, and
// prng_seed.c's /dev/urandom open() never touches a named file). Owns
// its own FILE*, deliberately independent of stdout redirection.

#include "rating.h"
#include <stdio.h>
#include <string.h>

#define RATING_CSV_HEADER "id,name,kind,strategy,rating,bt_strength,games,wins,draws"

bool rating_export_csv(const RatingSystem* rs, const char* filename)
{ FILE* f = fopen(filename, "w");
  if(!f) return false;

  fprintf(f, RATING_CSV_HEADER "\n");
  for(uint32_t i = 0; i < rs->num_entrants; i++)
  { const RatingEntry* e = &rs->entrants[i];
    fprintf(f, "%u,%s,%d,%d,%d,%.9f,%u,%u,%u\n",
            e->id, e->name, (int)e->kind, (int)e->strategy, e->rating,
            e->bt_strength, e->games_played, e->games_won, e->games_drawn);
  }

  fclose(f);
  return true;
} // rating_export_csv

// Parses one data line into rs->entrants[rs->num_entrants] and advances
// num_entrants. Malformed lines (wrong field count) are skipped rather
// than aborting the whole import.
static void import_one_line(RatingSystem* rs, const char* line)
{ RatingEntry e = { 0 };
  int kind_int, strategy_int;

  int fields = sscanf(line, "%u,%63[^,],%d,%d,%d,%lf,%u,%u,%u",
                      &e.id, e.name, &kind_int, &strategy_int, &e.rating,
                      &e.bt_strength, &e.games_played, &e.games_won, &e.games_drawn);
  if(fields != 9) return;

  e.kind = (RatingEntrantKind)kind_int;
  e.strategy = (AIStrategyType)strategy_int;

  uint32_t idx = rs->num_entrants++;
  e.id = idx;
  rs->entrants[idx] = e;

  if(e.kind == RATING_ENTRANT_AI && e.strategy == AI_STRATEGY_BOREALIS)
    rs->borealis_id = idx;
} // import_one_line

bool rating_import_csv(RatingSystem* rs, const char* filename)
{ FILE* f = fopen(filename, "r");
  if(!f) return false;

  char header[256];
  if(!fgets(header, sizeof(header), f) ||
     strncmp(header, RATING_CSV_HEADER, strlen(RATING_CSV_HEADER)) != 0)
  { fclose(f);
    return false;
  }

  rs->num_entrants = 0;
  rs->borealis_id = RATING_INVALID_ID;

  char line[320];
  while(fgets(line, sizeof(line), f) && rs->num_entrants < RATING_MAX_ENTRANTS)
    import_one_line(rs, line);

  fclose(f);
  return true;
} // rating_import_csv
