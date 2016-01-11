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

/*** Configuration ***/

/* This is just where the output from the test goes.  It's really just
 * meant to let you choose stdout or stderr, but if anyone really want
 * to direct it to a file let me know, it would be fairly easy to
 * support. */
#if !defined(MUNIT_OUTPUT_FILE)
#  define MUNIT_OUTPUT_FILE stdout
#endif

/* This is a bit more useful; it tells µnit how to format the seconds in
 * timed tests.  If your tests run for longer you might want to reduce
 * it, and if your computer is really fast and your tests are tiny you
 * can increase it. */
#if !defined(MUNIT_TEST_TIME_FORMAT)
#  define MUNIT_TEST_TIME_FORMAT "0.8f"
#endif

/*** End configuration ***/

#define _POSIX_C_SOURCE 200112L

#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#if !defined(_WIN32)
#  include <unistd.h>
#else
#  include <windows.h>
#endif

#include "munit.h"

/*** PRNG stuff ***/

/* This is (unless I screwed up, which is entirely possible) the
 * version of PCG with 32-bit state.  It was chosen because it has a
 * small enough state that we should reliably be able to use CAS
 * instead of requiring a lock for thread-safety.
 *
 * If I did screw up, I probably will not bother changing it unless
 * there is a significant bias.  It's really not important this be
 * particularly strong, as long as it is fairly random it's much more
 * important that it be reproducible, so bug reports have a better
 * chance of being reproducible. */

#if defined(__STDC_VERSION__) && (__STDC__VERSION__ >= 201112L) && !defined(__STDC_NO_ATOMICS__)
#  define HAVE_STDATOMIC
#error huh
#elif defined(__clang__)
#  if __has_extension(c_atomic)
#    define HAVE_CLANG_ATOMICS
#  endif
#endif

#if defined(HAVE_STDATOMIC)
#  define ATOMIC_UINT32_T _Atomic uint32_t
#  define ATOMIC_UINT32_INIT(x) ATOMIC_VAR_INIT(x)
#elif defined(HAVE_CLANG_ATOMICS)
#  define ATOMIC_UINT32_T _Atomic uint32_t
#  define ATOMIC_UINT32_INIT(x) (x)
#elif defined(_WIN32)
#  define ATOMIC_UINT32_T volatile LONG
#  define ATOMIC_UINT32_INIT(x) (x)
#else
#  define ATOMIC_UINT32_T volatile uint32_t
#  define ATOMIC_UINT32_INIT(x) (x)
#endif

static ATOMIC_UINT32_T munit_prng_state = ATOMIC_UINT32_INIT(42);

#if HAVE_STDATOMIC
#  include <stdatomic.h>
#  define munit_atomic_store(dest, value)         atomic_store(dest, value)
#  define munit_atomic_load(src)                  atomic_load(src)
#  define munit_atomic_cas(dest, expected, value) atomic_compare_exchange_weak(dest, expected, value)
#elif defined(HAVE_CLANG_ATOMICS)
#  define munit_atomic_store(dest, value)         __c11_atomic_store(dest, value, __ATOMIC_SEQ_CST)
#  define munit_atomic_load(src)                  __c11_atomic_load(src, __ATOMIC_SEQ_CST)
#  define munit_atomic_cas(dest, expected, value) __c11_atomic_compare_exchange_weak(dest, expected, value, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#elif defined(__GNUC__) && (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)
#  define munit_atomic_store(dest, value)         __atomic_store_n(dest, value, __ATOMIC_SEQ_CST)
#  define munit_atomic_load(src)                  __atomic_load_n(src, __ATOMIC_SEQ_CST)
#  define munit_atomic_cas(dest, expected, value) __atomic_compare_exchange_n(dest, expected, value, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#elif defined(__GNUC__) && (__GNUC__ >= 4)
#  define munit_atomic_store(dest,value)          do { *dest = value; } while (0)
#  define munit_atomic_load(src)                  (*src)
#  define munit_atomic_cas(dest, expected, value) __sync_bool_compare_and_swap(dest, *expected, value)
#elif defined(_WIN32) /* Untested */
#  define munit_atomic_store(dest,value)          do { *dest = value; } while (0)
#  define munit_atomic_load(src)                  (*src)
#  define munit_atomic_cas(dest, expected, value) InterlockedCompareExchange(dest, value, expected)
#else
#  error No atomic implementation
#endif

#define MUNIT_PRNG_MULTIPLIER UINT32_C(747796405)
#define MUNIT_PRNG_INCREMENT  1729

void
munit_rand_seed(uint32_t seed) {
  munit_atomic_store(&munit_prng_state, seed);
}

