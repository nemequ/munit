# µnit

µnit is a small unit testing framework for C99 and greater.  It has a
limited number of bells and almost no whistles.  It has no
dependencies (beyond libc), is permissively licensed (MIT), and is
easy to include into any project.

**µnit is a work in progress** and isn't yet ready to use.  Check back
in a couple weeks.

## Features

Features µnit currently includes include:

 * Handy assertion macros which make for nice error messages.
 * Reproducible cross-platform random number generation, including
   support for supplying a seed via CLI.
 * Fixtures.
 * Forking
   ([except on Windows](https://github.com/nemequ/munit/issues/2)).

Features µnit does not currently include, but some day may include
(a.k.a., if you file a PR…), include:

 * [TAP](http://testanything.org/) support; feel free to discuss in
   [issue #1](https://github.com/nemequ/munit/issues/1)
 * Timing (micro-benchmarking).  Code may be stolen from
   [here](https://github.com/quixdb/squash-benchmark/blob/master/timer.c).
   Probably coming soon.

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
 * µnit sucks, where can I find a *good* unit testing framework? —
   µnit isn't for everyone; people have different requirements and
   preferences.  Here are a few others you might want to look into:
   * [cmocka](https://cmocka.org/) and
     [cmockery2](https://github.com/lpabon/cmockery2) are both forks
     of [cmockery](https://code.google.com/p/cmockery/), which is no
     longer being developed.
   * [glib](https://developer.gnome.org/glib/stable/glib-Testing.html)
     has a testing framework which includes PRNG support.
   * [Unity](https://github.com/ThrowTheSwitch/Unity)
   * [minunit](https://github.com/siu/minunit)
   * [Check](https://libcheck.github.io/check/)
   * [cunit](http://cunit.sourceforge.net/)
   * [Criterion](https://github.com/Snaipe/Criterion)
   * You can
     [search for one on GitHub](https://github.com/search?l=C&q=unit+testing&type=Repositories&utf8=%E2%9C%93);
     some of them look interesting.
