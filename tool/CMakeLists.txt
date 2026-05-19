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
add_library(libwlmtool STATIC item.c menu.c)
target_include_directories(
  libwlmtool
  PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  PRIVATE
  "${LIBXDGBASEDIR_INCLUDE_DIRS}")
target_link_libraries(
  libwlmtool
  PRIVATE
  libbase
  libbase_plist
  desktop-parser
  PkgConfig::LIBXDGBASEDIR)
add_executable(
  wlmtool
  wlmtool.c)
target_link_libraries(
  wlmtool
  libwlmtool
  libbase)
target_compile_definitions(
  wlmtool PRIVATE
  "WLMAKER_VERSION_MAJOR=\"${WLMAKER_VERSION_MAJOR}\""
  "WLMAKER_VERSION_MINOR=\"${WLMAKER_VERSION_MINOR}\""
  "WLMAKER_VERSION_FULL=\"${WLMAKER_VERSION_FULL}\"")
install(
  TARGETS wlmtool
  DESTINATION "${CMAKE_INSTALL_BINDIR}")
if(iwyu_path_and_options)
  set_target_properties(
    wlmtool PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  set_target_properties(
    libwlmtool PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
endif()
add_subdirectory(desktop-parser)