static uint32_t
munit_rand_from_state(uint32_t state) {
  uint32_t res = ((state >> ((state >> 28) + 4)) ^ state) * UINT32_C(277803737);
  res ^= res >> 22;
  return res;
}

uint32_t
munit_rand_uint32(void) {
  uint32_t old;
  uint32_t state;

  do {
    old = munit_atomic_load(&munit_prng_state);
    state = old * MUNIT_PRNG_MULTIPLIER + (MUNIT_PRNG_INCREMENT | 1);
  } while (!munit_atomic_cas(&munit_prng_state, &old, state));

  return munit_rand_from_state(old);
}

uint32_t
munit_rand_make_seed(void) {
  uint32_t orig = (uint32_t) time(NULL);
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

/*** Timer code ***/

/* This section is definitely a bit messy, patches to clean it up
 * gratefully accepted. */

#define MUNIT_TIMER_METHOD_CLOCK_GETTIME 0
#define MUNIT_TIMER_METHOD_GETRUSAGE 1
#define MUNIT_TIMER_METHOD_GETPROCESSTIMES 2

#ifdef _WIN32
#  define MUNIT_TIMER_METHOD MUNIT_TIMER_METHOD_GETPROCESSTIMES
#  include <windows.h>
#  if _WIN32_WINNT >= 0x0600
     typedef DWORD MunitWallClock;
#  else
     typedef ULONGLONG MunitWallClock;
#  endif
     typedef FILETIME MunitCpuClock;
#elif defined(_POSIX_CPUTIME)
#  define MUNIT_TIMER_CPUTIME CLOCK_PROCESS_CPUTIME_ID
#  define MUNIT_TIMER_METHOD MUNIT_TIMER_METHOD_CLOCK_GETTIME
#  include <time.h>
   typedef struct timespec MunitWallClock;
   typedef struct timespec MunitCpuClock;
#elif defined(CLOCK_VIRTUAL)
#  define MUNIT_TIMER_CPUTIME CLOCK_VIRTUAL
#  define MUNIT_TIMER_METHOD MUNIT_TIMER_METHOD_CLOCK_GETTIME
#  include <time.h>
   typedef struct timespec MunitWallClock;
   typedef struct timespec MunitCpuClock;
#else
#  define MUNIT_TIMER_METHOD MUNIT_TIMER_METHOD_GETRUSAGE
#  include <sys/time.h>
#  include <sys/resource.h>
   typedef struct timeval MunitWallClock;
   typedef struct rusage MunitCpuClock;
#endif

static void
munit_wall_clock_get_time(MunitWallClock* clock) {
#if MUNIT_TIMER_METHOD == MUNIT_TIMER_METHOD_CLOCK_GETTIME
  if (clock_gettime (CLOCK_REALTIME, clock) != 0) {
    fputs ("Unable to get wall clock time\n", stderr);
    exit (-1);
  }
#elif MUNIT_TIMER_METHOD == MUNIT_TIMER_METHOD_GETRUSAGE
  if (gettimeofday(clock, NULL) != 0) {
    fputs ("Unable to get time\n", stderr);
  }
#elif MUNIT_TIMER_METHOD == MUNIT_TIMER_METHOD_GETPROCESSTIMES
  #if _WIN32_WINNT >= 0x0600
    *clock = GetTickCount64 ();
  #else
    *clock = GetTickCount ();
  #endif
#endif
}

static void
munit_cpu_clock_get_time(MunitCpuClock* clock) {
#if MUNIT_TIMER_METHOD == MUNIT_TIMER_METHOD_CLOCK_GETTIME
  if (clock_gettime (MUNIT_TIMER_CPUTIME, clock) != 0) {
    fputs ("Unable to get CPU clock time\n", stderr);
    exit (-1);
  }
#elif MUNIT_TIMER_METHOD == MUNIT_TIMER_METHOD_GETRUSAGE
  if (getrusage(RUSAGE_SELF, clock) != 0) {
    fputs ("Unable to get CPU clock time\n", stderr);
    exit (-1);
  }
#elif MUNIT_TIMER_METHOD == MUNIT_TIMER_METHOD_GETPROCESSTIMES
  FILETIME CreationTime, ExitTime, KernelTime;
  if (!GetProcessTimes (GetCurrentProcess(), &CreationTime, &ExitTime, &KernelTime, clock)) {
    fputs ("Unable to get CPU clock time\n", stderr);
  }
#endif
}

static double
munit_wall_clock_get_elapsed(MunitWallClock* start, MunitWallClock* end) {
#if MUNIT_TIMER_METHOD == MUNIT_TIMER_METHOD_CLOCK_GETTIME
  return
    (double) (end->tv_sec - start->tv_sec) +
    (((double) (end->tv_nsec - start->tv_nsec)) / 1000000000);
#elif MUNIT_TIMER_METHOD == MUNIT_TIMER_METHOD_GETRUSAGE
  return
    (double) (end->tv_sec - start->tv_sec) +
    (((double) (end->tv_usec - start->tv_usec)) / 1000000);
#elif MUNIT_TIMER_METHOD == MUNIT_TIMER_METHOD_GETPROCESSTIMES
#  if _WIN32_WINNT >= 0x0600
  return
    (((double) *end) - ((double) *start)) * 1000;
#  else
  if (MUNIT_LIKELY(*end > *start))
    return (((double) *end) - ((double) *start)) / 1000;
  else
    return (((double) *start) - ((double) *end)) / 1000;
#  endif
#endif
}

static double
munit_cpu_clock_get_elapsed(MunitCpuClock* start, MunitCpuClock* end) {
#if MUNIT_TIMER_METHOD == MUNIT_TIMER_METHOD_CLOCK_GETTIME
  return
    (double) (end->tv_sec - start->tv_sec) +
    (((double) (end->tv_nsec - start->tv_nsec)) / 1000000000);
#elif MUNIT_TIMER_METHOD == MUNIT_TIMER_METHOD_GETRUSAGE
  return
    (double) ((end->ru_utime.tv_sec + end->ru_stime.tv_sec) - (start->ru_utime.tv_sec + start->ru_stime.tv_sec)) +
    (((double) ((end->ru_utime.tv_usec + end->ru_stime.tv_usec) - (start->ru_utime.tv_usec + start->ru_stime.tv_usec))) / 1000000);
#elif MUNIT_TIMER_METHOD == MUNIT_TIMER_METHOD_GETPROCESSTIMES
  ULONGLONG start_cpu, end_cpu;

  start_cpu   = start->dwHighDateTime;
  start_cpu <<= sizeof (DWORD) * 8;
  start_cpu  |= start->dwLowDateTime;

  end_cpu   = end->dwHighDateTime;
  end_cpu <<= sizeof (DWORD) * 8;
  end_cpu  |= end->dwLowDateTime;

  return ((double) (end_cpu - start_cpu)) / 10000000;
#endif
}

/*** Test suite handling ***/

/* This is the part that should be handled in the child process */
static void
munit_test_child_exec(const MunitTest* test, void* user_data, unsigned int iterations, MUNIT_UNUSED MunitSuiteOptions options, uint32_t seed) {
  if ((test->options & MUNIT_TEST_OPTION_SINGLE_ITERATION) != 0)
    iterations = 1;

  MunitWallClock wall_clock_begin, wall_clock_end;
  MunitCpuClock cpu_clock_begin, cpu_clock_end;
  double elapsed_wall = 0.0;
  double elapsed_cpu = 0.0;
  unsigned int i = 0;
  do {
    munit_rand_seed(seed);
    void* data = (test->setup == NULL) ? user_data : test->setup(user_data);

    munit_wall_clock_get_time(&wall_clock_begin);
    munit_cpu_clock_get_time(&cpu_clock_begin);

    test->test(data);

    munit_wall_clock_get_time(&wall_clock_end);
    munit_cpu_clock_get_time(&cpu_clock_end);

    elapsed_wall += munit_wall_clock_get_elapsed(&wall_clock_begin, &wall_clock_end);
    elapsed_cpu += munit_cpu_clock_get_elapsed(&cpu_clock_begin, &cpu_clock_end);

    if (test->tear_down != NULL)
      test->tear_down(data);
  } while (++i < iterations);

  if (i == 1)
    fprintf(MUNIT_OUTPUT_FILE, "success %" MUNIT_TEST_TIME_FORMAT " seconds (%" MUNIT_TEST_TIME_FORMAT " seconds CPU).\n", elapsed_wall, elapsed_wall);
  else
    fprintf(MUNIT_OUTPUT_FILE, "success (%u iterations averaging %" MUNIT_TEST_TIME_FORMAT " seconds, %" MUNIT_TEST_TIME_FORMAT " seconds CPU).\n", iterations, elapsed_wall / iterations, elapsed_cpu / iterations);
}

static bool
munit_test_exec(const MunitTest* test, void* user_data, unsigned int iterations, MunitSuiteOptions options, uint32_t seed) {
  bool success = false;
#if !defined(_WIN32)
  const bool fork_requested = (((options & MUNIT_SUITE_OPTION_NO_FORK) == 0) && ((test->options & MUNIT_TEST_OPTION_NO_FORK) == 0));
  pid_t fork_pid;
#else
  const bool fork_requested = false;
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
    success = true;
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

  fprintf(MUNIT_OUTPUT_FILE, "%d of %d (%f%%) tests successful.\n",
	  successful, test_num, (((double) successful) / ((double) test_num)) * 100);

  return successful == test_num;
}
