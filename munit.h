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

#if !defined(MUNIT_H)
#define MUNIT_H

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__GNUC__)
#  define MUNIT_LIKELY(expr) (__builtin_expect ((expr), 1))
#  define MUNIT_UNLIKELY(expr) (__builtin_expect ((expr), 0))
#  define MUNIT_UNUSED __attribute__((__unused__))
#else
#  define MUNIT_LIKELY(expr) (expr)
#  define MUNIT_UNLIKELY(expr) (expr)
#  define MUNIT_UNUSED
#endif

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#  define MUNIT_ARRAY_PARAM(name) name
#else
#  define MUNIT_ARRAY_PARAM(name)
#endif

#if !defined(_WIN32)
#  define MUNIT_SIZE_MODIFIER "z"
#  define MUNIT_CHAR_MODIFIER "hh"
#  define MUNIT_SHORT_MODIFIER "h"
#else
#  if defined(_M_X64) || defined(__amd64__)
#    define MUNIT_SIZE_MODIFIER "I64"
#  else
#    define MUNIT_SIZE_MODIFIER ""
#  endif
#  define MUNIT_CHAR_MODIFIER ""
#  define MUNIT_SHORT_MODIFIER ""
#endif

#if !defined(munit_errorf)
#  include <stdio.h> /* For fprintf */
#  if !defined(MUNIT_NO_ABORT_ON_FAILURE)
#    include <stdlib.h> /* For abort */
#    define MUNIT_ABORT() abort()
#  else
#    define MUNIT_ABORT()
#  endif

#define munit_errorf(format, ...) \
  do { \
    fprintf(stderr, \
            "%s:%d: " format "\n", \
            __FILE__, __LINE__, __VA_ARGS__); \
    MUNIT_ABORT(); \
  } while (0)
#endif

#define munit_error(msg) \
  munit_errorf("%s", msg)

