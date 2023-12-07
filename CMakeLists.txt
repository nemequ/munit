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
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

set_target_properties (munit
  PROPERTIES
    C_STANDARD 99
    C_EXTENSIONS OFF
    C_STANDARD_REQUIRED ON
    PUBLIC_HEADER "munit.h"
    DEBUG_POSTFIX "d")

target_link_libraries (munit
  PUBLIC
    $<IF:$<TARGET_EXISTS:OpenMP::OpenMP_C>,OpenMP::OpenMP_C,"">)

install (TARGETS munit
         EXPORT
           munit
         PUBLIC_HEADER
           DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}
         LIBRARY
           DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}
         CONFIGURATIONS
          "RELEASE;DEBUG"
         EXPORT_LINK_INTERFACE_LIBRARIES)

export (TARGETS munit
  NAMESPACE
    MUnit::
  FILE
    FindMunit-Config.cmake
  EXPORT_LINK_INTERFACE_LIBRARIES)
