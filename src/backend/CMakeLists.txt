# Copyright 2025 Google LLC
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

SET(PUBLIC_HEADER_FILES backend.h output.h output_manager.h)

ADD_LIBRARY(backend STATIC backend.c output.c output_manager.c)

TARGET_INCLUDE_DIRECTORIES(
  backend
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE
  ${WLROOTS_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
SET_TARGET_PROPERTIES(
  backend PROPERTIES
  VERSION 1.0
  PUBLIC_HEADER "${PUBLIC_HEADER_FILES}"
)

TARGET_COMPILE_OPTIONS(
  backend PRIVATE
  ${WAYLAND_CFLAGS}
  ${WAYLAND_CFLAGS_OTHER}
)

TARGET_LINK_LIBRARIES(
  backend
  PRIVATE
  libbase
  libbase_plist
  toolkit
  PkgConfig::WLROOTS)
