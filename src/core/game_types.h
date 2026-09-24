// game_types.h
// All enums, structs, and type definitions for Oracle game

#ifndef GAME_TYPES_H
#define GAME_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "../structures/deckstack.h"
#include "../structures/card_collection.h"

// Player identification
typedef enum
{ PLAYER_A = 0,
  PLAYER_B = 1
} PlayerID;

// Number of players in today's engine. New code (src/visibility/, src/ui/gui/)
// should use this rather than a literal 2, so the planned 3-4 player engine
// rework is an extension, not a search-and-replace -- see
// doc/oracle_roadmap.md's "SDL3 GUI" item and
// ideas/9 gui/gui_architecture_synthesis.md section 5.1a. struct gamestate's
// own [2] arrays are untouched by this (pre-existing, not migrated here).
#define NUM_PLAYERS 2

// Game state enum
typedef enum
{ PLAYER_A_WINS = 0,
  PLAYER_B_WINS = 1,
  DRAW = 2,
  ACTIVE = 3
} GameStateEnum;

// Turn phase enum
typedef enum
{ ATTACK,
  DEFENSE
} TurnPhase;

// Card type enum
typedef enum
{ CHAMPION_CARD,
  DRAW_CARD,
  CASH_CARD
} CardType;

// Champion color enum
typedef enum
{ COLOR_RED,
  COLOR_INDIGO,
  COLOR_ORANGE,
  COLOR_NOT_APPLICABLE,
  COLOR_COUNT = COLOR_NOT_APPLICABLE
} ChampionColor;

// Champion species enum
typedef enum
{ SPECIES_HUMAN,
  SPECIES_ELF,
  SPECIES_DWARF,
  SPECIES_ORC,
  SPECIES_GOBLIN,
  SPECIES_DRAGON,
  SPECIES_HOBBIT,
  SPECIES_CENTAUR,
  SPECIES_MINOTAUR,
  SPECIES_AVEN,
  SPECIES_CYCLOPS,
  SPECIES_FAUN,
  SPECIES_FAIRY,
  SPECIES_KOATL,
  SPECIES_LYCAN,
  SPECIES_NOT_APPLICABLE,
  SPECIES_COUNT = SPECIES_NOT_APPLICABLE
} ChampionSpecies;

// Champion order enum (The Five Orders of Arcadia)
typedef enum
{ ORDER_A,  // Human, Elf, Dwarf - "Dawn Light"
  ORDER_B,  // Hobbit, Faun, Centaur - "Verdant Light"
  ORDER_C,  // Orc, Goblin, Minotaur - "Ember Light"
  ORDER_D,  // Dragon, Cyclops, Fairy - "Eternal Light"
  ORDER_E,  // Aven, Koatl, Lycan - "Moonlight"
  ORDER_NOT_APPLICABLE,
  ORDER_COUNT = ORDER_NOT_APPLICABLE
} ChampionOrder;

// Combo bonus table -- which scoring table calculate_combo_bonus() (combo_bonus.c)
// applies. Renamed from DeckType 2026-08-28: this names the scoring table, not the
// deck-construction method, since the ~15 planned deck-building approaches
// (ideas/10 Draft Format and Game Depth Addition Ideas/) will each need to point at
// whichever of these 3 tables applies to them -- several are expected to share one.
typedef enum
{ COMBO_BONUS_RANDOM,
  COMBO_BONUS_MONOCHROME,
  COMBO_BONUS_CUSTOM
} ComboBonusTable;

// Card structure
struct card
{ CardType card_type;
  uint8_t cost;

  // Champion card fields
  uint8_t champion_id;
  uint8_t defense_dice;
  uint8_t attack_base;
  ChampionColor color;
  ChampionSpecies species;
  ChampionOrder order;

  // Draw card fields
  uint8_t draw_num;
  uint8_t choose_num;

  // Calculated fields
  float expected_attack;
  float expected_defense;
  float attack_efficiency;
  float defense_efficiency;
  float power;

  // Cash card fields
  uint8_t exchange_cash;
}; // card

// Game state structure
struct gamestate
{ PlayerID current_player;
  uint16_t current_cash_balance[2];
  uint8_t current_energy[2];
  bool someone_has_zero_energy;

