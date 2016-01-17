# µnit

µnit is a small but full-featured unit testing framework for.  It has
no dependencies (beyond libc), is permissively licensed (MIT), and is
easy to include into any project.

[![Build status](https://travis-ci.org/nemequ/munit.svg?branch=master)](https://travis-ci.org/nemequ/munit)
[![Windows build status](https://ci.appveyor.com/api/projects/status/db515g5ifcwjohq7/branch/master?svg=true)](https://ci.appveyor.com/project/quixdb/munit/branch/master)

## Features

Features µnit currently includes include:

 * Handy assertion macros which make for nice error messages.
 * Reproducible cross-platform random number generation, including
   support for supplying a seed via CLI.
 * Timing of both wall-clock and CPU time.
 * Parametric tests.
 * Nested test suites.
 * Flexible CLI.
 * Forking
   ([except on Windows](https://github.com/nemequ/munit/issues/2)).
 * Hiding output of successful tests.

Features µnit does not currently include, but some day may include
(a.k.a., if you file a PR…), include:

 * [TAP](http://testanything.org/) support; feel free to discuss in
   [issue #1](https://github.com/nemequ/munit/issues/1)

## Documentation

Documentation is available in the form of the heavily-commented
[example.c](https://github.com/nemequ/munit/blob/master/example.c).

## FAQ

* Who uses it? — µnit was originally written for
  [Squash](https://quixdb.github.io/squash/).  To find other users you
  can
  [search GitHub](https://github.com/search?l=c&q=munit_suite_main&type=Code&utf8=%E2%9C%93).
  Or use another code search engine, or Google.
* Why another unit testing framework? — I couldn't find anything that
  met all my requirements:
  * Support for pseudo-random numbers.  I'm very fond of using a PRNG
    to randomize tests, which helps provide good coverage without
    taking too long.  Using libc's `rand` function isn't an option
    because it varies by platform, making it difficult to reproduce
    failures.
  * Easy to integrate without adding an external dependency.  Most of
    the frameworks with a big enough feature for my taste are spread
    across many files, and have their own build system, which can
    make integrating the framework with your build system difficult.
    With µnit you simply need to add one C file and include a single
    header.
  * Portable.  Relying on platform-specific functionality or linker
    trickery isn't feasible if your software is meant to be
    cross-platform compatible.
* µnit sucks, where can I find a *good* unit testing framework? —
  µnit isn't for everyone; people have different requirements and
  preferences.  Here are a few others you might want to look into:
  * [cmocka](https://cmocka.org/) and
    [cmockery2](https://github.com/lpabon/cmockery2) are both forks
    of [cmockery](https://code.google.com/p/cmockery/), which is no
    longer being developed.
  * [glib](https://developer.gnome.org/glib/stable/glib-Testing.html)
    has a testing framework which includes PRNG support.
  * [greatest](https://github.com/silentbicycle/greatest)
  * [Unity](https://github.com/ThrowTheSwitch/Unity)
  * [minunit](https://github.com/siu/minunit)
  * [Check](https://libcheck.github.io/check/)
  * [cunit](http://cunit.sourceforge.net/)
  * [Criterion](https://github.com/Snaipe/Criterion)
  * You can
    [search for one on GitHub](https://github.com/search?l=C&q=unit+testing&type=Repositories&utf8=%E2%9C%93);
    some of them look interesting.
