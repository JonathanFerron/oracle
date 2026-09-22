// gen_policy_corpus.c
// A14 AlphaOracle Prime Plus I Stage 2 self-play corpus generator -- see
// doc/ai_agents.md's A14 section.
//
// Plays real A11 (ismctsnn) games -- never determinized/simulated-away
// games -- across the same curated opponent pool A11's own gen_corpus.c
// used (mirror | vs_a7 | vs_a3 | vs_a4 | vs_a6), logging from A11's own
// decision points only. Record format is the state+outcome+full-legal-
// move-list format this agent's policy head needs, NOT A11's own
// state+outcome-only format -- the two corpora are not interchangeable
// despite the shared teacher.
//
// A11 IS the teacher here, not A14 itself: A14 has no trained weights yet
// (that is what this corpus is for), so playing A14's own games now would
// just be self-play under a net that always returns value=0.5/uniform
// priors -- useless. A11 (rating 74, the roster ceiling) is the strongest
// available teacher; its own visit-count distribution is what a policy
// head first learns to imitate.
//
// Record format (bumped 2026-09-22, A16 Session 1 item 1.3): 537 (state,
// ISMCTSNNStateVector) + 1 (outcome) + 1 (num_moves) + 1 (total_visits,
// the root visit-count sum this decision's search actually accumulated --
// NEW, see below) + 128 * 9 (per-move type, count, play[3], target[3],
// visit_fraction) = 1692 raw native-endian float32s back to back, no
// per-file header -- same concatenate-byte-for-byte convention as A11's
// own corpus. Unused move slots (index >= num_moves) are zero-padded.
// play[]/target[] hold CATALOG indices (puct_move_features(),
// ai_strat_puct_policy.h), never fullDeck[] indices, so the Python side
// never needs the deck table; -1.0f marks an unused play/target sub-slot
// (never a valid catalog index, 0..104). Load in Python via
// numpy.fromfile(path, dtype=np.float32).reshape(-1, 1692).
//
// total_visits was added so a policy-target temperature (pi_i ~
// visit_i^(1/tau)) can be applied at TRAINING time instead of only ever
// baked into the corpus at temperature=1.0 -- see
// aicalibsrc/puct/train_puct_net.py's --policy-target-temperature.
// Recorded as raw provenance (also useful on its own for diagnosing how
// concentrated a given decision's search was, doc/ai_agents.md's A14
// section Finding 2), even though the temperature transform itself only
// needs visit_fraction: visit_i = fraction_i * total_visits, so
// visit_i^(1/tau) = fraction_i^(1/tau) * total_visits^(1/tau), and the
// second factor is constant across one record's moves, cancelling under
// renormalization -- fraction alone is sufficient, total_visits is not
// actually read by that code path. Shards written before this date have
// no total_visits column (1691 floats/record, the OLD_RECORD_DIM the
// Python side still loads, with total_visits reported as NaN for those
// records) -- the two widths are coprime, so a shard would need ~2.86
// million records before size-based format detection could ever be
// ambiguous, far past anything this project generates.
//
// visit_fraction is read back from ismcts_last_root_visit_distribution()
// (ai_strat_ismcts_search.h) IMMEDIATELY after the real decision call
// returns -- that accessor reflects whatever search just ran, and this
// file's own PRE-move state capture (record_pending(), below) is for that
// exact same decision, so the two always line up (see
// ai_strat_ismcts_search.h's own file-static-is-safe-here rationale: this
// project's parallelism is process-level, never threads). A move
// enumerated by this file's own get_available_moves() call but never
// expanded during the search (arena exhaustion, widening, or simply
// outside the iteration budget) gets visit_fraction 0.0 -- a real,
// correct training target (the search itself never favoured it), not a
// missing value.
//
// Usage:
//   gen_policy_corpus <mirror|vs_a7|vs_a3|vs_a4|vs_a6> <numgames> <seed> <output_path> [limit_iterations]
//
// <limit_iterations> defaults to A11's shipped ISMCTS_DEFAULTS (4000) if
// omitted -- pass a smaller value only for a quick wiring smoke test,
// never for a real corpus (it must reflect the agent this is distilling
// from). Requires a trained A11 weights file at the fixed path below
// (ISMCTSNN_DEFAULT_WEIGHTS_PATH, ai_strat_ismctsnn.h) -- refuses to run
// without it, since an unloaded A11 degrades silently to plain A10 and the
// corpus would then be teaching a policy head to imitate the wrong agent.

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
#include "../../src/ai_strat/ai_strat_ismcts_tree.h"
#include "../../src/ai_strat/ai_strat_ismcts_search.h"
#include "../../src/ai_strat/ai_strat_ismctsnn.h"
#include "../../src/ai_strat/ai_strat_ismctsnn_state.h"
#include "../../src/ai_strat/ai_strat_puct_policy.h"
#include "../../src/ai_strat/ai_strat_playout.h"
#include "../../src/actions/move_gen.h"
#include "../../src/roles/stda/stda_auto.h"

