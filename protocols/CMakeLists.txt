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

INCLUDE(WaylandProtocol)

SET(SOURCES)
WaylandProtocol_ADD(
  SOURCES
  BASE_NAME wlmaker-icon-unstable-v1
  PROTOCOL_FILE wlmaker-icon-unstable-v1.xml
  SIDE server)

PKG_GET_VARIABLE(WAYLAND_PROTOCOLDIR wayland-protocols pkgdatadir)
# Needing XDG Shell, since the toplevel icon protocol refers to it.
WaylandProtocol_ADD(
  SOURCES
  BASE_NAME xdg-shell
  PROTOCOL_FILE "${WAYLAND_PROTOCOLDIR}/stable/xdg-shell/xdg-shell.xml"
  SIDE server)

ADD_LIBRARY(wlmaker_protocols STATIC)
TARGET_SOURCES(
  wlmaker_protocols PRIVATE
  ${SOURCES})

TARGET_COMPILE_OPTIONS(
  wlmaker_protocols PRIVATE
  ${WAYLAND_CFLAGS}
  ${WAYLAND_CFLAGS_OTHER})
