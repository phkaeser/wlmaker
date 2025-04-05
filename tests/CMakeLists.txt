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

ADD_EXECUTABLE(toolkit_test toolkit_test.c)
TARGET_LINK_LIBRARIES(toolkit_test toolkit)
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
