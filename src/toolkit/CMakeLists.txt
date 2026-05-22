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
set(public_header_files
  base.h
  bordered.h
  box.h
  buffer.h
  button.h
  container.h
  desktop.h
  dock.h
  element.h
  fsm.h
  gfxbuf.h
  image.h
  input.h
  layer.h
  menu.h
  menu_item.h
  output_tracker.h
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
  window.h
  workspace.h
)
add_library(toolkit STATIC)
target_sources(toolkit PRIVATE
  base.c
  bordered.c
  box.c
  buffer.c
  button.c
  container.c
  desktop.c
  dock.c
  element.c
  fsm.c
  gfxbuf.c
  image.c
  input.c
  layer.c
  menu.c
  menu_item.c
  output_tracker.c
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
  window.c
  workspace.c
)
target_include_directories(
  toolkit PUBLIC
  "${WLROOTS_INCLUDE_DIRS}"
  "${CAIRO_INCLUDE_DIRS}"
  "${PROJECT_SOURCE_DIR}/include"
  "${PROJECT_BINARY_DIR}/third_party/protocols"
)
target_include_directories(
  toolkit PRIVATE
  "${PROJECT_SOURCE_DIR}/include/toolkit"
)
set_target_properties(
  toolkit PROPERTIES
  VERSION 1.0
  PUBLIC_HEADER "${public_header_files}"
)
add_dependencies(
  toolkit
  protocol_headers)
target_compile_options(
  toolkit PRIVATE
  "${WAYLAND_SERVER_CFLAGS}"
  "${WAYLAND_SERVER_CFLAGS_OTHER}"
)
target_link_libraries(
  toolkit
  PUBLIC libbase PkgConfig::CAIRO PkgConfig::WLROOTS
  PRIVATE PkgConfig::WAYLAND_SERVER libbase_plist
)
if(iwyu_path_and_options)
  set_target_properties(
    toolkit PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
endif()
