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

FIND_PACKAGE(Doxygen)
IF(DOXYGEN_FOUND)

  FIND_FILE(PLANTUML_JAR
    NAMES plantuml.jar
    HINTS ENV{PLANTUML_JAR_PATH}
    PATHS
    /usr/global/share/java/plantuml/
    /usr/local/share/java/plantuml/
    /usr/share/java/
    /usr/local/share/java/
    /usr/share/plantuml/)
  IF(PLANTUML_JAR)
    SET(DOXYGEN_PLANTUML_JAR_FILE ${PLANTUML_JAR})
  ELSE()
    SET(DOXYGEN_PLANTUML_JAR_FILE "")
    MESSAGE(
      NOTICE
      "Did not find plantuml.jar -- Will not generate class diagrams.")
  ENDIF(PLANTUML_JAR_FOUND)

  IF(config_DOXYGEN_CRITICAL)
    SET(DOXYGEN_WARN_AS_ERROR "YES")
  ELSE(config_DOXYGEN_CRITICAL)
    SET(DOXYGEN_WARN_AS_ERROR "NO")
  ENDIF(config_DOXYGEN_CRITICAL)

  # set input and output files
  SET(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
  SET(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

  # request to configure the file
  CONFIGURE_FILE(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
  MESSAGE(NOTICE "Doxygen available, adding 'doc' target.")

  ADD_CUSTOM_TARGET(
    doc
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
    DEPENDS ${DOXYGEN_OUT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen."
    VERBATIM)

ELSE()
  MESSAGE(NOTICE "Doxygen not found. Not adding 'doc' target to generate API documentation.")
ENDIF(DOXYGEN_FOUND)
