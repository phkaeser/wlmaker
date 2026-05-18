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
cmake_minimum_required(VERSION 3.13)
find_package(Doxygen)
if(DOXYGEN_FOUND)
  find_file(PLANTUML_JAR
    NAMES plantuml.jar
    HINTS ENV{PLANTUML_JAR_PATH}
    PATHS
    /usr/global/share/java/plantuml/
    /usr/local/share/java/plantuml/
    /usr/share/java/
    /usr/local/share/java/
    /usr/share/plantuml/)
  if(PLANTUML_JAR)
    set(DOXYGEN_PLANTUML_JAR_FILE ${PLANTUML_JAR})
  else()
    set(DOXYGEN_PLANTUML_JAR_FILE "")
    message(
      NOTICE
      "Did not find plantuml.jar -- Will not generate class diagrams.")
  endif()
  if(config_DOXYGEN_CRITICAL)
    set(DOXYGEN_WARN_AS_ERROR "YES")
  else()
    set(DOXYGEN_WARN_AS_ERROR "NO")
  endif()

  # set input and output files
  set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
  set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

  # request to configure the file
  configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
  message(NOTICE "Doxygen available, adding 'doc' target.")
  add_custom_target(
    doc
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
    DEPENDS ${DOXYGEN_OUT} ${PROJECT_BINARY_DIR}/etc/RootMenuDebian.plist
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen."
    VERBATIM)
  file(
    COPY ${PROJECT_SOURCE_DIR}/tests/data/toolkit/primitive_close_icon.png
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/html")
  file(
    COPY ${PROJECT_SOURCE_DIR}/tests/data/toolkit/primitive_minimize_icon.png
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/html")
  file(
    COPY ${PROJECT_SOURCE_DIR}/share/wlmaker.svg
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/html")
else()
  message(NOTICE "Doxygen not found. Not adding 'doc' target to generate API documentation.")
endif()
