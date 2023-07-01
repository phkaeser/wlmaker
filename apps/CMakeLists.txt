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

ADD_LIBRARY(wlclient STATIC)
SET(SOURCES wlclient.c wlclient.h)
WaylandProtocol_ADD(
  SOURCES
  BASE_NAME xdg-shell
  PROTOCOL_FILE "${WAYLAND_PROTOCOL_DIR}/stable/xdg-shell/xdg-shell.xml"
  SIDE client)
WaylandProtocol_ADD(
  SOURCES
  BASE_NAME wlmaker-icon-unstable-v1
  PROTOCOL_FILE "${PROJECT_SOURCE_DIR}/protocols/wlmaker-icon-unstable-v1.xml"
  SIDE client)
TARGET_SOURCES(wlclient PRIVATE ${SOURCES})
TARGET_INCLUDE_DIRECTORIES(
  wlclient PRIVATE
  ${WAYLAND_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR})
TARGET_LINK_LIBRARIES(
  wlclient
  base
  PkgConfig::WAYLAND)

ADD_EXECUTABLE(wlmclock wlmclock.c)
TARGET_LINK_LIBRARIES(wlmclock wlclient)