  struct deck_stack deck[2];
  Hand hand[2];
  Discard discard[2];
  CombatZone combat_zone[2];

  uint16_t turn;
  GameStateEnum game_state;
  TurnPhase turn_phase;
  PlayerID player_to_move;
  ComboBonusTable combo_bonus_table; // which combo_bonus.c table combat.c should use
}; // gamestate

/* Game mode enumeration */
typedef enum
{ MODE_NONE = 0,
  MODE_STDA_AUTO,    /* Standalone automated */
  MODE_STDA_SIM,     /* Standalone simulation */
  MODE_STDA_CLI,     /* Standalone text command line interface */
  MODE_STDA_TUI,     /* Standalone text UI */
  MODE_STDA_GUI,     /* Standalone graphical UI */
  MODE_STDA_RATING,  /* Standalone Bradley-Terry rating benchmark -- see src/rating/ */
  MODE_SERVER,       /* Server mode */
  MODE_CLIENT_SIM,   /* Client simulation */
  MODE_CLIENT_CLI,   /* Client text command line interface */
  MODE_CLIENT_TUI,   /* Client text UI */
  MODE_CLIENT_GUI,   /* Client graphical UI */
  MODE_CLIENT_AI     /* AI agent client */
} game_mode_t;

/* UI language codes */
typedef enum
{ LANG_EN = 0,  /* English (default) */
  LANG_FR,      /* French */
  LANG_ES       /* Spanish */
} ui_language_t;

/* Player Type: interactive or AI */
typedef enum
{ INTERACTIVE_PLAYER = 0,
  AI_PLAYER = 1
} PlayerType;

