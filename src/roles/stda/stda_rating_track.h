// stda_rating_track.h
// Optional (--rating.track, off by default) human Bradley-Terry rating
// tracking for a human-vs-AI game in stda.cli/stda.tui. Loads/creates a
// RatingSystem from cfg->rating_file, applies the incremental A^delta
// update after the game, prints the human's new rating, and persists the
// result -- a no-op (RatingTrackState.active stays false) for any other
// player-type combination (human-vs-human, AI-vs-AI) or when
// cfg->rating_track is unset, so normal play is completely unaffected.

#ifndef STDA_RATING_TRACK_H
#define STDA_RATING_TRACK_H

#include "../../core/game_types.h"
#include "../../ui/shared/player_config.h"
#include "../../rating/rating.h"

typedef struct
{ bool active;
  PlayerID human_seat;
  uint32_t human_id;
  uint32_t opponent_id;
} RatingTrackState;

// Call once player configuration is finalised (after get_ai_strategies()),
// before the game loop runs. `rs` is caller-owned (a local in
// run_mode_stda_cli()/run_mode_stda_tui()) so no global rating state is
// needed -- see CLAUDE.md's no-global-config rule and the v2 spec defect
// this specifically avoids repeating (rating.h's top-of-file comment).
// May print a matchmaking suggestion (rating_find_opponent()); never
// changes which agent was actually configured.
RatingTrackState stda_rating_track_start(config_t* cfg, PlayerConfig* pconfig,
                                         RatingSystem* rs);

// Call once gstate->game_state is final. No-op if state->active is false.
void stda_rating_track_finish(config_t* cfg, const RatingTrackState* state,
                              RatingSystem* rs, const struct gamestate* gstate);

#endif // STDA_RATING_TRACK_H
