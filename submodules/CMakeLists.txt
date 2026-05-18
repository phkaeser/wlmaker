# Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
# Copyright 2026 Google LLC
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
add_subdirectory(libbase)

# Falling back to a provided version of `inih`: The distribution's appears to
# use the default settings and definitions, among which there is a maximum line
# length of 200 characters. That doesn't work for `.desktop` files.
#
# Obtained from: https://github.com/benhoyt/inih
#
# (C) Copyright (c) 2009, Ben Hoyt -- under "New BSD license".
#
# For wlmaker, the library is configured to permit much longer lines, and use
# the heap for dynamic sizing.
add_library(inih STATIC ${CMAKE_CURRENT_SOURCE_DIR}/inih/ini.c)
target_compile_definitions(
  inih PUBLIC
  INI_MAX_LINE=65536
  INI_USE_STACK=0
  INI_ALLOW_REALLOC=1)
target_include_directories(
  inih
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/inih)
set_target_properties(
  inih PROPERTIES
  VERSION 62
  PUBLIC_HEADER ini.h)
