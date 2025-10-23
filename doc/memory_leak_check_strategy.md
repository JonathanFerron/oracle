# Valgrind Memory Leak Detection Guide for Oracle Project

## 1. Valgrind - The Primary Tool

### Basic Commands

```bash
# Basic leak check
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./bin/oracle

# More detailed output with log file
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --log-file=valgrind-out.txt \
         ./bin/oracle

# For better stack traces, compile with debug symbols
gcc -g -O0 -Wall -std=c23 src/*.c -o bin/oracle -lm
```

### Understanding Valgrind Output

**Key leak categories to monitor:**
- **Definitely lost**: Direct memory leaks (critical - must fix)
- **Indirectly lost**: Memory lost due to losing pointer to container (critical - must fix)
- **Possibly lost**: Suspicious but may be intentional (investigate)
- **Still reachable**: Memory still pointed to at exit (usually acceptable)

## 2. Manual Code Audit Checklist - Oracle Specific

### High Priority Areas in Your Code:

#### A. HDCLL_toArray()
Location: `src/hdcll.c:145`, `src/hdcll.h:16`

**Every call to `HDCLL_toArray()` MUST be followed by `free()`**

#### B. Strategy Set Memory
Location: `src/strategy.c`

- `create_strategy_set()` allocates memory
- `free_strategy_set()` must be called to free memory

### Memory Allocation Patterns to Check:
- [x] `malloc()` in `HDCLL_insertNodeAtBeginning()` - freed by `HDCLL_emptyOut()`

### Common Leak Scenarios to Check:
- [x] Error paths - `HDCLL_insertNodeAtBeginning()` returns on malloc failure

## 3. Debug Wrapper Functions for Tracking

### debug_mem.h
```c
#ifndef DEBUG_MEM_H
#define DEBUG_MEM_H

#include <stddef.h>

#ifdef DEBUG_MEMORY
    #define malloc(s) debug_malloc(s, __FILE__, __LINE__)
    #define calloc(n,s) debug_calloc(n, s, __FILE__, __LINE__)
    #define free(p) debug_free(p, __FILE__, __LINE__)
    #define realloc(p,s) debug_realloc(p, s, __FILE__, __LINE__)
#endif

void* debug_malloc(size_t size, const char* file, int line);
void* debug_calloc(size_t n, size_t size, const char* file, int line);
void debug_free(void* ptr, const char* file, int line);
void* debug_realloc(void* ptr, size_t size, const char* file, int line);
void debug_report_leaks(void);

#endif
```

### debug_mem.c
```c
#include "debug_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct alloc_info {
  void* ptr;
  size_t size;
  const char* file;
  int line;
  struct alloc_info* next;
} alloc_info;

static alloc_info* alloc_list = NULL;
static size_t total_allocated = 0;
static size_t total_freed = 0;

static void add_alloc(void* p, size_t sz, const char* f, int ln) {
  alloc_info* info = malloc(sizeof(alloc_info));
  info->ptr = p;
  info->size = sz;
  info->file = f;
  info->line = ln;
  info->next = alloc_list;
  alloc_list = info;
  total_allocated += sz;
}

static void remove_alloc(void* p) {
  alloc_info** curr = &alloc_list;
  while (*curr) {
    if ((*curr)->ptr == p) {
      alloc_info* tmp = *curr;
      total_freed += tmp->size;
      *curr = tmp->next;
      free(tmp);
      return;
    }
    curr = &(*curr)->next;
  }
}

void* debug_malloc(size_t sz, const char* f, int ln) {
  void* p = malloc(sz);
  if (p) add_alloc(p, sz, f, ln);
  printf("[MALLOC] %zu bytes at %s:%d\n", sz, f, ln);
  return p;
}

void* debug_calloc(size_t n, size_t sz, const char* f, int ln) {
  void* p = calloc(n, sz);
  if (p) add_alloc(p, n * sz, f, ln);
  printf("[CALLOC] %zu bytes at %s:%d\n", n * sz, f, ln);
  return p;
}

void debug_free(void* p, const char* f, int ln) {
  if (p) {
    remove_alloc(p);
    printf("[FREE] %p at %s:%d\n", p, f, ln);
    free(p);
  }
}

void* debug_realloc(void* p, size_t sz, const char* f, int ln) {
  if (p) remove_alloc(p);
  void* new_p = realloc(p, sz);
  if (new_p) add_alloc(new_p, sz, f, ln);
  printf("[REALLOC] %p -> %p (%zu bytes) at %s:%d\n", 
         p, new_p, sz, f, ln);
  return new_p;
}

void debug_report_leaks(void) {
  printf("\n=== Memory Leak Report ===\n");
  printf("Total allocated: %zu bytes\n", total_allocated);
  printf("Total freed: %zu bytes\n", total_freed);
  printf("Leaked: %zu bytes\n", total_allocated - total_freed);
  
  if (alloc_list) {
    printf("\nUnfreed allocations:\n");
    for (alloc_info* curr = alloc_list; curr; curr = curr->next) {
      printf("  %zu bytes at %s:%d (ptr: %p)\n",
             curr->size, curr->file, curr->line, curr->ptr);
    }
  } else {
    printf("No memory leaks detected!\n");
  }
  printf("==========================\n\n");
}
```

