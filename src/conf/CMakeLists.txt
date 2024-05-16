# Copyright 2024 Google LLC
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

FIND_PACKAGE(FLEX 2.6 REQUIRED)
FLEX_TARGET(
  scanner
  scanner.l
  ${CMAKE_CURRENT_BINARY_DIR}/scanner.c
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/scanner.h)

FIND_PACKAGE(BISON 3.0 REQUIRED)
BISON_TARGET(
  parser
  parser.y
  ${CMAKE_CURRENT_BINARY_DIR}/parser.c
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.h)

ADD_FLEX_BISON_DEPENDENCY(scanner parser)


ADD_EXECUTABLE(
  conf_test
  conf_test.c
  ${CMAKE_CURRENT_BINARY_DIR}/scanner.c
  ${CMAKE_CURRENT_BINARY_DIR}/parser.c)
TARGET_LINK_LIBRARIES(conf_test base)
TARGET_INCLUDE_DIRECTORIES(conf_test PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/)
ADD_TEST(NAME conf_test COMMAND conf_test)
