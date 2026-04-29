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

ADD_LIBRARY(libwlmtool STATIC gen_menu.c item.c menu.c)
TARGET_INCLUDE_DIRECTORIES(
  libwlmtool
  PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  PRIVATE
  ${LIBXDGBASEDIR_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(
  libwlmtool
  PRIVATE
  libbase_plist
  desktop-parser
  PkgConfig::LIBXDGBASEDIR)

ADD_EXECUTABLE(wlmtool wlmtool.c)
TARGET_LINK_LIBRARIES(
  wlmtool
  libwlmtool
  libbase)
TARGET_COMPILE_DEFINITIONS(
  wlmtool PRIVATE
  WLMAKER_VERSION_MAJOR="${WLMAKER_VERSION_MAJOR}"
  WLMAKER_VERSION_MINOR="${WLMAKER_VERSION_MINOR}"
  WLMAKER_VERSION_FULL="${WLMAKER_VERSION_FULL}")

INSTALL(
  TARGETS wlmtool
  DESTINATION ${CMAKE_INSTALL_BINDIR})

IF(iwyu_path_and_options)
  SET_TARGET_PROPERTIES(
    wlmtool PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  SET_TARGET_PROPERTIES(
    libwlmtool PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
ENDIF(iwyu_path_and_options)

ADD_SUBDIRECTORY(desktop-parser)
