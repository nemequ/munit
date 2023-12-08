cmake_minimum_required (VERSION 3.27)

project (munit
  LANGUAGES C
  VERSION 0.2.0
  HOMEPAGE_URL "https://github.com/nemequ/munit"
  DESCRIPTION "µnit is a small testing framework for C ")

find_package (OpenMP)

add_library (munit munit.c)

include (GNUInstallDirs)
include (CMakePackageConfigHelpers)

target_include_directories (munit
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

set_target_properties (munit
  PROPERTIES
    C_STANDARD 99
    C_EXTENSIONS OFF
    C_STANDARD_REQUIRED ON
    DEBUG_POSTFIX "_d"
    RELWITHDEBINFO_POSTFIX "_rd"
    MINSIZEREL_POSTFIX "_msr")

target_link_libraries (munit
  PUBLIC
    $<IF:$<TARGET_EXISTS:OpenMP::OpenMP_C>,OpenMP::OpenMP_C,"">)

install (
  TARGETS munit
  EXPORT munit-targets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
                      "${CMAKE_INSTALL_INCLUDEDIR}/munit"
)

install (
  EXPORT munit-targets
  FILE munit-targets.cmake
  NAMESPACE MUnit::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/munit"
)

configure_package_config_file(
  "cmake/Config.cmake.in"
  ${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

install(
  FILES "${PROJECT_BINARY_DIR}/munit-config.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/munit"
)

install (FILES munit.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
