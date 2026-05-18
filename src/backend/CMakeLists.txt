# Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
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
cmake_minimum_required(VERSION 3.13)
set(PUBLIC_HEADER_FILES
  backend.h
  output.h
  output_config.h
  output_manager.h)
add_library(backend STATIC
  backend.c
  output.c
  output_config.c
  output_manager.c)
target_include_directories(
  backend
  PUBLIC
  ${PROJECT_SOURCE_DIR}/include
  PRIVATE
  ${WAYLAND_SERVER_INCLUDE_DIRS}
  ${WLROOTS_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}/include
)
target_include_directories(
  backend
  PRIVATE
  ${PROJECT_SOURCE_DIR}/include/backend
)
set_target_properties(
  backend PROPERTIES
  VERSION 1.0
  PUBLIC_HEADER "${PUBLIC_HEADER_FILES}"
)
target_compile_options(
  backend PRIVATE
  ${WAYLAND_SERVER_CFLAGS}
  ${WAYLAND_SERVER_CFLAGS_OTHER}
)
target_link_libraries(
  backend
  PUBLIC
  libbase
  libbase_plist
  PkgConfig::WAYLAND_SERVER
  PRIVATE
  toolkit
  PkgConfig::WLROOTS)
if(iwyu_path_and_options)
  set_target_properties(
    backend PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
endif()
