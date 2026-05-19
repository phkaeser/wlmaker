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

pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED IMPORTED_TARGET wayland-protocols>=1.32)
pkg_check_modules(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server>=1.22.0)

pkg_get_variable(wayland_protocol_dir wayland-protocols pkgdatadir)

include(WaylandProtocol)

add_library(wlmaker_protocols INTERFACE)

waylandprotocol_add(
  wlmaker_protocols
  BASE_NAME wlmaker-icon-unstable-v1
  PROTOCOL_FILE "wlmaker-icon-unstable-v1.xml"
  SIDE server)

waylandprotocol_add(
  wlmaker_protocols
  BASE_NAME ext-input-observation-v1
  PROTOCOL_FILE "ext-input-observation-v1.xml"
  SIDE server)

# Needing XDG Shell, since the toplevel icon protocol refers to it.
waylandprotocol_add(
  wlmaker_protocols
  BASE_NAME xdg-shell
  PROTOCOL_FILE "${wayland_protocol_dir}/stable/xdg-shell/xdg-shell.xml"
  SIDE server)

target_compile_options(
  wlmaker_protocols INTERFACE
  "${WAYLAND_SERVER_CFLAGS}"
  "${WAYLAND_SERVER_CFLAGS_OTHER}"
  "${WAYLAND_PROTOCOLS_CFLAGS}"
  "${WAYLAND_PROTOCOLS_CFLAGS_OTHER}")