#define GEN_POLICY_CORPUS_MAX_RECORDS_PER_GAME (2 * MAX_NUMBER_OF_TURNS + 100)
#define GEN_POLICY_FLOATS_PER_MOVE 9 // type, count, play[3], target[3], visit_fraction

typedef struct
{ ISMCTSNNStateVector state;
  struct gamestate pre_move_gstate; // for build_move_features()'s own
  // get_available_moves() call -- the LIVE gstate has already been
  // mutated by the real decision by the time that runs (see
  // logging_attack_strategy()/logging_defense_strategy() below), so it
  // cannot be re-read for this; a plain struct copy is cheap
  // (sizeof(struct gamestate) is a few hundred bytes, not KB).
  PlayerID observer;
  uint8_t num_moves;
  uint32_t total_visits; // added 2026-09-22 (A16 Session 1, item 1.3) --
  // see this file's own header comment on the record format bump
  float move_features[MOVE_GEN_MAX_MOVES * GEN_POLICY_FLOATS_PER_MOVE];
} PendingPolicyRecord;

static PendingPolicyRecord g_pending[GEN_POLICY_CORPUS_MAX_RECORDS_PER_GAME];
static uint32_t g_pending_count = 0;
static FILE* g_out = NULL;
static uint64_t g_total_records = 0;
static uint64_t g_total_games = 0;
static MoveGenLimits g_limits; // set in main() from the teacher's own ISMCTSParams

// Writes one move's 9-float block (type, count, play[3], target[3],
// visit_fraction) to `slot`.
static void write_move_block(float* slot, const GameMove* move, float visit_fraction)
{ PUCTMoveFeatures f;
  puct_move_features(move, &f);

  slot[0] = (float)f.type;
  slot[1] = (float)f.count;
  slot[2] = (float)f.play[0];
  slot[3] = (float)f.play[1];
  slot[4] = (float)f.play[2];
  slot[5] = (float)f.target[0];
  slot[6] = (float)f.target[1];
  slot[7] = (float)f.target[2];
  slot[8] = visit_fraction;
} // write_move_block

// Re-enumerates this decision's full legal move list (the same call the
// search itself made, same limits) and cross-references it against the
// search that JUST ran (ismcts_last_root_visit_distribution()) to build
// this record's move-feature block. Returns the number of legal moves
// written (num_moves); the rest of `out` (up to MOVE_GEN_MAX_MOVES) is
// zero-padded. `*out_total_visits` receives the root visit-count sum this
// decision's search accumulated (added 2026-09-22, see this file's own
// header comment).
static uint8_t build_move_features(const struct gamestate* gstate, PlayerID player, float* out,
                                   uint32_t* out_total_visits)
{ GameMove moves[MOVE_GEN_MAX_MOVES];
  uint8_t n = get_available_moves(gstate, player, &g_limits, moves, MOVE_GEN_MAX_MOVES);

  RootVisitRecord visits[MOVE_GEN_MAX_MOVES];
  uint8_t visit_n = ismcts_last_root_visit_distribution(visits, MOVE_GEN_MAX_MOVES);
  uint32_t total_visits = 0;
  for(uint8_t i = 0; i < visit_n; i++) total_visits += visits[i].visits;
  *out_total_visits = total_visits;

  for(uint8_t i = 0; i < n; i++)
  { uint32_t v = 0;
    for(uint8_t j = 0; j < visit_n; j++)
      if(ismcts_move_equal(&moves[i], &visits[j].move))
      { v = visits[j].visits;
        break;
      }
    float fraction = (total_visits > 0) ? ((float)v / (float)total_visits) : 0.0f;
    write_move_block(out + (size_t)i * GEN_POLICY_FLOATS_PER_MOVE, &moves[i], fraction);
  }
  for(uint8_t i = n; i < MOVE_GEN_MAX_MOVES; i++)
    memset(out + (size_t)i * GEN_POLICY_FLOATS_PER_MOVE, 0, sizeof(float) * GEN_POLICY_FLOATS_PER_MOVE);

  return n;
} // build_move_features

