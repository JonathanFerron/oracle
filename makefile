# Makefile for Oracle: The Champions of Arcadia
# Updated to work with refactored modular structure

CC := gcc
SRCDIR := src
TESTSRCDIR := testsrc
OLDSRCDIR := oldsrc
BUILDDIR := obj
OLDBUILDDIR := oldobj
BINDIR := bin
TARGET := $(BINDIR)/oracle
SRCEXT := c
INCEXT := h
LIBS := -lm
#LIBS=-pthread


# Automatically find all .c files in src directory
SOURCES := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
OBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.o))

# Compiler flags
CFLAGS := -g -Og -Wall -std=c23

# Test targets
TEST_COMBO_TARGET := $(BINDIR)/test_combo
TEST_COMBO_SRCS := $(TESTSRCDIR)/test_combo_bonus.c \
                   $(SRCDIR)/combo_bonus.c \
                   $(SRCDIR)/game_constants.c
TEST_COMBO_OBJS := $(TESTSRCDIR)/test_combo_bonus.o \
                   $(SRCDIR)/combo_bonus.o \
                   $(SRCDIR)/game_constants.o

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

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning..."
	$(RM) -r $(BUILDDIR)/* $(BINDIR)/*
	@echo "Clean complete"

# Debug build (keeps your debug flags)
.PHONY: debug
debug: CFLAGS := -g -O0 -Wall -std=c23 -DDEBUG
debug: clean all
	@echo "Debug build complete"

# Test combo bonus calculator
.PHONY: test_combo
test_combo: $(TEST_COMBO_TARGET)

$(TEST_COMBO_TARGET): $(TEST_COMBO_OBJS)
	@echo "Linking test_combo..."
	@mkdir -p $(BINDIR)
	$(CC) $(TEST_COMBO_OBJS) -o $(TEST_COMBO_TARGET) $(LIBS)
	@echo "Test build complete: $(TEST_COMBO_TARGET)"

OLDCODE_TARGET := $(BINDIR)/oracle_old
OLDCODE_SRCS := $(shell find $(OLDSRCDIR) -type f -name *.$(SRCEXT))
OLDCODE_OBJS := $(patsubst $(OLDSRCDIR)/%,$(OLDBUILDDIR)/%,$(OLDCODE_SRCS:.$(SRCEXT)=.o))

.PHONY: oldcode
oldcode: $(OLDCODE_TARGET)

$(OLDCODE_TARGET): $(OLDCODE_OBJS)
	@echo "Linking ..."
	@mkdir -p $(BINDIR)
	$(CC) $^ -o $(OLDCODE_TARGET) $(LIBS)
	@echo "Build complete: $(OLDCODE_TARGET)"

$(OLDBUILDDIR)/%.o: $(OLDSRCDIR)/%.$(SRCEXT)
	@mkdir -p "$(@D)"
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c -o $@ $<

# Help target
.PHONY: help
help:
	@echo "Oracle: The Champions of Arcadia - Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  all          - Build the project (default)"
	@echo "  clean        - Remove build artifacts"
	@echo "  debug        - Build with debug symbols and -O0"
	@echo "  test_combo   - Build combo bonus tests"
	@echo "  help         - Show this help message"
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
