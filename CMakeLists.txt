cmake_minimum_required(VERSION 3.7...3.18)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project("munit"  VERSION 0.2.0
                 DESCRIPTION "µnit Testing Library for C"
                 LANGUAGES C
       )

# Options
option(BUILD_SHARED_LIBS "Build shared library" ON)
option(BUILD_STATIC_LIBS "Build static library" ON)
option(BUILD_EXAMPLE "Build example program" OFF)

if(NOT(${BUILD_SHARED_LIBS} OR ${BUILD_STATIC_LIBS}))
    message(FATAL_ERROR "Error: At least one of the static library and the shared library must be built.")
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
configure_file(munit.pc.in ${CMAKE_SOURCE_DIR}/lib/pkgconfig/munit.pc @ONLY)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS_DEBUG "-g -Wall -Wextra -Wpedantic")
set(CMAKE_BUILD_TYPE Release)

add_library(munit_mod MODULE)
target_sources(munit_mod PRIVATE src/munit.c)
target_include_directories(munit_mod PRIVATE include)

if(${BUILD_SHARED_LIBS})
    add_library(munit SHARED $<TARGET_OBJECTS:munit_mod>)
    target_include_directories(munit PUBLIC include)
    set_target_properties(munit PROPERTIES OUTPUT_NAME "munit")
endif()

if(${BUILD_STATIC_LIBS})
    add_library(munit_static STATIC $<TARGET_OBJECTS:munit_mod>)
    target_include_directories(munit_static PUBLIC include)
    set_target_properties(munit_static PROPERTIES OUTPUT_NAME "munit")
    install(TARGETS munit_static ARCHIVE)
endif()

if(${BUILD_EXAMPLE})
    add_executable(munit_example)
    target_sources(munit_example PRIVATE src/example.c)
    if(${BUILD_STATIC_LIBS})
        target_link_libraries(munit_example munit_static)
    else()
        target_link_libraries(munit_example munit)
    endif()
endif()

# Installation
install(FILES ${CMAKE_SOURCE_DIR}/lib/pkgconfig/munit.pc DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
install(FILES include/munit.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)

if(${BUILD_SHARED_LIBS})
    install(TARGETS munit LIBRARY)
endif()

if(${BUILD_STATIC_LIBS})
    install(TARGETS munit_static LIBRARY)
endif()
