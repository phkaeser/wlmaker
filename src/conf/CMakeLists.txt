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


SET(LEXER_OUT scanner.c)

FLEX_TARGET(SCANNER scanner.l scanner.c DEFINES_FILE scanner.h)



ADD_EXECUTABLE(conf_test conf_test.c scanner.c)
TARGET_LINK_LIBRARIES(conf_test base)
ADD_TEST(NAME conf_test COMMAND conf_test)
