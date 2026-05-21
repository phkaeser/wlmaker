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

include(WaylandProtocol)

pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client>=1.22.0)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED IMPORTED_TARGET wayland-protocols>=1.32)

pkg_get_variable(wayland_protocol_dir wayland-protocols pkgdatadir)

add_library(libwlclient STATIC)

set(public_header_files
  dblbuf.h
  icon.h
  libwlclient.h
  xdg_toplevel.h
)

set_target_properties(
  libwlclient PROPERTIES
  VERSION 1.0
  PUBLIC_HEADER "${public_header_files}")

set(sources
  client.c
  dblbuf.c
  icon.c
  xdg_toplevel.c
)

waylandprotocol_add(
  libwlclient
  BASE_NAME xdg-decoration
  PROTOCOL_FILE "${wayland_protocol_dir}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"
  SIDE client)

waylandprotocol_add(
  libwlclient
  BASE_NAME xdg-shell
  PROTOCOL_FILE "${wayland_protocol_dir}/stable/xdg-shell/xdg-shell.xml"
  SIDE client)

waylandprotocol_add(
  libwlclient
  BASE_NAME wlmaker-icon-unstable-v1
  PROTOCOL_FILE "${PROJECT_SOURCE_DIR}/protocols/wlmaker-icon-unstable-v1.xml"
  SIDE client)

waylandprotocol_add(
  libwlclient
  BASE_NAME ext-input-observation-v1
  PROTOCOL_FILE "${PROJECT_SOURCE_DIR}/protocols/ext-input-observation-v1.xml"
  SIDE client)

target_sources(libwlclient PRIVATE "${sources}")

target_include_directories(
  libwlclient
  PUBLIC
  "${PROJECT_SOURCE_DIR}/src"
  PRIVATE
  "${WAYLAND_CLIENT_INCLUDE_DIRS}"
  "${XKBCOMMON_INCLUDE_DIRS}"
  "${CMAKE_CURRENT_BINARY_DIR}")

target_link_libraries(
  libwlclient
  libbase
  PkgConfig::WAYLAND_CLIENT
  PkgConfig::XKBCOMMON)

include(CheckSymbolExists)
check_symbol_exists(signalfd "sys/signalfd.h" HAVE_SIGNALFD)
if(NOT HAVE_SIGNALFD)
  pkg_check_modules(EPOLL REQUIRED IMPORTED_TARGET epoll-shim)
  target_link_libraries(
    libwlclient
    PkgConfig::EPOLL)
endif()

if(iwyu_path_and_options)
  set_target_properties(
    libwlclient PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
endif()
