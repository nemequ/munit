#include <stdint.h>
#include <limits.h>
#include <time.h>

#include <stdatomic.h>

#include "munit.h"

/* PRNG stuff
 *
 * This is (unless I screwed up, which is entirely possible) the
 * version of PCG with 32-bit state.  It was chosen because it has a
 * small enough state that we should reliably be able to use CAS
 * instead of requiring a lock for thread-safety.
 *
 * If I did screw up, I probably will not bother changing it unless
 * there is a significant bias.  It's really not important this be
 * particularly strong, as long as it is fairly random it's much more
 * important that it be reproducible, so bug reports have a better
 * chance of being reproducible. */

static _Atomic uint32_t munit_prng_state = 42;

#define MUNIT_PRNG_MULTIPLIER UINT32_C(747796405)
#define MUNIT_PRNG_INCREMENT  1729

void
munit_rand_seed(uint32_t seed) {
  atomic_store(&munit_prng_state, seed);
}

static uint32_t
munit_rand_from_state(uint32_t state) {
  uint32_t res = ((state >> ((state >> 28) + 4)) ^ state) * UINT32_C(277803737);
  res ^= res >> 22;
  return res;
}

uint32_t
munit_rand_uint32(void) {
  uint32_t old, state;

  do {
    old = atomic_load(&munit_prng_state);
    state = old * MUNIT_PRNG_MULTIPLIER + (MUNIT_PRNG_INCREMENT | 1);
  } while (!atomic_compare_exchange_weak(&munit_prng_state, &old, state));

  return munit_rand_from_state(old);
}

uint32_t
munit_rand_make_seed(void) {
  uint8_t orig = (uint32_t) time(NULL);
  orig = orig * MUNIT_PRNG_MULTIPLIER + (MUNIT_PRNG_INCREMENT | 1);
  return munit_rand_from_state(orig);
}

void
munit_rand_memory(size_t size, uint8_t buffer[MUNIT_ARRAY_PARAM(size)]) {
  size_t members_remaining = size / sizeof(uint32_t);
  size_t bytes_remaining = size % sizeof(uint32_t);
  uint32_t* b = (uint32_t*) buffer;
  uint32_t rv;
  while (members_remaining-- > 0) {
    rv = munit_rand_uint32();
    memcpy(b++, &rv, sizeof(uint32_t));
  }
  if (bytes_remaining != 0) {
    rv = munit_rand_uint32();
    memcpy(b, &rv, sizeof(uint32_t));
  }
}

int
munit_rand_int() {
#if (INT_MAX <= INT32_MAX)
  return (int) munit_rand_uint32();
#elif (INT_MAX == INT64_MAX)
  return (((uint64_t) munit_rand_uint32 ()) << 32) | ((uint64_t) munit_rand_uint32());
#else /* Wow, okay... */
  int r;
  munit_rand_memory(sizeof(r), &r);
  return r;
#endif
}

/* Create gtint/ugtint type which are greater than int.  Probably
 * int64_t/uint64_t. */
#if INT_MAX < INT32_MAX
typedef int32_t gtint;
typedef uint32_t ugtint;
#elif INT_MAX < INT64_MAX
typedef int64_t gtint;
typedef uint64_t ugtint;
#elif defined(__GNUC__) && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 4)))
typedef __int128 gtint;
typedef unsigned __int128 ugtint;
#else
#error Unable to locate a type larger than int
#endif

int
munit_rand_int_range(int min, int max) {
  munit_assert(min < max);

  if (min == max)
    return min;
  else if (min > max)
    return munit_rand_int_range(max, min);

  /* TODO this is overflowy, biased, and generally buggy. */

  gtint range = (max - min) + 1;
  return (munit_rand_uint32() % range) + min;
}

#define MUNIT_DOUBLE_TRANSFORM (2.3283064365386962890625e-10)

double
munit_rand_double(void) {
  /* algorithm from glib, except rand_int may be negative for us.
     This is probably buggy, too;
     http://mumble.net/~campbell/tmp/random_real.c explains how to do
     it right. */
  double retval = (munit_rand_int() & (UINT_MAX / 2)) * MUNIT_DOUBLE_TRANSFORM;
  retval = (retval + (munit_rand_int () & (UINT_MAX / 2))) * MUNIT_DOUBLE_TRANSFORM;
  if (MUNIT_UNLIKELY(retval >= 1.0))
    return munit_rand_double();
  return retval;
}

/*** Test suite handling ***/

#define MUNIT_OUTPUT_FILE stdout

static void
munit_test_exec(const MunitTest* test, void* user_data, unsigned int iterations, uint32_t seed) {
  munit_rand_seed(seed);
  fprintf(MUNIT_OUTPUT_FILE, "%s: ", test->name);
  fflush(MUNIT_OUTPUT_FILE);
  unsigned int i = 0;
  do {
    test->test(user_data);
  } while (++i < iterations);

  if (i == 1)
    fprintf(MUNIT_OUTPUT_FILE, "success.\n");
  else
    fprintf(MUNIT_OUTPUT_FILE, "success (%u iterations).\n", iterations);
}

void
munit_suite_run(const MunitSuite* suite, void* user_data) {
  uint32_t seed;

  {
    char* seed_str = getenv("MUNIT_SEED");
    if (seed_str != NULL) {
      fprintf(stderr, "ENV SEED = %s\n", seed_str);
    } else {
      seed = munit_rand_make_seed();
    }
  }

  fflush(stderr);
  fprintf(MUNIT_OUTPUT_FILE, "Running test suite with seed 0x%08" PRIx32 "...\n", seed);

  size_t test_num = 0;
  for (const MunitTest* test = suite->tests ;
       test->test != NULL ;
       test++) {
    munit_test_exec(test, user_data, suite->iterations, seed);
    test_num++;
  }
}
