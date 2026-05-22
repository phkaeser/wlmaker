# Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
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

cmake_minimum_required(VERSION 3.13)

# Conditional build for subdirectories: only build submodules/subdirectories
# if cmake is run directly in this examples directory, rather than from root.
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)

  # If not found: Try 'pip3 install --user meson'
  find_program(MESON_EXECUTABLE NAMES meson REQUIRED)
  find_program(NINJA_EXECUTABLE NAMES ninja REQUIRED)

  include(ExternalProject)

  # Extra packages required on Debian:
  #
  # libgtk-3-dev
  # libgirepository1.0-dev
  # valac
  #
  # Hm. Could also have installed gtk-layer-shell-examples ?
  # Also, there is https://github.com/wmww/gtk4-layer-shell.
  ExternalProject_Add(gtk_layer_shell
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gtk-layer-shell"
    INSTALL_DIR "${CMAKE_INSTALL_PREFIX}"
    CONFIGURE_COMMAND "${MESON_EXECUTABLE}" --prefix=<INSTALL_DIR> <BINARY_DIR> <SOURCE_DIR> -Dexamples=true
    BUILD_COMMAND "${NINJA_EXECUTABLE}" -C <BINARY_DIR>
    INSTALL_COMMAND "${NINJA_EXECUTABLE}" -C <BINARY_DIR> install
  )
endif()

add_executable(
  example_toplevel
  example_toplevel.c)
target_include_directories(example_toplevel PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(
  example_toplevel
  m
  libwlclient)

add_executable(
  example_layer_surface
  example_layer_surface.c)
target_include_directories(example_layer_surface PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(
  example_layer_surface
  m
  libwlclient)

if(iwyu_path_and_options)
  set_target_properties(
    example_toplevel PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  set_target_properties(
    example_layer_surface PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
endif()
