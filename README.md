# µnit

µnit is a small but full-featured unit testing framework for C. It has
no dependencies (beyond libc), is permissively licensed (MIT), and is
easy to include into any project.

For more information, see
[the µnit web site](https://nemequ.github.io/munit).

[![Build status](https://travis-ci.org/nemequ/munit.svg?branch=master)](https://travis-ci.org/nemequ/munit)
[![Windows build status](https://ci.appveyor.com/api/projects/status/db515g5ifcwjohq7/branch/master?svg=true)](https://ci.appveyor.com/project/quixdb/munit/branch/master)

## Features

Features µnit currently includes include:

- Handy assertion macros which make for nice error messages.
- Reproducible cross-platform random number generation, including
  support for supplying a seed via CLI.
- Timing of both wall-clock and CPU time.
- Parameterized tests.
- Nested test suites.
- Flexible CLI.
- Forking
  ([except on Windows](https://github.com/nemequ/munit/issues/2)).
- Hiding output of successful tests.

Features µnit does not currently include, but some day may include
(a.k.a., if you file a PR…), include:

- [TAP](http://testanything.org/) support; feel free to discuss in
  [issue #1](https://github.com/nemequ/munit/issues/1)

### Integrate µnit into your project with meson

In your `subprojects` folder put a `munit.wrap` file containing:

```
[wrap-git]
directory=munit
url=https://github.com/nemequ/munit/
revision=head
```

Then you can use a subproject fallback when you include munit as a
dependency to your project: `dependency('munit', fallback: ['munit', 'munit_dep'])`

### Integrate µnit into your project with CMake

You can include µnit as a git submodule and this will expose a `munit` target which can be linked to in your project. Assuming your project is located at `$PROJECT_ROOT`, first add µnit as a submodule:

```
cd $PROJECT_ROOT
mkdir extern && cd extern
> git submodule add https://github.com/nemequ/munit
```

Then, in your `CMakeLists.txt` file (again, assumed to be located in `$PROJECT_ROOT`), add the following line:

```
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/munit)
```

and this will expose the `munit` and `munit_static` targets, which can be linked against in your project with `target_link_libraries`. These targets also have the relevant header file embedded within them.

## Documentation

See [the µnit web site](https://nemequ.github.io/munit).

Additionally, there is a heavily-commented
[example.c](https://github.com/nemequ/munit/blob/master/example.c) in
the repository.
