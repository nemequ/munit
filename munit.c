/* Copyright (c) 2013-2016 Evan Nemerson <evan@nemerson.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

/* TODO: need fallbacks (especially for Windows). */
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

/* This is the part that should be handled in the child process */
static void
munit_test_child_exec(const MunitTest* test, void* user_data, unsigned int iterations, MUNIT_UNUSED MunitSuiteOptions options, uint32_t seed) {
  if ((test->options & MUNIT_TEST_OPTION_SINGLE_ITERATION) != 0)
    iterations = 1;

  unsigned int i = 0;
  do {
    munit_rand_seed(seed);
    void* data = (test->setup == NULL) ? user_data : test->setup(user_data);

    test->test(data);

    if (test->tear_down != NULL)
      test->tear_down(data);
  } while (++i < iterations);

  if (i == 1)
    fprintf(MUNIT_OUTPUT_FILE, "success.\n");
  else
    fprintf(MUNIT_OUTPUT_FILE, "success (%u iterations).\n", iterations);
}

#if !defined(_WIN32)
#  include <unistd.h> /* for fork() */
#endif

static bool
munit_test_exec(const MunitTest* test, void* user_data, unsigned int iterations, MunitSuiteOptions options, uint32_t seed) {
  bool success;
  const bool fork_requested = (((options & MUNIT_SUITE_OPTION_NO_FORK) == 0) && ((test->options & MUNIT_TEST_OPTION_NO_FORK) == 0));
#if !defined(_WIN32)
  pid_t fork_pid;
#endif

  fprintf(MUNIT_OUTPUT_FILE, "%s: ", test->name);
  fflush(MUNIT_OUTPUT_FILE);

  if (fork_requested) {
#if !defined(_WIN32)
    int pipefd[2];
    if (pipe(pipefd) != 0) {
      fprintf(MUNIT_OUTPUT_FILE, "unable to create pipe: %s\n", strerror(errno));
      return false;
    }

    fork_pid = fork ();
    if (fork_pid == 0) {
      close(pipefd[0]);
      munit_test_child_exec(test, user_data, iterations, options, seed);
      success = true;
      write(pipefd[1], &success, sizeof(bool));
      exit(EXIT_SUCCESS);
    } else if (fork_pid == -1) {
      fprintf(MUNIT_OUTPUT_FILE, "unable to fork()\n");
      close(pipefd[0]);
      close(pipefd[1]);
      success = false;
    } else {
      close(pipefd[1]);
      size_t bytes_read = read(pipefd[0], &success, sizeof(bool));
      if (bytes_read != sizeof(bool) || !success) {
	success = false;
	fputc('\n', MUNIT_OUTPUT_FILE);
      }
    }
#else
    /* We don't (yet?) support forking on Windows */
    munit_test_child_exec(test, user_data, iterations, options, seed);
#endif
  } else {
    munit_test_child_exec(test, user_data, iterations, options, seed);
  }

  return success;
}

bool
munit_suite_run(const MunitSuite* suite, void* user_data) {
  uint32_t seed;

  {
    char* seed_str = getenv("MUNIT_SEED");
    if (seed_str != NULL) {
      char* envptr = NULL;
      unsigned long long ts = strtoull(seed_str, &envptr, 0);
      if (*envptr != '\0' || ts > UINT32_MAX) {
	fprintf(stderr, "Invalid seed specified.\n");
	return false;
      }
      seed = (uint32_t) ts;
    } else {
      seed = munit_rand_make_seed();
    }
  }

  fflush(stderr);
  fprintf(MUNIT_OUTPUT_FILE, "Running test suite with seed 0x%08" PRIx32 "...\n", seed);

  unsigned int test_num = 0;
  unsigned int successful = 0;
  for (const MunitTest* test = suite->tests ;
       test->test != NULL ;
       test++) {
    successful += munit_test_exec(test, user_data, suite->iterations, suite->options, seed) ? 1 : 0;
    test_num++;
  }

  fprintf(MUNIT_OUTPUT_FILE, "%d of %d (%g%%) tests successful.\n",
	  successful, test_num, (((double) successful) / ((double) test_num)) * 100);

  return successful == test_num;
}
