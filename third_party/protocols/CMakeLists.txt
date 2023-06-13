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

FIND_PROGRAM(
  WAYLAND_SCANNER_EXECUTABLE
  NAMES wayland-scanner
  REQUIRED)
IF(NOT WAYLAND_SCANNER_EXECUTABLE)
  MESSAGE(FATAL_ERROR "'wayland-scanner' executable not found.")
ENDIF()

PKG_GET_VARIABLE(PROTOCOL_DIR wayland-protocols pkgdatadir)

ADD_CUSTOM_COMMAND(
  OUTPUT wlr-layer-shell-unstable-v1-protocol.h
  COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header ${CMAKE_CURRENT_SOURCE_DIR}/../protocols/wlr-layer-shell-unstable-v1.xml wlr-layer-shell-unstable-v1-protocol.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../protocols/wlr-layer-shell-unstable-v1.xml
  VERBATIM)

ADD_CUSTOM_COMMAND(
  OUTPUT xdg-shell-protocol.h
  COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header ${PROTOCOL_DIR}/stable/xdg-shell/xdg-shell.xml xdg-shell-protocol.h
  DEPENDS ${PROTOCOL_DIR}/stable/xdg-shell/xdg-shell.xml
  VERBATIM)

ADD_LIBRARY(
  protocol_headers
  OBJECT
  wlr-layer-shell-unstable-v1-protocol.h
  xdg-shell-protocol.h)
