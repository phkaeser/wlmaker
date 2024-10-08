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
  bordered.h
  box.h
  buffer.h
  button.h
  container.h
  content.h
  dock.h
  element.h
  env.h
  fsm.h
  gfxbuf.h
  image.h
  input.h
  layer.h
  lock.h
  panel.h
  popup.h
  primitives.h
  rectangle.h
  resizebar.h
  resizebar_area.h
  root.h
  style.h
  surface.h
  tile.h
  titlebar.h
  titlebar_button.h
  titlebar_title.h
  toolkit.h
  util.h
  window.h
  workspace.h
)

ADD_LIBRARY(toolkit STATIC)
TARGET_SOURCES(toolkit PRIVATE
  bordered.c
  box.c
  buffer.c
  button.c
  container.c
  content.c
  dock.c
  element.c
  env.c
  fsm.c
  gfxbuf.c
  image.c
  layer.c
  lock.c
  panel.c
  popup.c
  primitives.c
  rectangle.c
  resizebar.c
  resizebar_area.c
  root.c
  style.c
  surface.c
  tile.c
  titlebar.c
  titlebar_button.c
  titlebar_title.c
  util.c
  window.c
  workspace.c
)
TARGET_INCLUDE_DIRECTORIES(
  toolkit PRIVATE
  ${WLROOTS_INCLUDE_DIRS}
)
SET_TARGET_PROPERTIES(
  toolkit PROPERTIES
  VERSION 1.0
  PUBLIC_HEADER "${PUBLIC_HEADER_FILES}"
)

TARGET_COMPILE_OPTIONS(
  toolkit PRIVATE
  ${WAYLAND_CFLAGS}
  ${WAYLAND_CFLAGS_OTHER}
)

TARGET_INCLUDE_DIRECTORIES(
  toolkit PRIVATE
  ${CAIRO_INCLUDE_DIRS}
)

TARGET_LINK_LIBRARIES(
  toolkit
  base
  PkgConfig::CAIRO
  PkgConfig::WAYLAND
  PkgConfig::WLROOTS
)

ADD_EXECUTABLE(toolkit_test toolkit_test.c)
TARGET_LINK_LIBRARIES(toolkit_test toolkit)
TARGET_COMPILE_DEFINITIONS(
  toolkit_test PUBLIC TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/testdata")

ADD_TEST(NAME toolkit_test COMMAND toolkit_test)
