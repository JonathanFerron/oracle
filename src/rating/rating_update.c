// rating_update.c
// Incremental (real-time) rating updates -- the adaptive A^delta rule.
// See rating.h's rating_update_match() and doc/changelog.md for how this
// differs from the v2 spec: games_played now advances per game (so
// adaptive A actually decays within a match, not only between matches),
// and wins/losses/draws are interleaved instead of applied as three
// separate all-or-nothing blocks (the update is path-dependent, so that
// ordering was a systematic bias, not just cosmetic).

#include "rating.h"
#include "rating_internal.h"
#include <math.h>

typedef enum
{ OUTCOME_WIN1,
  OUTCOME_WIN2,
  OUTCOME_DRAW
} GameOutcome;

// One A^delta step for a single game between id1 and id2. Advances both
// entrants' games_played/games_won/games_drawn as part of the same step,
// so rating_get_adaptive_a() sees an up-to-date game count for every
// subsequent game in the same match.
static void apply_single_game(RatingSystem* rs, uint32_t id1, uint32_t id2,
                              GameOutcome outcome)
{ RatingEntry* e1 = &rs->entrants[id1];
  RatingEntry* e2 = &rs->entrants[id2];

  double a1 = rating_get_adaptive_a(rs, id1);
  double a2 = rating_get_adaptive_a(rs, id2);
  double a_avg = (a1 + a2) / 2.0;

  double expected1 = e1->bt_strength / (e1->bt_strength + e2->bt_strength);
  double actual1 = (outcome == OUTCOME_WIN1) ? 1.0 : (outcome == OUTCOME_DRAW) ? 0.5 : 0.0;
  double delta = actual1 - expected1;

  e1->bt_strength *= pow(a_avg, delta);
  e2->bt_strength *= pow(a_avg, -delta);
  if(e1->bt_strength < RATING_STRENGTH_FLOOR) e1->bt_strength = RATING_STRENGTH_FLOOR;
  if(e2->bt_strength < RATING_STRENGTH_FLOOR) e2->bt_strength = RATING_STRENGTH_FLOOR;

  e1->games_played++;
  e2->games_played++;
  switch(outcome)
  { case OUTCOME_WIN1:
      e1->games_won++;
      break;
    case OUTCOME_WIN2:
      e2->games_won++;
      break;
    case OUTCOME_DRAW:
      e1->games_drawn++;
      e2->games_drawn++;
      break;
  }
} // apply_single_game

// Deterministic proportional interleave of the three outcome categories
// (largest-remainder / "most under-represented next" merge) -- avoids
// both RNG dependence and the all-wins-then-all-losses bias.
static GameOutcome pick_next_outcome(const uint32_t* remaining, const uint32_t* total)
{ GameOutcome pick = OUTCOME_WIN1;
  double best_fraction = 2.0; // worse than any real done/total ratio

  for(GameOutcome o = OUTCOME_WIN1; o <= OUTCOME_DRAW; o++)
  { if(remaining[o] == 0) continue;

    double fraction = (double)(total[o] - remaining[o]) / (double)total[o];
    if(fraction < best_fraction)
    { best_fraction = fraction;
      pick = o;
    }
  }
  return pick;
} // pick_next_outcome

void rating_update_match(RatingSystem* rs, const MatchResult* result)
{ uint32_t id1 = result->entrant1_id;
  uint32_t id2 = result->entrant2_id;
  if(id1 >= rs->num_entrants || id2 >= rs->num_entrants) return;

  uint32_t total[3] = { result->entrant1_wins, result->entrant2_wins, result->draws };
  uint32_t remaining[3] = { total[0], total[1], total[2] };
  uint32_t total_games = total[0] + total[1] + total[2];

  for(uint32_t game = 0; game < total_games; game++)
  { GameOutcome outcome = pick_next_outcome(remaining, total);
    remaining[outcome]--;
    apply_single_game(rs, id1, id2, outcome);
  }

  if(id1 == rs->borealis_id || id2 == rs->borealis_id)
    rating_rebalance_to_borealis(rs);
} // rating_update_match