// Captures the PRE-move state/legal-move-list for `observer` -- called
// BEFORE the real decision, so `gstate` and this file's own
// get_available_moves() call reflect the position being decided; the
// move-feature block's visit_fraction column is filled in AFTER the real
// decision returns (see record_pending()'s call sites below), once
// ismcts_last_root_visit_distribution() actually has this decision's data.
static void record_pending(const struct gamestate* gstate, PlayerID observer)
{ if(g_pending_count >= GEN_POLICY_CORPUS_MAX_RECORDS_PER_GAME) return; // defensive
  PendingPolicyRecord* rec = &g_pending[g_pending_count];
  ismctsnn_encode_state(gstate, observer, &rec->state);
  rec->pre_move_gstate = *gstate;
  rec->observer = observer;
  g_pending_count++;
} // record_pending

// StrategySet hooks for whichever seat(s) are AI_STRATEGY_ISMCTS_NN in
// this game -- logs the pre-move state, defers to the real A11 decision
// (which runs the search and applies the chosen move), then fills in this
// same record's move-feature block from the search that just ran. Mirrors
// aicalibsrc/ismctsnn/gen_corpus.c's logging_attack_strategy/
// logging_defense_strategy wrapping pattern, extended for the policy
// target.
static void logging_attack_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID observer = gstate->current_player;
  record_pending(gstate, observer);
  uint32_t idx = g_pending_count - 1;
  ismctsnn_attack_strategy(gstate, ctx); // mutates *gstate -- use the
  g_pending[idx].num_moves = // pre-move snapshot below, not gstate itself
    build_move_features(&g_pending[idx].pre_move_gstate, observer, g_pending[idx].move_features,
                        &g_pending[idx].total_visits);
} // logging_attack_strategy

static void logging_defense_strategy(struct gamestate* gstate, GameContext* ctx)
{ PlayerID observer = 1 - gstate->current_player;
  record_pending(gstate, observer);
  uint32_t idx = g_pending_count - 1;
  ismctsnn_defense_strategy(gstate, ctx); // mutates *gstate -- use the
  g_pending[idx].num_moves = // pre-move snapshot below, not gstate itself
    build_move_features(&g_pending[idx].pre_move_gstate, observer, g_pending[idx].move_features,
                        &g_pending[idx].total_visits);
} // logging_defense_strategy

static void flush_game_records(const struct gamestate* final_gstate)
{ for(uint32_t i = 0; i < g_pending_count; i++)
  { PendingPolicyRecord* rec = &g_pending[i];
    float outcome = mc_outcome_for(final_gstate, rec->observer);
    float num_moves_f = (float)rec->num_moves;
    float total_visits_f = (float)rec->total_visits;
    fwrite(&rec->state, sizeof(ISMCTSNNStateVector), 1, g_out);
    fwrite(&outcome, sizeof(float), 1, g_out);
    fwrite(&num_moves_f, sizeof(float), 1, g_out);
    fwrite(&total_visits_f, sizeof(float), 1, g_out);
    fwrite(rec->move_features, sizeof(float), MOVE_GEN_MAX_MOVES * GEN_POLICY_FLOATS_PER_MOVE, g_out);
  }
  g_total_records += g_pending_count;
  g_pending_count = 0;
  fflush(g_out); // one completed game's worth of I/O -- see gen_corpus.c's
  // own comment: a wall-clock-bounded launcher never loses more than the
  // single game in flight when killed, not every buffered complete game
} // flush_game_records

