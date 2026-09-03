// gen_corpus.c
// A11 IS-MCTS+NN ("AlphaOracle Prime") Stage 1 self-play corpus generator --
// see "Confirmed plan" step 1 in
// ideas/A11 ai agent is-mcts + nn (alphaoracle prime)/about.md.
//
// Plays real games (never determinized/simulated-away games -- this is
// actual A10 self-play, not search-internal sampling) across the curated
// opponent pool (mirror A10, A10 vs A7 Grandmaster, A10 vs A3 Borealis) and,
// at every decision A10 itself makes (never the opponent's, per the
// confirmed plan), encodes the information-set state via
// ismctsnn_encode_state() (ai_strat_ismctsnn_state.h) *before* the move is
// applied. Once a game ends, every state recorded during it is paired with
// that game's outcome from the recording seat's own perspective
// (mc_outcome_for()'s existing 0.0/0.5/1.0 convention) and appended to the
// output file.
//
// Record format (locked in alongside the state vector's byte layout): each
// record is ISMCTSNN_STATE_DIM+1 = 538 raw native-endian float32s back to
// back -- the ISMCTSNNStateVector's 537 floats (safe to fwrite() directly,
// see that header's _Static_assert) followed by one outcome float. No
// per-file header, so shards from independent parallel runs (see below)
// concatenate byte-for-byte with no reprocessing. Load in Python via
// numpy.fromfile(path, dtype=np.float32).reshape(-1, 538).
//
// Parallelism is process-level, not thread-level: this binary plays its
// games sequentially against a single GameContext RNG stream (same pattern
// as calib_ismcts_timing.c), so generating a large corpus means running
// several instances with disjoint seeds against separate output paths (see
// local_training_plan.md's "embarrassingly parallel" framing) and pointing
// the training script at the whole set of shard files.
//
// Usage: gen_corpus <mirror|vs_a7|vs_a3> <numgames> <seed> <output_path> [limit_iterations]
//
// <limit_iterations> defaults to A10's shipped ISMCTS_DEFAULTS (4000) if
// omitted -- pass a smaller value only for a quick wiring smoke test, never
// for a real corpus (the corpus must reflect the agent A11 is actually
// distilling from).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/core/game_types.h"
#include "../../src/core/game_context.h"
#include "../../src/core/game_constants.h"
#include "../../src/core/game_state.h"
#include "../../src/core/turn_logic.h"
#include "../../src/ai_strat/ai_strategy.h"
#include "../../src/ai_strat/ai_strat_ismcts1.h"
#include "../../src/ai_strat/ai_strat_playout.h"
#include "../../src/ai_strat/ai_strat_ismctsnn_state.h"
#include "../../src/roles/stda/stda_auto.h"

#define GEN_CORPUS_MAX_RECORDS_PER_GAME (2 * MAX_NUMBER_OF_TURNS + 100)

typedef struct
{ ISMCTSNNStateVector state;
  PlayerID observer;
} PendingRecord;

static PendingRecord g_pending[GEN_CORPUS_MAX_RECORDS_PER_GAME];
static uint32_t g_pending_count = 0;
static FILE* g_out = NULL;
static uint64_t g_total_records = 0;
static uint64_t g_total_games = 0;

static void record_pending(const struct gamestate* gstate, PlayerID observer)
{ if(g_pending_count >= GEN_CORPUS_MAX_RECORDS_PER_GAME) return; // defensive; never expected
  ismctsnn_encode_state(gstate, observer, &g_pending[g_pending_count].state);
  g_pending[g_pending_count].observer = observer;
  g_pending_count++;
} // record_pending

// StrategySet hooks for whichever seat(s) are AI_STRATEGY_ISMCTS in this
// game -- logs the pre-move information set, then defers to the real A10
// decision unchanged. Mirrors calib_ismcts_timing.c's timed_attack_strategy/
// timed_defense_strategy wrapping pattern.
static void logging_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ record_pending(gstate, gstate->current_player);
  ismcts_attack_strategy(gstate, ctx);
} // logging_attack_strategy

static void logging_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ record_pending(gstate, 1 - gstate->current_player);
  ismcts_defense_strategy(gstate, ctx);
} // logging_defense_strategy

static void flush_game_records(const struct gamestate* final_gstate)
{ for(uint32_t i = 0; i < g_pending_count; i++)
  { float outcome = mc_outcome_for(final_gstate, g_pending[i].observer);
    fwrite(&g_pending[i].state, sizeof(ISMCTSNNStateVector), 1, g_out);
    fwrite(&outcome, sizeof(float), 1, g_out);
  }
  g_total_records += g_pending_count;
  g_pending_count = 0;
  fflush(g_out); // one completed game's worth of I/O -- cheap, and means a
  // wall-clock-bounded launcher (`timeout`, no SIGTERM handler here) never
  // loses more than the single game in flight when it's killed, not every
  // buffered-but-unwritten complete game since the last libc stdio flush.
} // flush_game_records

