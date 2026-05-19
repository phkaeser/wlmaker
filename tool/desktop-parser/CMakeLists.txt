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
add_library(desktop-parser STATIC desktop-parser.c)
target_link_libraries(
  desktop-parser
  PRIVATE
  libbase
  inih)
target_include_directories(
  desktop-parser
  PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/..")
set_target_properties(
  desktop-parser PROPERTIES
  VERSION 1.0
  PUBLIC_HEADER desktop-parser.h)
if(iwyu_path_and_options)
  set_target_properties(
    desktop-parser PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
endif()
