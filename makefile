# Makefile for Oracle: The Champions of Arcadia
# Updated to work with refactored modular structure

CC := gcc
SRCDIR := src
TESTSRCDIR := testsrc
AICALIBDIR := aicalibsrc
BUILDDIR := obj
BINDIR := bin
TARGET := $(BINDIR)/oracle
SRCEXT := c
INCEXT := h
LIBS := -lm -lncursesw
#LIBS=-pthread -lncursesw -lpanelw -lformw -lmenuw


# Automatically find all .c files in src directory
SOURCES := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
OBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.o))

# Compiler flags
# -MMD -MP generate a per-object .d file listing the headers it includes, so
# a header edit correctly triggers a rebuild of everything that includes it
# (directly or transitively) -- see the -include $(DEPS) below.
CFLAGS := -g -Og -Wall -std=c23 -MMD -MP

# Shared source lists for anything that links the whole engine + AI-strategy
# roster in-process (every CALIB_*_SRCS below, plus TEST_RECALL_SRCS). Before
# A8, each of those seven-plus lists spelled out the same ~11 engine files and
# ~14 agent files by hand; adding a new agent meant editing all of them. Now a
# new agent's ai_strat_<name>*.c files are added to AGENT_SRCS exactly once.
ENGINE_SRCS := $(SRCDIR)/core/card_actions.c \
               $(SRCDIR)/core/combat.c \
               $(SRCDIR)/core/combo_bonus.c \
               $(SRCDIR)/core/game_constants.c \
               $(SRCDIR)/core/game_context.c \
               $(SRCDIR)/core/game_state.c \
               $(SRCDIR)/core/turn_logic.c \
               $(SRCDIR)/structures/card_collection.c \
               $(SRCDIR)/structures/deckstack.c \
               $(SRCDIR)/util/mtwister.c \
               $(SRCDIR)/util/rnd.c \
               $(SRCDIR)/actions/move_gen.c \
               $(SRCDIR)/actions/move_apply.c

AGENT_SRCS := $(SRCDIR)/ai_strat/ai_strategy.c \
              $(SRCDIR)/ai_strat/ai_strat_random.c \
              $(SRCDIR)/ai_strat/ai_strat_common.c \
              $(SRCDIR)/ai_strat/ai_strat_lib_heuristics.c \
              $(SRCDIR)/ai_strat/ai_strat_valuebased.c \
              $(SRCDIR)/ai_strat/ai_strat_combo_threshold.c \
              $(SRCDIR)/ai_strat/ai_strat_borealis.c \
              $(SRCDIR)/ai_strat/ai_strat_borealis_enum.c \
              $(SRCDIR)/ai_strat/ai_strat_balanced_rules.c \
              $(SRCDIR)/ai_strat/ai_strat_heuristic.c \
              $(SRCDIR)/ai_strat/ai_strat_tactical.c \
              $(SRCDIR)/ai_strat/ai_strat_hbt.c \
              $(SRCDIR)/ai_strat/ai_strat_hbt_enum.c \
              $(SRCDIR)/ai_strat/ai_strat_hbt_cards.c \
              $(SRCDIR)/ai_strat/ai_strat_hbt2ply.c \
              $(SRCDIR)/ai_strat/ai_strat_hbt2ply_reply.c \
              $(SRCDIR)/ai_strat/ai_strat_playout.c \
              $(SRCDIR)/ai_strat/ai_strat_simplemc_search.c \
              $(SRCDIR)/ai_strat/ai_strat_simplemc1.c \
              $(SRCDIR)/ai_strat/ai_strat_clairvoyant1.c \
              $(SRCDIR)/ai_strat/ai_strat_ismcts_tree.c \
              $(SRCDIR)/ai_strat/ai_strat_ismcts_search.c \
              $(SRCDIR)/ai_strat/ai_strat_ismcts_flat.c \
              $(SRCDIR)/ai_strat/ai_strat_ismcts1.c

# Test targets
# Object paths are mapped into $(BUILDDIR) (mirroring the main build's pattern rule
# below) rather than left inside $(SRCDIR)/$(TESTSRCDIR), so test builds share objects
# with bin/oracle instead of recompiling in place and leaving stray .o files in the
# source tree.
TEST_COMBO_TARGET := $(BINDIR)/test_combo
TEST_COMBO_SRCS := $(TESTSRCDIR)/test_combo_bonus.c \
                   $(SRCDIR)/core/combo_bonus.c \
                   $(SRCDIR)/core/game_constants.c
TEST_COMBO_OBJS := $(BUILDDIR)/testsrc/test_combo_bonus.o \
                   $(BUILDDIR)/core/combo_bonus.o \
                   $(BUILDDIR)/core/game_constants.o

