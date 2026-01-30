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

ADD_SUBDIRECTORY(libwlclient)
ADD_SUBDIRECTORY(primitives)

ADD_LIBRARY(wlm_graph_shared STATIC wlm_graph_shared.c)
TARGET_INCLUDE_DIRECTORIES(wlm_graph_shared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
TARGET_LINK_LIBRARIES(wlm_graph_shared libwlclient primitives)

ADD_EXECUTABLE(example_toplevel example_toplevel.c)
TARGET_INCLUDE_DIRECTORIES(example_toplevel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
TARGET_LINK_LIBRARIES(example_toplevel m libwlclient)

ADD_EXECUTABLE(wlmclock wlmclock.c)
TARGET_INCLUDE_DIRECTORIES(wlmclock PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
TARGET_LINK_LIBRARIES(wlmclock libwlclient primitives m)

ADD_EXECUTABLE(wlmeyes wlmeyes.c)
TARGET_INCLUDE_DIRECTORIES(wlmeyes PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
TARGET_LINK_LIBRARIES(wlmeyes m libwlclient)

ADD_EXECUTABLE(wlmcpugraph wlmcpugraph.c)
TARGET_INCLUDE_DIRECTORIES(wlmcpugraph PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
TARGET_LINK_LIBRARIES(wlmcpugraph wlm_graph_shared libwlclient primitives)

ADD_EXECUTABLE(wlmmemgraph wlmmemgraph.c)
TARGET_INCLUDE_DIRECTORIES(wlmmemgraph PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
TARGET_LINK_LIBRARIES(wlmmemgraph wlm_graph_shared libwlclient primitives)

ADD_EXECUTABLE(wlmnetgraph wlmnetgraph.c)
TARGET_INCLUDE_DIRECTORIES(wlmnetgraph PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
TARGET_LINK_LIBRARIES(wlmnetgraph wlm_graph_shared libwlclient primitives)

INSTALL(
  TARGETS wlmclock wlmeyes wlmcpugraph wlmmemgraph wlmnetgraph
  DESTINATION ${CMAKE_INSTALL_BINDIR})

IF(iwyu_path_and_options)
  SET_TARGET_PROPERTIES(
    wlm_graph_shared PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  SET_TARGET_PROPERTIES(
    example_toplevel PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  SET_TARGET_PROPERTIES(
    wlmclock PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  SET_TARGET_PROPERTIES(
    wlmeyes PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  SET_TARGET_PROPERTIES(
    wlmcpugraph PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  SET_TARGET_PROPERTIES(
    wlmmemgraph PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
  SET_TARGET_PROPERTIES(
    wlmnetgraph PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
ENDIF(iwyu_path_and_options)
