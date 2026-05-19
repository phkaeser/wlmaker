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

cmake_minimum_required(VERSION 3.13)

find_program(
  WAYLAND_SCANNER_EXECUTABLE
  NAMES wayland-scanner
  REQUIRED)
if(NOT WAYLAND_SCANNER_EXECUTABLE)
  message(FATAL_ERROR "'wayland-scanner' executable not found.")
endif()

pkg_get_variable(protocol_dir wayland-protocols pkgdatadir)

add_custom_command(
  OUTPUT wlr-layer-shell-unstable-v1-protocol.h
  COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" client-header "${CMAKE_CURRENT_SOURCE_DIR}/../protocols/wlr-layer-shell-unstable-v1.xml" "wlr-layer-shell-unstable-v1-protocol.h"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../protocols/wlr-layer-shell-unstable-v1.xml"
  VERBATIM)

add_custom_command(
  OUTPUT xdg-shell-protocol.h
  COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" client-header "${protocol_dir}/stable/xdg-shell/xdg-shell.xml" "xdg-shell-protocol.h"
  DEPENDS "${protocol_dir}/stable/xdg-shell/xdg-shell.xml"
  VERBATIM)

add_library(
  protocol_headers
  OBJECT
  wlr-layer-shell-unstable-v1-protocol.h
  xdg-shell-protocol.h)
set_target_properties(
  protocol_headers PROPERTIES
  LINKER_LANGUAGE C)