// Available AI strategies. Order matches the canonical roster/rating table
// in doc/ai_agents.md (ideas/G1 general info and G2 parameter storing/
// optimization are support material, not themselves agents, so they have no
// entry here).
//
// AI_STRATEGY_GREEDY_POWER and AI_STRATEGY_COMBO_AWARE were renamed to
// AI_STRATEGY_BOREALIS and AI_STRATEGY_COMBO_THRESHOLD (2026-08-21) per the
// names file's "Required renames": Borealis is now the Bradley-Terry
// benchmark, and "Combo Aware" no longer discriminates now that Borealis
// also computes combo bonuses.
//
// Lives here (rather than in ui/shared/player_config.h, where it used to be)
// so src/ai_strat/ can reference it without depending on src/ui/.
typedef enum
{ AI_STRATEGY_RANDOM = 0,
  AI_STRATEGY_VALUE_BASED,     // doc/ai_agents.md's A1 section
  AI_STRATEGY_COMBO_THRESHOLD, // doc/ai_agents.md's A2 section
  AI_STRATEGY_BOREALIS,        // doc/ai_agents.md's A3 section
  AI_STRATEGY_BALANCED,        // doc/ai_agents.md's A4 section
  AI_STRATEGY_HEURISTIC,       // doc/ai_agents.md's A5 section
  AI_STRATEGY_TACTICAL,        // doc/ai_agents.md's A6 section
  AI_STRATEGY_HYBRID_HBT,      // doc/ai_agents.md's A7 section
  AI_STRATEGY_SIMPLE_MC,       // doc/ai_agents.md's A8 section
  AI_STRATEGY_HBT_2PLY,        // doc/ai_agents.md's A9 section
  AI_STRATEGY_ISMCTS,          // doc/ai_agents.md's A10 section
  AI_STRATEGY_ISMCTS_NN,       // doc/ai_agents.md's A11 section
  AI_STRATEGY_CLAIRVOYANT,     // doc/ai_agents.md's A12 section
  // progressive-pruning MC search, opponent-side rollouts
  // use a cheap heuristic instead of pure random
  AI_STRATEGY_CARTOGRAPHER,    // A13 Cartographer -- calibrated and registered
  // 2026-09-04, appended here (not restored to a "13th slot" mid-list) since
  // enum ordinal is a menu/table index everywhere -- see doc/ai_agents.md's
  // A13 section and doc/changelog.md's 2026-09-04 entry. Every mechanism
  // measured at parity with A7 (never a strength improvement, hplus_trust
  // conclusively harmful and pinned off) -- registered anyway for its
  // distinct playing character (race-aware risk appetite, deck-aware draw
  // timing), not for strength. Ships with the Stage 4 calibrated config, not
  // the neutral/A7-recovering one the superset guarantee still allows.
  AI_STRATEGY_ISMCTS_PUCT,     // A14 AlphaOracle Prime Plus I -- doc/ai_agents.md's
  // A14 section. PUCT (learned policy prior directing tree selection,
  // ai_strat_puct_search.c) + a two-head value/policy net
  // (ai_strat_puct_net.c) replacing A10/A11's plain UCT + single-head value
  // net -- the first agent in this lineage whose SEARCH MECHANISM itself
  // differs from A10's, not just its leaf evaluator. Registered
  // unconditionally on the mechanism (Jonathan's call, 2026-09-08) --
  // strength is measured and reported (doc/ai_agents.md), not a
  // registration gate, matching A13's own precedent above.
  AI_STRATEGY_DAREDEVIL,       // A15 Risk Threshold -- doc/ai_agents.md's
  // A15 section. A direct transcription of Jonathan's own real-table
  // decision procedure (Q&A pass, 2026-09-08/09), not a design aimed at a
  // rating target -- registration is unconditional on the measured number,
  // a third distinct reason after A13 (character) and A14 (mechanism). See
  // src/ai_strat/ai_strat_a15.h for the full rule chain.
  AI_STRATEGY_JUNIOR,          // Junior -- doc/ai_agents.md's Junior section.
  // Not part of the A1-A15 ladder: a gap-filler agent (2026-09-11) targeting
  // the largest gap on the roster's rating scale (Random at 2 to A1 at 24),
  // aimed at the log-strength midpoint (~7), not a linear one (13) -- see
  // doc/ai_agents.md's Junior section for why those differ. A stripped-down
  // A1: cost-blind raw-power ranking, one card per attack instead of two,
  // unconditional (no threshold) defense.
  // Gap-2 cluster (A4 Balanced Rules at 36 -> A15/Borealis at 48/50), four
  // agents spread across targets {38, 41, 43, 46} rather than clustered on
  // one midpoint -- see doc/ai_agents.md's gap-2 section. `.a`-`.c` below
  // are measured as-is first; AI_STRATEGY_INCONSISTENT (`.d`) is a
  // per-decision weighted mixture of three existing engines, tuned last to
  // fill whichever target the other three leave uncovered.
  AI_STRATEGY_AUDITOR,         // "Corrected Ledger" / The Auditor (gap-2.a):
  // A4 Balanced Rules, pushed a bit further past its own identity-safe
  // calibration optimum toward (not all the way to) the degenerate
  // spend-everything/never-defend extreme A4's own calibration comment
  // documents finding but rejecting.
  AI_STRATEGY_IMPERSONATOR,    // "Uncalibrated Power" / The Impersonator
  // (gap-2.b): A3 Borealis's exact scoring engine, deliberately mistuned
  // luna_value (the one dial Borealis's own docs call out as the strength
  // dial).
  AI_STRATEGY_JOURNEYMAN,      // "Partial Synthesis" / The Journeyman
  // (gap-2.c): a simplified A2 Combo Threshold attack (single/2-card
  // combos only) plus a simplified A4 Balanced Rules defense (capped
  // selection, no resource-target cash gating) -- two techniques, neither
  // fully mastered.
  AI_STRATEGY_INCONSISTENT,    // "Weighted Mixture" / The Inconsistent
  // (gap-2.d): per-decision weighted delegation among A2/A4/A3's own
  // unmodified attack/defense functions (weights bounded [20%,60%] each).
  // The one deliberate exception to this cluster's otherwise-deterministic
  // design -- genuine unpredictability is the point. Tuned last, to fill
  // whichever gap-2 target the other three left uncovered.
  // Gap-3 cluster (A6 Tactical at 52 -> A9/A14 at 62), four agents spread
  // across targets {54, 56, 58, 60} -- same spread-not-cluster approach as
  // gap-2, see doc/ai_agents.md's gap-3 section.
  AI_STRATEGY_SPARRING_PARTNER, // "HBT Lite" / The Sparring Partner
  // (gap-3.a): A7 Hybrid HBT's T->H coupling (A6's aggression factor
  // modulating A5's advantage weights), ported verbatim, but no Layer B
  // (A4) penalty and no lethal-combo hold -- two of A7's three
  // ingredients, not three.
  AI_STRATEGY_OPPORTUNIST,      // "Tactical Plus" / The Opportunist
  // (gap-3.b): A6 Tactical's exact mechanism, unchanged, plus a
  // lethal-combo hold (ported from A3) gating the greedily-selected
  // attack -- one added feature, not a synthesis.
  AI_STRATEGY_ADEPT,            // "Reduced Heuristic" / The Adept
  // (gap-3.c): A5 Heuristic's exact enumeration shape, with the
  // cards-advantage term and the opponent-energy taper both dropped --
  // energy + cash advantage only, flat throughout the game.
  AI_STRATEGY_EXPERIMENTER,     // "Weighted Mixture II" / The Experimenter
  // (gap-3.d): per-decision weighted delegation among A3/A6/A5's own
  // unmodified attack/defense functions (weights bounded [20%,60%] each).
  // Same mechanism as AI_STRATEGY_INCONSISTENT. Tuned last, to fill
  // whichever gap-3 target the other three left uncovered.
  AI_STRATEGY_COUNT
} AIStrategyType;

