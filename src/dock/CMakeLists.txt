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

add_executable(wlmdock wlmdock.c dock.c)


target_include_directories(
  wlmdock
  PRIVATE
  "${PROJECT_SOURCE_DIR}/src"
  "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(
  wlmdock
  PRIVATE
  libbase
  libwlclient
  wlmaker_lib
)

if(iwyu_path_and_options)
  set_target_properties(
    wlmdock PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
endif()

install(TARGETS wlmdock DESTINATION "${CMAKE_INSTALL_BINDIR}")
