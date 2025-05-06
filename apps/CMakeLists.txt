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

ADD_EXECUTABLE(wlmclock wlmclock.c)
TARGET_INCLUDE_DIRECTORIES(wlmclock PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
TARGET_LINK_LIBRARIES(wlmclock libwlclient primitives m)

ADD_EXECUTABLE(example_toplevel example_toplevel.c)
TARGET_INCLUDE_DIRECTORIES(example_toplevel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
TARGET_LINK_LIBRARIES(example_toplevel m libwlclient)

INSTALL(TARGETS wlmclock DESTINATION bin)
