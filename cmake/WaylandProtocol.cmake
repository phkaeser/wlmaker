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

FIND_PROGRAM(
  WaylandScanner_EXECUTABLE
  NAMES wayland-scanner)
MARK_AS_ADVANCED(WaylandScanner_EXECUTABLE)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(
  WaylandScanner
  FOUND_VAR WaylandScanner_FOUND
  REQUIRED_VARS WaylandScanner_EXECUTABLE)

# -----------------------------------------------------------------------------
# Builds a library for the protocol, and adds as dependency to target_var.
FUNCTION(WaylandProtocol_ADD target_var)
  IF(NOT WaylandScanner_EXECUTABLE)
    MESSAGE(FATAL_ERROR "'wayland-scanner' executable required, not found.")
  ENDIF()

  # Parse and verify arguments.
  SET(oneValueArgs PROTOCOL_FILE BASE_NAME SIDE)
  CMAKE_PARSE_ARGUMENTS(ARGS "" "${oneValueArgs}" "" ${ARGN})
  IF(NOT ${ARGS_SIDE} STREQUAL "client" AND NOT ${ARGS_SIDE} STREQUAL "server")
    MESSAGE(FATAL_ERROR "SIDE arg must be \"client\" or \"server\".")
  ENDIF()
  IF(ARGS_UNPARSED_ARGUMENTS)
    MESSAGE(FATAL_ERROR "Unknown args passed to _wayland_protocol_add: \"${ARGS_UNPARSED_ARGUMENTS}\"")
  ENDIF(ARGS_UNPARSED_ARGUMENTS)

  GET_FILENAME_COMPONENT(_protocol_file ${ARGS_PROTOCOL_FILE} ABSOLUTE)

  # Generate the client header.
  SET(_header "${CMAKE_CURRENT_BINARY_DIR}/${ARGS_BASE_NAME}-${ARGS_SIDE}-protocol.h")
  SET_SOURCE_FILES_PROPERTIES(${_header} GENERATED)
  ADD_CUSTOM_COMMAND(
    OUTPUT ${_header}
    COMMAND ${WaylandScanner_EXECUTABLE} "${ARGS_SIDE}-header" ${_protocol_file} ${_header}
    DEPENDS ${WaylandScanner_EXECUTABLE} ${_protocol_file}
    VERBATIM)

  # Generate the glue code.
  SET(_glue_code "${CMAKE_CURRENT_BINARY_DIR}/${ARGS_BASE_NAME}-protocol.c")
  SET_SOURCE_FILES_PROPERTIES(${_glue_code} GENERATED)
  ADD_CUSTOM_COMMAND(
    OUTPUT ${_glue_code}
    COMMAND ${WaylandScanner_EXECUTABLE} private-code ${_protocol_file} ${_glue_code}
    DEPENDS ${WaylandScanner_EXECUTABLE} ${_protocol_file}
    VERBATIM)

  SET(libName "lib-${ARGS_BASE_NAME}-${ARGS_SIDE}")

  ADD_LIBRARY("${libName}" STATIC)
  ADD_DEPENDENCIES(${target_var} "${libName}")
  TARGET_SOURCES("${libName}" PRIVATE "${_glue_code}" "${_header}")
  SET_TARGET_PROPERTIES("${libName}" PROPERTIES VERSION 1.0 PUBLIC_HEADER "${_header}")

  # Add dependencies.
  IF(${ARGS_SIDE} STREQUAL "client")
    PKG_CHECK_MODULES(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client>=1.22.0)
    TARGET_INCLUDE_DIRECTORIES(
      "${libName}" PRIVATE
      ${WAYLAND_CLIENT_INCLUDE_DIRS})
  ELSE()
    PKG_CHECK_MODULES(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server>=1.22.0)
    TARGET_INCLUDE_DIRECTORIES(
      "${libName}" PRIVATE
      ${WAYLAND_SERVER_INCLUDE_DIRS})
  ENDIF()

  # The target may be an INTERFACE library. That needs INTERFACE linking.
  GET_PROPERTY(target_type TARGET ${target_var} PROPERTY TYPE)
  IF(target_type STREQUAL "INTERFACE_LIBRARY")
    TARGET_LINK_LIBRARIES(${target_var} INTERFACE "${libName}")
  ELSE()
    TARGET_LINK_LIBRARIES(${target_var} "${libName}")
  ENDIF()

ENDFUNCTION()