TEST_RECALL_TARGET := $(BINDIR)/test_recall
# ui/shared/player_config.c pulls in the whole AI-strategy roster transitively
# (display_ai_strategy_menu()/get_ai_strategy_choice() call
# ai_strategy_is_implemented(), which needs ai_strategy.c's STRATEGY_REGISTRY
# populated -- same "linking player_config.c means linking the roster it
# depends on" reasoning as the CALIB_*_SRCS lists below), even though this
# test itself never touches AI strategy selection -- format_player_label()
# (cli_display.c/cli_action_display.c, added alongside A4) is the only reason
# player_config.c is linked here at all.
TEST_RECALL_SRCS := $(TESTSRCDIR)/test_recall.c \
                    $(SRCDIR)/ui/cli/cli_input.c \
                    $(SRCDIR)/ui/cli/cli_io.c \
                    $(SRCDIR)/ui/cli/cli_display.c \
                    $(SRCDIR)/ui/cli/cli_action_display.c \
                    $(SRCDIR)/ui/interactive/game_commands.c \
                    $(SRCDIR)/ui/interactive/game_commands_cards.c \
                    $(SRCDIR)/ui/shared/player_config.c \
                    $(ENGINE_SRCS) \
                    $(AGENT_SRCS)
TEST_RECALL_OBJS := $(BUILDDIR)/testsrc/test_recall.o \
                    $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(filter $(SRCDIR)/%,$(TEST_RECALL_SRCS)))

TEST_CASH_TARGET := $(BINDIR)/test_cash_exchange
TEST_CASH_SRCS := $(TESTSRCDIR)/test_cash_exchange.c \
                  $(SRCDIR)/core/card_actions.c \
                  $(SRCDIR)/core/game_constants.c \
                  $(SRCDIR)/core/game_context.c \
                  $(SRCDIR)/structures/card_collection.c \
                  $(SRCDIR)/structures/deckstack.c \
                  $(SRCDIR)/util/mtwister.c \
                  $(SRCDIR)/util/rnd.c
TEST_CASH_OBJS := $(BUILDDIR)/testsrc/test_cash_exchange.o \
                  $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(filter $(SRCDIR)/%,$(TEST_CASH_SRCS)))

# Test combat.c's combo-bonus table selection (added 2026-08-28 alongside the
# ComboBonusTable rename/fix -- no prior test exercised combat.c's combo-bonus
# integration at all, only combo_bonus.c's own routing). Narrow TEST_CASH_SRCS
# shape plus combat.c/combo_bonus.c/game_state.c (needed for setup_game()'s
# default), not the whole-engine TEST_RECALL_SRCS/TEST_MOVES_SRCS shape --
# this test never touches AI strategies.
TEST_COMBAT_TARGET := $(BINDIR)/test_combat
TEST_COMBAT_SRCS := $(TESTSRCDIR)/test_combat.c \
                    $(SRCDIR)/core/combat.c \
                    $(SRCDIR)/core/combo_bonus.c \
                    $(SRCDIR)/core/game_constants.c \
                    $(SRCDIR)/core/game_context.c \
                    $(SRCDIR)/core/game_state.c \
                    $(SRCDIR)/structures/card_collection.c \
                    $(SRCDIR)/structures/deckstack.c \
                    $(SRCDIR)/util/mtwister.c \
                    $(SRCDIR)/util/rnd.c
TEST_COMBAT_OBJS := $(BUILDDIR)/testsrc/test_combat.o \
                    $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(filter $(SRCDIR)/%,$(TEST_COMBAT_SRCS)))

# ai_strat_hbt2ply_reply.c started as just the surrogate-hand builder
# (fullDeck + Hand/Discard/CombatZone only) but has since grown A9's
# two-ply scoring/reply-oracle functions too, which pull in the full HBT
# stack (ai_strat_hbt_enum.c's hbt_advantage()/predicted_damage()/etc.) --
# same "link the whole engine + roster" reasoning as TEST_RECALL_SRCS/
# TEST_MOVES_SRCS above, not the narrow TEST_CASH_SRCS shape this target
# started with.
TEST_HBT2PLY_REPLY_TARGET := $(BINDIR)/test_hbt2ply_reply
TEST_HBT2PLY_REPLY_SRCS := $(TESTSRCDIR)/test_hbt2ply_reply.c \
                           $(ENGINE_SRCS) \
                           $(AGENT_SRCS)
TEST_HBT2PLY_REPLY_OBJS := $(BUILDDIR)/testsrc/test_hbt2ply_reply.o \
                           $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(filter $(SRCDIR)/%,$(TEST_HBT2PLY_REPLY_SRCS)))

# Test move enumeration/application/playout (src/actions/,
# ai_strat_playout.c) -- the engine-level infrastructure A8 Simple Monte
# Carlo (and later A9-A11) builds on. src/actions/*.c live in ENGINE_SRCS and
# ai_strat_playout.c/ai_strat_simplemc*.c live in AGENT_SRCS now that A8
# registers and references them, so this target needs nothing beyond those
# two shared lists plus its own test file.
TEST_MOVES_TARGET := $(BINDIR)/test_moves
TEST_MOVES_SRCS := $(TESTSRCDIR)/test_moves.c \
                   $(ENGINE_SRCS) \
                   $(AGENT_SRCS)
TEST_MOVES_OBJS := $(BUILDDIR)/testsrc/test_moves.o \
                   $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(filter $(SRCDIR)/%,$(TEST_MOVES_SRCS)))

