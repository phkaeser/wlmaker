# Copyright 2024 Google LLC
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

CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/wlmaker.desktop.in
  ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.desktop
  @ONLY)
INSTALL(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.desktop
  TYPE DATA)

CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/wlmclock.desktop.in
  ${CMAKE_CURRENT_BINARY_DIR}/wlmclock.desktop
  @ONLY)
INSTALL(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/wlmclock.desktop
  TYPE DATA)
