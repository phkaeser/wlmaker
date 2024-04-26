# Copyright 2024 Google LLC
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
# cmake -DCMAKE_INSTALL_PREFIX:PATH=${HOME}/.local . -B build

CMAKE_MINIMUM_REQUIRED(VERSION 3.13)
PROJECT(wlmaker VERSION 0.1
  DESCRIPTION "Wayland Maker - Examples & Tools")

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

# If not found: Try 'pip3 install --user meson'
FIND_PROGRAM(MESON_EXECUTABLE NAMES meson REQUIRED)
FIND_PROGRAM(NINJA_EXECUTABLE NAMES ninja REQUIRED)

INCLUDE(ExternalProject)

# Extra packages required on Debian:
#
# libgtk-3-dev
# libgirepository1.0-dev
# valac
ExternalProject_Add(gtk_layer_shell
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gtk-layer-shell"
  INSTALL_DIR ${CMAKE_INSTALL_PREFIX}
  CONFIGURE_COMMAND ${MESON_EXECUTABLE} --prefix=<INSTALL_DIR> <BINARY_DIR> <SOURCE_DIR> -Dexamples=true
  BUILD_COMMAND ${NINJA_EXECUTABLE} -C <BINARY_DIR>
  INSTALL_COMMAND ${NINJA_EXECUTABLE} -C <BINARY_DIR> install
)