# Test A10 IS-MCTS's node arena/UCT tree (ai_strat_ismcts_tree.c) and search
# loop (ai_strat_ismcts_search.c) -- same whole-roster reasoning as
# TEST_MOVES_SRCS above (both now live in AGENT_SRCS).
TEST_ISMCTS_TARGET := $(BINDIR)/test_ismcts
TEST_ISMCTS_SRCS := $(TESTSRCDIR)/test_ismcts.c \
                    $(ENGINE_SRCS) \
                    $(AGENT_SRCS)
TEST_ISMCTS_OBJS := $(BUILDDIR)/testsrc/test_ismcts.o \
                    $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(filter $(SRCDIR)/%,$(TEST_ISMCTS_SRCS)))

# src/rating/ is dependency-free (game_types.h + libc only -- see rating.h),
# so this test needs no engine objects beyond the rating module itself.
TEST_RATING_TARGET := $(BINDIR)/test_rating
TEST_RATING_SRCS := $(TESTSRCDIR)/test_rating.c \
                    $(SRCDIR)/rating/rating_core.c \
                    $(SRCDIR)/rating/rating_update.c \
                    $(SRCDIR)/rating/rating_batch.c \
                    $(SRCDIR)/rating/rating_csv.c
TEST_RATING_OBJS := $(BUILDDIR)/testsrc/test_rating.o \
                    $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(filter $(SRCDIR)/%,$(TEST_RATING_SRCS)))

# Calibration harness for A1 Value Based's tunable parameters (see
# aicalibsrc/value/). One subfolder per agent under aicalibsrc/ as more
# agents get calibration tooling. Links the engine directly (same pattern as
# the TEST_* targets above) plus stda_auto.c (for run_simulation()) and
# player_config.c (stda_auto.c's own dependency, for
# get_ai_strategies()/parse_ai_strategy_shorthand()). Links every agent
# ai_strategy.c's STRATEGY_REGISTRY references (not just Value Based) plus
# ai_strat_lib_heuristics.c (the shared mulligan/discard-to-7 default every
# registry entry falls back to) -- anything that links ai_strategy.c needs
# the whole roster it registers, updated here each time a new agent joins it.
CALIB_VALUEBASED_TARGET := $(BINDIR)/calib_valuebased
CALIB_VALUEBASED_SRCS := $(AICALIBDIR)/value/calib_valuebased.c \
                         $(ENGINE_SRCS) \
                         $(AGENT_SRCS) \
                         $(SRCDIR)/roles/stda/stda_auto.c \
                         $(SRCDIR)/ui/shared/player_config.c
CALIB_VALUEBASED_OBJS := $(BUILDDIR)/aicalibsrc/value/calib_valuebased.o \
                         $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(filter $(SRCDIR)/%,$(CALIB_VALUEBASED_SRCS)))

# Calibration harness for A2 Combo Threshold's tunable parameters (see
# aicalibsrc/combo/). Same pattern as CALIB_VALUEBASED_* above, same
# whole-roster reasoning.
CALIB_COMBO_TARGET := $(BINDIR)/calib_combo_threshold
CALIB_COMBO_SRCS := $(AICALIBDIR)/combo/calib_combo_threshold.c \
                    $(ENGINE_SRCS) \
                    $(AGENT_SRCS) \
                    $(SRCDIR)/roles/stda/stda_auto.c \
                    $(SRCDIR)/ui/shared/player_config.c
CALIB_COMBO_OBJS := $(BUILDDIR)/aicalibsrc/combo/calib_combo_threshold.o \
                    $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(filter $(SRCDIR)/%,$(CALIB_COMBO_SRCS)))

# Calibration harness for A3 Borealis's tunable parameters (see
# aicalibsrc/borealis/). Same pattern and same whole-roster reasoning as
# CALIB_VALUEBASED_*/CALIB_COMBO_* above.
CALIB_BOREALIS_TARGET := $(BINDIR)/calib_borealis
CALIB_BOREALIS_SRCS := $(AICALIBDIR)/borealis/calib_borealis.c \
                       $(ENGINE_SRCS) \
                       $(AGENT_SRCS) \
                       $(SRCDIR)/roles/stda/stda_auto.c \
                       $(SRCDIR)/ui/shared/player_config.c
CALIB_BOREALIS_OBJS := $(BUILDDIR)/aicalibsrc/borealis/calib_borealis.o \
                       $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(filter $(SRCDIR)/%,$(CALIB_BOREALIS_SRCS)))

# Calibration harness for A4 Balanced Rules's tunable parameters (see
# aicalibsrc/balanced/). Same pattern and same whole-roster reasoning as
# CALIB_VALUEBASED_*/CALIB_COMBO_*/CALIB_BOREALIS_* above.
CALIB_BALANCED_TARGET := $(BINDIR)/calib_balanced
CALIB_BALANCED_SRCS := $(AICALIBDIR)/balanced/calib_balanced.c \
                       $(ENGINE_SRCS) \
                       $(AGENT_SRCS) \
                       $(SRCDIR)/roles/stda/stda_auto.c \
                       $(SRCDIR)/ui/shared/player_config.c
CALIB_BALANCED_OBJS := $(BUILDDIR)/aicalibsrc/balanced/calib_balanced.o \
                       $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(filter $(SRCDIR)/%,$(CALIB_BALANCED_SRCS)))

