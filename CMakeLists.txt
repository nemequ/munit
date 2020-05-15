
cmake_minimum_required(VERSION 2.8)

set(CMAKE_C_STANDARD 99)

project(munit C)

if(NOT DEFINED BUILD_SHARED_LIBS)
	set(BUILD_SHARED_LIBS OFF)
endif()

add_library(munit munit.c munit.h)
