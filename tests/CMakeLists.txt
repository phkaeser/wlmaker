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

ADD_EXECUTABLE(backend_test backend_test.c)
ADD_DEPENDENCIES(backend_test backend)
TARGET_LINK_LIBRARIES(backend_test backend)
ADD_TEST(NAME backend_test COMMAND backend_test)

ADD_EXECUTABLE(toolkit_test toolkit_test.c)
TARGET_LINK_LIBRARIES(toolkit_test toolkit)
TARGET_INCLUDE_DIRECTORIES(
  toolkit_test PRIVATE ${PROJECT_SOURCE_DIR}/include/toolkit)
TARGET_COMPILE_DEFINITIONS(
  toolkit_test PUBLIC TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data")
ADD_TEST(NAME toolkit_test COMMAND toolkit_test)

ADD_EXECUTABLE(wlmaker_test wlmaker_test.c)
ADD_DEPENDENCIES(wlmaker_test wlmaker_lib)
TARGET_INCLUDE_DIRECTORIES(
  wlmaker_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
TARGET_LINK_LIBRARIES(wlmaker_test PRIVATE wlmaker_lib)
TARGET_COMPILE_DEFINITIONS(
  wlmaker_test PUBLIC TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data")
ADD_TEST(NAME wlmaker_test COMMAND wlmaker_test)

ADD_EXECUTABLE(desktop_parser_test desktop_parser_test.c)
ADD_DEPENDENCIES(desktop_parser_test libbase desktop-parser)
TARGET_LINK_LIBRARIES(
  desktop_parser_test
  desktop-parser
  libbase)
TARGET_COMPILE_DEFINITIONS(
  desktop_parser_test PUBLIC TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data")

IF(iwyu_path_and_options)
  SET_TARGET_PROPERTIES(
    backend_test PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  SET_TARGET_PROPERTIES(
    toolkit_test PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  SET_TARGET_PROPERTIES(
    wlmaker_test PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
ENDIF(iwyu_path_and_options)
