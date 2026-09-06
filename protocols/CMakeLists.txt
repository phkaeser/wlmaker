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

waylandprotocol_add_library(
  wlmaker_icon_unstable_v1_server_protocol_lib
  PROTOCOL_FILE "wlmaker-icon-unstable-v1.xml"
  SIDE server)

waylandprotocol_add_library(
  wlmaker_icon_unstable_v1_client_protocol_lib
  PROTOCOL_FILE "wlmaker-icon-unstable-v1.xml"
  SIDE client)

waylandprotocol_add_library(
  ext_input_observation_v1_server_protocol_lib
  PROTOCOL_FILE "ext-input-observation-v1.xml"
  SIDE server)

waylandprotocol_add_library(
  ext_input_observation_v1_client_protocol_lib
  PROTOCOL_FILE "ext-input-observation-v1.xml"
  SIDE client)

add_library(wlmaker_protocols INTERFACE)
target_link_libraries(
  wlmaker_protocols
  INTERFACE
  wlmaker_icon_unstable_v1_server_protocol_lib
  ext_input_observation_v1_server_protocol_lib
  xdg_shell_server_protocol_lib)