# Calibration harness for A5 Heuristic's tunable parameters (see
# aicalibsrc/heuristic/). Same pattern and same whole-roster reasoning as
# CALIB_VALUEBASED_*/CALIB_COMBO_*/CALIB_BOREALIS_*/CALIB_BALANCED_* above.
CALIB_HEURISTIC_TARGET := $(BINDIR)/calib_heuristic
CALIB_HEURISTIC_SRCS := $(AICALIBDIR)/heuristic/calib_heuristic.c \
                        $(ENGINE_SRCS) \
                        $(AGENT_SRCS) \
                        $(SRCDIR)/roles/stda/stda_auto.c \
                        $(SRCDIR)/ui/shared/player_config.c
CALIB_HEURISTIC_OBJS := $(BUILDDIR)/aicalibsrc/heuristic/calib_heuristic.o \
                        $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(filter $(SRCDIR)/%,$(CALIB_HEURISTIC_SRCS)))

# Calibration harness for A6 Tactical's tunable parameters (see
# aicalibsrc/tactical/). Same pattern and same whole-roster reasoning as
# CALIB_VALUEBASED_*/CALIB_COMBO_*/CALIB_BOREALIS_*/CALIB_BALANCED_*/
# CALIB_HEURISTIC_* above.
CALIB_TACTICAL_TARGET := $(BINDIR)/calib_tactical
CALIB_TACTICAL_SRCS := $(AICALIBDIR)/tactical/calib_tactical.c \
                       $(ENGINE_SRCS) \
                       $(AGENT_SRCS) \
                       $(SRCDIR)/roles/stda/stda_auto.c \
                       $(SRCDIR)/ui/shared/player_config.c
CALIB_TACTICAL_OBJS := $(BUILDDIR)/aicalibsrc/tactical/calib_tactical.o \
                       $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(filter $(SRCDIR)/%,$(CALIB_TACTICAL_SRCS)))

# Calibration harness for A7 Hybrid HBT's tunable parameters (see
# aicalibsrc/hbt/). Same pattern and same whole-roster reasoning as
# CALIB_VALUEBASED_*/CALIB_COMBO_*/CALIB_BOREALIS_*/CALIB_BALANCED_*/
# CALIB_HEURISTIC_*/CALIB_TACTICAL_* above.
CALIB_HBT_TARGET := $(BINDIR)/calib_hbt
CALIB_HBT_SRCS := $(AICALIBDIR)/hbt/calib_hbt.c \
                  $(ENGINE_SRCS) \
                  $(AGENT_SRCS) \
                  $(SRCDIR)/roles/stda/stda_auto.c \
                  $(SRCDIR)/ui/shared/player_config.c
CALIB_HBT_OBJS := $(BUILDDIR)/aicalibsrc/hbt/calib_hbt.o \
                  $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(filter $(SRCDIR)/%,$(CALIB_HBT_SRCS)))

# Calibration harness for A9 HBT 2-Ply's tunable parameters (see
# aicalibsrc/hbt2ply/). Same pattern and same whole-roster reasoning as
# CALIB_VALUEBASED_*/CALIB_COMBO_*/CALIB_BOREALIS_*/CALIB_BALANCED_*/
# CALIB_HEURISTIC_*/CALIB_TACTICAL_*/CALIB_HBT_* above.
CALIB_HBT2PLY_TARGET := $(BINDIR)/calib_hbt2ply
CALIB_HBT2PLY_SRCS := $(AICALIBDIR)/hbt2ply/calib_hbt2ply.c \
                      $(ENGINE_SRCS) \
                      $(AGENT_SRCS) \
                      $(SRCDIR)/roles/stda/stda_auto.c \
                      $(SRCDIR)/ui/shared/player_config.c
CALIB_HBT2PLY_OBJS := $(BUILDDIR)/aicalibsrc/hbt2ply/calib_hbt2ply.o \
                      $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(filter $(SRCDIR)/%,$(CALIB_HBT2PLY_SRCS)))

# Calibration harness for A8 Simple Monte Carlo's tunable parameters (see
# aicalibsrc/simplemc/). Same pattern and same whole-roster reasoning as
# CALIB_VALUEBASED_*/CALIB_COMBO_*/CALIB_BOREALIS_*/CALIB_BALANCED_*/
# CALIB_HEURISTIC_*/CALIB_TACTICAL_*/CALIB_HBT_* above.
CALIB_SIMPLEMC_TARGET := $(BINDIR)/calib_simplemc
CALIB_SIMPLEMC_SRCS := $(AICALIBDIR)/simplemc/calib_simplemc.c \
                       $(ENGINE_SRCS) \
                       $(AGENT_SRCS) \
                       $(SRCDIR)/roles/stda/stda_auto.c \
                       $(SRCDIR)/ui/shared/player_config.c
CALIB_SIMPLEMC_OBJS := $(BUILDDIR)/aicalibsrc/simplemc/calib_simplemc.o \
                       $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(filter $(SRCDIR)/%,$(CALIB_SIMPLEMC_SRCS)))

