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

/* If you have long test names you might want to consider bumping
 * this.  The result information takes 43 characters. */
#if !defined(MUNIT_TEST_NAME_LEN)
#  define MUNIT_TEST_NAME_LEN 37
#endif

/*** End configuration ***/

#define _XOPEN_SOURCE 600
#define _POSIX_C_SOURCE 200112L

#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#if !defined(_WIN32)
#  include <unistd.h>
#  include <sys/types.h>
#  include <sys/wait.h>
#else
#  include <windows.h>
#  include <io.h>
#  include <fcntl.h>
#  if !defined(STDERR_FILENO)
#    define STDERR_FILENO _fileno(stderr)
#  endif
#endif

#include "munit.h"

#define MUNIT_STRINGIFY(x) #x
#define MUNIT_XSTRINGIFY(x) MUNIT_STRINGIFY(x)

/*** Logging ***/

static MunitLogLevel munit_log_level_visible = MUNIT_LOG_INFO;
static MunitLogLevel munit_log_level_fatal = MUNIT_LOG_ERROR;

/* At certain warning levels, mingw will trigger warnings about
 * suggesting the format attribute, which we've explicity *not* set
 * because it will then choke on our attempts to use the MS-specific
 * I64 modifier for size_t (which we have to use since MSVC doesn't
 * support the C99 z modifier). */

#if defined(__MINGW32__) || defined(__MINGW64__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wsuggest-attribute=format"
#endif

void
munit_log_ex(MunitLogLevel level, const char* filename, int line, const char* format, ...) {
  va_list ap;

  if (level >= munit_log_level_visible) {
    switch(level) {
      case MUNIT_LOG_DEBUG:
        fprintf(stderr, "DEBUG> %s:%d: ", filename, line);
        break;
      case MUNIT_LOG_INFO:
        fprintf(stderr, "INFO>  %s:%d: ", filename, line);
        break;
      case MUNIT_LOG_WARNING:
        fprintf(stderr, "WARN>  %s:%d: ", filename, line);
        break;
      case MUNIT_LOG_ERROR:
        fprintf(stderr, "ERROR> %s:%d: ", filename, line);
        break;
    }

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fputc('\n', stderr);
  }

  if (level >= munit_log_level_fatal)
    abort();
}

#if defined(__MINGW32__) || defined(__MINGW64__)
#pragma GCC diagnostic pop
#endif

/*** Memory allocation ***/

void*
munit_malloc_ex(const char* filename, int line, size_t size) {
  if (size == 0)
    return NULL;

  void* ptr = calloc(1, size);
  if (MUNIT_UNLIKELY(ptr == NULL)) {
    munit_log_ex (MUNIT_LOG_ERROR, filename, line, "Failed to allocate %zu bytes.", size);
  }

  return ptr;
}

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

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_ATOMICS__)
#  define HAVE_STDATOMIC
#elif defined(__clang__)
#  if __has_extension(c_atomic)
#    define HAVE_CLANG_ATOMICS
#  endif
#endif

#if defined(HAVE_STDATOMIC)
#  include <stdatomic.h>
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

static ATOMIC_UINT32_T munit_rand_state = ATOMIC_UINT32_INIT(42);

#if defined(HAVE_STDATOMIC)
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
#  define munit_atomic_cas(dest, expected, value) InterlockedCompareExchange(dest, value, *expected)
#else
#  error No atomic implementation
#endif

#define MUNIT_PRNG_MULTIPLIER UINT32_C(747796405)
#define MUNIT_PRNG_INCREMENT  1729

void
  munit_rand_seed(uint32_t seed) {
  munit_atomic_store(&munit_rand_state, seed);
}

static uint32_t
munit_rand_from_state(uint32_t state) {
  uint32_t res = ((state >> ((state >> 28) + 4)) ^ state) * UINT32_C(277803737);
  res ^= res >> 22;
  return res;
}

static uint32_t
munit_prng_uint32(uint32_t* state) {
  *state = *state * MUNIT_PRNG_MULTIPLIER + (MUNIT_PRNG_INCREMENT | 1);
  uint32_t res = ((*state >> ((*state >> 28) + 4)) ^ *state) * UINT32_C(277803737);
  res ^= res >> 22;
  return res;
}

static void
munit_prng_seed(uint32_t* state) {
  uint32_t orig = (uint32_t) time(NULL);
  orig = orig * MUNIT_PRNG_MULTIPLIER + (MUNIT_PRNG_INCREMENT | 1);
  *state = munit_prng_uint32(&orig);
}

static uint32_t
munit_rand_uint32(void) {
  uint32_t old;
  uint32_t state;

  do {
    old = munit_atomic_load(&munit_rand_state);
    state = old * MUNIT_PRNG_MULTIPLIER + (MUNIT_PRNG_INCREMENT | 1);
  } while (!munit_atomic_cas(&munit_rand_state, &old, state));

  return munit_rand_from_state(old);
}

void
munit_rand_memory(size_t size, uint8_t buffer[MUNIT_ARRAY_PARAM(size)]) {
  size_t members_remaining = size / sizeof(uint32_t);
  size_t bytes_remaining = size % sizeof(uint32_t);
  uint8_t* b = buffer;
  uint32_t rv;
  while (members_remaining-- > 0) {
    rv = munit_rand_uint32();
    memcpy(b, &rv, sizeof(uint32_t));
    b += sizeof(uint32_t);
  }
  if (bytes_remaining != 0) {
    rv = munit_rand_uint32();
    memcpy(b, &rv, bytes_remaining);
  }
}

