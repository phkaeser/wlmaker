# Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
# Copyright 2024 Google LLC
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

# During the INCLUDE() call, CMAKE_CURRENT_LIST_DIR is this file's path.
# Store it for later, during the FUNCTION() call it'll be the caller's path.
set(EMBED_BINARY_TEMPLATE_DIR ${CMAKE_CURRENT_LIST_DIR})

# Adds a static C library to embed the binary file with the specified prefix.
#
# Generates a C source and header file from the contents of `binary_file`, and
# adds both of these to a new library target `library_target`.
#
# The embedded data can then be accessed as external variables:
#
#    #include "<prefix>.h"
#
#    * uint8_t embedded_binary_<prefix>_data[];
#    * const size_t embedded_binary_<prefix>_size;
function(EmbedBinary_ADD_LIBRARY library_target prefix binary_file)
  set(output_basename "${CMAKE_CURRENT_BINARY_DIR}/${prefix}")
  set(output_source "${output_basename}.c")
  set(output_header "${output_basename}.h")
  set(generated_cmake "${output_basename}.cmake")
  set(template_header "${EMBED_BINARY_TEMPLATE_DIR}/embed_binary.h.in")
  set(template_source "${EMBED_BINARY_TEMPLATE_DIR}/embed_binary.c.in")
  configure_file(
    "${EMBED_BINARY_TEMPLATE_DIR}/embed.cmake.in"
    "${generated_cmake}"
    @ONLY)
  add_custom_command(
    OUTPUT "${output_source}" "${output_header}"
    COMMAND ${CMAKE_COMMAND} -P "${generated_cmake}"
    MAIN_DEPENDENCY "${binary_file}"
    DEPENDS "${generated_cmake}" "${template_header}" "${template_source}"
  )
  add_library(${library_target} STATIC)
  target_sources(
    ${library_target} PRIVATE
    "${output_source}"
    "${output_header}")
  target_include_directories(
    ${library_target} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
  set_target_properties(
    ${library_target}
    PROPERTIES VERSION 1.0
    PUBLIC_HEADER "${output_header}")
endfunction()
