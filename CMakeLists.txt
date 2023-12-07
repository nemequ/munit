cmake_minimum_required (VERSION 3.27)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})

project (munit
  LANGUAGES C
  VERSION 0.2.0
  HOMEPAGE_URL "https://github.com/nemequ/munit"
  DESCRIPTION "µnit is a small testing framework for C ")

find_package (OpenMP)

add_library (munit munit.c)

include(GNUInstallDirs)

target_include_directories (munit
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

set_target_properties (munit
  PROPERTIES
    C_STANDARD 99
    C_EXTENSIONS OFF
    C_STANDARD_REQUIRED ON
    DEBUG_POSTFIX "d"
    RELWITHDEBINFO_POSTFIX "rd")

target_link_libraries (munit
  PUBLIC
    $<IF:$<TARGET_EXISTS:OpenMP::OpenMP_C>,OpenMP::OpenMP_C,"">)

install (TARGETS munit
         EXPORT
          munit
         DESTINATION
          ${CMAKE_INSTALL_LIBDIR}
         LIBRARY
         CONFIGURATIONS
          "Debug;Release")

install (EXPORT munit DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/munit)
install (FILES munit.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
