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
CFLAGS := -g -Og -Wall -std=c23

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
              $(SRCDIR)/ai_strat/ai_strat_playout.c \
              $(SRCDIR)/ai_strat/ai_strat_simplemc_search.c \
              $(SRCDIR)/ai_strat/ai_strat_simplemc1.c

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
	$(RM) -r $(BUILDDIR)/* $(BINDIR)/oracle* $(BINDIR)/test_combo $(BINDIR)/test_recall $(BINDIR)/test_cash_exchange $(BINDIR)/test_rating $(BINDIR)/test_moves $(BINDIR)/calib_valuebased $(BINDIR)/calib_combo_threshold $(BINDIR)/calib_borealis $(BINDIR)/calib_balanced $(BINDIR)/calib_heuristic $(BINDIR)/calib_tactical $(BINDIR)/calib_hbt $(BINDIR)/calib_simplemc
	$(RM) $(SRCDIR)/*.o $(SRCDIR)/*/*.o $(SRCDIR)/*/*/*.o $(SRCDIR)/*/*/*/*.o $(TESTSRCDIR)/*.o $(AICALIBDIR)/*.o
	@echo "Clean complete"

# Debug build
.PHONY: debug
debug: CFLAGS := -g -Og -Wall -std=c23 -DDEBUG -DDEBUG_ENABLED=1
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

# Test move enumeration (src/actions/move_gen.c)
.PHONY: test_moves
test_moves: $(TEST_MOVES_TARGET)
	./$(TEST_MOVES_TARGET)

$(TEST_MOVES_TARGET): $(TEST_MOVES_OBJS)
	@echo "Linking test_moves..."
	@mkdir -p $(BINDIR)
	$(CC) $(TEST_MOVES_OBJS) -o $(TEST_MOVES_TARGET) $(LIBS)
	@echo "Test build complete: $(TEST_MOVES_TARGET)"

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

# Calibration harness (see aicalibsrc/simplemc/README.md or the file header for CLI usage)
.PHONY: calib_simplemc
calib_simplemc: $(CALIB_SIMPLEMC_TARGET)

$(CALIB_SIMPLEMC_TARGET): $(CALIB_SIMPLEMC_OBJS)
	@echo "Linking calib_simplemc..."
	@mkdir -p $(BINDIR)
	$(CC) $(CALIB_SIMPLEMC_OBJS) -o $(CALIB_SIMPLEMC_TARGET) $(LIBS)
	@echo "Build complete: $(CALIB_SIMPLEMC_TARGET)"

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
	@echo "  test_stda_auto   - Diff 'oracle -sa -p' output against bin/expectedresults.txt"
	@echo "  calib_valuebased - Build the Value Based parameter calibration harness (aicalibsrc/)"
	@echo "  calib_combo_threshold - Build the Combo Threshold parameter calibration harness (aicalibsrc/)"
	@echo "  calib_borealis   - Build the Borealis parameter calibration harness (aicalibsrc/)"
	@echo "  calib_balanced   - Build the Balanced Rules parameter calibration harness (aicalibsrc/)"
	@echo "  calib_heuristic  - Build the Heuristic parameter calibration harness (aicalibsrc/)"
	@echo "  calib_tactical   - Build the Tactical parameter calibration harness (aicalibsrc/)"
	@echo "  calib_hbt        - Build the Hybrid HBT parameter calibration harness (aicalibsrc/)"
	@echo "  calib_simplemc   - Build the Simple Monte Carlo parameter calibration harness (aicalibsrc/)"
	@echo "  format           - Format the c and h source files using astyle"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Current configuration:"
	@echo "  Source dir:  $(SRCDIR)"
	@echo "  Build dir:   $(BUILDDIR)"
	@echo "  Target:      $(TARGET)"
	@echo "  Compiler:    $(CC)"
	@echo "  Flags:       $(CFLAGS)"
	
# Future Test Additions:
# To add more tests, simply follow this pattern:
# Add after TEST_COMBO_* definitions
# TEST_COMBAT_TARGET := $(BINDIR)/test_combat
# TEST_COMBAT_SRCS := $(SRCDIR)/test_combat.c \
#                     $(SRCDIR)/combat.c \
#                     $(SRCDIR)/combo_bonus.c \
#                     $(SRCDIR)/game_constants.c
# TEST_COMBAT_OBJS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(TEST_COMBAT_SRCS:.$(SRCEXT)=.o))

# Add build rule
# $(TEST_COMBAT_TARGET): $(TEST_COMBAT_OBJS)
# 	@echo "Linking test_combat..."
# 	@mkdir -p $(BINDIR)
# 	$(CC) $(TEST_COMBAT_OBJS) -o $(TEST_COMBAT_TARGET) $(LIBS)

# Add test target
# .PHONY: test_combat
# test_combat: $(TEST_COMBAT_TARGET)
