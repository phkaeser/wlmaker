# Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
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
cmake_minimum_required(VERSION 3.13)
add_subdirectory(primitives)

# add_wlm_app
#
# Helper function to define wayland-maker application targets.
# Creates an executable using the source file '${name}.c', configures it with
# private include directories, links it with the specified libraries, and sets up
# include-what-you-use configurations if enabled.
#
# Arguments:
#   name: Name of the application target and source file (excluding '.c' extension).
#   ARGN: List of libraries to link against.
function(add_wlm_app name)
  add_executable(${name} ${name}.c)
  target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(${name} ${ARGN})
  if(iwyu_path_and_options)
    set_target_properties(
      ${name} PROPERTIES
      C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  endif()
endfunction()

add_library(wlm_graph_shared STATIC wlm_graph_shared.c)
target_include_directories(wlm_graph_shared PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(
  wlm_graph_shared
  libwlclient
  primitives)
if(iwyu_path_and_options)
  set_target_properties(
    wlm_graph_shared PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
endif()

add_wlm_app(wlmclock libwlclient primitives m)
add_wlm_app(wlmeyes m libwlclient)
add_wlm_app(wlmcpugraph wlm_graph_shared libwlclient primitives)
add_wlm_app(wlmmemgraph wlm_graph_shared libwlclient primitives)
add_wlm_app(wlmnetgraph wlm_graph_shared libwlclient primitives)
add_wlm_app(wlmbattery libbase libwlclient primitives)

install(
  TARGETS wlmclock wlmeyes wlmcpugraph wlmmemgraph wlmnetgraph wlmbattery
  DESTINATION "${CMAKE_INSTALL_BINDIR}")
