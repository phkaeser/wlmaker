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

CMAKE_MINIMUM_REQUIRED(VERSION 3.13)

ADD_LIBRARY(primitives STATIC)
TARGET_SOURCES(
  primitives PRIVATE
  primitives.h primitives.c
  segment_display.h segment_display.c)
TARGET_INCLUDE_DIRECTORIES(
  primitives PRIVATE)
TARGET_LINK_LIBRARIES(
  primitives
  libbase)

ADD_EXECUTABLE(segment_display_test
  segment_display_test.c
  segment_display.c
  segment_display.h)
TARGET_COMPILE_DEFINITIONS(
  segment_display_test PRIVATE
  TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
TARGET_INCLUDE_DIRECTORIES(
  segment_display_test PRIVATE
  ${CAIRO_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(
  segment_display_test PRIVATE
  libbase
  PkgConfig::CAIRO)
ADD_TEST(
  NAME segment_display_test
  COMMAND segment_display_test)

IF(iwyu_path_and_options)
  SET_TARGET_PROPERTIES(
    primitives PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  SET_TARGET_PROPERTIES(
    segment_display_test PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
ENDIF(iwyu_path_and_options)
