# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Default arguments:
# cmake -DCMAKE_INSTALL_PREFIX:PATH=${HOME}/.local -Dconfig_DOXYGEN_CRITICAL=ON -Dconfig_WERROR=ON -B build
# CC=clang cmake -DCMAKE_INSTALL_PREFIX:PATH=${HOME}/.local -Dconfig_DOXYGEN_CRITICAL=ON -B build-clang

CMAKE_MINIMUM_REQUIRED(VERSION 3.13)
PROJECT(wlmaker VERSION 0.1
  DESCRIPTION "Wayland Maker - A Wayland compositor inspired by Window Maker"
  LANGUAGES C)

# Requires out-of-source builds.
FILE(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOCATION_PATH)
IF(EXISTS "${LOCATION_PATH}")
  MESSAGE(
    FATAL_ERROR
    "You cannot build into a source directory (containing a CMakeLists.txt file).\n"
    "Please make a build subdirectory, for example:\n"
    "cmake -B build\n"
    "(cd build && make)")
ENDIF()

# Defaults to /usr/local/lib for installation.
INCLUDE(GNUInstallDirs)
INCLUDE(CTest)

FIND_PACKAGE(PkgConfig REQUIRED)

PKG_CHECK_MODULES(CAIRO REQUIRED IMPORTED_TARGET cairo>=1.16.0)
PKG_CHECK_MODULES(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon>=1.5.0)

# https://github.com/ianlancetaylor/libbacktrace.
# The REQUIRED arg is available only from cmake 3.28.
FIND_LIBRARY(LIBBACKTRACE backtrace)
IF(LIBBACKTRACE)
  MESSAGE(STATUS "Found libbacktrace: ${LIBBACKTRACE}")
ELSE()
  MESSAGE(STATUS "libbacktrace not found. Will compile without.")
ENDIF()

# Pruning includes.
FIND_PROGRAM(iwyu_executable NAMES include-what-you-use iwyu OPTIONAL)
IF(iwyu_executable)
  SET(iwyu_path_and_options
    ${iwyu_executable}
    -Xiwyu --error=1 -Xiwyu --transitive_includes_only -Xiwyu --mapping_file=${PROJECT_SOURCE_DIR}/iwyu-mappings.imp)
ENDIF(iwyu_executable)

# Configuration. Remove CMakeCache.txt to rerun...
OPTION(config_DEBUG "Include debugging information" ON)
OPTION(config_OPTIM "Optimizations" OFF)
OPTION(config_DOXYGEN_CRITICAL "Whether to fail on doxygen warnings" OFF)
OPTION(config_WERROR "Make all compiler warnings into errors." OFF)

# Toplevel compile options, for GCC and clang.
IF(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  ADD_COMPILE_OPTIONS(-Wall -Wextra)
  IF(config_WERROR)
    ADD_COMPILE_OPTIONS(-Werror)
  ENDIF(config_WERROR)

  IF(config_DEBUG)
    ADD_COMPILE_OPTIONS(-ggdb -DDEBUG)
  ENDIF(config_DEBUG)

  IF(config_OPTIM)
    ADD_COMPILE_OPTIONS(-O2)
  ELSE (config_OPTIM)
    ADD_COMPILE_OPTIONS(-O0)
  ENDIF(config_OPTIM)

  # CMake provides absolute paths to GCC, hence the __FILE__ macro includes the
  # full path. This option resets it to a path relative to project source.
  ADD_COMPILE_OPTIONS(-fmacro-prefix-map=${PROJECT_SOURCE_DIR}=.)

ENDIF(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
SET(CMAKE_C_STANDARD 11)

LIST(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

ADD_SUBDIRECTORY(apps)
ADD_SUBDIRECTORY(doc)
ADD_SUBDIRECTORY(etc)
ADD_SUBDIRECTORY(icons)
ADD_SUBDIRECTORY(protocols)
ADD_SUBDIRECTORY(third_party/protocols)
ADD_SUBDIRECTORY(share)
ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(tests)

# Adds submodules last, to permit checking on already-existing targets.
ADD_SUBDIRECTORY(submodules/libbase)
