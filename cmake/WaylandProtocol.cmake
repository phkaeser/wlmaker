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
find_program(
  WaylandScanner_EXECUTABLE
  NAMES wayland-scanner)
mark_as_advanced(WaylandScanner_EXECUTABLE)
find_package_handle_standard_args(
  WaylandScanner
  FOUND_VAR WaylandScanner_FOUND
  REQUIRED_VARS WaylandScanner_EXECUTABLE)

# -----------------------------------------------------------------------------
# Builds a library for the protocol, and adds as dependency to target_var.
function(WaylandProtocol_ADD target_var)
  if(NOT WaylandScanner_EXECUTABLE)
    message(FATAL_ERROR "'wayland-scanner' executable required, not found.")
  endif()

  # Parse and verify arguments.
  set(oneValueArgs PROTOCOL_FILE BASE_NAME SIDE)
  cmake_parse_arguments(ARGS "" "${oneValueArgs}" "" ${ARGN})
  if(NOT ${ARGS_SIDE} STREQUAL "client" AND NOT ${ARGS_SIDE} STREQUAL "server")
    message(FATAL_ERROR "SIDE arg must be \"client\" or \"server\".")
  endif()
  if(ARGS_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "Unknown args passed to _wayland_protocol_add: \"${ARGS_UNPARSED_ARGUMENTS}\"")
  endif()
  get_filename_component(_protocol_file ${ARGS_PROTOCOL_FILE} ABSOLUTE)

  # Generate the client header.
  set(_header "${CMAKE_CURRENT_BINARY_DIR}/${ARGS_BASE_NAME}-${ARGS_SIDE}-protocol.h")
  set_source_files_properties(${_header} GENERATED)
  add_custom_command(
    OUTPUT ${_header}
    COMMAND ${WaylandScanner_EXECUTABLE} "${ARGS_SIDE}-header" ${_protocol_file} ${_header}
    DEPENDS ${WaylandScanner_EXECUTABLE} ${_protocol_file}
    VERBATIM)

  # Generate the glue code.
  set(_glue_code "${CMAKE_CURRENT_BINARY_DIR}/${ARGS_BASE_NAME}-protocol.c")
  set_source_files_properties(${_glue_code} GENERATED)
  add_custom_command(
    OUTPUT ${_glue_code}
    COMMAND ${WaylandScanner_EXECUTABLE} private-code ${_protocol_file} ${_glue_code}
    DEPENDS ${WaylandScanner_EXECUTABLE} ${_protocol_file}
    VERBATIM)
  set(libName "lib-${ARGS_BASE_NAME}-${ARGS_SIDE}")
  add_library("${libName}" STATIC)
  add_dependencies(${target_var} "${libName}")
  target_sources("${libName}" PRIVATE "${_glue_code}" "${_header}")
  set_target_properties("${libName}" PROPERTIES VERSION 1.0 PUBLIC_HEADER "${_header}")

  # Add dependencies.
  if(${ARGS_SIDE} STREQUAL "client")
    pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client>=1.22.0)
    target_include_directories(
      "${libName}" PRIVATE
      ${WAYLAND_CLIENT_INCLUDE_DIRS})
  else()
    pkg_check_modules(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server>=1.22.0)
    target_include_directories(
      "${libName}" PRIVATE
      ${WAYLAND_SERVER_INCLUDE_DIRS})
  endif()

  # The target may be an INTERFACE library. That needs INTERFACE linking.
  get_property(target_type TARGET ${target_var} PROPERTY TYPE)
  if(target_type STREQUAL "INTERFACE_LIBRARY")
    target_link_libraries(
      ${target_var}
      INTERFACE
      "${libName}")
  else()
    target_link_libraries(
      ${target_var}
      "${libName}")
  endif()
endfunction()
