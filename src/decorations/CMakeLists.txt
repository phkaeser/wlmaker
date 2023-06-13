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

ADD_LIBRARY(decorations
  element.c
  margin.c
  resizebar.c
  titlebar.c
  window_decorations.c
  element.h
  margin.h
  resizebar.h
  titlebar.h
  window_decorations.h)

TARGET_COMPILE_OPTIONS(
  decorations PRIVATE
  ${WAYLAND_CFLAGS}
  ${WAYLAND_CFLAGS_OTHER}
)

TARGET_INCLUDE_DIRECTORIES(
  decorations PRIVATE
  ${CAIRO_INCLUDE_DIRS}
  # TODO(kaeser@gubbe.ch): These should not be required.
  ${PROJECT_BINARY_DIR}/protocols
  ${PIXMAN_INCLUDE_DIRS}
)

TARGET_LINK_LIBRARIES(
  decorations base toolkit)