// Builds this game's StrategySet: both seats set to their agent types as
// usual, then the seat(s) actually playing AI_STRATEGY_ISMCTS get their
// attack/defense hooks overridden to the logging wrappers above. `ismcts_seat`
// is ignored for "mirror" (both seats log); otherwise it says which seat is
// A10 this game -- callers alternate it across games so the corpus isn't
// seat-biased (see project_seat_advantage_investigation.md: the effect is
// real and agent-dependent).
static StrategySet* build_strategy_set(const char* matchup, PlayerID ismcts_seat)
{ StrategySet* strategies = create_strategy_set();

  if(strcmp(matchup, "mirror") == 0)
  { set_player_strategy_by_type(strategies, PLAYER_A, AI_STRATEGY_ISMCTS);
    set_player_strategy_by_type(strategies, PLAYER_B, AI_STRATEGY_ISMCTS);
    strategies->attack_strategy[PLAYER_A] = logging_attack_strategy;
    strategies->attack_strategy[PLAYER_B] = logging_attack_strategy;
    strategies->defense_strategy[PLAYER_A] = logging_defense_strategy;
    strategies->defense_strategy[PLAYER_B] = logging_defense_strategy;
    return strategies;
  }

  AIStrategyType opponent = (strcmp(matchup, "vs_a7") == 0) ? AI_STRATEGY_HYBRID_HBT
                            : AI_STRATEGY_BOREALIS;
  PlayerID opp_seat = 1 - ismcts_seat;
  set_player_strategy_by_type(strategies, ismcts_seat, AI_STRATEGY_ISMCTS);
  set_player_strategy_by_type(strategies, opp_seat, opponent);
  strategies->attack_strategy[ismcts_seat] = logging_attack_strategy;
  strategies->defense_strategy[ismcts_seat] = logging_defense_strategy;
  return strategies;
} // build_strategy_set

// Mirrors play_stda_auto_game() (stda_auto.c) but keeps the final gstate in
// hand instead of only feeding it to gstats -- flush_game_records() needs
// its game_state to label this game's buffered records via mc_outcome_for().
static void play_and_log_one_game(StrategySet* strategies, GameContext* ctx)
{ struct gamestate gstate;
  setup_game(INITIAL_CASH_DEFAULT, &gstate, ctx);
  gstate.turn = 0;
  gstate.turn_phase = ATTACK;
  gstate.player_to_move = gstate.current_player;

  apply_mulligan(&gstate, strategies, ctx);

  do
  { play_turn(NULL, &gstate, strategies, ctx);
  }
  while(gstate.turn < MAX_NUMBER_OF_TURNS && !gstate.someone_has_zero_energy);

  if(!gstate.someone_has_zero_energy)
    gstate.game_state = DRAW;

  flush_game_records(&gstate);
  g_total_games++;

  DeckStk_emptyOut(&gstate.deck[PLAYER_A]);
  DeckStk_emptyOut(&gstate.deck[PLAYER_B]);
} // play_and_log_one_game

int main(int argc, char** argv)
{ if(argc != 5 && argc != 6)
  { fprintf(stderr,
            "Usage: %s <mirror|vs_a7|vs_a3> <numgames> <seed> <output_path> [limit_iterations]\n",
            argv[0]);
    return EXIT_FAILURE;
  }
  const char* matchup = argv[1];
  if(strcmp(matchup, "mirror") != 0 && strcmp(matchup, "vs_a7") != 0
     && strcmp(matchup, "vs_a3") != 0)
  { fprintf(stderr, "matchup must be one of: mirror, vs_a7, vs_a3\n");
    return EXIT_FAILURE;
  }
  uint32_t numgames = (uint32_t)strtoul(argv[2], NULL, 10);
  unsigned long seed = strtoul(argv[3], NULL, 10);
  const char* output_path = argv[4];
  uint32_t limit_iterations = (argc == 6) ? (uint32_t)strtoul(argv[5], NULL, 10) : 0;

  g_out = fopen(output_path, "ab");
  if(g_out == NULL)
  { fprintf(stderr, "Failed to open '%s' for append\n", output_path);
    return EXIT_FAILURE;
  }

  ISMCTSParams params = ISMCTS_DEFAULTS;
  if(limit_iterations > 0) params.limit_iterations = limit_iterations;
  ismcts_set_params(PLAYER_A, &params);
  ismcts_set_params(PLAYER_B, &params);

  config_t cfg = {0};
  cfg.prng_seed = seed;
  GameContext* ctx = create_game_context(&cfg);

  for(uint32_t i = 0; i < numgames; i++)
  { PlayerID ismcts_seat = (i % 2 == 0) ? PLAYER_A : PLAYER_B;
    StrategySet* strategies = build_strategy_set(matchup, ismcts_seat);
    play_and_log_one_game(strategies, ctx);
    free_strategy_set(strategies);

    if((i + 1) % 500 == 0 || i + 1 == numgames)
      fprintf(stderr, "%s: %u/%u games, %lu records so far\n", matchup, i + 1, numgames,
              (unsigned long)g_total_records);
  }

  fprintf(stderr, "%s: done -- %lu games, %lu records written to %s\n", matchup,
          (unsigned long)g_total_games, (unsigned long)g_total_records, output_path);

  destroy_game_context(ctx);
  fclose(g_out);
  return EXIT_SUCCESS;
} // main
