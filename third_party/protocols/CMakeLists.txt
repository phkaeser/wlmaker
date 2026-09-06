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

pkg_get_variable(protocol_dir wayland-protocols pkgdatadir)

waylandprotocol_add_library(
  wlr_layer_shell_unstable_v1_client_protocol_lib
  PROTOCOL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/wlr-layer-shell-unstable-v1.xml"
  SIDE client
  WLROOTS)

waylandprotocol_add_library(
  cursor_shape_v1_client_protocol_lib
  PROTOCOL_FILE "${protocol_dir}/staging/cursor-shape/cursor-shape-v1.xml"
  SIDE client)

waylandprotocol_add_library(
  tablet_v2_client_protocol_lib
  PROTOCOL_FILE "${protocol_dir}/stable/tablet/tablet-v2.xml"
  SIDE client)

waylandprotocol_add_library(
  xdg_decoration_client_protocol_lib
  BASE_NAME xdg-decoration
  PROTOCOL_FILE "${protocol_dir}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"
  SIDE client)

waylandprotocol_add_library(
  xdg_shell_client_protocol_lib
  PROTOCOL_FILE "${protocol_dir}/stable/xdg-shell/xdg-shell.xml"
  SIDE client)

waylandprotocol_add_library(
  xdg_shell_server_protocol_lib
  PROTOCOL_FILE "${protocol_dir}/stable/xdg-shell/xdg-shell.xml"
  SIDE server)

waylandprotocol_add_library(
  ext_foreign_toplevel_list_v1_client_protocol_lib
  PROTOCOL_FILE "${protocol_dir}/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml"
  SIDE client)
