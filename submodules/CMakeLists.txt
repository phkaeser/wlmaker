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

CMAKE_MINIMUM_REQUIRED(VERSION 3.13)

ADD_SUBDIRECTORY(libbase)

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
ADD_LIBRARY(inih STATIC ${CMAKE_CURRENT_SOURCE_DIR}/inih/ini.c)
TARGET_COMPILE_DEFINITIONS(
  inih PUBLIC
  INI_MAX_LINE=65536
  INI_USE_STACK=0
  INI_ALLOW_REALLOC=1)
TARGET_INCLUDE_DIRECTORIES(
  inih
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/inih)
SET_TARGET_PROPERTIES(
  inih PROPERTIES
  VERSION 62
  PUBLIC_HEADER ini.h)
