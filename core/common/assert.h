/**
 * Debug assertions. SWITCH_ASSERT becomes a no-op in release builds.
 * SWITCH_ASSERT_ALWAYS always fires, even in release.
 */
#ifndef SWITCH_COMMON_ASSERT_H
#define SWITCH_COMMON_ASSERT_H

#include <stdio.h>
#include <stdlib.h>

#define SWITCH_ASSERT_ALWAYS(cond, msg)                                        \
  do                                                                           \
  {                                                                            \
    if (!(cond))                                                               \
    {                                                                          \
      fprintf(stderr, "ASSERT FAILED: %s\n  at %s:%d (%s)\n  condition: %s\n", \
              (msg), __FILE__, __LINE__, __func__, #cond);                     \
      abort();                                                                 \
    }                                                                          \
  } while (0)

#ifdef NDEBUG
#define SWITCH_ASSERT(cond, msg) ((void)0)
#else
#define SWITCH_ASSERT(cond, msg) SWITCH_ASSERT_ALWAYS(cond, msg)
#endif

#endif /* SWITCH_COMMON_ASSERT_H */
