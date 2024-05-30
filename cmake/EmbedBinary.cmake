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

CMAKE_MINIMUM_REQUIRED(VERSION 3.13)

# During the INCLUDE() call, CMAKE_CURRENT_LIST_DIR is this file's path.
# Store it for later, during the FUNCTION() call it'll be the caller's path.
SET(EMBED_BINARY_TEMPLATE_DIR ${CMAKE_CURRENT_LIST_DIR})

# Adds a C library to embed the binary file with the specified prefix.
#
# Generates a C source and header file from the contents of `binary_file`, and
# adds both of these to a new library target `library_target`.
#
# The embedded data can then be accessed as external variables:
#    const uint8_t embedded_binary_<prefix>_data[];
#    const size_t embedded_binary_<prefix>_size;
FUNCTION(EmbedBinary_ADD_LIBRARY library_target prefix binary_file)

  # Create header, create source.
  #EmbedBinary(${binary_file} ${prefix} source header)
  SET(output_basename "${CMAKE_CURRENT_BINARY_DIR}/${prefix}")
  SET(output_source "${output_basename}.c")
  SET(output_header "${output_basename}.h")
  SET(generated_cmake "${output_basename}.cmake")

  SET(template_header "${EMBED_BINARY_TEMPLATE_DIR}/embed_binary.h.in")
  SET(template_source "${EMBED_BINARY_TEMPLATE_DIR}/embed_binary.c.in")

  CONFIGURE_FILE(
    ${EMBED_BINARY_TEMPLATE_DIR}/embed.cmake.in
    ${generated_cmake}
    @ONLY)

  ADD_CUSTOM_COMMAND(
    OUTPUT ${output_source} ${output_header}
    COMMAND ${CMAKE_COMMAND} -P ${generated_cmake}
    MAIN_DEPENDENCY ${binary_file}
    DEPENDS "${generated_cmake}" "${template_header}" "${template_source}"
  )

  ADD_LIBRARY(${library_target} STATIC)
  TARGET_SOURCES(${library_target} PRIVATE ${output_source} ${output_header})
  TARGET_INCLUDE_DIRECTORIES(${library_target} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
  SET_TARGET_PROPERTIES(
    ${library_target}
    PROPERTIES VERSION 1.0
    PUBLIC_HEADER ${output_header})

ENDFUNCTION()
