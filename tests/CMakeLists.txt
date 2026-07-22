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

add_executable(backend_test backend_test.c)
add_dependencies(backend_test backend)
target_link_libraries(backend_test backend)
add_test(NAME backend_test COMMAND backend_test)

add_executable(input_test input_test.c)
add_dependencies(input_test libbase input)
target_link_libraries(input_test libbase input)
target_compile_definitions(
  input_test PUBLIC "TEST_DATA_DIR=\"${PROJECT_SOURCE_DIR}/tests/data\"")
add_test(NAME input_test COMMAND input_test)

add_executable(toolkit_test toolkit_test.c)
target_link_libraries(toolkit_test toolkit)
target_include_directories(
  toolkit_test PRIVATE "${PROJECT_SOURCE_DIR}/include/toolkit")
target_compile_definitions(
  toolkit_test PUBLIC "TEST_DATA_DIR=\"${PROJECT_SOURCE_DIR}/tests/data\"")
add_test(NAME toolkit_test COMMAND toolkit_test)

add_executable(wlmaker_test wlmaker_test.c)
add_dependencies(wlmaker_test wlmaker_lib)
target_include_directories(
  wlmaker_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(wlmaker_test PRIVATE wlmaker_lib)
target_compile_definitions(
  wlmaker_test PUBLIC "TEST_DATA_DIR=\"${PROJECT_SOURCE_DIR}/tests/data\"")
add_test(NAME wlmaker_test COMMAND wlmaker_test)

add_executable(util_test util_test.c)
add_dependencies(util_test wlm_util_lib)
target_include_directories(
  util_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(util_test PRIVATE libbase wlm_util_lib)
target_compile_definitions(
  util_test PUBLIC "TEST_DATA_DIR=\"${PROJECT_SOURCE_DIR}/tests/data\"")
add_test(NAME util_test COMMAND util_test)

add_executable(desktop_parser_test desktop_parser_test.c)
add_dependencies(desktop_parser_test libbase desktop-parser)
target_link_libraries(
  desktop_parser_test
  desktop-parser
  libbase)
target_compile_definitions(
  desktop_parser_test PUBLIC "TEST_DATA_DIR=\"${PROJECT_SOURCE_DIR}/tests/data\"")

add_executable(wlmtool_test wlmtool_test.c)
add_dependencies(wlmtool_test libbase libwlmtool)
target_link_libraries(wlmtool_test libbase libwlmtool)
target_compile_definitions(
  wlmtool_test PUBLIC "TEST_DATA_DIR=\"${PROJECT_SOURCE_DIR}/tests/data\"")

if(iwyu_path_and_options)
  set_target_properties(
    backend_test PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  set_target_properties(
    input_test PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  set_target_properties(
    toolkit_test PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  set_target_properties(
    wlmaker_test PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  set_target_properties(
    util_test PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  set_target_properties(
    desktop_parser_test PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  set_target_properties(
    wlmtool_test PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
endif()
