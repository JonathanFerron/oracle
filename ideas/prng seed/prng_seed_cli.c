/* prng_seed.h - PRNG seed management header */
#ifndef PRNG_SEED_H
#define PRNG_SEED_H

#include <stdint.h>
#include <stdbool.h>

#define M_TWISTER_SEED 1337
#define MT_SEED_MAX 0xFFFFFFFFUL  /* Maximum valid seed for MT19937 */

/* PRNG seed configuration structure */
typedef struct {
    uint32_t seed;
    bool use_random;
} prng_config_t;

/* Initialize PRNG configuration with defaults */
void prng_config_init(prng_config_t* config);

/* Generate a random seed using system entropy */
uint32_t generate_random_seed(void);

/* Parse seed from command line argument, returns true on success */
bool parse_seed_arg(const char* arg, uint32_t* seed);

/* Validate and clamp seed to legal values */
uint32_t validate_seed(uint32_t seed);

#endif /* PRNG_SEED_H */

/* prng_seed.c - PRNG seed management implementation */
#include "prng_seed.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

void prng_config_init(prng_config_t* config)
{ config->seed = 0;
  config->use_random = true;
}

uint32_t generate_random_seed(void)
{ uint32_t seed = 0;
  
#ifdef _WIN32
  /* Windows: Use CryptGenRandom for cryptographic-quality entropy */
  HCRYPTPROV hprov = 0;
  if (CryptAcquireContext(&hprov, NULL, NULL, PROV_RSA_FULL,
                          CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
  { CryptGenRandom(hprov, sizeof(seed), (BYTE*)&seed);
    CryptReleaseContext(hprov, 0);
    return seed;
  }
#else
  /* Unix/Linux: Try /dev/urandom first */
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd >= 0)
  { if (read(fd, &seed, sizeof(seed)) == sizeof(seed))
    { close(fd);
      return seed;
    }
    close(fd);
  }
#endif
  
  /* Fallback: Use time-based seed (less secure but portable) */
  seed = (uint32_t)time(NULL);
  seed ^= (uint32_t)(clock() << 16);
  
  /* Mix in some additional entropy from pointer addresses */
  void* stack_addr = &seed;
  seed ^= (uint32_t)(uintptr_t)stack_addr;
  
  return seed;
}

bool parse_seed_arg(const char* arg, uint32_t* seed)
{ if (!arg || !seed) return false;
  
  /* Check for empty string */
  if (*arg == '\0')
  { fprintf(stderr, "Warning: Empty seed value, using default %u\n",
            M_TWISTER_SEED);
    *seed = M_TWISTER_SEED;
    return true;
  }
  
  /* Skip leading whitespace */
  while (*arg == ' ' || *arg == '\t') arg++;
  
  /* Check for negative sign (strtoul silently wraps negative values) */
  if (*arg == '-')
  { fprintf(stderr, "Warning: Negative seed '%s' not allowed, ", arg);
    fprintf(stderr, "using default %u\n", M_TWISTER_SEED);
    *seed = M_TWISTER_SEED;
    return true;
  }
  
  char* endptr;
  errno = 0;
  
  /* Try parsing as decimal first */
  unsigned long val = strtoul(arg, &endptr, 10);
  
  /* If decimal failed, try hexadecimal (0x prefix) */
  if (*endptr != '\0' && strncmp(arg, "0x", 2) == 0)
  { endptr = NULL;
    val = strtoul(arg, &endptr, 16);
  }
  
  /* Check for parsing errors or non-numeric garbage */
  if (errno == ERANGE || (endptr && *endptr != '\0'))
  { /* Skip trailing whitespace before declaring error */
    if (endptr)
    { while (*endptr == ' ' || *endptr == '\t') endptr++;
      if (*endptr == '\0')
      { /* Valid number with trailing whitespace */
        errno = 0;
      }
    }
    
    if (errno == ERANGE || (endptr && *endptr != '\0'))
    { fprintf(stderr, "Warning: Invalid seed '%s', ", arg);
      fprintf(stderr, "using default %u\n", M_TWISTER_SEED);
      *seed = M_TWISTER_SEED;
      return true;
    }
  }
  
  /* Check for overflow (value exceeds uint32_t) */
  if (val > MT_SEED_MAX)
  { fprintf(stderr, "Warning: Seed value %lu exceeds maximum %u, ",
            val, MT_SEED_MAX);
    fprintf(stderr, "using default seed %u\n", M_TWISTER_SEED);
    *seed = M_TWISTER_SEED;
    return true;
  }
  
  *seed = (uint32_t)val;
  return true;
}

uint32_t validate_seed(uint32_t seed)
{ /* MT19937 accepts all uint32_t values, so just return as-is */
  /* This function exists for future extensions or different PRNGs */
  return seed;
}

/* Command line parsing additions for main.c */

/* Add to your existing parse_args function or similar: */

/*
Example integration in main.c:

#include "prng_seed.h"

static prng_config_t prng_config;

void parse_args(int argc, char** argv)
{ int i;
  prng_config_init(&prng_config);
  
  for (i = 1; i < argc; i++)
  { if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "-pr") == 0 ||
        strcmp(argv[i], "--prng.seed") == 0)
    { // Check if there's a next argument
      if (i + 1 < argc && argv[i + 1][0] != '-')
      { // Seed value provided
        if (parse_seed_arg(argv[i + 1], &prng_config.seed))
        { prng_config.use_random = false;
          i++; // Skip next argument
        }
        else
        { fprintf(stderr, "Warning: Invalid seed '%s', using default %u\n",
                  argv[i + 1], M_TWISTER_SEED);
          prng_config.seed = M_TWISTER_SEED;
          prng_config.use_random = false;
          i++;
        }
      }
      else
      { // No seed provided, use default
        prng_config.seed = M_TWISTER_SEED;
        prng_config.use_random = false;
      }
    }
    // ... other argument parsing ...
  }
  
  // Initialize PRNG with chosen seed
  if (prng_config.use_random)
  { prng_config.seed = generate_random_seed();
    if (verbose)
    { printf("Using random seed: %u\n", prng_config.seed);
    }
  }
  else
  { if (verbose)
    { printf("Using specified seed: %u\n", prng_config.seed);
    }
  }
  
  // Initialize your MT PRNG here
  mt_init(validate_seed(prng_config.seed));
}
*/

void print_usage(const char* prog)
{ printf("Usage: %s [OPTIONS]\n\n", prog);
  printf("Options:\n");
  printf("  -h,  -he, --help              Show this help message\n");
  printf("  -v,  -vb, --verbose           Enable verbose output\n");
  printf("  -V,  -vr, --version           Show version information\n");
  printf("  -n,  -ns, --numsim N          Set number of simulations to N\n");
  printf("  -i,  -in, --input FILE        Use FILE as input configuration\n");
  printf("  -o,  -ou, --output FILE       Output to FILE instead of stdout\n");
  printf("  -p,  -pr, --prng.seed [SEED]  Set PRNG seed (default: %u)\n",
         M_TWISTER_SEED);
  printf("                                If SEED omitted, uses default\n");
  printf("                                If option omitted, uses random seed\n\n");
  printf("Game Modes:\n");
  printf("  -a,  -sa, --stda.auto         Standalone automated mode\n");
  printf("  -s,  -ss, --stda.sim          Standalone simulation mode (ncurses)\n");
  printf("  -l,  -sl, --stda.cli          Standalone command line interface\n");
  printf("  -t,  -st, --stda.tui          Standalone text UI (ncurses)\n");
  printf("  -g,  -sg, --stda.gui          Standalone graphical UI\n");
  printf("  -S,  -sv, --server            Server mode\n");
  printf("  -C,  -cs, --client.sim        Client simulation mode (ncurses)\n");
  printf("  -L,  -cl, --client.cli        Client command line interface\n");
  printf("  -T,  -ct, --client.tui        Client text UI mode\n");
  printf("  -G,  -cg, --client.gui        Client graphical UI mode\n");
  printf("  -A,  -ai, --ai AGENT          AI agent client mode\n");
}
