#include "munit.h"

/* Tests are functions that return void, and take a single void*
 * parameter.  We'll get to what that parameter is later. */
static void test_compare (void* data) {
  /* This is probably obvious, but lets start with the basics. */
  munit_assert(0 != 1);

  /* There is also the more verbose, but slightly more descriptive
     munit_assert_true/false: */
  munit_assert_false(0);

  /* You can also call munit_error and munit_errorf yourself.  We
   * won't do it is used to indicate a failure, but here is what it
   * would look like: */
  /* munit_error("FAIL"); */
  /* munit_errorf("Goodbye, cruel %s", "world"); */

  /* There are macros for comparing lots of types.  Sure, in this case
   * you could just assert('a' == 'a'), but wait! */
  munit_assert_cmp_char('a', ==, 'a');

  /* If you did that here, a failed assertion would just say something
   * like "assertion failed: val_uchar == 'b'".  µnit will actually
   * tell you the values in addition, so a failure here would result
   * in "assertion failed: val_uchar == 'b' ('X' == 'b')." */
  const unsigned char val_uchar = 'b';
  munit_assert_cmp_uchar(val_uchar, ==, 'b');

  /* Obviously we can handle values larger than 'char' and 'uchar'.
   * There are versions for char, short, int, long, long long,
   * int8/16/32/64_t, as well as the unsigned versions of them all. */
  const short val_short = 1729;
  munit_assert_cmp_short(42, <, val_short);

  /* Of course there is also support for doubles and floats. */
  double pi = 3.141592654;
  munit_assert_cmp_double(pi, ==, 3.141592654);

  /* If you want to compare two doubles for equality, you might want
   * to consider using munit_assert_double_equal.  It compares two
   * doubles for equality within a precison of 1.0 x 10^-(precision).
   * Note that precision (the third argument to the macro) needs to be
   * fully evaluated to an integer by the preprocessor so µnit doesn't
   * have to depend pow, which is often in libm not libc. */
  munit_assert_double_equal(3.141592654, 3.141592653589793, 9);

  /* And munit_assert_string_equal/nequal */
  const char* foo = "bar";
  munit_assert_string_equal(foo, "bar");

  /* A personal favorite which is fantastic if you're working with
   * binary data: */
  munit_assert_memory_equal(7, "stewardesses", "steward");

  /* "stewardesses" is the longest word you can type on a QWERTY
   * keyboard with only one hand, which makes it loads of fun to type.
   * For the longest word in English without repeating any letters,
   * uncopyrightables has dermatoglyphics (and uncopyrightable) beat
   * by a character.. */
  munit_assert_cmp_size(strlen("uncopyrightables"), >, strlen("dermatoglyphics"));

  /* Lets verify that the data parameter is what we expected.  We'll
     get to where this comes from in a bit. */
  munit_assert_string_equal(data, "victory");
}

void test_rand(MUNIT_UNUSED void* user_data) {
  /* One thing missing from a lot of unit testing frameworks is a
   * random number generator.  You can't just use srand/rand because
   * the implementation varies across different platforms, and it's
   * important to be able to look at the seed used in a failing test
   * to see if you can reproduce it.  Some randomness is a fantastic
   * thing to have in your tests, I don't know why more people don't
   * do it...
   *
   * µnit's PRNG is re-seeded with the same value for each iteration
   * of each test.  The seed is retrieved from the MUNIT_SEED
   * envirnment variable or, if none is provided, one will be
   * (pseudo-)randomly generated.
   *
   * You can get a random integer (between INT_MIN and INT_MAX,
   * inclusive). */
  int random_int = munit_rand_int();
  munit_assert(random_int < 0 || random_int > 0);

  /* Or maybe you want a double, between 0 and 1: */
  double random_dbl = munit_rand_double();
  munit_assert_cmp_double(random_dbl, >=, 0.0);
  munit_assert_cmp_double(random_dbl, <=, 1.0);

  /* If you need an integer in a given range */
  random_int = munit_rand_int_range(-10, 10);
  munit_assert_cmp_int(random_int, >=, -10);
  munit_assert_cmp_int(random_int, <=, 10);

  /* Of course, you want to be able to reproduce bugs discovered
   * during testing, so every time the tests are run they print the
   * random seed used.  When you want to reproduce a result, just put
   * that random seed in the MUNIT_SEED environment variable; it even
   * works on different platforms.
   *
   * If you want this to pass, use 0xdeadbeef as the random seed (and
   * uncomment it): */
  /* random_int = munit_rand_int(); */
  /* munit_assert_cmp_int(random_int, ==, -1075473528); */
}

/* We'll get to these soon. */
static void*
test_compare_setup(void* user_data) {
  munit_assert_string_equal(user_data, "µnit");
  return strdup("victory");
}

static void
test_compare_tear_down(void* fixture) {
  munit_assert_string_equal(fixture, "victory");
  free(fixture);
}

/* Creating a test suite is pretty simple.  First, you'll need an
 * array of tests: */
static const MunitTest test_suite_tests[] = {
  {
    /* The name is just a unique human-readable way to identify the
     * test. You can use it to run a specific test if you want, but
     * usually it's purely decorative. */
    "/example/compare",
    /* You probably won't be surprised to learn that the tests are
     * functions. */
    test_compare,
    /* If you want, you can supply a function to set up a fixture.  If
     * you supply NULL, the user_data parameter from munit_suite_run
     * will be used directly.  If, however, you provide a callback
     * here the user_data parameter will be passed to this callback,
     * and the return value from this callback will be passed to the
     * test function.
     *
     * For our example we don't really need a fixture, but lets
     * provide one anyways. */
    test_compare_setup,
    /* If you passed a callback for the fixture setup function, you
     * may want to pass a corresponding callback here to reverse the
     * operation. */
    test_compare_tear_down,
    /* Finally, there is a bitmask for options you can pass here.
     * It's currently empty, but we have plans!  You can provide
     * either MUNIT_TEST_OPTION_NONE or 0 here to use the defaults. */
    MUNIT_TEST_OPTION_NONE },
  /* Usually this is written in a much more compact format; all these
   * comments kind of ruin that, though.  Here is how you'll usually
   * see entries written: */
  { "/example/rand", test_rand, NULL, NULL, 0 },
  /* To tell the test runner when the array is over, just add a NULL
   * entry at the end. */
  { NULL, }
};

/* Now we'll actually declare the test suite.  You could do this in
 * the main function, or on the heap, or whatever you want. */
static const MunitSuite test_suite = {
  /* The first parameter is the array of test suites. */
  test_suite_tests,
  /* An interesting feature of µnit is that it supports automatically
   * running multiple iterations of the tests.  This is usually only
   * interesting if you make use of the PRNG to randomize your tests
   * cases a bit, or if you are doing performance testing and want to
   * average multiple runs. */
  1,
  /* Just like MUNIT_TEST_OPTION_NONE, you can provide
   * MUNIT_SUITE_OPTION_NONE or 0 to use the default settings. */
  MUNIT_SUITE_OPTION_NONE
};

/* This is only necessary for EXIT_SUCCESS/EXIT_FAILURE, which you
 * *should* be using but probably aren't and, therefore, probably
 * don't need. */
#include <stdlib.h>

int main(void) {
  /* Finally, we'll actually run our test suite!  That second argument
   * is the user_data parameter which will be passed either to the
   * test or (if provided) the fixture setup function. */
  return munit_suite_run(&test_suite, "µnit") ? EXIT_SUCCESS : EXIT_FAILURE;
}