# Phase 3 timing-only harness for A10 IS-MCTS (see aicalibsrc/ismcts/
# calib_ismcts_timing.c) -- measures wall-clock per-decision latency to pin
# limit_iterations to ~1s/decision. Not a params sweep/optimize harness (that
# is Phase 5's separate calib_ismcts.c, added later); same whole-roster link
# set as the other CALIB_* targets regardless.
CALIB_ISMCTS_TIMING_TARGET := $(BINDIR)/calib_ismcts_timing
CALIB_ISMCTS_TIMING_SRCS := $(AICALIBDIR)/ismcts/calib_ismcts_timing.c \
                            $(ENGINE_SRCS) \
                            $(AGENT_SRCS) \
                            $(SRCDIR)/roles/stda/stda_auto.c \
                            $(SRCDIR)/ui/shared/player_config.c
CALIB_ISMCTS_TIMING_OBJS := $(BUILDDIR)/aicalibsrc/ismcts/calib_ismcts_timing.o \
                            $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(filter $(SRCDIR)/%,$(CALIB_ISMCTS_TIMING_SRCS)))

# Phase 5 lean efficiency-dial sweep harness for A10 IS-MCTS (see
# aicalibsrc/ismcts/calib_ismcts_efficiency.c) -- not the full DE-optimizer
# pipeline other agents have, see that file's header comment for why.
CALIB_ISMCTS_EFFICIENCY_TARGET := $(BINDIR)/calib_ismcts_efficiency
CALIB_ISMCTS_EFFICIENCY_SRCS := $(AICALIBDIR)/ismcts/calib_ismcts_efficiency.c \
                                $(ENGINE_SRCS) \
                                $(AGENT_SRCS) \
                                $(SRCDIR)/roles/stda/stda_auto.c \
                                $(SRCDIR)/ui/shared/player_config.c
CALIB_ISMCTS_EFFICIENCY_OBJS := $(BUILDDIR)/aicalibsrc/ismcts/calib_ismcts_efficiency.o \
                                $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(filter $(SRCDIR)/%,$(CALIB_ISMCTS_EFFICIENCY_SRCS)))

# Phase 6 diagnostic harness for A10 IS-MCTS's rollout policy (see
# aicalibsrc/ismcts/calib_ismcts_rollout_policy.c) -- tests the "same
# rollout-policy bias as A8" hypothesis for the Step 1 budget-curve plateau.
CALIB_ISMCTS_ROLLOUT_TARGET := $(BINDIR)/calib_ismcts_rollout_policy
CALIB_ISMCTS_ROLLOUT_SRCS := $(AICALIBDIR)/ismcts/calib_ismcts_rollout_policy.c \
                             $(ENGINE_SRCS) \
                             $(AGENT_SRCS) \
                             $(SRCDIR)/roles/stda/stda_auto.c \
                             $(SRCDIR)/ui/shared/player_config.c
CALIB_ISMCTS_ROLLOUT_OBJS := $(BUILDDIR)/aicalibsrc/ismcts/calib_ismcts_rollout_policy.o \
                             $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(filter $(SRCDIR)/%,$(CALIB_ISMCTS_ROLLOUT_SRCS)))

# Default target
all: $(TARGET)

# Link object files to create executable
$(TARGET): $(OBJECTS)
	@echo "Linking..."
	@mkdir -p $(BINDIR)
	$(CC) $^ -o $(TARGET) $(LIBS)
	@echo "Build complete: $(TARGET)"

