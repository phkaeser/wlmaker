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

# Embeds a binary file: Creates source and header, with corresponding
# _data and _prefix definitions to address and size the data.
#
# Rationale for using cmake function:
#
# Using GCC and the GNU linker, the canonical way would be to use
# "ld -r -b binary -o <file.o> <file>", respectively
# "objcopy -I binary -O <bfd> <file> <file.o>".
# The latter needs the <bfd> target (specific for the target architecture), and
# the former ties the build to the GNU chain.
FUNCTION(EmbedBinary binary_file prefix generated_source generated_header)
  SET(output_basename "${CMAKE_CURRENT_BINARY_DIR}/${prefix}")

  # TODO: Should have 'binary_file' as dependency to the output.

  FILE(READ ${binary_file} binary_content HEX)
  STRING(REGEX MATCHALL "([A-Fa-f0-9][A-Fa-f0-9])" separated_content ${binary_content})

  SET(output_source "
// Generated source via 'GenerateEmbedders' from \"${binary_file}\".

#include <stddef.h>
#include <stdint.h>

const uint8_t wlmaker_embedded_${prefix}_data[] = {
")

  SET(tokens 0)
  FOREACH(token IN LISTS separated_content)
    IF(tokens EQUAL 0)
      STRING(APPEND output_source "    ")
    ENDIF()
    STRING(APPEND output_source "0x${token}, ")
    MATH(EXPR tokens "${tokens}+1")
    IF(tokens GREATER 16)
      STRING(APPEND output_source "\n")
      SET(tokens 0)
    ENDIF()
  ENDFOREACH()

  STRING(APPEND output_source "}\;

const size_t wlmaker_embedded_${prefix}_size = sizeof(wlmaker_embedded_${prefix}_data)\;
")

  SET(output_header "
// Generated header via 'GenerateEmbedders' from \"${binary_file}\".

#ifndef __EMBEDDED_${prefix}_H__
#define __EMBEDDED_${prefix}_H__

#include <stdint.h>

extern const uint8_t wlmaker_embedded_${prefix}_data[]\;
extern const size_t wlmaker_embedded_${prefix}_size\;

#endif // __EMBEDDED_${prefix}_H__
")


  FILE(WRITE "${output_basename}.c" ${output_source})
  FILE(WRITE "${output_basename}.h" ${output_header})

  SET(${generated_source} "${output_basename}.c" PARENT_SCOPE)
  SET(${generated_header} "${output_basename}.h" PARENT_SCOPE)

  # ADD_CUSTOM_TARGET(generated_source DEPENDS binary_file) ?
  # https://cmake.org/cmake/help/latest/guide/tutorial/Adding%20a%20Custom%20Command%20and%20Generated%20File.html
  # https://stackoverflow.com/questions/14776463/compile-and-add-an-object-file-from-a-binary-with-cmake
  # http://gareus.org/wiki/embedding_resources_in_executables
ENDFUNCTION()

FUNCTION(GenerateEmbedCMake generated_cmake_file)
  FILE(WRITE ${generated_cmake_file} "MESSAGE(WARNING, \"hello world\")")
ENDFUNCTION()

# Adds a C library to embed the binary file with the specified prefix.
#
# Generates a C source and header file from the contents of `binary_file`, and
# adds both of these to a new library target `library_target`.
#
# The embedded data can then be accessed as external variables:
#    const uint8_t wlmaker_embedded_<prefix>_data[];
#    const size_t wlmaker_embedded_<prefix>_size;
FUNCTION(AddLibraryToEmbedBinary library_target prefix binary_file)

  # Create header, create source.
  #EmbedBinary(${binary_file} ${prefix} source header)
  SET(output_basename "${CMAKE_CURRENT_BINARY_DIR}/${prefix}")
  SET(output_source "${output_basename}.c")
  SET(output_header "${output_basename}.h")
  SET(generated_cmake "${output_basename}.cmake")

  GenerateEmbedCMake(${generated_cmake})

  ADD_CUSTOM_COMMAND(
    OUTPUT ${output_source} ${output_header}
    COMMAND ${CMAKE_COMMAND} -P ${generated_cmake}
    MAIN_DEPENDENCY ${binary_file}
  )

  ADD_LIBRARY(${library_target} STATIC)
  TARGET_SOURCES(${library_target} PRIVATE ${output_source} ${output_header})
  TARGET_INCLUDE_DIRECTORIES(${library_target} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
  SET_TARGET_PROPERTIES(
    ${library_target}
    PROPERTIES VERSION 1.0
    PUBLIC_HEADER ${output_header})

ENDFUNCTION()
