CC := gcc
SRCDIR := src
BUILDDIR := obj
TARGET := bin/oracle
SRCEXT := c
INCEXT := h
#LIBS=-pthread

SOURCES := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
OBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.o))
CFLAGS := -g -Og -Wall
#-O0

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo "Linking..."
	$(CC) $^ -o $(TARGET) $(LIBS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.$(SRCEXT) # $(SRCDIR)/%.$(INCEXT)	
	@mkdir -p "$(@D)"
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	@echo "Cleaning..."
	$(RM) -r $(BUILDDIR)/* $(TARGET)

