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
# cmake -DCMAKE_INSTALL_PREFIX:PATH=${HOME}/.local -Dconfig_DOXYGEN_CRITICAL=ON -Dconfig_WERROR=ON -DIWYU_MODE=FAIL -B build
# CC=clang cmake -DCMAKE_INSTALL_PREFIX:PATH="${HOME}/.local" -Dconfig_DOXYGEN_CRITICAL=ON -Dconfig_WERROR=ON -DIWYU_MODE=FAIL -B build-clang
#
# Useor "/etc".

CMAKE_MINIMUM_REQUIRED(VERSION 3.13)
PROJECT(
  wlmaker
  VERSION "0.7.1"
  DESCRIPTION "Wayland Maker - A Wayland compositor inspired by Window Maker"
  HOMEPAGE_URL "https://github.com/phkaeser/wlmaker"
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

SET(WLMAKER_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
SET(WLMAKER_VERSION_MINOR ${PROJECT_VERSION_MINOR})
SET(WLMAKER_VERSION_FULL ${PROJECT_VERSION})
# PROJECT version check: When on a git repository, we verify that the PROJECT
# version is equal-or-larger than the git version.
IF(EXISTS "${CMAKE_SOURCE_DIR}/.git")
  EXECUTE_PROCESS(
    COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_SOURCE_DIR}"
    git describe --always --tags --dirty
    OUTPUT_VARIABLE git_version
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE git_result)

  IF(git_result EQUAL 0)
    STRING(REGEX MATCH "^v([0-9]+)\\.([0-9]+)" DUMMY_VAR ${git_version})
    IF(NOT CMAKE_MATCH_COUNT EQUAL 2)
      MESSAGE(FATAL_ERROR "Not a v<MAJOR>.<MINOR> version \"${git_version}\"")
    ENDIF()

    IF(${PROJECT_VERSION_MAJOR} LESS ${CMAKE_MATCH_1} OR
        ${PROJECT_VERSION_MINOR} LESS ${CMAKE_MATCH_2})
      MESSAGE(FATAL_ERROR "Git ${CMAKE_MATCH_1}.${CMAKE_MATCH_2} >= "
        "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
    ENDIF()

    SET(WLMAKER_VERSION_MAJOR ${CMAKE_MATCH_1})
    SET(WLMAKER_VERSION_MINOR ${CMAKE_MATCH_2})
    SET(WLMAKER_VERSION_FULL ${git_version})
  ENDIF()
ENDIF()

# Defaults to /usr/local/lib for installation.
INCLUDE(GNUInstallDirs)
IF(CMAKE_INSTALL_PREFIX STREQUAL "/usr")
  # For targets /usr, forces configurations into XDG config default path.
  SET(WLMAKER_INSTALL_CONFIG_DIR "/etc/xdg/wlmaker")
ELSE()
  SET(WLMAKER_INSTALL_CONFIG_DIR "${CMAKE_INSTALL_SYSCONFDIR}/xdg/wlmaker")
ENDIF()

INCLUDE(CTest)

FIND_PACKAGE(PkgConfig REQUIRED)

PKG_CHECK_MODULES(CAIRO REQUIRED IMPORTED_TARGET cairo>=1.16.0)
PKG_CHECK_MODULES(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon>=1.5.0)
PKG_CHECK_MODULES(LIBXDGBASEDIR REQUIRED IMPORTED_TARGET libxdg-basedir>=1.2)

# https://github.com/ianlancetaylor/libbacktrace.
# The REQUIRED arg is available only from cmake 3.28.
FIND_LIBRARY(LIBBACKTRACE backtrace)
IF(LIBBACKTRACE)
  MESSAGE(STATUS "Found libbacktrace: ${LIBBACKTRACE}")
ELSE()
  MESSAGE(STATUS "libbacktrace not found. Will compile without.")
ENDIF()

# Configuration. Remove CMakeCache.txt to rerun...
OPTION(config_DEBUG "Include debugging information" ON)
OPTION(config_OPTIM "Optimizations" OFF)
OPTION(config_DOXYGEN_CRITICAL "Whether to fail on doxygen warnings" OFF)
OPTION(config_WERROR "Make all compiler warnings into errors." OFF)

SET(IWYU_MODE "OFF" CACHE STRING "Whether to run `iwyu`. OFF, WARN or FAIL.")
SET_PROPERTY(CACHE IWYU_MODE PROPERTY STRINGS "OFF" "WARN" "FAIL")

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

  # Run iwyu when available.
  IF((IWYU_MODE STREQUAL "WARN") OR (IWYU_MODE STREQUAL "FAIL"))
    FIND_PROGRAM(iwyu_executable NAMES include-what-you-use iwyu REQUIRED)

    IF(IWYU_MODE STREQUAL "FAIL")
      SET(iwyu_error "1")
    ELSE(IWYU_MODE STREQUAL "FAIL")
      SET(iwyu_error "0")
    ENDIF(IWYU_MODE STREQUAL "FAIL")

    IF(iwyu_executable)
      SET(iwyu_path_and_options
        ${iwyu_executable}
        -Xiwyu --error=${iwyu_error} -Xiwyu --transitive_includes_only -Xiwyu --mapping_file=${PROJECT_SOURCE_DIR}/iwyu-mappings.imp)
    ENDIF(iwyu_executable)

    MESSAGE(STATUS "Configured iwyu (${iwyu_executable}) to run in mode \"${IWYU_MODE}\"")
  ELSEIF(IWYU_MODE STREQUAL "OFF")
    # Permitted value, we don't set iwyu_path_and_options.
  ELSE()
    MESSAGE(FATAL_ERROR
      "Invalid value for IWYU_MODE: \"${IWYU_MODE}\". Must be OFF, WARN or FAIL")
  ENDIF()

ENDIF(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
SET(CMAKE_C_STANDARD 11)

LIST(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

ADD_SUBDIRECTORY(apps)
ADD_SUBDIRECTORY(etc)
ADD_SUBDIRECTORY(protocols)
ADD_SUBDIRECTORY(third_party/protocols)
ADD_SUBDIRECTORY(share)
ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(tests)
ADD_SUBDIRECTORY(tool)

ADD_SUBDIRECTORY(doc)

# Adds submodules last, to permit checking on already-existing targets.
ADD_SUBDIRECTORY(submodules)
