# Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
# Copyright 2026 Google LLC
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
cmake_minimum_required(VERSION 3.13)
set(public_header_files
  cursor.h
  keyboard.h
  manager.h)
add_library(wlminput_lib STATIC
  cursor.c
  keyboard.c
  pointer.c
  manager.c)
set_target_properties(
  wlminput_lib PROPERTIES
  VERSION 1.0
  PUBLIC_HEADER "${public_header_files}"
)
add_dependencies(
  wlminput_lib
  wlmtoolkit_lib)
target_compile_options(
  wlminput_lib PRIVATE
  "${WAYLAND_SERVER_CFLAGS}"
  "${WAYLAND_SERVER_CFLAGS_OTHER}"
)
target_include_directories(
  wlminput_lib
  PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${Libinput_INCLUDE_DIRS}"
  "${WAYLAND_SERVER_INCLUDE_DIRS}"
  "${XKBCOMMON_INCLUDE_DIRS}"
)
target_include_directories(
  wlminput_lib
  PRIVATE
  "${WLROOTS_INCLUDE_DIRS}"
)
target_link_libraries(
  wlminput_lib
  PUBLIC
  wlmtoolkit_lib
  PRIVATE
  Libinput::Libinput
  PkgConfig::WAYLAND_SERVER
  PkgConfig::WLROOTS
  PkgConfig::XKBCOMMON
  inih
  libbase
  libbase_plist)
if(iwyu_path_and_options)
  set_target_properties(
    wlminput_lib PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
endif()
