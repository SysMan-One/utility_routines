#+++
#       DESCRIPTION: Uitility Routines --- a set of routines for plain C coders
#
#       AUTHOR: Ruslan R. Laishev
#
#       CREATION DATE:  8-MAR-2024
#
#       MODIFICATION HISTORY:
#
#---

cmake_policy(SET CMP0048 NEW)
include(CMakePrintHelpers)

cmake_policy(SET CMP0048 NEW)
project(StarLet  VERSION 1.0.0 DESCRIPTION "Utility Routines")

cmake_minimum_required(VERSION 3.10)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_COLOR_MAKEFILE   ON)
set(CMAKE_C_STANDARD 11)
#set (CMAKE_C_COMPILER /usr/bin/clang)                                                  # Only fgor debug\test purpose


# Predefine project
set( CPACK_PACKAGE_NAME  "${PROJECT_NAME}")
set( CPACK_PACKAGE_VERSION_MAJOR 1)
set( CPACK_PACKAGE_VERSION_MINOR 0)
set( CPACK_PACKAGE_VERSION_PATCH 0)


add_definitions( -D__ARCH__NAME__="${CMAKE_HOST_SYSTEM_PROCESSOR}" )

if (CMAKE_BUILD_TYPE)
       	string(TOLOWER ${CMAKE_BUILD_TYPE} build_type)                                  # Additional definitons for DEBUG builds
endif()

if (build_type STREQUAL debug)
       	add_definitions( -D_DEBUG=1 -D__TRACE__=1 -DDEBUG=1)                            # Add "-w" to disable warnings
endif()



include_directories (
                ./
)


add_library (starlet STATIC
               	avproto.c
               	avproto.h
               	cli_routines.c
               	cli_routines.h
               	utility_routines.c
               	utility_routines.h
)
