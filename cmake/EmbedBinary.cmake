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
FUNCTION(EmbedBinary binary_file prefix generated_source generated_header)
  SET(output_basename "${CMAKE_CURRENT_BINARY_DIR}/${prefix}")

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

extern uint8_t wlmaker_embedded_${prefix}_data[]\;
extern size_t wlmaker_embedded_${prefix}_size\;

#endif // __EMBEDDED_${prefix}_H__
")


  FILE(WRITE "${output_basename}.c" ${output_source})
  FILE(WRITE "${output_basename}.h" ${output_header})

  SET(${generated_source} "${output_basename}.c" PARENT_SCOPE)
  SET(${generated_header} "${output_basename}.h" PARENT_SCOPE)

ENDFUNCTION()
