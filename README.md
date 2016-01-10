# µnit

µnit is a small unit testing framework for C99 and greater.  It has a
limited number of bells and almost no whistles.  It has no
dependencies (beyond libc), is permissively licensed (MIT), and is
easy to include into any project.

**µnit is currently a work in progress**.  Check back in a couple
weeks.

## Features

Features µnit currently includes include:

 * Reproducible cross-platform random number generation, including
   support for supplying a seed via CLI.
 * Handy assertion macros which make for nice error messages.
 * Fixtures.
 * Forking (except on Windows; patches to add Windows support will be
   gratefully accepted).

Features it will not include (you can use another framework, such as
[cmocka](https://cmocka.org/),
[glib](https://developer.gnome.org/glib/stable/glib-Testing.html),
[cmockery2](https://github.com/lpabon/cmockery2), etc.) include:

 * Mocking

Features µnit does not currently include, but some day may include
(a.k.a., if you file a PR…), include:

 * [TAP](http://testanything.org/) support
 * Timing (micro-benchmarking).  Code may be stolen from
   [here](https://github.com/quixdb/squash-benchmark/blob/master/timer.c).

## Documentation

See [example.c](https://github.com/nemequ/munit/blob/master/example.c).

## TITYMBW

 * WTF is "TITYMBW"? — "Things I Thought You Might Be Wondering".  I'm
   writing this before I actually release the software, so "Frequently
   Asked Questions" isn't quite right…
 * WTF is "WTF"? —
   [World Taekwondo Federation](http://www.wtf.org/).
 * Who uses it? — You can
   [search GitHub](https://github.com/search?l=c&q=munit_suite_run&type=Code&utf8=%E2%9C%93).
   Or use another code search engine, or Google.
 * Why another unit testing framework? — I couldn't find anything that
   met all my requirements:
   * Easy to integrate without adding an external dependency.
   * Works on both Windows and good operating systems.
   * Included support for pseudo-random numbers.
   * Permissively licensed.
