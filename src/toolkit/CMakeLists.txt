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

SET(PUBLIC_HEADER_FILES
  base.h
  bordered.h
  box.h
  buffer.h
  button.h
  container.h
  dock.h
  element.h
  fsm.h
  gfxbuf.h
  image.h
  input.h
  layer.h
  menu.h
  menu_item.h
  panel.h
  popup.h
  primitives.h
  rectangle.h
  resizebar.h
  resizebar_area.h
  root.h
  style.h
  surface.h
  test.h
  tile.h
  titlebar.h
  titlebar_button.h
  titlebar_title.h
  toolkit.h
  util.h
  window2.h
  workspace.h
)

ADD_LIBRARY(toolkit STATIC)
TARGET_SOURCES(toolkit PRIVATE
  base.c
  bordered.c
  box.c
  buffer.c
  button.c
  container.c
  dock.c
  element.c
  fsm.c
  gfxbuf.c
  image.c
  input.c
  layer.c
  menu.c
  menu_item.c
  panel.c
  popup.c
  primitives.c
  rectangle.c
  resizebar.c
  resizebar_area.c
  root.c
  style.c
  surface.c
  test.c
  tile.c
  titlebar.c
  titlebar_button.c
  titlebar_title.c
  util.c
  window2.c
  workspace.c
)
TARGET_INCLUDE_DIRECTORIES(
  toolkit PUBLIC
  ${WLROOTS_INCLUDE_DIRS}
  ${CAIRO_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_BINARY_DIR}/third_party/protocols
)
TARGET_INCLUDE_DIRECTORIES(
  toolkit PRIVATE
  ${PROJECT_SOURCE_DIR}/include/toolkit
)
SET_TARGET_PROPERTIES(
  toolkit PROPERTIES
  VERSION 1.0
  PUBLIC_HEADER "${PUBLIC_HEADER_FILES}"
)
ADD_DEPENDENCIES(
  toolkit
  protocol_headers)

TARGET_COMPILE_OPTIONS(
  toolkit PRIVATE
  ${WAYLAND_SERVER_CFLAGS}
  ${WAYLAND_SERVER_CFLAGS_OTHER}
)

TARGET_LINK_LIBRARIES(
  toolkit
  PUBLIC libbase PkgConfig::CAIRO PkgConfig::WLROOTS
  PRIVATE PkgConfig::WAYLAND_SERVER
)

IF(iwyu_path_and_options)
  SET_TARGET_PROPERTIES(
    toolkit PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
ENDIF(iwyu_path_and_options)
