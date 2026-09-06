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

pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client>=1.22.0)

add_library(wlmclient_lib STATIC)

set(public_header_files
  dblbuf.h
  icon.h
  layer_surface.h
  wlclient.h
  xdg_toplevel.h
)

set_target_properties(
  wlmclient_lib PROPERTIES
  VERSION 1.0
  PUBLIC_HEADER "${public_header_files}")

target_sources(
  wlmclient_lib PRIVATE
  dblbuf.c
  icon.c
  layer_surface.c
  wlclient.c
  xdg_toplevel.c
)

target_include_directories(
  wlmclient_lib
  PUBLIC
  "${PROJECT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_BINARY_DIR}"
  PRIVATE
  "${WAYLAND_CLIENT_INCLUDE_DIRS}"
  "${XKBCOMMON_INCLUDE_DIRS}")

target_link_libraries(
  wlmclient_lib
  libbase
  cursor_shape_v1_client_protocol_lib
  ext_input_observation_v1_client_protocol_lib
  tablet_v2_client_protocol_lib
  wlmaker_icon_unstable_v1_client_protocol_lib
  wlr_layer_shell_unstable_v1_client_protocol_lib
  xdg_decoration_client_protocol_lib
  xdg_shell_client_protocol_lib
  PkgConfig::WAYLAND_CLIENT
  PkgConfig::XKBCOMMON)

include(CheckSymbolExists)
check_symbol_exists(signalfd "sys/signalfd.h" HAVE_SIGNALFD)
if(NOT HAVE_SIGNALFD)
  pkg_check_modules(EPOLL REQUIRED IMPORTED_TARGET epoll-shim)
  target_link_libraries(
    wlmclient_lib
    PkgConfig::EPOLL)
endif()

if(iwyu_path_and_options)
  set_target_properties(
    wlmclient_lib PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
endif()
