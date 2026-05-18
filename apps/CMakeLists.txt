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
add_subdirectory(libwlclient)
add_subdirectory(primitives)
add_library(wlm_graph_shared STATIC wlm_graph_shared.c)
target_include_directories(wlm_graph_shared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
  wlm_graph_shared
  libwlclient
  primitives)
add_executable(
  example_toplevel
  example_toplevel.c)
target_include_directories(example_toplevel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
  example_toplevel
  m
  libwlclient)
add_executable(
  wlmclock
  wlmclock.c)
target_include_directories(wlmclock PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
  wlmclock
  libwlclient
  primitives
  m)
add_executable(
  wlmeyes
  wlmeyes.c)
target_include_directories(wlmeyes PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
  wlmeyes
  m
  libwlclient)
add_executable(
  wlmcpugraph
  wlmcpugraph.c)
target_include_directories(wlmcpugraph PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
  wlmcpugraph
  wlm_graph_shared
  libwlclient
  primitives)
add_executable(
  wlmmemgraph
  wlmmemgraph.c)
target_include_directories(wlmmemgraph PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
  wlmmemgraph
  wlm_graph_shared
  libwlclient
  primitives)
add_executable(
  wlmnetgraph
  wlmnetgraph.c)
target_include_directories(wlmnetgraph PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
  wlmnetgraph
  wlm_graph_shared
  libwlclient
  primitives)
add_executable(
  wlmbattery
  wlmbattery.c)
target_include_directories(wlmbattery PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
  wlmbattery
  libbase
  libwlclient
  primitives)
install(
  TARGETS wlmclock wlmeyes wlmcpugraph wlmmemgraph wlmnetgraph wlmbattery
  DESTINATION "${CMAKE_INSTALL_BINDIR}")
if(iwyu_path_and_options)
  set_target_properties(
    wlm_graph_shared PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  set_target_properties(
    example_toplevel PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  set_target_properties(
    wlmclock PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  set_target_properties(
    wlmeyes PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  set_target_properties(
    wlmcpugraph PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  set_target_properties(
    wlmmemgraph PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  set_target_properties(
    wlmnetgraph PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
endif()
