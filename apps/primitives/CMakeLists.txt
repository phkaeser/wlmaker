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
add_library(primitives STATIC)
target_sources(
  primitives PRIVATE
  primitives.h primitives.c
  segment_display.h segment_display.c)
target_include_directories(
  primitives PRIVATE)
target_link_libraries(
  primitives
  libbase)
add_executable(segment_display_test
  segment_display_test.c
  segment_display.c
  segment_display.h)
target_compile_definitions(
  segment_display_test PRIVATE
  TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(
  segment_display_test PRIVATE
  ${CAIRO_INCLUDE_DIRS})
target_link_libraries(
  segment_display_test PRIVATE
  libbase
  PkgConfig::CAIRO)
add_test(
  NAME segment_display_test
  COMMAND segment_display_test)
if(iwyu_path_and_options)
  set_target_properties(
    primitives PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  set_target_properties(
    segment_display_test PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
endif()
