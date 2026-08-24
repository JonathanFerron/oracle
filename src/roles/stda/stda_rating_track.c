// stda_rating_track.c
// See stda_rating_track.h.

#include "stda_rating_track.h"
#include "../../ui/shared/localization.h"
#include <stdio.h>

#define RATING_TRACK_SUGGESTION_RANGE 15

// Registers (or finds) the human and the AI opponent as rating entrants,
// loading cfg->rating_file first if given (a missing/unreadable file is
// not an error -- rs simply starts empty, same as a fresh --stda.rating run).
static void ensure_entrants(config_t* cfg, PlayerConfig* pconfig, RatingSystem* rs,
                            PlayerID human, RatingTrackState* state)
{ rating_init(rs, NULL);
  if(cfg->rating_file) rating_import_csv(rs, cfg->rating_file);

  state->human_id = rating_find_by_name(rs, pconfig->player_names[human]);
  if(state->human_id == RATING_INVALID_ID)
    state->human_id = rating_register_human(rs, pconfig->player_names[human]);

  PlayerID ai = 1 - human;
  state->opponent_id = rating_find_by_strategy(rs, pconfig->ai_strategies[ai]);
  if(state->opponent_id == RATING_INVALID_ID)
  { const char* shorthand = get_ai_strategy_shorthand(pconfig->ai_strategies[ai]);
    state->opponent_id = rating_register_ai(rs, shorthand ? shorthand : "ai",
                                            pconfig->ai_strategies[ai]);
  }
} // ensure_entrants

static void print_matchmaking_suggestion(config_t* cfg, const RatingSystem* rs,
                                         const RatingTrackState* state)
{ uint32_t suggestion = rating_find_opponent(rs, state->human_id,
                                             RATING_TRACK_SUGGESTION_RANGE);
  if(suggestion == RATING_INVALID_ID || suggestion == state->opponent_id) return;

  const RatingEntry* s = rating_get_entrant(rs, suggestion);
  printf("%s: %s (%s %d)\n",
         LOCALIZED_STRING("Suggested opponent near your rating",
                          "Adversaire suggere pres de votre cote",
                          "Oponente sugerido cerca de tu puntuacion"),
         s->name, LOCALIZED_STRING("rating", "cote", "puntuacion"), s->rating);
} // print_matchmaking_suggestion

RatingTrackState stda_rating_track_start(config_t* cfg, PlayerConfig* pconfig,
                                         RatingSystem* rs)
{ RatingTrackState state = { false, PLAYER_A, RATING_INVALID_ID, RATING_INVALID_ID };
  if(!cfg->rating_track) return state;

  bool a_human = pconfig->player_types[PLAYER_A] == INTERACTIVE_PLAYER;
  bool b_human = pconfig->player_types[PLAYER_B] == INTERACTIVE_PLAYER;
  if(a_human == b_human) return state; // only a single human-vs-AI game is tracked

  state.human_seat = a_human ? PLAYER_A : PLAYER_B;
  ensure_entrants(cfg, pconfig, rs, state.human_seat, &state);
  if(state.human_id == RATING_INVALID_ID || state.opponent_id == RATING_INVALID_ID)
    return state; // roster full or a name/shorthand collision -- stay inactive

  print_matchmaking_suggestion(cfg, rs, &state);
  state.active = true;
  return state;
} // stda_rating_track_start

static bool human_won(const struct gamestate* gstate, PlayerID human_seat)
{ return (gstate->game_state == PLAYER_A_WINS && human_seat == PLAYER_A) ||
         (gstate->game_state == PLAYER_B_WINS && human_seat == PLAYER_B);
} // human_won

void stda_rating_track_finish(config_t* cfg, const RatingTrackState* state,
                              RatingSystem* rs, const struct gamestate* gstate)
{ if(!state->active) return;
  // ACTIVE means the player quit mid-game (run_game_loop's EXIT_SIGNAL
  // path) rather than the game actually resolving -- recording that as a
  // loss would be wrong, so skip tracking entirely rather than guess.
  if(gstate->game_state == ACTIVE) return;

  bool draw = (gstate->game_state == DRAW);
  bool won = human_won(gstate, state->human_seat);
  MatchResult m = { state->human_id, state->opponent_id,
                    won ? 1u : 0u, (!won && !draw) ? 1u : 0u, draw ? 1u : 0u
                  };

  int32_t before = rating_get_entrant(rs, state->human_id)->rating;
  rating_update_match(rs, &m);
  int32_t after = rating_get_entrant(rs, state->human_id)->rating;

  printf("\n%s: %d -> %d (%+d)\n",
         LOCALIZED_STRING("Your Borealis rating", "Votre cote boreale",
                          "Tu puntuacion boreal"),
         before, after, after - before);

  if(rs->borealis_id != RATING_INVALID_ID)
  { double p = rating_win_probability(rs, state->human_id, rs->borealis_id);
    if(p >= 0.0)
      printf("%s: %.1f%%\n",
             LOCALIZED_STRING("Win probability vs Borealis",
                              "Probabilite de victoire contre Boreal",
                              "Probabilidad de victoria contra Boreal"),
             p * 100.0);
  }

  if(cfg->rating_file && !rating_export_csv(rs, cfg->rating_file))
    fprintf(stderr, "Warning: failed to write rating CSV to %s\n", cfg->rating_file);
} // stda_rating_track_finish