int
munit_rand_int(void) {
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

static int
munit_rand_int_range_salted(int min, int max, uint32_t salt) {
  munit_assert(min < max);

  if (min == max)
    return min;
  else if (min > max)
    return munit_rand_int_range(max, min);

  /* TODO this is overflowy, biased, and generally buggy. */

  gtint range = (max - min) + 1;
  return ((munit_rand_uint32() ^ salt) % range) + min;
}

int
munit_rand_int_range(int min, int max) {
  return munit_rand_int_range_salted(min, max, 0);
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

#define MUNIT_CPU_TIME_METHOD_CLOCK_GETTIME 0
#define MUNIT_CPU_TIME_METHOD_CLOCK 1
#define MUNIT_CPU_TIME_METHOD_GETPROCESSTIMES 2
#define MUNIT_CPU_TIME_METHOD_GETRUSAGE 3

#define MUNIT_WALL_TIME_METHOD_CLOCK_GETTIME 8
#define MUNIT_WALL_TIME_METHOD_GETTIMEOFDAY 9
#define MUNIT_WALL_TIME_METHOD_QUERYPERFORMANCECOUNTER 10
#define MUNIT_WALL_TIME_METHOD_MACH_ABSOLUTE_TIME 11

#if defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0)
#  define MUNIT_CPU_TIME_METHOD  MUNIT_CPU_TIME_METHOD_CLOCK_GETTIME
#  define MUNIT_WALL_TIME_METHOD MUNIT_WALL_TIME_METHOD_CLOCK_GETTIME
#elif defined(_WIN32)
#  define MUNIT_CPU_TIME_METHOD  MUNIT_CPU_TIME_METHOD_GETPROCESSTIMES
#  define MUNIT_WALL_TIME_METHOD MUNIT_WALL_TIME_METHOD_QUERYPERFORMANCECOUNTER
#elif defined(__MACH__)
#  define MUNIT_CPU_TIME_METHOD  MUNIT_CPU_TIME_METHOD_GETRUSAGE
#  define MUNIT_WALL_TIME_METHOD MUNIT_WALL_TIME_METHOD_MACH_ABSOLUTE_TIME
#else
#  define MUNIT_CPU_TIME_METHOD  MUNIT_CPU_TIME_METHOD_GETRUSAGE
#  define MUNIT_WALL_TIME_METHOD MUNIT_WALL_TIME_METHOD_GETTIMEOFDAY
#endif

#if MUNIT_CPU_TIME_METHOD == MUNIT_CPU_TIME_METHOD_CLOCK_GETTIME
#include <time.h>
typedef struct timespec MunitCpuClock;
#elif MUNIT_CPU_TIME_METHOD == MUNIT_CPU_TIME_METHOD_CLOCK
#include <time.h>
typedef clock_t MunitCpuClock;
#elif MUNIT_CPU_TIME_METHOD == MUNIT_CPU_TIME_METHOD_GETPROCESSTIMES
#include <windows.h>
typedef FILETIME MunitCpuClock;
#elif MUNIT_CPU_TIME_METHOD == MUNIT_CPU_TIME_METHOD_GETRUSAGE
#include <sys/time.h>
#include <sys/resource.h>
typedef struct rusage MunitCpuClock;
#endif

#if MUNIT_WALL_TIME_METHOD == MUNIT_WALL_TIME_METHOD_CLOCK_GETTIME
#include <time.h>
typedef struct timespec MunitWallClock;
#elif MUNIT_WALL_TIME_METHOD == MUNIT_WALL_TIME_METHOD_GETTIMEOFDAY
typedef struct timeval MunitWallClock;
#elif MUNIT_WALL_TIME_METHOD == MUNIT_WALL_TIME_METHOD_QUERYPERFORMANCECOUNTER
typedef LARGE_INTEGER MunitWallClock;
#elif MUNIT_WALL_TIME_METHOD == MUNIT_WALL_TIME_METHOD_MACH_ABSOLUTE_TIME
#include <mach/mach.h>
#include <mach/mach_time.h>
typedef uint64_t MunitWallClock;
#endif

static void
munit_wall_clock_get_time(MunitWallClock* wallclock) {
#if MUNIT_WALL_TIME_METHOD == MUNIT_WALL_TIME_METHOD_CLOCK_GETTIME
  if (clock_gettime(CLOCK_MONOTONIC, wallclock) != 0) {
    fputs("Unable to get wall clock time\n", stderr);
    exit(EXIT_FAILURE);
  }
#elif MUNIT_WALL_TIME_METHOD == MUNIT_WALL_TIME_METHOD_QUERYPERFORMANCECOUNTER
  QueryPerformanceCounter(wallclock);
#elif MUNIT_WALL_TIME_METHOD == MUNIT_WALL_TIME_METHOD_GETTIMEOFDAY
  if (gettimeofday(wallclock, NULL) != 0) {
    fputs("Unable to get wall clock time\n", stderr);
    exit(EXIT_FAILURE);
  }
#elif MUNIT_WALL_TIME_METHOD == MUNIT_WALL_TIME_METHOD_MACH_ABSOLUTE_TIME
  *wallclock = mach_absolute_time();
#endif
}

static void
munit_cpu_clock_get_time(MunitCpuClock* cpuclock) {
#if MUNIT_CPU_TIME_METHOD == MUNIT_CPU_TIME_METHOD_CLOCK_GETTIME
  static const clockid_t clock_id =
#if defined(_POSIX_CPUTIME) || defined(CLOCK_PROCESS_CPUTIME_ID)
    CLOCK_PROCESS_CPUTIME_ID
#elif defined(CLOCK_VIRTUAL)
    CLOCK_VIRTUAL
#else
#error No clock found
#endif
    ;

  if (clock_gettime(clock_id, cpuclock) != 0) {
    fputs("Unable to get CPU clock time\n", stderr);
    exit(EXIT_FAILURE);
  }
#elif MUNIT_CPU_TIME_METHOD == MUNIT_CPU_TIME_METHOD_GETPROCESSTIMES
  FILETIME CreationTime, ExitTime, KernelTime;
  if (!GetProcessTimes (GetCurrentProcess(), &CreationTime, &ExitTime, &KernelTime, cpuclock)) {
    fputs("Unable to get CPU clock time\n", stderr);
  }
#elif MUNIT_CPU_TIME_METHOD == MUNIT_CPU_TIME_METHOD_CLOCK
  *cpuclock = clock();
#elif MUNIT_CPU_TIME_METHOD == MUNIT_CPU_TIME_METHOD_GETRUSAGE
  if (getrusage(RUSAGE_SELF, cpuclock) != 0) {
    fputs("Unable to get CPU clock time\n", stderr);
    exit(EXIT_FAILURE);
  }
#endif
}

static double
munit_wall_clock_get_elapsed(MunitWallClock* start, MunitWallClock* end) {
#if MUNIT_WALL_TIME_METHOD == MUNIT_WALL_TIME_METHOD_CLOCK_GETTIME
  return
    (double) (end->tv_sec - start->tv_sec) +
    (((double) (end->tv_nsec - start->tv_nsec)) / 1000000000);
#elif MUNIT_WALL_TIME_METHOD == MUNIT_WALL_TIME_METHOD_QUERYPERFORMANCECOUNTER
  LARGE_INTEGER Frequency;
  LONGLONG elapsed_ticks;
  QueryPerformanceFrequency(&Frequency);
  elapsed_ticks = end->QuadPart - start->QuadPart;
  return ((double) elapsed_ticks) / ((double) Frequency.QuadPart);
#elif MUNIT_WALL_TIME_METHOD == MUNIT_WALL_TIME_METHOD_GETTIMEOFDAY
  return
    (double) (end->tv_sec - start->tv_sec) +
    (((double) (end->tv_usec - start->tv_usec)) / 1000000);
#elif MUNIT_WALL_TIME_METHOD == MUNIT_WALL_TIME_METHOD_MACH_ABSOLUTE_TIME
  static mach_timebase_info_data_t timebase_info = { 0, 0 };
  if (timebase_info.denom == 0)
    (void) mach_timebase_info(&timebase_info);

  return ((*end - *start) * timebase_info.numer / timebase_info.denom) / 1000000000.0;
#endif
}

static double
munit_cpu_clock_get_elapsed(MunitCpuClock* start, MunitCpuClock* end) {
#if MUNIT_CPU_TIME_METHOD == MUNIT_CPU_TIME_METHOD_CLOCK_GETTIME
  return
    (double) (end->tv_sec - start->tv_sec) +
    (((double) (end->tv_nsec - start->tv_nsec)) / 1000000000);
#elif MUNIT_CPU_TIME_METHOD == MUNIT_CPU_TIME_METHOD_GETPROCESSTIMES
  ULONGLONG start_cpu, end_cpu;

  start_cpu   = start->dwHighDateTime;
  start_cpu <<= sizeof (DWORD) * 8;
  start_cpu  |= start->dwLowDateTime;

  end_cpu   = end->dwHighDateTime;
  end_cpu <<= sizeof (DWORD) * 8;
  end_cpu  |= end->dwLowDateTime;

  return ((double) (end_cpu - start_cpu)) / 10000000;
#elif MUNIT_CPU_TIME_METHOD == MUNIT_CPU_TIME_METHOD_CLOCK
  return ((double) (*end - *start)) / CLOCKS_PER_SEC;
#elif MUNIT_CPU_TIME_METHOD == MUNIT_CPU_TIME_METHOD_GETRUSAGE
  return
    (double) ((end->ru_utime.tv_sec + end->ru_stime.tv_sec) - (start->ru_utime.tv_sec + start->ru_stime.tv_sec)) +
    (((double) ((end->ru_utime.tv_usec + end->ru_stime.tv_usec) - (start->ru_utime.tv_usec + start->ru_stime.tv_usec))) / 1000000);
#endif
}

/*** Test suite handling ***/

typedef struct {
  unsigned int successful;
  unsigned int skipped;
  unsigned int failed;
  unsigned int errored;
  double cpu_clock;
  double wall_clock;
} MunitReport;

typedef struct {
  const char* prefix;
  const MunitSuite* suite;
  const char** tests;
  uint32_t seed;
  MunitParameter* parameters;
  bool single_parameter_mode;
  void* user_data;
  MunitReport report;
  bool colorize;
} MunitTestRunner;

const char*
munit_parameters_get(const MunitParameter params[], const char* key) {
  for (const MunitParameter* param = params ; param != NULL && param->name != NULL ; param++)
    if (strcmp(param->name, key) == 0)
      return param->value;
  return NULL;
}

static void
munit_print_time(FILE* fp, double seconds) {
  fprintf(fp, "%" MUNIT_TEST_TIME_FORMAT, seconds);
}

/* Add a paramter to an array of parameters. */
static MunitResult
munit_parameters_add(size_t* params_size, MunitParameter* params[MUNIT_ARRAY_PARAM(*params_size)], char* name, char* value) {
  *params = realloc(*params, sizeof(MunitParameter) * (*params_size + 2));
  if (*params == NULL)
    return MUNIT_ERROR;

  (*params)[*params_size].name = name;
  (*params)[*params_size].value = value;
  (*params_size)++;
  (*params)[*params_size].name = NULL;
  (*params)[*params_size].value = NULL;

  return MUNIT_OK;
}

/* Concatenate two strings, but just return one of the components
 * unaltered if the other is NULL or "". */
static char*
munit_maybe_concat(size_t* len, char* prefix, char* suffix) {
  char* res;
  size_t res_l;
  const size_t prefix_l = prefix != NULL ? strlen(prefix) : 0;
  const size_t suffix_l = suffix != NULL ? strlen(suffix) : 0;
  if (prefix_l == 0 && suffix_l == 0) {
    res = NULL;
    res_l = 0;
  } else if (prefix_l == 0 && suffix_l != 0) {
    res = suffix;
    res_l = suffix_l;
  } else if (prefix_l != 0 && suffix_l == 0) {
    res = prefix;
    res_l = prefix_l;
  } else {
    res_l = prefix_l + suffix_l;
    res = malloc(res_l + 1);
    memcpy(res, prefix, prefix_l);
    memcpy(res + prefix_l, suffix, suffix_l);
    res[res_l] = 0;
  }

  if (len != NULL)
    *len = res_l;

  return res;
}

/* Possbily free a string returned by munit_maybe_concat. */
static void
munit_maybe_free_concat(char* s, const char* prefix, const char* suffix) {
  if (prefix != s && suffix != s)
    free(s);
}

/* Cheap string hash function, just used to salt the PRNG. */
static uint32_t
munit_str_hash(const char* name) {
  const char *p;
  uint32_t h = UINT32_C(5381);

  for (p = name; *p != '\0'; p++)
    h = (h << 5) + h + *p;

  return h;
}

#if defined(_WIN32)
static int
pipe(int pipefd[2]) {
  HANDLE readfd;
  HANDLE writefd;
  if (!CreatePipe(&readfd, &writefd, NULL, 0))
    return -1;
  pipefd[0] = _open_osfhandle((intptr_t) readfd, _O_RDONLY);
  pipefd[1] = _open_osfhandle((intptr_t) writefd, _O_APPEND);
  return 0;
}
#endif

static void
munit_splice(int from, int to) {
  uint8_t buf[1024];
#if !defined(_WIN32)
  ssize_t len ;
  ssize_t bytes_written;
#else
  int len;
  int bytes_written;
#endif
  do {
    len = read(from, buf, sizeof(buf));
    if (len > 0) {
      bytes_written = 0;
      do {
        bytes_written += write(to, buf + bytes_written, len - bytes_written);
        if (bytes_written < 0)
          break;
      } while (bytes_written < len);
    }
    else
      break;
  } while (true);
}

/* This is the part that should be handled in the child process */
static MunitResult
munit_test_runner_exec(MunitTestRunner* runner, const MunitTest* test, const MunitParameter params[], MunitReport* report) {
  unsigned int iterations = ((test->options & MUNIT_TEST_OPTION_SINGLE_ITERATION) == 0) ?
    runner->suite->iterations : 1;
  MunitResult result = MUNIT_FAIL;
  MunitWallClock wall_clock_begin, wall_clock_end;
  MunitCpuClock cpu_clock_begin, cpu_clock_end;
  unsigned int i = 0;
  do {
    munit_rand_seed(runner->seed);
    void* data = (test->setup == NULL) ? runner->user_data : test->setup(params, runner->user_data);

    munit_wall_clock_get_time(&wall_clock_begin);
    munit_cpu_clock_get_time(&cpu_clock_begin);

    result = test->test(params, data);

    munit_wall_clock_get_time(&wall_clock_end);
    munit_cpu_clock_get_time(&cpu_clock_end);

    if (test->tear_down != NULL)
      test->tear_down(data);

    if (MUNIT_LIKELY(result == MUNIT_OK)) {
      report->successful++;
      report->wall_clock += munit_wall_clock_get_elapsed(&wall_clock_begin, &wall_clock_end);
      report->cpu_clock += munit_cpu_clock_get_elapsed(&cpu_clock_begin, &cpu_clock_end);
    } else {
      switch (result) {
        case MUNIT_SKIP:
	  report->skipped++;
	  break;
        case MUNIT_FAIL:
	  report->failed++;
	  break;
        case MUNIT_ERROR:
	  report->errored++;
	  break;
        case MUNIT_OK:
        default:
	  break;
      }
      break;
    }
  } while (++i < iterations);

  return result;
}

static void
munit_test_runner_print_color(const MunitTestRunner* runner, const char* string, char color) {
  if (runner->colorize)
    fprintf(MUNIT_OUTPUT_FILE, "\x1b[3%cm%s\x1b[39m", color, string);
  else
    fputs(string, MUNIT_OUTPUT_FILE);
}

/* Run a test with the specified parameters. */
static void
munit_test_runner_run_test_with_params(MunitTestRunner* runner, const MunitTest* test, const MunitParameter params[]) {
  MunitResult result = MUNIT_OK;
  MunitReport report = { 0, 0, 0, 0, 0.0, 0.0 };

  if (params != NULL) {
    unsigned int output_l = 2;
    fputs("  ", MUNIT_OUTPUT_FILE);
    bool first = true;
    for (const MunitParameter* param = params ; param != NULL && param->name != NULL ; param++) {
      if (!first) {
        fputs(", ", MUNIT_OUTPUT_FILE);
        output_l += 2;
      } else {
        first = false;
      }

      output_l += fprintf(MUNIT_OUTPUT_FILE, "%s=%s", param->name, param->value);
    }
    while (output_l++ < MUNIT_TEST_NAME_LEN) {
      fputc(' ', MUNIT_OUTPUT_FILE);
    }
  }

  fflush(MUNIT_OUTPUT_FILE);

#if !defined(_WIN32)
  int pipefd[2];
  int redir_stderr[2];
  if (pipe(pipefd) != 0) {
    fprintf(MUNIT_OUTPUT_FILE, "Error: unable to create pipe: %s\n", strerror(errno));
    result = MUNIT_ERROR;
  } else if (pipe(redir_stderr) != 0) {
    fprintf(MUNIT_OUTPUT_FILE, "Error: unable to redirect stderr: %s\n", strerror(errno));
    result = MUNIT_ERROR;
  }

  if (result != MUNIT_OK) {
    goto print_result;
  }

  pid_t fork_pid = fork ();
  if (fork_pid == 0) {
    close(redir_stderr[0]);
    close(pipefd[0]);

    dup2(redir_stderr[1], STDERR_FILENO);
    close(redir_stderr[1]);

    munit_test_runner_exec(runner, test, params, &report);

#if !defined(_WIN32)
    ssize_t bytes_written = 0;
#else
    int bytes_written = 0;
#endif
    do {
      bytes_written += write(pipefd[1], ((uint8_t*) (&report)) + bytes_written, sizeof(report) - bytes_written);
      if (bytes_written < 0)
        exit(EXIT_FAILURE);
    } while ((size_t) bytes_written < sizeof(report));
    close(pipefd[1]);
    exit(EXIT_SUCCESS);
  } else if (fork_pid == -1) {
    fprintf(MUNIT_OUTPUT_FILE, "Error: unable to fork(): %s (%d)\n", strerror(errno), errno);
    close(pipefd[0]);
    close(pipefd[1]);
    close(redir_stderr[0]);
    close(redir_stderr[1]);
    result = MUNIT_ERROR;
  } else {
    close(pipefd[1]);
    close(redir_stderr[1]);
    size_t bytes_read = read(pipefd[0], &report, sizeof(report));
    if (bytes_read != sizeof(report)) {
      report.failed++;
    }
    close(pipefd[0]);
    waitpid(fork_pid, NULL, 0);
  }

 print_result:

#else
  int pipefd[2] = { -1, -1 };
  int old_stderr;
  int piperes = pipe(pipefd);
  if (piperes != 0) {
    fprintf(MUNIT_OUTPUT_FILE, "Error: unable to create pipe: %s\n", strerror(errno));
    result = MUNIT_ERROR;
  } else {
    old_stderr = dup(STDERR_FILENO);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[1]);
  }

  munit_test_runner_exec(runner, test, params, &report);

  if (piperes != 0) {
    dup2(old_stderr, STDERR_FILENO);
    close(old_stderr);
  }
#endif

  fputs("[ ", MUNIT_OUTPUT_FILE);
  if (report.failed > 0) {
    munit_test_runner_print_color(runner, "FAIL", '1');
    fputs(" ", MUNIT_OUTPUT_FILE);
    runner->report.failed++;
    result = MUNIT_FAIL;
  } else if (report.errored > 0) {
    munit_test_runner_print_color(runner, "ERROR", '1');
    runner->report.errored++;
    result = MUNIT_ERROR;
  } else if (report.skipped > 0) {
    munit_test_runner_print_color(runner, "SKIP", '3');
    fputs(" ", MUNIT_OUTPUT_FILE);
    runner->report.skipped++;
    result = MUNIT_SKIP;
  } else if (report.successful > 1) {
    munit_test_runner_print_color(runner, "OK", '2');
    fputs("    ] [ ", MUNIT_OUTPUT_FILE);
    munit_print_time(MUNIT_OUTPUT_FILE, report.wall_clock / ((double) report.successful));
    fputs(" / ", MUNIT_OUTPUT_FILE);
    munit_print_time(MUNIT_OUTPUT_FILE, report.cpu_clock / ((double) report.successful));
    fprintf(MUNIT_OUTPUT_FILE, " CPU ]\n  %-" MUNIT_XSTRINGIFY(MUNIT_TEST_NAME_LEN) "s Total: [ ", "");
    munit_print_time(MUNIT_OUTPUT_FILE, report.wall_clock);
    fputs(" / ", MUNIT_OUTPUT_FILE);
    munit_print_time(MUNIT_OUTPUT_FILE, report.cpu_clock);
    fputs(" CPU", MUNIT_OUTPUT_FILE);
    runner->report.successful++;
    result = MUNIT_OK;
  } else if (report.successful > 0) {
    munit_test_runner_print_color(runner, "OK", '2');
    fputs("    ] [ ", MUNIT_OUTPUT_FILE);
    munit_print_time(MUNIT_OUTPUT_FILE, report.wall_clock);
    fputs(" / ", MUNIT_OUTPUT_FILE);
    munit_print_time(MUNIT_OUTPUT_FILE, report.cpu_clock);
    fputs(" CPU", MUNIT_OUTPUT_FILE);
    runner->report.successful++;
    result = MUNIT_OK;
  }
  fputs(" ]\n", MUNIT_OUTPUT_FILE);

  if (result == MUNIT_FAIL || result == MUNIT_ERROR) {
    fflush(MUNIT_OUTPUT_FILE);
#if !defined(_WIN32)
    munit_splice(redir_stderr[0], STDERR_FILENO);
#else
    munit_splice(pipefd[0], STDERR_FILENO);
#endif
    fflush(stderr);
  }

#if !defined(_WIN32)
  close(redir_stderr[0]);
#else
  if (piperes != 0)
    close(pipefd[0]);
#endif
}

static void
munit_test_runner_run_test_wild(MunitTestRunner* runner,
                                const MunitTest* test,
                                const char* test_name,
                                MunitParameter* params,
                                MunitParameter* p) {
  const MunitParameterEnum* pe;
  for (pe = test->parameters ; pe != NULL && pe->name != NULL ; pe++) {
    if (p->name == pe->name)
      break;
  }

  if (pe == NULL)
    return;

  for (char** values = pe->values ; *values != NULL ; values++) {
    MunitParameter* next = p + 1;
    p->value = *values;
    if (next->name == NULL) {
      munit_test_runner_run_test_with_params(runner, test, params);
    } else {
      munit_test_runner_run_test_wild(runner, test, test_name, params, next);
    }
  }
}

/* Run a single test, with every combination of parameters
 * requested. */
static void
munit_test_runner_run_test(MunitTestRunner* runner,
                           const MunitTest* test,
                           const char* prefix) {
  char* test_name = munit_maybe_concat(NULL, (char*) prefix, (char*) test->name);

  munit_rand_seed(runner->seed);

  fprintf(MUNIT_OUTPUT_FILE, "%-" MUNIT_XSTRINGIFY(MUNIT_TEST_NAME_LEN) "s", test_name);

  if (test->parameters == NULL) {
    /* No parameters.  Simple, nice. */
    munit_test_runner_run_test_with_params(runner, test, NULL);
  } else {
    /* The array of parameters to pass to
     * munit_test_runner_run_test_with_params */
    MunitParameter* params = NULL;
    size_t params_l = 0;
    /* Wildcard parameters are parameters which have possible values
     * specified in the test, but no specific value was passed to the
     * CLI.  That means we want to run the test once for every
     * possible combination of parameter values or, if --single was
     * passed to the CLI, a single time with a random set of
     * parameters. */
    MunitParameter* wild_params = NULL;
    size_t wild_params_l = 0;

    fputc('\n', MUNIT_OUTPUT_FILE);

    for (const MunitParameterEnum* pe = test->parameters ; pe != NULL && pe->name != NULL ; pe++) {
      /* Did we received a value for this parameter from the CLI? */
      bool filled = false;
      for (const MunitParameter* cli_p = runner->parameters ; cli_p != NULL && cli_p->name != NULL ; cli_p++) {
        if (strcmp(cli_p->name, pe->name) == 0) {
          if (MUNIT_UNLIKELY(munit_parameters_add(&params_l, &params, pe->name, cli_p->value) != MUNIT_OK))
            goto cleanup;
          filled = true;
          break;
        }
      }
      if (filled)
        continue;

      /* Nothing from CLI, is the enum NULL/empty?  We're not a
       * fuzzer… */
      if (pe->values == NULL || pe->values[0] == NULL)
        continue;

      /* If --single was passed to the CLI, choose a value from the
       * list of possibilities randomly. */
      if (runner->single_parameter_mode) {
        unsigned int possible = 0;
        for (char** vals = pe->values ; *vals != NULL ; vals++)
          possible++;
        /* We want the tests to be reproducible, even if you're only
         * running a single test, but we don't want every test with
         * the same number of parameters to choose the same parameter
         * number, so use the test name as a primitive salt. */
        const int pidx = munit_rand_int_range_salted(0, possible - 1, munit_str_hash(test_name));
        if (MUNIT_UNLIKELY(munit_parameters_add(&params_l, &params, pe->name, pe->values[pidx]) != MUNIT_OK))
          goto cleanup;
      } else {
        /* We want to try every permutation.  Put in a placeholder
         * entry, we'll iterate through them later. */
        if (MUNIT_UNLIKELY(munit_parameters_add(&wild_params_l, &wild_params, pe->name, NULL) != MUNIT_OK))
          goto cleanup;
      }
    }

    if (wild_params_l != 0) {
      const unsigned int first_wild = params_l;
      for (const MunitParameter* wp = wild_params ; wp != NULL && wp->name != NULL ; wp++) {
        for (const MunitParameterEnum* pe = test->parameters ; pe != NULL && pe->name != NULL ; pe++) {
          if (strcmp(wp->name, pe->name) == 0) {
            if (MUNIT_UNLIKELY(munit_parameters_add(&params_l, &params, pe->name, pe->values[0]) != MUNIT_OK))
              goto cleanup;
          }
        }
      }

      munit_test_runner_run_test_wild(runner, test, test_name, params, params + first_wild);
    } else {
      munit_test_runner_run_test_with_params(runner, test, params);
    }

  cleanup:
    free(params);
    free(wild_params);
  }

  munit_maybe_free_concat(test_name, prefix, test->name);
}

/* Recurse through the suite and run all the tests.  If a list of
 * tests to run was provied on the command line, run only those
 * tests.  */
static void
munit_test_runner_run_suite(MunitTestRunner* runner,
                            const MunitSuite* suite,
                            const char* prefix) {
  size_t pre_l;
  char* pre = munit_maybe_concat(&pre_l, (char*) prefix, (char*) suite->prefix);

  /* Run the tests. */
  for (const MunitTest* test = suite->tests ; test != NULL && test->test != NULL ; test++) {
    if (runner->tests != NULL) { /* Specific tests were requested on the CLI */
      for (const char** test_name = runner->tests ; test_name != NULL && *test_name != NULL ; test_name++) {
        if (strncmp(pre, *test_name, pre_l) == 0 &&
            strncmp(test->name, *test_name + pre_l, strlen(*test_name + pre_l)) == 0) {
          munit_test_runner_run_test(runner, test, pre);
        }
      }
    } else { /* Run all tests */
      munit_test_runner_run_test(runner, test, pre);
    }
  }

  /* Run any child suites. */
  for (const MunitSuite* child_suite = suite->suites ; child_suite != NULL && child_suite->prefix != NULL ; child_suite++) {
    munit_test_runner_run_suite(runner, child_suite, pre);
  }

  munit_maybe_free_concat(pre, prefix, suite->prefix);
}

static void
munit_test_runner_run(MunitTestRunner* runner) {
  munit_test_runner_run_suite(runner, runner->suite, NULL);
}

static void
munit_print_help(int argc, const char* argv[MUNIT_ARRAY_PARAM(argc + 1)], void* user_data, const MunitArgument arguments[]) {
  printf("USAGE: %s [OPTIONS...] [TEST...]\n\n", argv[0]);
  puts(" --seed SEED\n"
       "           Value used to seed the PRNG.  Must be a 32-bit integer in\n"
       "           decimal notation with no separators (commas, decimals,\n"
       "           spaces, etc.), or hexidecimal prefixed by \"0x\".\n"
       " --param name value\n"
       "           A parameter key/value pair which will be passed to any test\n"
       "           with takes a parameter of that name.  If not provided,\n"
       "           the test will be run once for each possible parameter\n"
       "           value.\n"
       " --list    Write a list of all available tests.\n"
       " --list-params\n"
       "           Write a list of all available tests and their possible\n"
       "           parameters.\n"
       " --single  Run each parameterized test in a single configuration instead\n"
       "           of every possible combination\n"
       " --log-visible debug|info|warning|error\n"
       " --log-fatal debug|info|warning|error\n"
       "           Set the level at which messages of different severities are\n"
       "           visible, or cause the test to terminate.\n"
       " --color auto|always|never\n"
       "           Colorize (or don't) the output.\n"
       " --help    Print this help message and exit.");
  for (const MunitArgument* arg = arguments ; arg != NULL && arg->name != NULL ; arg++)
    arg->write_help(arg, user_data);
}

static const MunitArgument*
munit_arguments_find(const MunitArgument arguments[], const char* name) {
  for (const MunitArgument* arg = arguments ; arg != NULL && arg->name != NULL ; arg++)
    if (strcmp(arg->name, name) == 0)
      return arg;

  return NULL;
}

static void
munit_suite_list_tests (const MunitSuite* suite, bool show_params, const char* prefix) {
  size_t pre_l;
  char* pre = munit_maybe_concat(&pre_l, (char*) prefix, (char*) suite->prefix);

  for (const MunitTest* test = suite->tests ;
       test != NULL && test->name != NULL ;
       test++) {
    if (pre != NULL)
      fputs(pre, stdout);
    puts(test->name);

    if (show_params) {
      for (const MunitParameterEnum* params = test->parameters ;
           params != NULL && params->name != NULL ;
           params++) {
        fprintf(stdout, " - %s: ", params->name);
        if (params->values == NULL) {
          puts("Any");
        } else {
          bool first = true;
          for (char** val = params->values ;
               *val != NULL ;
               val++ ) {
            if(!first) {
              fputs(", ", stdout);
            } else {
              first = false;
            }
            fputs(*val, stdout);
          }
          putc('\n', stdout);
        }
      }
    }
  }

  for (const MunitSuite* child_suite = suite->suites ; child_suite != NULL && child_suite->prefix != NULL ; child_suite++) {
    munit_suite_list_tests(child_suite, show_params, pre);
  }

  munit_maybe_free_concat(pre, prefix, suite->prefix);
}

int
munit_suite_main_custom(const MunitSuite* suite, void* user_data,
                        int argc, const char* argv[MUNIT_ARRAY_PARAM(argc + 1)],
                        const MunitArgument arguments[]) {
  int result = EXIT_FAILURE;
  MunitTestRunner runner = {
    .prefix = NULL,
    .suite = suite,
    .tests = NULL,
    .seed = 0,
    .parameters = NULL,
    .single_parameter_mode = false,
    .user_data = user_data,
    .report = {
      .successful = 0,
      .skipped = 0,
      .failed = 0,
      .errored = 0,
      .cpu_clock = 0.0,
      .wall_clock = 0.0
    },
    .colorize = false
  };
  size_t parameters_size = 0;
  size_t tests_size = 0;

  munit_prng_seed(&(runner.seed));
  runner.colorize = isatty(fileno(MUNIT_OUTPUT_FILE));

  for (int arg = 1 ; arg < argc ; arg++) {
    if (strncmp("--", argv[arg], 2) == 0) {
      if (strcmp("seed", argv[arg] + 2) == 0) {
        if (arg + 1 >= argc) {
          fputs("Error: --seed requires an argument.\n", stderr);
          goto cleanup;
        }

        char* envptr = NULL;
        unsigned long long ts = strtoull(argv[arg + 1], &envptr, 0);
        if (*envptr != '\0' || ts > UINT32_MAX) {
          fprintf(stderr, "Error: invalid seed (%s) specified.\n", argv[arg + 1]);
          goto cleanup;
        }
        runner.seed = (uint32_t) ts;

        arg++;
      } else if (strcmp("param", argv[arg] + 2) == 0) {
        if (arg + 2 >= argc) {
          fputs("Error: --param requires two arguments.\n", stderr);
          goto cleanup;
        }

        runner.parameters = realloc(runner.parameters, sizeof(MunitParameter) * (parameters_size + 2));
        if (runner.parameters == NULL) {
          fputs("Error: failed to allocate memory.\n", stderr);
          goto cleanup;
        }
        runner.parameters[parameters_size].name = (char*) argv[arg + 1];
        runner.parameters[parameters_size].value = (char*) argv[arg + 2];
        parameters_size++;
        runner.parameters[parameters_size].name = NULL;
        runner.parameters[parameters_size].value = NULL;
        arg += 2;
      } else if (strcmp("color", argv[arg] + 2) == 0) {
        if (arg + 1 >= argc) {
          fputs("Error: --color requires an argument.\n", stderr);
          goto cleanup;
        }

        if (strcmp(argv[arg + 1], "always") == 0)
          runner.colorize = true;
        else if (strcmp(argv[arg + 1], "never") == 0)
          runner.colorize = false;
        else if (strcmp(argv[arg + 1], "auto") == 0)
          runner.colorize = isatty(fileno(MUNIT_OUTPUT_FILE));
        else {
          fprintf(stderr, "Error: invalid value (`%s') passed to --color.\n", argv[arg + 1]);
          goto cleanup;
        }

        arg++;
      } else if (strcmp("help", argv[arg] + 2) == 0) {
        munit_print_help(argc, argv, user_data, arguments);
        result = EXIT_SUCCESS;
        goto cleanup;
      } else if (strcmp("single", argv[arg] + 2) == 0) {
        runner.single_parameter_mode = 1;
      } else if (strcmp("log-visible", argv[arg] + 2) == 0 ||
                 strcmp("log-fatal", argv[arg] + 2) == 0) {
        MunitLogLevel level;

        if (arg + 1 >= argc) {
          fprintf(stderr, "Error: %s requires an argument.\n", argv[arg]);
          goto cleanup;
        }

        if (strcmp(argv[arg + 1], "debug") == 0)
          level = MUNIT_LOG_DEBUG;
        else if (strcmp(argv[arg + 1], "info") == 0)
          level = MUNIT_LOG_DEBUG;
        else if (strcmp(argv[arg + 1], "warning") == 0)
          level = MUNIT_LOG_DEBUG;
        else if (strcmp(argv[arg + 1], "error") == 0)
          level = MUNIT_LOG_DEBUG;
        else {
          fprintf(stderr, "Error: invalid log level `%s'.\n", argv[arg + 1]);
          goto cleanup;
        }

        if (strcmp("log-visible", argv[arg] + 2) == 0)
          munit_log_level_visible = level;
        else
          munit_log_level_fatal = level;

        arg++;
      } else if (strcmp("list", argv[arg] + 2) == 0) {
        munit_suite_list_tests(suite, false, NULL);
        result = EXIT_SUCCESS;
        goto cleanup;
      } else if (strcmp("list-params", argv[arg] + 2) == 0) {
        munit_suite_list_tests(suite, true, NULL);
        result = EXIT_SUCCESS;
        goto cleanup;
      } else {
        const MunitArgument* argument = munit_arguments_find(arguments, argv[arg] + 2);
        if (argument == NULL) {
          fprintf (stderr, "Unknown argument `%s'.\n", argv[arg]);
          goto cleanup;
        }

        if (!argument->parse_argument(suite, user_data, &arg, argc, argv))
          goto cleanup;
      }
    } else {
      runner.tests = realloc((void*) runner.tests, sizeof(char*) * (tests_size + 2));
      if (runner.tests == NULL) {
        fputs("Error: failed to allocate memory.\n", stderr);
        goto cleanup;
      }
      runner.tests[tests_size++] = argv[arg];
      runner.tests[tests_size] = NULL;
    }
  }

  fflush(stderr);
  fprintf(MUNIT_OUTPUT_FILE, "Running test suite with seed 0x%08" PRIx32 "...\n", runner.seed);

  munit_test_runner_run(&runner);

  const unsigned int tests_run = runner.report.successful + runner.report.failed;
  const unsigned int tests_total = tests_run + runner.report.skipped;
  if (tests_run == 0) {
    fprintf(stderr, "No tests run, %d (100%%) skipped.\n", runner.report.skipped);
  } else {
    fprintf(MUNIT_OUTPUT_FILE, "%d of %d (%0.0f%%) tests successful, %d (%0.0f%%) test skipped.\n",
            runner.report.successful, tests_run,
            (((double) runner.report.successful) / ((double) tests_run)) * 100.0,
            runner.report.skipped,
            (((double) runner.report.skipped) / ((double) tests_total)) * 100.0);
  }

  if (runner.report.failed == 0 && runner.report.errored == 0) {
    result = EXIT_SUCCESS;
  }

 cleanup:
  free(runner.parameters);
  free((void*) runner.tests);

  return result;
}

int
munit_suite_main(const MunitSuite* suite, void* user_data,
                 int argc, const char* argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
  return munit_suite_main_custom(suite, user_data, argc, argv, NULL);
}