# Compile source files to object files
$(BUILDDIR)/%.o: $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p "$(@D)"
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile test source files to object files (kept out of $(TESTSRCDIR) itself)
$(BUILDDIR)/testsrc/%.o: $(TESTSRCDIR)/%.$(SRCEXT)
	@mkdir -p "$(@D)"
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile calibration source files to object files (kept out of $(AICALIBDIR) itself)
$(BUILDDIR)/aicalibsrc/%.o: $(AICALIBDIR)/%.$(SRCEXT)
	@mkdir -p "$(@D)"
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning..."
	$(RM) -r $(BUILDDIR)/* $(BINDIR)/oracle* $(BINDIR)/test_combo $(BINDIR)/test_recall $(BINDIR)/test_cash_exchange $(BINDIR)/test_rating $(BINDIR)/test_moves $(BINDIR)/test_ismcts $(BINDIR)/test_hbt2ply_reply $(BINDIR)/test_combat $(BINDIR)/calib_valuebased $(BINDIR)/calib_combo_threshold $(BINDIR)/calib_borealis $(BINDIR)/calib_balanced $(BINDIR)/calib_heuristic $(BINDIR)/calib_tactical $(BINDIR)/calib_hbt $(BINDIR)/calib_hbt2ply $(BINDIR)/calib_simplemc $(BINDIR)/calib_ismcts_timing $(BINDIR)/calib_ismcts_efficiency $(BINDIR)/calib_ismcts_rollout_policy
	$(RM) $(SRCDIR)/*.o $(SRCDIR)/*/*.o $(SRCDIR)/*/*/*.o $(SRCDIR)/*/*/*/*.o $(TESTSRCDIR)/*.o $(AICALIBDIR)/*.o
	@echo "Clean complete"

# Debug build
.PHONY: debug
debug: CFLAGS := -g -Og -Wall -std=c23 -MMD -MP -DDEBUG -DDEBUG_ENABLED=1
debug: clean all
	@echo "Debug build complete"

# Test combo bonus calculator
.PHONY: test_combo
test_combo: $(TEST_COMBO_TARGET)
	./$(TEST_COMBO_TARGET)

$(TEST_COMBO_TARGET): $(TEST_COMBO_OBJS)
	@echo "Linking test_combo..."
	@mkdir -p $(BINDIR)
	$(CC) $(TEST_COMBO_OBJS) -o $(TEST_COMBO_TARGET) $(LIBS)
	@echo "Test build complete: $(TEST_COMBO_TARGET)"

# Test recall mechanic
.PHONY: test_recall
test_recall: $(TEST_RECALL_TARGET)
	./$(TEST_RECALL_TARGET)

$(TEST_RECALL_TARGET): $(TEST_RECALL_OBJS)
	@echo "Linking test_recall..."
	@mkdir -p $(BINDIR)
	$(CC) $(TEST_RECALL_OBJS) -o $(TEST_RECALL_TARGET) $(LIBS)
	@echo "Test build complete: $(TEST_RECALL_TARGET)"

# Test cash exchange (interactive path)
.PHONY: test_cash_exchange
test_cash_exchange: $(TEST_CASH_TARGET)
	./$(TEST_CASH_TARGET)

$(TEST_CASH_TARGET): $(TEST_CASH_OBJS)
	@echo "Linking test_cash_exchange..."
	@mkdir -p $(BINDIR)
	$(CC) $(TEST_CASH_OBJS) -o $(TEST_CASH_TARGET) $(LIBS)
	@echo "Test build complete: $(TEST_CASH_TARGET)"

# Test combat.c's combo-bonus table selection
.PHONY: test_combat
test_combat: $(TEST_COMBAT_TARGET)
	./$(TEST_COMBAT_TARGET)

$(TEST_COMBAT_TARGET): $(TEST_COMBAT_OBJS)
	@echo "Linking test_combat..."
	@mkdir -p $(BINDIR)
	$(CC) $(TEST_COMBAT_OBJS) -o $(TEST_COMBAT_TARGET) $(LIBS)
	@echo "Test build complete: $(TEST_COMBAT_TARGET)"

# Test A9 HBT 2-Ply's surrogate-hand builder
.PHONY: test_hbt2ply_reply
test_hbt2ply_reply: $(TEST_HBT2PLY_REPLY_TARGET)
	./$(TEST_HBT2PLY_REPLY_TARGET)

$(TEST_HBT2PLY_REPLY_TARGET): $(TEST_HBT2PLY_REPLY_OBJS)
	@echo "Linking test_hbt2ply_reply..."
	@mkdir -p $(BINDIR)
	$(CC) $(TEST_HBT2PLY_REPLY_OBJS) -o $(TEST_HBT2PLY_REPLY_TARGET) $(LIBS)
	@echo "Test build complete: $(TEST_HBT2PLY_REPLY_TARGET)"

# Test move enumeration (src/actions/move_gen.c)
.PHONY: test_moves
test_moves: $(TEST_MOVES_TARGET)
	./$(TEST_MOVES_TARGET)

$(TEST_MOVES_TARGET): $(TEST_MOVES_OBJS)
	@echo "Linking test_moves..."
	@mkdir -p $(BINDIR)
	$(CC) $(TEST_MOVES_OBJS) -o $(TEST_MOVES_TARGET) $(LIBS)
	@echo "Test build complete: $(TEST_MOVES_TARGET)"

# Test A10 IS-MCTS's node arena/UCT tree and search loop
.PHONY: test_ismcts
test_ismcts: $(TEST_ISMCTS_TARGET)
	./$(TEST_ISMCTS_TARGET)

$(TEST_ISMCTS_TARGET): $(TEST_ISMCTS_OBJS)
	@echo "Linking test_ismcts..."
	@mkdir -p $(BINDIR)
	$(CC) $(TEST_ISMCTS_OBJS) -o $(TEST_ISMCTS_TARGET) $(LIBS)
	@echo "Test build complete: $(TEST_ISMCTS_TARGET)"

# Test the Bradley-Terry rating system (src/rating/)
.PHONY: test_rating
test_rating: $(TEST_RATING_TARGET)
	./$(TEST_RATING_TARGET)

$(TEST_RATING_TARGET): $(TEST_RATING_OBJS)
	@echo "Linking test_rating..."
	@mkdir -p $(BINDIR)
	$(CC) $(TEST_RATING_OBJS) -o $(TEST_RATING_TARGET) $(LIBS)
	@echo "Test build complete: $(TEST_RATING_TARGET)"

# Calibration harness (see aicalibsrc/value/README.md or the file header for CLI usage)
.PHONY: calib_valuebased
calib_valuebased: $(CALIB_VALUEBASED_TARGET)

$(CALIB_VALUEBASED_TARGET): $(CALIB_VALUEBASED_OBJS)
	@echo "Linking calib_valuebased..."
	@mkdir -p $(BINDIR)
	$(CC) $(CALIB_VALUEBASED_OBJS) -o $(CALIB_VALUEBASED_TARGET) $(LIBS)
	@echo "Build complete: $(CALIB_VALUEBASED_TARGET)"

# Calibration harness (see aicalibsrc/combo/README.md or the file header for CLI usage)
.PHONY: calib_combo_threshold
calib_combo_threshold: $(CALIB_COMBO_TARGET)

$(CALIB_COMBO_TARGET): $(CALIB_COMBO_OBJS)
	@echo "Linking calib_combo_threshold..."
	@mkdir -p $(BINDIR)
	$(CC) $(CALIB_COMBO_OBJS) -o $(CALIB_COMBO_TARGET) $(LIBS)
	@echo "Build complete: $(CALIB_COMBO_TARGET)"

# Calibration harness (see aicalibsrc/borealis/README.md or the file header for CLI usage)
.PHONY: calib_borealis
calib_borealis: $(CALIB_BOREALIS_TARGET)

$(CALIB_BOREALIS_TARGET): $(CALIB_BOREALIS_OBJS)
	@echo "Linking calib_borealis..."
	@mkdir -p $(BINDIR)
	$(CC) $(CALIB_BOREALIS_OBJS) -o $(CALIB_BOREALIS_TARGET) $(LIBS)
	@echo "Build complete: $(CALIB_BOREALIS_TARGET)"

# Calibration harness (see aicalibsrc/balanced/README.md or the file header for CLI usage)
.PHONY: calib_balanced
calib_balanced: $(CALIB_BALANCED_TARGET)

$(CALIB_BALANCED_TARGET): $(CALIB_BALANCED_OBJS)
	@echo "Linking calib_balanced..."
	@mkdir -p $(BINDIR)
	$(CC) $(CALIB_BALANCED_OBJS) -o $(CALIB_BALANCED_TARGET) $(LIBS)
	@echo "Build complete: $(CALIB_BALANCED_TARGET)"

# Calibration harness (see aicalibsrc/heuristic/README.md or the file header for CLI usage)
.PHONY: calib_heuristic
calib_heuristic: $(CALIB_HEURISTIC_TARGET)

$(CALIB_HEURISTIC_TARGET): $(CALIB_HEURISTIC_OBJS)
	@echo "Linking calib_heuristic..."
	@mkdir -p $(BINDIR)
	$(CC) $(CALIB_HEURISTIC_OBJS) -o $(CALIB_HEURISTIC_TARGET) $(LIBS)
	@echo "Build complete: $(CALIB_HEURISTIC_TARGET)"

# Calibration harness (see aicalibsrc/tactical/README.md or the file header for CLI usage)
.PHONY: calib_tactical
calib_tactical: $(CALIB_TACTICAL_TARGET)

$(CALIB_TACTICAL_TARGET): $(CALIB_TACTICAL_OBJS)
	@echo "Linking calib_tactical..."
	@mkdir -p $(BINDIR)
	$(CC) $(CALIB_TACTICAL_OBJS) -o $(CALIB_TACTICAL_TARGET) $(LIBS)
	@echo "Build complete: $(CALIB_TACTICAL_TARGET)"

# Calibration harness (see aicalibsrc/hbt/README.md or the file header for CLI usage)
.PHONY: calib_hbt
calib_hbt: $(CALIB_HBT_TARGET)

$(CALIB_HBT_TARGET): $(CALIB_HBT_OBJS)
	@echo "Linking calib_hbt..."
	@mkdir -p $(BINDIR)
	$(CC) $(CALIB_HBT_OBJS) -o $(CALIB_HBT_TARGET) $(LIBS)
	@echo "Build complete: $(CALIB_HBT_TARGET)"

# Calibration harness (see aicalibsrc/hbt2ply/README.md or the file header for CLI usage)
.PHONY: calib_hbt2ply
calib_hbt2ply: $(CALIB_HBT2PLY_TARGET)

$(CALIB_HBT2PLY_TARGET): $(CALIB_HBT2PLY_OBJS)
	@echo "Linking calib_hbt2ply..."
	@mkdir -p $(BINDIR)
	$(CC) $(CALIB_HBT2PLY_OBJS) -o $(CALIB_HBT2PLY_TARGET) $(LIBS)
	@echo "Build complete: $(CALIB_HBT2PLY_TARGET)"

# Calibration harness (see aicalibsrc/simplemc/README.md or the file header for CLI usage)
.PHONY: calib_simplemc
calib_simplemc: $(CALIB_SIMPLEMC_TARGET)

$(CALIB_SIMPLEMC_TARGET): $(CALIB_SIMPLEMC_OBJS)
	@echo "Linking calib_simplemc..."
	@mkdir -p $(BINDIR)
	$(CC) $(CALIB_SIMPLEMC_OBJS) -o $(CALIB_SIMPLEMC_TARGET) $(LIBS)
	@echo "Build complete: $(CALIB_SIMPLEMC_TARGET)"

# Phase 3 timing-only harness for A10 IS-MCTS (see aicalibsrc/ismcts/calib_ismcts_timing.c)
.PHONY: calib_ismcts_timing
calib_ismcts_timing: $(CALIB_ISMCTS_TIMING_TARGET)

$(CALIB_ISMCTS_TIMING_TARGET): $(CALIB_ISMCTS_TIMING_OBJS)
	@echo "Linking calib_ismcts_timing..."
	@mkdir -p $(BINDIR)
	$(CC) $(CALIB_ISMCTS_TIMING_OBJS) -o $(CALIB_ISMCTS_TIMING_TARGET) $(LIBS)
	@echo "Build complete: $(CALIB_ISMCTS_TIMING_TARGET)"

# Phase 5 lean efficiency-dial sweep harness for A10 IS-MCTS
.PHONY: calib_ismcts_efficiency
calib_ismcts_efficiency: $(CALIB_ISMCTS_EFFICIENCY_TARGET)

$(CALIB_ISMCTS_EFFICIENCY_TARGET): $(CALIB_ISMCTS_EFFICIENCY_OBJS)
	@echo "Linking calib_ismcts_efficiency..."
	@mkdir -p $(BINDIR)
	$(CC) $(CALIB_ISMCTS_EFFICIENCY_OBJS) -o $(CALIB_ISMCTS_EFFICIENCY_TARGET) $(LIBS)
	@echo "Build complete: $(CALIB_ISMCTS_EFFICIENCY_TARGET)"

# Phase 6 rollout-policy diagnostic harness for A10 IS-MCTS
.PHONY: calib_ismcts_rollout_policy
calib_ismcts_rollout_policy: $(CALIB_ISMCTS_ROLLOUT_TARGET)

$(CALIB_ISMCTS_ROLLOUT_TARGET): $(CALIB_ISMCTS_ROLLOUT_OBJS)
	@echo "Linking calib_ismcts_rollout_policy..."
	@mkdir -p $(BINDIR)
	$(CC) $(CALIB_ISMCTS_ROLLOUT_OBJS) -o $(CALIB_ISMCTS_ROLLOUT_TARGET) $(LIBS)
	@echo "Build complete: $(CALIB_ISMCTS_ROLLOUT_TARGET)"

.PHONY: format
format:
	astyle --project --suffix=none --recursive --exclude=ideas "*.c,*.h"

.PHONY: test_stda_auto
test_stda_auto: $(TARGET)
	@./$(TARGET) -sa -p | diff -w -B - bin/expectedresults.txt && \
	    echo "✓ Test PASSED" || (echo "✗ Test FAILED"; exit 1)

# Help target
.PHONY: help
help:
	@echo "Oracle: The Champions of Arcadia - Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  all          - Build the project (default)"
	@echo "  clean            - Remove build artifacts"
	@echo "  debug            - Build with debug symbols and -Og"
	@echo "  test_combo       - Build and run combo bonus tests"
	@echo "  test_recall      - Build and run recall mechanic tests"
	@echo "  test_cash_exchange - Build and run cash exchange tests"
	@echo "  test_rating      - Build and run Bradley-Terry rating system tests"
	@echo "  test_moves       - Build and run move enumeration (src/actions/) tests"
	@echo "  test_ismcts      - Build and run A10 IS-MCTS node arena/UCT tree tests"
	@echo "  test_hbt2ply_reply - Build and run A9 HBT 2-Ply surrogate-hand tests"
	@echo "  test_combat      - Build and run combat.c's combo-bonus table selection tests"
	@echo "  test_stda_auto   - Diff 'oracle -sa -p' output against bin/expectedresults.txt"
	@echo "  calib_valuebased - Build the Value Based parameter calibration harness (aicalibsrc/)"
	@echo "  calib_combo_threshold - Build the Combo Threshold parameter calibration harness (aicalibsrc/)"
	@echo "  calib_borealis   - Build the Borealis parameter calibration harness (aicalibsrc/)"
	@echo "  calib_balanced   - Build the Balanced Rules parameter calibration harness (aicalibsrc/)"
	@echo "  calib_heuristic  - Build the Heuristic parameter calibration harness (aicalibsrc/)"
	@echo "  calib_tactical   - Build the Tactical parameter calibration harness (aicalibsrc/)"
	@echo "  calib_hbt        - Build the Hybrid HBT parameter calibration harness (aicalibsrc/)"
	@echo "  calib_hbt2ply    - Build the HBT 2-Ply parameter calibration harness (aicalibsrc/)"
	@echo "  calib_simplemc   - Build the Simple Monte Carlo parameter calibration harness (aicalibsrc/)"
	@echo "  calib_ismcts_timing - Build the A10 IS-MCTS per-decision timing harness (aicalibsrc/ismcts/)"
	@echo "  calib_ismcts_efficiency - Build the A10 IS-MCTS efficiency-dial sweep harness (aicalibsrc/ismcts/)"
	@echo "  format           - Format the c and h source files using astyle"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Current configuration:"
	@echo "  Source dir:  $(SRCDIR)"
	@echo "  Build dir:   $(BUILDDIR)"
	@echo "  Target:      $(TARGET)"
	@echo "  Compiler:    $(CC)"
	@echo "  Flags:       $(CFLAGS)"

# Pull in the header-dependency files -MMD -MP generated alongside every .o
# (see CFLAGS above) so editing a .h correctly triggers a rebuild of every
# .c that includes it, not just the .c files touched directly. Found via
# $(shell find ...) rather than a static list since .d files only exist
# after a .o has been compiled at least once (harmless on a clean checkout
# or right after `make clean` -- there's simply nothing to -include yet).
DEPS := $(shell find $(BUILDDIR) -type f -name '*.d' 2>/dev/null)
-include $(DEPS)
