cmake_minimum_required (VERSION 3.16)

project ("munit"
          LANGUAGES C
          DESCRIPTION "µnit Testing Library for C"
          HOMEPAGE_URL "https://github.com/nemequ/munit"
          VERSION 0.2.0)

option (BUILD_SHARED_LIBS "Build shared library" ON)
option (BUILD_TESTS "Build tests" ON)

set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_library (munit)

target_sources (munit
                PRIVATE
                  munit.c)

target_sources (munit
                INTERFACE
                  munit.h)

set_target_properties (munit
                        PROPERTIES
                        C_STANDARD 99
                        C_STANDARD_REQUIRED ON
                        # to pass only c99 not gnu99
                        C_EXTENSIONS OFF)

configure_file (cmake/munit.pc.in
                ${CMAKE_BINARY_DIR}/lib/pkgconfig/munit.pc
                @ONLY)

if (BUILD_EXAMPLE)
  add_executable (munit_example)

  target_sources (munit_example
                  PRIVATE example.c)

  target_link_libraries (munit_example
                          PRIVATE munit)

  include (CTest)

  add_test (munit_example ${CMAKE_BINARY_DIR}/bin/munit_example)
endif ()

install (FILES ${CMAKE_BINARY_DIR}/lib/pkgconfig/munit.pc
          DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
install (FILES munit.h
          DESTINATION ${CMAKE_INSTALL_PREFIX}/include)

install (TARGETS munit LIBRARY)
