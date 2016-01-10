/* Want to see what these look like when they all fail (i.e., don't
   abort() on failure)?  Uncomment this: */
/* #define MUNIT_NO_ABORT_ON_FAILURE */

#include "munit.h"

#include <limits.h>

#include <time.h>

int main(void)
{
  const int foo = 1729;
  const int bar = 42;
  munit_assert_cmp_char('a', ==, 'b');
  munit_assert_cmp_uchar('a', ==, 'b');
  munit_assert_cmp_short(1729, ==, 42);
  munit_assert_cmp_int(foo, <=, bar);
  munit_assert_cmp_uint(foo, <=, bar);
  munit_assert_cmp_llong(foo, <=, bar);
  munit_assert_cmp_uint8(4, ==, 5);
  munit_assert_cmp_int64(foo, ==, bar);
  munit_assert_memory_equal(8, "stewardesses", "stewards");
  munit_assert(0 == 1);
  munit_errorf("Hello, %s", "world");
  munit_assert_string_equal("foo", "foo" "bar");
  munit_error("Hi there, world");

  munit_rand_seed(time(NULL));

  for (int x = 0 ; x < 2 ; x++) {
    fprintf (stderr, "Random[%2d]: %" PRIu32 "\n", x, (uint32_t) munit_rand_int());
  }

  for (int x = 0; x < 16 ; x++) {
    fprintf (stderr, "Random[%2d]: %d / %g\n", x, munit_rand_int_range (0, INT_MAX), munit_rand_double());
  }

  return 0;
}
