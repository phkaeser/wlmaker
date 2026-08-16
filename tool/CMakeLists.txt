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

include(WaylandProtocol)

pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client>=1.22.0)
pkg_get_variable(wayland_protocol_dir wayland-protocols pkgdatadir)

add_library(libwlmtool STATIC item.c menu.c toplevel_list.c)
waylandprotocol_add(
  libwlmtool
  BASE_NAME ext-foreign-toplevel-list-v1
  PROTOCOL_FILE "${wayland_protocol_dir}/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml"
  SIDE client)

target_include_directories(
  libwlmtool
  PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${LIBXDGBASEDIR_INCLUDE_DIRS}"
  "${WAYLAND_CLIENT_INCLUDE_DIRS}"
)
target_link_libraries(
  libwlmtool
  libbase
  libbase_plist
  wlmclient_lib
  desktop-parser
  PkgConfig::LIBXDGBASEDIR
  PkgConfig::WAYLAND_CLIENT
)
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
