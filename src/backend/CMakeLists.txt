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

SET(PUBLIC_HEADER_FILES
  backend.h
  output.h
  output_config.h
  output_manager.h)

ADD_LIBRARY(backend STATIC
  backend.c
  output.c
  output_config.c
  output_manager.c)

TARGET_INCLUDE_DIRECTORIES(
  backend
  PUBLIC
  ${PROJECT_SOURCE_DIR}/include
  PRIVATE
  ${WAYLAND_SERVER_INCLUDE_DIRS}
  ${WLROOTS_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}/include
)
TARGET_INCLUDE_DIRECTORIES(
  backend
  PRIVATE
  ${PROJECT_SOURCE_DIR}/include/backend
)

SET_TARGET_PROPERTIES(
  backend PROPERTIES
  VERSION 1.0
  PUBLIC_HEADER "${PUBLIC_HEADER_FILES}"
)

TARGET_COMPILE_OPTIONS(
  backend PRIVATE
  ${WAYLAND_SERVER_CFLAGS}
  ${WAYLAND_SERVER_CFLAGS_OTHER}
)

TARGET_LINK_LIBRARIES(
  backend
  PUBLIC
  libbase
  libbase_plist
  PkgConfig::WAYLAND_SERVER
  PRIVATE
  toolkit
  PkgConfig::WLROOTS)

IF(iwyu_path_and_options)
  SET_TARGET_PROPERTIES(
    backend PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
ENDIF(iwyu_path_and_options)