### To use debug memory tracking:

1. Add debug_mem.c and debug_mem.h to your src/ directory
2. In main.c, add before `return EXIT_SUCCESS;`:
```c
#ifdef DEBUG_MEMORY
  debug_report_leaks();
#endif
```
3. Compile with: `make oracle-memtrack`

## 4. Makefile Integration

Add these targets to your existing Makefile (after line 103):

```makefile
# Memory leak detection targets
.PHONY: memcheck memcheck-full oracle-memtrack

# Debug build with symbols for Valgrind
oracle-debug: CFLAGS := -g -O0 -Wall -std=c23
oracle-debug: clean all
	@echo "Debug build complete for Valgrind"

# Debug build with memory tracking
oracle-memtrack: CFLAGS := -DDEBUG_MEMORY -g -O0 -Wall -std=c23
oracle-memtrack: SOURCES := $(SOURCES) src/debug_mem.c
oracle-memtrack: clean all
	@echo "Memory tracking build complete"

# Basic Valgrind check
memcheck: oracle-debug
	@echo "Running Valgrind basic check..."
	valgrind --leak-check=full \
	         --show-leak-kinds=all \
	         --track-origins=yes \
	         $(TARGET)

# Detailed Valgrind check with log file
memcheck-full: oracle-debug
	@echo "Running Valgrind detailed check..."
	valgrind --leak-check=full \
	         --show-leak-kinds=all \
	         --track-origins=yes \
	         --verbose \
	         --log-file=valgrind-report.txt \
	         $(TARGET)
	@echo "\n=== Valgrind Report Summary ==="
	@grep "LEAK SUMMARY" valgrind-report.txt || true
	@grep "definitely lost" valgrind-report.txt || echo "✓ No definite leaks found!"
	@grep "indirectly lost" valgrind-report.txt || echo "✓ No indirect leaks found!"
	@echo "\nFull report saved to: valgrind-report.txt"

# Run with debug memory wrapper
memtrack: oracle-memtrack
	$(TARGET)
```

### Using the Makefile

```bash
# Quick leak check
make memcheck

# Detailed leak check with report
make memcheck-full

# Run with debug memory wrapper
make memtrack

# Clean up
make clean
```

## 5. Quick Start Command

```bash
# Step 1: Clean and build with debug symbols
make clean
make oracle-debug

# Step 2: Run comprehensive Valgrind check
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         ./bin/oracle 2>&1 | tee leak-report.txt

# Step 3: Review results
grep -A 5 "LEAK SUMMARY" leak-report.txt

# Step 4: Check for critical leaks
echo "=== Checking for definite leaks ==="
grep "definitely lost" leak-report.txt
echo "=== Checking for indirect leaks ==="
grep "indirectly lost" leak-report.txt
```

## 6. Workflow Summary

1. **Build with debug symbols**: `make oracle-debug`
2. **Run Valgrind**: `make memcheck-full`
3. **Review report**: Look for "definitely lost" and "indirectly lost"
4. **Identify source**: Use stack trace to find allocation site
5. **Fix leak**: Add missing `free()` calls or fix cleanup logic
6. **Verify fix**: Run Valgrind again
7. **Optional**: Use `make memtrack` during development for real-time feedback
