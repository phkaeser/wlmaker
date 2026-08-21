# Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
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

cmake_minimum_required(VERSION 3.13)

include(WaylandProtocol)

pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client>=1.22.0)
pkg_get_variable(wayland_protocol_dir wayland-protocols pkgdatadir)

add_library(wlmdock_lib STATIC)
waylandprotocol_add(
  wlmdock_lib
  BASE_NAME ext-foreign-toplevel-list-v1
  PROTOCOL_FILE "${wayland_protocol_dir}/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml"
  SIDE client)

target_compile_definitions(
  wlmdock_lib PRIVATE "WLMAKER_SOURCE_DIR=\"${PROJECT_SOURCE_DIR}\"")
target_sources(
  wlmdock_lib
  PRIVATE
  launcher.c
  subcompositor.c
  tilebox.c
  toplevel.c)
target_include_directories(
  wlmdock_lib
  PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
  PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${PROJECT_SOURCE_DIR}/src"
  "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries(
  wlmdock_lib
  libbase
  wlmclient_lib
  wlmaker_lib
  wlmtoolkit_lib)