// Same matchup pool and opponent mapping as aicalibsrc/ismctsnn/gen_corpus.c's
// build_strategy_set() -- see that file's own header comment for why this
// pool, not pure mirror self-play (A5 deliberately excluded: it's already
// A10's -- and so A11's -- own rollout policy).
static StrategySet* build_strategy_set(const char* matchup, PlayerID ismctsnn_seat)
{ StrategySet* strategies = create_strategy_set();

  if(strcmp(matchup, "mirror") == 0)
  { set_player_strategy_by_type(strategies, PLAYER_A, AI_STRATEGY_ISMCTS_NN);
    set_player_strategy_by_type(strategies, PLAYER_B, AI_STRATEGY_ISMCTS_NN);
    strategies->attack_strategy[PLAYER_A] = logging_attack_strategy;
    strategies->attack_strategy[PLAYER_B] = logging_attack_strategy;
    strategies->defense_strategy[PLAYER_A] = logging_defense_strategy;
    strategies->defense_strategy[PLAYER_B] = logging_defense_strategy;
    return strategies;
  }

  AIStrategyType opponent = (strcmp(matchup, "vs_a7") == 0) ? AI_STRATEGY_HYBRID_HBT
                            : (strcmp(matchup, "vs_a4") == 0) ? AI_STRATEGY_BALANCED
                            : (strcmp(matchup, "vs_a6") == 0) ? AI_STRATEGY_TACTICAL
                            : AI_STRATEGY_BOREALIS;
  PlayerID opp_seat = 1 - ismctsnn_seat;
  set_player_strategy_by_type(strategies, ismctsnn_seat, AI_STRATEGY_ISMCTS_NN);
  set_player_strategy_by_type(strategies, opp_seat, opponent);
  strategies->attack_strategy[ismctsnn_seat] = logging_attack_strategy;
  strategies->defense_strategy[ismctsnn_seat] = logging_defense_strategy;
  return strategies;
} // build_strategy_set

// Mirrors play_stda_auto_game() (stda_auto.c) but keeps the final gstate in
// hand instead of only feeding it to gstats -- flush_game_records() needs
// it to label this game's buffered records via mc_outcome_for().
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
            "Usage: %s <mirror|vs_a7|vs_a3|vs_a4|vs_a6> <numgames> <seed> <output_path> "
            "[limit_iterations]\n",
            argv[0]);
    return EXIT_FAILURE;
  }
  const char* matchup = argv[1];
  if(strcmp(matchup, "mirror") != 0 && strcmp(matchup, "vs_a7") != 0
     && strcmp(matchup, "vs_a3") != 0 && strcmp(matchup, "vs_a4") != 0
     && strcmp(matchup, "vs_a6") != 0)
  { fprintf(stderr, "matchup must be one of: mirror, vs_a7, vs_a3, vs_a4, vs_a6\n");
    return EXIT_FAILURE;
  }
  uint32_t numgames = (uint32_t)strtoul(argv[2], NULL, 10);
  unsigned long seed = strtoul(argv[3], NULL, 10);
  const char* output_path = argv[4];
  uint32_t limit_iterations = (argc == 6) ? (uint32_t)strtoul(argv[5], NULL, 10) : 0;

  if(!ismctsnn_load_weights(ISMCTSNN_DEFAULT_WEIGHTS_PATH))
  { fprintf(stderr, "gen_policy_corpus: failed to load A11 weights from '%s' -- "
                      "refusing to generate a corpus that would silently teach a policy "
                      "head to imitate plain A10 instead\n",
            ISMCTSNN_DEFAULT_WEIGHTS_PATH);
    return EXIT_FAILURE;
  }

  g_out = fopen(output_path, "ab");
  if(g_out == NULL)
  { fprintf(stderr, "Failed to open '%s' for append\n", output_path);
    return EXIT_FAILURE;
  }

  ISMCTSParams params = ismctsnn_get_default_params(); // nn_value_trust=1.0
  if(limit_iterations > 0) params.limit_iterations = limit_iterations;
  ismctsnn_set_params(PLAYER_A, &params);
  ismctsnn_set_params(PLAYER_B, &params);
  g_limits = (MoveGenLimits)
  { .max_recall_variants = params.limit_recall_variants,
      .max_cash_variants = params.limit_cash_variants
  };

  config_t cfg = {0};
  cfg.prng_seed = seed;
  GameContext* ctx = create_game_context(&cfg);

  for(uint32_t i = 0; i < numgames; i++)
  { PlayerID ismctsnn_seat = (i % 2 == 0) ? PLAYER_A : PLAYER_B;
    StrategySet* strategies = build_strategy_set(matchup, ismctsnn_seat);
    play_and_log_one_game(strategies, ctx);
    free_strategy_set(strategies);

    if((i + 1) % 100 == 0 || i + 1 == numgames)
      fprintf(stderr, "%s: %u/%u games, %lu records so far\n", matchup, i + 1, numgames,
              (unsigned long)g_total_records);
  }

  fprintf(stderr, "%s: done -- %lu games, %lu records written to %s\n", matchup,
          (unsigned long)g_total_games, (unsigned long)g_total_records, output_path);

  destroy_game_context(ctx);
  fclose(g_out);
  return EXIT_SUCCESS;
} // main
