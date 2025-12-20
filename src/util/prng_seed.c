/* ============================================================
   prng_seed.c - PRNG seed management implementation
   ============================================================ */

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

#include "prng_seed.h"
#include "../core/game_constants.h"

void prng_config_init(prng_config_t* config)
{ config->seed = 0;
  config->use_random = true;
}

uint32_t generate_random_seed(void)
{ uint32_t seed = 0;

  #ifdef _WIN32
  /* Windows: Use CryptGenRandom for cryptographic-quality entropy */
  HCRYPTPROV hprov = 0;
  if(CryptAcquireContext(&hprov, NULL, NULL, PROV_RSA_FULL,
                         CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
  { CryptGenRandom(hprov, sizeof(seed), (BYTE*)&seed);
    CryptReleaseContext(hprov, 0);
    return seed;
  }
  #else
  /* Unix/Linux: Try /dev/urandom first */
  int fd = open("/dev/urandom", O_RDONLY);
  if(fd >= 0)
  { if(read(fd, &seed, sizeof(seed)) == sizeof(seed))
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
{ if(!arg || !seed) return false;

  /* Check for empty string */
  if(*arg == '\0')
  { fprintf(stderr, "Warning: Empty seed value, using default %lu\n",
            M_TWISTER_SEED);
    *seed = M_TWISTER_SEED;
    return true;
  }

  /* Skip leading whitespace */
  while(*arg == ' ' || *arg == '\t') arg++;

  /* Check for negative sign (strtoul silently wraps negative values) */
  if(*arg == '-')
  { fprintf(stderr, "Warning: Negative seed '%s' not allowed, ", arg);
    fprintf(stderr, "using default %lu\n", M_TWISTER_SEED);
    *seed = M_TWISTER_SEED;
    return true;
  }

  char* endptr;
  errno = 0;

  /* Try parsing as decimal first */
  unsigned long val = strtoul(arg, &endptr, 10);

  /* If decimal failed, try hexadecimal (0x prefix) */
  if(*endptr != '\0' && strncmp(arg, "0x", 2) == 0)
  { endptr = NULL;
    val = strtoul(arg, &endptr, 16);
  }

  /* Check for parsing errors or non-numeric garbage */
  if(errno == ERANGE || (endptr && *endptr != '\0'))
  { /* Skip trailing whitespace before declaring error */
    if(endptr)
    { while(*endptr == ' ' || *endptr == '\t') endptr++;
      if(*endptr == '\0')
      { /* Valid number with trailing whitespace */
        errno = 0;
      }
    }

    if(errno == ERANGE || (endptr && *endptr != '\0'))
    { fprintf(stderr, "Warning: Invalid seed '%s', ", arg);
      fprintf(stderr, "using default %lu\n", M_TWISTER_SEED);
      *seed = M_TWISTER_SEED;
      return true;
    }
  }

  /* Check for overflow (value exceeds uint32_t) */
  if(val > MT_SEED_MAX)
  { fprintf(stderr, "Warning: Seed value %lu exceeds maximum %lu, ",
            val, MT_SEED_MAX);
    fprintf(stderr, "using default seed %lu\n", M_TWISTER_SEED);
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
