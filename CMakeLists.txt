# Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
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
cmake_minimum_required(VERSION 3.14)
project(
  wlmaker
  VERSION "0.8"
  DESCRIPTION "Wayland Maker - A Wayland compositor inspired by Window Maker"
  HOMEPAGE_URL "https://github.com/phkaeser/wlmaker"
  LANGUAGES C)

# Requires out-of-source builds.
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" location_path)
if(EXISTS "${location_path}")
  message(
    FATAL_ERROR
    "You cannot build into a source directory (containing a CMakeLists.txt file).\n"
    "Please make a build subdirectory, for example:\n"
    "cmake -B build\n"
    "(cd build && make)")
endif()
set(WLMAKER_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(WLMAKER_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(WLMAKER_VERSION_FULL "${PROJECT_VERSION}")
# PROJECT version check: When on a git repository, we verify that the PROJECT
# version is equal-or-larger than the git version.
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E chdir "${CMAKE_SOURCE_DIR}"
    git describe --always --tags --dirty
    OUTPUT_VARIABLE git_version
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE git_result)
  if(git_result EQUAL 0)
    string(REGEX MATCH "^v([0-9]+)\\.([0-9]+)" dummy_var "${git_version}")
    if(NOT CMAKE_MATCH_COUNT EQUAL 2)
      message(FATAL_ERROR "Not a v<MAJOR>.<MINOR> version \"${git_version}\"")
    endif()
    if("${PROJECT_VERSION_MAJOR}" LESS "${CMAKE_MATCH_1}" OR
        "${PROJECT_VERSION_MINOR}" LESS "${CMAKE_MATCH_2}")
      message(FATAL_ERROR "Git ${CMAKE_MATCH_1}.${CMAKE_MATCH_2} >= "
        "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
    endif()
    set(WLMAKER_VERSION_MAJOR "${CMAKE_MATCH_1}")
    set(WLMAKER_VERSION_MINOR "${CMAKE_MATCH_2}")
    set(WLMAKER_VERSION_FULL "${git_version}")
  endif()
endif()

# Defaults to /usr/local/lib for installation.
include(GNUInstallDirs)
if(CMAKE_INSTALL_PREFIX STREQUAL "/usr")
  # For targets /usr, forces configurations into XDG config default path.
  set(WLMAKER_INSTALL_CONFIG_DIR "/etc/xdg/wlmaker")
else()
  set(WLMAKER_INSTALL_CONFIG_DIR "${CMAKE_INSTALL_SYSCONFDIR}/xdg/wlmaker")
endif()
include(CTest)
find_package(PkgConfig REQUIRED)
find_package(Libinput)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo>=1.16.0)
pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon>=1.5.0)
pkg_check_modules(LIBXDGBASEDIR REQUIRED IMPORTED_TARGET libxdg-basedir>=1.2)

# https://github.com/ianlancetaylor/libbacktrace.
# The REQUIRED arg is available only from cmake 3.28.
find_library(LIBBACKTRACE backtrace)
if(LIBBACKTRACE)
  message(STATUS "Found libbacktrace: ${LIBBACKTRACE}")
else()
  message(STATUS "libbacktrace not found. Will compile without.")
endif()

# Configuration. Remove CMakeCache.txt to rerun...
option(config_DEBUG "Include debugging information" ON)
option(config_OPTIM "Optimizations" OFF)
option(config_DOXYGEN_CRITICAL "Whether to fail on doxygen warnings" OFF)
option(config_WERROR "Make all compiler warnings into errors." OFF)
set(IWYU_MODE OFF CACHE STRING "Whether to run `iwyu`. OFF, WARN or FAIL.")
set_property(CACHE IWYU_MODE PROPERTY STRINGS "OFF" "WARN" "FAIL")

# Toplevel compile options, for GCC and clang.
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wall -Wextra)
  if(config_WERROR)
    add_compile_options(-Werror)
  endif()
  if(config_DEBUG)
    add_compile_options(-ggdb -DDEBUG)
  endif()
  if(config_OPTIM)
    add_compile_options(-O2)
  else()
    add_compile_options(-O0)
  endif()

  # CMake provides absolute paths to GCC, hence the __FILE__ macro includes the
  # full path. This option resets it to a path relative to project source.
  add_compile_options("-fmacro-prefix-map=${PROJECT_SOURCE_DIR}=.")

  # Run iwyu when available.
  if((IWYU_MODE STREQUAL "WARN") OR (IWYU_MODE STREQUAL "FAIL"))
    find_program(iwyu_executable NAMES include-what-you-use iwyu REQUIRED)
    if(IWYU_MODE STREQUAL "FAIL")
      set(iwyu_error "1")
    else()
      set(iwyu_error "0")
    endif()
    if(iwyu_executable)
      set(iwyu_path_and_options
        ${iwyu_executable}
        -Xiwyu --error=${iwyu_error} -Xiwyu --transitive_includes_only -Xiwyu --mapping_file=${PROJECT_SOURCE_DIR}/iwyu-mappings.imp)
    endif()
    message(STATUS "Configured iwyu (${iwyu_executable}) to run in mode \"${IWYU_MODE}\"")
  elseif(IWYU_MODE STREQUAL "OFF")
    # Permitted value, we don't set iwyu_path_and_options.
  else()
    message(FATAL_ERROR
      "Invalid value for IWYU_MODE: \"${IWYU_MODE}\". Must be OFF, WARN or FAIL")
  endif()
endif()
set(CMAKE_C_STANDARD 11)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
add_subdirectory(apps)
add_subdirectory(examples)
add_subdirectory(etc)
add_subdirectory(protocols)
add_subdirectory(third_party/protocols)
add_subdirectory(share)
add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(tool)
add_subdirectory(doc)

# Adds submodules last, to permit checking on already-existing targets.
add_subdirectory(submodules)