#define munit_assert(expr) \
  do { \
    if (!MUNIT_LIKELY(expr)) { \
      munit_error("assertion failed: " #expr); \
    } \
  } while (0)
#define munit_assert_true(expr) \
  munit_assert (!!(expr))
#define munit_assert_false(expr) \
  munit_assert (!(expr))

#define munit_assert_cmp_type_full(prefix, suffix, T, fmt, a, op, b)   \
  do { \
    T munit_tmp_a_ = (T)(a); \
    T munit_tmp_b_ = (T)(b); \
    if (!(munit_tmp_a_ op munit_tmp_b_)) {                               \
      munit_errorf("assertion failed: " #a " " #op " " #b " (" prefix "%" fmt suffix " " #op " " prefix "%" fmt suffix ")", \
                   munit_tmp_a_, munit_tmp_b_); \
    } \
  } while (0)

#define munit_assert_cmp_type(T, fmt, a, op, b) \
  munit_assert_cmp_type_full("", "", T, fmt, a, op, b)

#define munit_assert_cmp_char(a, op, b) \
  munit_assert_cmp_type_full("'\\x", "'", char, "02" MUNIT_CHAR_MODIFIER "x", a, op, b)
#define munit_assert_cmp_uchar(a, op, b) \
  munit_assert_cmp_type_full("'\\x", "'", unsigned char, "02" MUNIT_CHAR_MODIFIER "x", a, op, b)
#define munit_assert_cmp_short(a, op, b) \
  munit_assert_cmp_type(short, MUNIT_SHORT_MODIFIER "d", a, op, b)
#define munit_assert_cmp_ushort(a, op, b) \
  munit_assert_cmp_type(unsigned short, MUNIT_SHORT_MODIFIER "u", a, op, b)
#define munit_assert_cmp_int(a, op, b) \
  munit_assert_cmp_type(int, "d", a, op, b)
#define munit_assert_cmp_uint(a, op, b) \
  munit_assert_cmp_type(unsigned int, "u", a, op, b)
#define munit_assert_cmp_long(a, op, b) \
  munit_assert_cmp_type(long int, "ld", a, op, b)
#define munit_assert_cmp_ulong(a, op, b) \
  munit_assert_cmp_type(unsigned long int, "lu", a, op, b)
#define munit_assert_cmp_llong(a, op, b) \
  munit_assert_cmp_type(long long int, "lld", a, op, b)
#define munit_assert_cmp_ullong(a, op, b) \
  munit_assert_cmp_type(unsigned long long int, "u", a, op, b)

#define munit_assert_cmp_size(a, op, b) \
  munit_assert_cmp_type(size_t, MUNIT_SIZE_MODIFIER "u", a, op, b)

#define munit_assert_cmp_float(a, op, b) \
  munit_assert_cmp_type(float, "f", a, op, b)
#define munit_assert_cmp_double(a, op, b) \
  munit_assert_cmp_type(double, "g", a, op, b)
#define munit_assert_cmp_ptr(a, op, b) \
  munit_assert_cmp_type(const void*, "p", a, op, b)

#include <inttypes.h>
#define munit_assert_cmp_int8(a, op, b) \
  munit_assert_cmp_type(int8_t, PRIi8, a, op, b)
#define munit_assert_cmp_uint8(a, op, b) \
  munit_assert_cmp_type(uint8_t, PRIu8, a, op, b)
#define munit_assert_cmp_int16(a, op, b) \
  munit_assert_cmp_type(int16_t, PRIi16, a, op, b)
#define munit_assert_cmp_uint16(a, op, b) \
  munit_assert_cmp_type(uint16_t, PRIu16, a, op, b)
#define munit_assert_cmp_int32(a, op, b) \
  munit_assert_cmp_type(int32_t, PRIi32, a, op, b)
#define munit_assert_cmp_uint32(a, op, b) \
  munit_assert_cmp_type(uint32_t, PRIu32, a, op, b)
#define munit_assert_cmp_int64(a, op, b) \
  munit_assert_cmp_type(int64_t, PRIi64, a, op, b)
#define munit_assert_cmp_uint64(a, op, b) \
  munit_assert_cmp_type(uint64_t, PRIu64, a, op, b)

#define munit_assert_double_equal(a, b, precision) \
  do { \
    const double munit_tmp_a_ = (const double) (a); \
    const double munit_tmp_b_ = (const double) (b); \
    const double munit_tmp_diff_ = ((munit_tmp_a_ - munit_tmp_b_) < 0) ? \
      -(munit_tmp_a_ - munit_tmp_b_) : \
      (munit_tmp_a_ - munit_tmp_b_); \
    if (MUNIT_UNLIKELY(munit_tmp_diff_ > 1e-##precision)) { \
      munit_errorf("assertion failed: " #a " == " #b " (%0." #precision "g == %0." #precision "g)", \
		   munit_tmp_a_, munit_tmp_b_); \
    } \
  } while (0)

#include <string.h>
#define munit_assert_string_equal(a, b) \
  do { \
    const char* munit_tmp_a_ = (const char*) a; \
    const char* munit_tmp_b_ = (const char*) b; \
    if (MUNIT_UNLIKELY(strcmp (munit_tmp_a_, munit_tmp_b_) != 0)) { \
      munit_errorf("assertion failed: string " #a " == " #b " (\"%s\" == \"%s\")", \
                   munit_tmp_a_, munit_tmp_b_); \
    } \
  } while (0)

#define munit_assert_string_nequal(a, b) \
  do { \
    const char* munit_tmp_a_ = (const char*) a; \
    const char* munit_tmp_b_ = (const char*) b; \
    if (MUNIT_UNLIKELY(strcmp (munit_tmp_a_, munit_tmp_b_) == 0)) { \
      munit_errorf("assertion failed: string " #a " != " #b " (\"%s\" == \"%s\")", \
                   munit_tmp_a_, munit_tmp_b_); \
    } \
  } while (0)

#define munit_assert_memory_equal(size, a, b) \
  do { \
    const uint8_t* munit_tmp_a_ = (const uint8_t*) a; \
    const uint8_t* munit_tmp_b_ = (const uint8_t*) b; \
    const size_t size_ = (size_t) size; \
    size_t pos; \
    for (pos = 0 ; pos < size_ ; pos++) { \
      if (MUNIT_UNLIKELY(munit_tmp_a_[pos] != munit_tmp_b_[pos])) { \
        munit_errorf("assertion failed: memory " #a " == " #b ", at offset %" MUNIT_SIZE_MODIFIER "u", pos); \
        break; \
      } \
    } \
  } while (0)

#define munit_assert_ptr_equal(a, b) \
  munit_assert_cmp_ptr(a, ==, b)
#define munit_assert_null(ptr) \
  munit_assert_cmp_ptr(ptr, ==, NULL)
#define munit_assert_non_null(ptr) \
  munit_assert_cmp_ptr(ptr, !=, NULL)

/*** Random number generation ***/

void munit_rand_seed(uint32_t seed);

int munit_rand_int(void);
int munit_rand_int_range(int min, int max);
double munit_rand_double(void);
void munit_rand_memory (size_t size, uint8_t buffer[MUNIT_ARRAY_PARAM(size)]);

/* If a test implements a setup function then the value that function
 * returns is passed to the user_data_or_fixture parameter.  If the
 * setup function is NULL then it will be the user_data passed to the
 * function used to run the test (likely munit_suite_run). */

typedef void  (* MunitTestFunc)(void* user_data_or_fixture);
typedef void* (* MunitTestSetup)(void* user_data);
typedef void  (* MunitTestTearDown)(void* fixture);

typedef enum {
  MUNIT_TEST_OPTION_NONE = 0,
  MUNIT_TEST_OPTION_SINGLE_ITERATION = 1 << 0,
  MUNIT_TEST_OPTION_NO_RESET = 1 << 1,
  MUNIT_TEST_OPTION_NO_FORK = 1 << 2,
  /* MUNIT_TEST_OPTION_NO_TIME = 1 << 3, */
} MunitTestOptions;

typedef struct {
  const char*       name;
  MunitTestFunc     test;
  MunitTestSetup    setup;
  MunitTestTearDown tear_down;
  MunitTestOptions  options;
} MunitTest;

typedef enum {
  MUNIT_SUITE_OPTION_NONE = 0,
  MUNIT_SUITE_OPTION_NO_FORK = 1 << 1,
  /* MUNIT_SUITE_OPTION_NO_TIME = 1 << 2, */
} MunitSuiteOptions;

typedef struct {
  const MunitTest*  tests;
  unsigned int      iterations;
  MunitSuiteOptions options;
} MunitSuite;

_Bool munit_suite_run_test(const MunitSuite* suite, const char* test, void* user_data);
_Bool munit_suite_run(const MunitSuite* suite, void* user_data);

#if defined(__cplusplus)
}
#endif

#endif /* !defined(MUNIT_H) */
