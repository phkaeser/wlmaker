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

ADD_LIBRARY(desktop-parser STATIC desktop-parser.c)
TARGET_LINK_LIBRARIES(desktop-parser PRIVATE libbase inih)
TARGET_INCLUDE_DIRECTORIES(
  desktop-parser
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/..)
SET_TARGET_PROPERTIES(
  desktop-parser PROPERTIES
  VERSION 1.0
  PUBLIC_HEADER desktop-parser.h)

IF(iwyu_path_and_options)
  SET_TARGET_PROPERTIES(
    desktop-parser PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
ENDIF(iwyu_path_and_options)
