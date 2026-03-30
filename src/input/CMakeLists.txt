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

CMAKE_MINIMUM_REQUIRED(VERSION 3.13)

SET(PUBLIC_HEADER_FILES
  cursor.h
  keyboard.h
  manager.h)

ADD_LIBRARY(input STATIC
  cursor.c
  keyboard.c
  pointer.c
  manager.c)

SET_TARGET_PROPERTIES(
  input PROPERTIES
  VERSION 1.0
  PUBLIC_HEADER "${PUBLIC_HEADER_FILES}"
)

TARGET_COMPILE_OPTIONS(
  input PRIVATE
  ${WAYLAND_SERVER_CFLAGS}
  ${WAYLAND_SERVER_CFLAGS_OTHER}
)

TARGET_INCLUDE_DIRECTORIES(
  input
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${Libinput_INCLUDE_DIRS}
  ${WAYLAND_SERVER_INCLUDE_DIRS}
  ${XKBCOMMON_INCLUDE_DIRS}
)
TARGET_INCLUDE_DIRECTORIES(
  input
  PRIVATE
  ${WLROOTS_INCLUDE_DIRS}
)
TARGET_LINK_LIBRARIES(
  input
  PUBLIC
  PRIVATE
  Libinput::Libinput
  PkgConfig::WAYLAND_SERVER
  PkgConfig::WLROOTS
  PkgConfig::XKBCOMMON
  inih
  libbase
  libbase_plist
  toolkit)

IF(iwyu_path_and_options)
  SET_TARGET_PROPERTIES(
    input PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
ENDIF(iwyu_path_and_options)
