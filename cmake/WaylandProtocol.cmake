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
# Adds a C library for the client- or server-side interface of the protocol.
function(waylandprotocol_add_library target_name)
  if(NOT WaylandScanner_EXECUTABLE)
    message(FATAL_ERROR "'wayland-scanner' executable required, not found.")
  endif()

  # Parse and verify arguments.
  set(options WLROOTS)
  set(one_value_args PROTOCOL_FILE BASE_NAME SIDE)
  cmake_parse_arguments(args "${options}" "${one_value_args}" "" ${ARGN})
  if(args_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "Unknown args passed to waylandprotocol_add_library: \"${args_UNPARSED_ARGUMENTS}\"")
  endif()
  if(NOT "${args_SIDE}" STREQUAL "client" AND NOT "${args_SIDE}" STREQUAL "server")
    message(FATAL_ERROR "SIDE arg must be \"client\" or \"server\".")
  endif()
  if(NOT args_PROTOCOL_FILE)
    message(FATAL_ERROR "PROTOCOL_FILE argument is required.")
  endif()
  get_filename_component(_protocol_file "${args_PROTOCOL_FILE}" ABSOLUTE)
  if(NOT EXISTS "${_protocol_file}")
    message(FATAL_ERROR "Protocol file \"${args_PROTOCOL_FILE}\" not found.")
  endif()

  if(args_BASE_NAME)
    set(_base_name "${args_BASE_NAME}")
  else()
    get_filename_component(_base_name "${args_PROTOCOL_FILE}" NAME_WLE)
  endif()

  # Generate the interface header.
  if(args_WLROOTS)
    set(_header "${CMAKE_CURRENT_BINARY_DIR}/${_base_name}-protocol.h")
  else()
    set(_header "${CMAKE_CURRENT_BINARY_DIR}/${_base_name}-${args_SIDE}-protocol.h")
  endif()
  set_source_files_properties("${_header}" PROPERTIES GENERATED TRUE)
  add_custom_command(
    OUTPUT "${_header}"
    COMMAND "${WaylandScanner_EXECUTABLE}" "${args_SIDE}-header" "${_protocol_file}" "${_header}"
    DEPENDS "${WaylandScanner_EXECUTABLE}" "${_protocol_file}"
    VERBATIM)

  # Generate the interface glue code.
  set(_glue_code "${CMAKE_CURRENT_BINARY_DIR}/${_base_name}-protocol.c")
  set_source_files_properties("${_glue_code}" PROPERTIES GENERATED TRUE)
  add_custom_command(
    OUTPUT "${_glue_code}"
    COMMAND "${WaylandScanner_EXECUTABLE}" private-code "${_protocol_file}" "${_glue_code}"
    DEPENDS "${WaylandScanner_EXECUTABLE}" "${_protocol_file}"
    VERBATIM)

  # Setup the library.
  add_library("${target_name}" STATIC "${_glue_code}" "${_header}")
  set_target_properties(
    "${target_name}"
    PROPERTIES
    VERSION 1.0
    PUBLIC_HEADER "${_header}")
  target_include_directories(
    "${target_name}"
    PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}")

  # Add dependencies.
  if("${args_SIDE}" STREQUAL "client")
    pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client>=1.22.0)
    target_link_libraries(
      "${target_name}"
      PUBLIC
      PkgConfig::WAYLAND_CLIENT)
  else()
    pkg_check_modules(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server>=1.22.0)
    target_link_libraries(
      "${target_name}"
      PUBLIC
      PkgConfig::WAYLAND_SERVER)
  endif()
endfunction()

