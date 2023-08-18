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
PKG_CHECK_MODULES(LIBDRM REQUIRED IMPORTED_TARGET libdrm>=2.4.114)
PKG_CHECK_MODULES(PIXMAN REQUIRED IMPORTED_TARGET pixman-1>=0.42.2)
PKG_CHECK_MODULES(
  WAYLAND REQUIRED IMPORTED_TARGET
  wayland-client>=1.22.90
  wayland-protocols>=1.31
  wayland-server>=1.22.90)
PKG_GET_VARIABLE(WAYLAND_PROTOCOL_DIR wayland-protocols pkgdatadir)
PKG_CHECK_MODULES(WLROOTS REQUIRED IMPORTED_TARGET wlroots>=0.17)
PKG_CHECK_MODULES(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon>=1.0.3)  # 1.4.1)

# Configuration. Remove CMakeCache.txt to rerun...
OPTION(config_DEBUG "Include debugging information" ON)
OPTION(config_OPTIM "Optimizations" OFF)
OPTION(config_DOXYGEN_CRITICAL "Whether to fail on doxygen warnings" OFF)

# Toplevel compile options, for GCC.
IF(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  ADD_COMPILE_OPTIONS(-Wall -Wextra -Werror)

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

ENDIF(CMAKE_C_COMPILER_ID STREQUAL "GNU")
SET(CMAKE_C_STANDARD 11)

LIST(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

ADD_SUBDIRECTORY(apps)
ADD_SUBDIRECTORY(doc)
ADD_SUBDIRECTORY(icons)
ADD_SUBDIRECTORY(protocols)
ADD_SUBDIRECTORY(third_party/protocols)
ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(src/decorations)
ADD_SUBDIRECTORY(src/toolkit)

# Adds submodules last, to permit checking on already-existing targets.
ADD_SUBDIRECTORY(submodules/libbase)
