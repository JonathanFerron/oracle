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
TEST_RECALL_SRCS := $(TESTSRCDIR)/test_recall.c \
                    $(SRCDIR)/core/card_actions.c \
                    $(SRCDIR)/core/game_constants.c \
                    $(SRCDIR)/core/game_context.c \
                    $(SRCDIR)/ui/cli/cli_input.c \
                    $(SRCDIR)/ui/cli/cli_io.c \
                    $(SRCDIR)/ui/cli/cli_display.c \
                    $(SRCDIR)/ui/cli/cli_action_display.c \
                    $(SRCDIR)/ui/interactive/game_commands.c \
                    $(SRCDIR)/ui/interactive/game_commands_cards.c \
                    $(SRCDIR)/structures/card_collection.c \
                    $(SRCDIR)/structures/deckstack.c \
                    $(SRCDIR)/util/mtwister.c \
                    $(SRCDIR)/util/rnd.c
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

# Calibration harness for A1 Value Based's tunable parameters (see
# aicalibsrc/value/). One subfolder per agent under aicalibsrc/ as more
# agents get calibration tooling. Links the engine directly (same pattern as
# the TEST_* targets above) plus stda_auto.c (for run_simulation()) and
# player_config.c (stda_auto.c's own dependency, for
# get_ai_strategies()/parse_ai_strategy_shorthand()).
CALIB_VALUEBASED_TARGET := $(BINDIR)/calib_valuebased
CALIB_VALUEBASED_SRCS := $(AICALIBDIR)/value/calib_valuebased.c \
                         $(SRCDIR)/core/card_actions.c \
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
                         $(SRCDIR)/ai_strat/ai_strategy.c \
                         $(SRCDIR)/ai_strat/ai_strat_random.c \
                         $(SRCDIR)/ai_strat/ai_strat_common.c \
                         $(SRCDIR)/ai_strat/ai_strat_valuebased.c \
                         $(SRCDIR)/roles/stda/stda_auto.c \
                         $(SRCDIR)/ui/shared/player_config.c
CALIB_VALUEBASED_OBJS := $(BUILDDIR)/aicalibsrc/value/calib_valuebased.o \
                         $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(filter $(SRCDIR)/%,$(CALIB_VALUEBASED_SRCS)))

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
	$(RM) -r $(BUILDDIR)/* $(BINDIR)/oracle* $(BINDIR)/test_combo $(BINDIR)/test_recall $(BINDIR)/test_cash_exchange $(BINDIR)/calib_valuebased
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

# Calibration harness (see aicalibsrc/value/README.md or the file header for CLI usage)
.PHONY: calib_valuebased
calib_valuebased: $(CALIB_VALUEBASED_TARGET)

$(CALIB_VALUEBASED_TARGET): $(CALIB_VALUEBASED_OBJS)
	@echo "Linking calib_valuebased..."
	@mkdir -p $(BINDIR)
	$(CC) $(CALIB_VALUEBASED_OBJS) -o $(CALIB_VALUEBASED_TARGET) $(LIBS)
	@echo "Build complete: $(CALIB_VALUEBASED_TARGET)"

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
	@echo "  test_stda_auto   - Diff 'oracle -sa -p' output against bin/expectedresults.txt"
	@echo "  calib_valuebased - Build the Value Based parameter calibration harness (aicalibsrc/)"
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
