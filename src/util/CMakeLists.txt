# Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
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

add_library(wlm_util_lib STATIC)
target_sources(
  wlm_util_lib
  PRIVATE
  backtrace.c
  files.c
  subprocess_monitor.c
)

target_include_directories(
  wlm_util_lib
  PRIVATE
  "${LIBXDGBASEDIR_INCLUDE_DIRS}"
  # TODO(kaeser@gubbe.ch): Move that to client-side compatible implementation.
  "${WAYLAND_SERVER_INCLUDE_DIRS}"
)
target_link_libraries(
  wlm_util_lib
  PRIVATE
  $<$<BOOL:${LIBBACKTRACE}>:${LIBBACKTRACE}>
  PkgConfig::LIBXDGBASEDIR
  PkgConfig::WAYLAND_SERVER
  libbase
)

if(LIBBACKTRACE)
  target_compile_definitions(wlm_util_lib PRIVATE WLMAKER_HAVE_LIBBACKTRACE)
endif()

if(iwyu_path_and_options)
  set_target_properties(
    wlm_util_lib PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
endif()
