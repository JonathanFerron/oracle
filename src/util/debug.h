// debug.h
// Compile-time debug configuration

#ifndef DEBUG_H
#define DEBUG_H

// Set to 1 to enable debug output, 0 to disable
// Can be overridden with: gcc -DDEBUG_ENABLED=1
#ifndef DEBUG_ENABLED
  #define DEBUG_ENABLED 0
#endif

// Debug macros for conditional compilation
#if DEBUG_ENABLED
  #define DEBUG_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
  #define DEBUG_ONLY(code) code
#else
  #define DEBUG_PRINT(fmt, ...) ((void)0)
  #define DEBUG_ONLY(code) ((void)0)
#endif

#endif // DEBUG_H