/* Configuration structure */
typedef struct
{ game_mode_t mode;
  bool verbose;
  int numsim;
  char* input_file;
  char* output_file;
  char* ai_agent;
  ui_language_t language;
  uint32_t prng_seed;
  bool use_random_seed;
  void* player_config;  /* PlayerConfig* - forward declaration avoidance */
  AIStrategyType agent[2];  /* -Aa/-ai.a and -Ab/-ai.b: per-player agent for
                               --stda.auto (see cmdline.c). Index by PlayerID. */
  bool agent_set[2];        /* whether agent[i] was actually given on the CLI,
                               vs. left at its AI_STRATEGY_RANDOM zero-value. */
  int rating_games;         /* --rating.games: games per seat-orientation for
                                MODE_STDA_RATING. 0 = use the mode's own default. */
  char* rating_file;        /* --rating.file: CSV to seed from (if present) and
                                write. NULL = no persistence. */
  bool rating_method_gradient; /* --rating.method=gradient (default: MM). A
                                   plain bool, not a RatingBatchMethod, to
                                   avoid game_types.h depending on
                                   src/rating/rating.h (same reasoning as
                                   player_config's void*, above). */
  bool rating_track;        /* --rating.track: enable human rating tracking
                                in stda.cli/stda.tui. Off by default. */
  char* rating_agents;      /* --rating.agents: comma-separated AI strategy
                                shorthands to restrict MODE_STDA_RATING's
                                round-robin to (see stda_rating.c's
                                register_implemented_agents()). NULL = every
                                implemented agent (today's default
                                behaviour) -- mainly useful to exclude
                                expensive tree-search agents (A10/A11) from
                                a quick fit. */
  char* nn_weights;         /* --ai.weights: path to A11 AlphaOracle Prime's
                                value-net weights, loaded once at startup
                                (main.c). NULL = ISMCTSNN_DEFAULT_WEIGHTS_PATH
                                (the packaged assets/ismctsnn/ asset). */
  char* puct_weights;       /* --ai.puct-weights: path to A14 AlphaOracle
                                Prime Plus I's two-head net weights, loaded
                                once at startup (main.c). NULL =
                                PUCT_DEFAULT_WEIGHTS_PATH (the packaged
                                assets/puct/ asset). Separate flag from
                                --ai.weights -- two independent weight files
                                for two independent agents/net modules. */
} config_t;

#include "game_constants.h"

// Game statistics structure
struct gamestats
{ uint16_t cumul_player_wins[2];
  uint16_t cumul_number_of_draws;
  uint16_t game_end_turn_number[MAX_NUMBER_OF_SIM];  // look into dynamically allocating space for this as we don't need 1000 entries for interactive modes (we just need one)
  uint16_t simnum;
}; // gamestats

#endif // GAME_TYPES_H
