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

CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/wlmaker.desktop.in
  ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.desktop
  @ONLY)
# Install to both wayland-sessions/ and applications/ -- it can run in windowed
# mode.
INSTALL(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.desktop
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
INSTALL(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.desktop
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/wayland-sessions)

CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/wlmaker.wlmclock.desktop.in
  ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmclock.desktop
  @ONLY)
INSTALL(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmclock.desktop
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)

CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/wlmaker.wlmcpugraph.desktop.in
  ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmcpugraph.desktop
  @ONLY)
INSTALL(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmcpugraph.desktop
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)

CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/wlmaker.wlmnetgraph.desktop.in
  ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmnetgraph.desktop
  @ONLY)
INSTALL(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmnetgraph.desktop
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)

CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/wlmaker.wlmmemgraph.desktop.in
  ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmmemgraph.desktop
  @ONLY)
INSTALL(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmmemgraph.desktop
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)

CONFIGURE_FILE(
  ${CMAKE_CURRENT_SOURCE_DIR}/wlmaker.wlmeyes.desktop.in
  ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmeyes.desktop
  @ONLY)
INSTALL(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmeyes.desktop
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)

FIND_PROGRAM(DESKTOP_FILE_VALIDATOR NAMES desktop-file-validate OPTIONAL)
IF(DESKTOP_FILE_VALIDATOR)
  ADD_TEST(
    NAME wlmaker_desktop_test
    COMMAND ${DESKTOP_FILE_VALIDATOR} "${CMAKE_CURRENT_BINARY_DIR}/wlmaker.desktop")
  # We also want to report errors that are not "fatal" yet.
  SET_PROPERTY(
    TEST wlmaker_desktop_test
    PROPERTY FAIL_REGULAR_EXPRESSION " error: ")
  ADD_TEST(
    NAME wlmclock_desktop_test
    COMMAND ${DESKTOP_FILE_VALIDATOR} "${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmclock.desktop")
  # We also want to report errors that are not "fatal" yet.
  SET_PROPERTY(
    TEST wlmclock_desktop_test
    PROPERTY FAIL_REGULAR_EXPRESSION " error: ")
  ADD_TEST(
    NAME wlmcpugraph_desktop_test
    COMMAND ${DESKTOP_FILE_VALIDATOR} "${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmcpugraph.desktop")
  # We also want to report errors that are not "fatal" yet.
  SET_PROPERTY(
    TEST wlmcpugraph_desktop_test
    PROPERTY FAIL_REGULAR_EXPRESSION " error: ")
  ADD_TEST(
    NAME wlmnetgraph_desktop_test
    COMMAND ${DESKTOP_FILE_VALIDATOR} "${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmnetgraph.desktop")
  # We also want to report errors that are not "fatal" yet.
  SET_PROPERTY(
    TEST wlmnetgraph_desktop_test
    PROPERTY FAIL_REGULAR_EXPRESSION " error: ")
  ADD_TEST(
    NAME wlmmemgraph_desktop_test
    COMMAND ${DESKTOP_FILE_VALIDATOR} "${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmmemgraph.desktop")
  # We also want to report errors that are not "fatal" yet.
  SET_PROPERTY(
    TEST wlmmemgraph_desktop_test
    PROPERTY FAIL_REGULAR_EXPRESSION " error: ")
  ADD_TEST(
    NAME wlmeyes_desktop_test
    COMMAND ${DESKTOP_FILE_VALIDATOR} "${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmeyes.desktop")
  # We also want to report errors that are not "fatal" yet.
  SET_PROPERTY(
    TEST wlmeyes_desktop_test
    PROPERTY FAIL_REGULAR_EXPRESSION " error: ")
ELSE()
  MESSAGE(STATUS "Skipping .desktop file validation -- `desktop-file-validate` not found")
ENDIF()

INSTALL(FILES
  wlmaker.svg
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)
INSTALL(FILES
  wlmaker-48x48.png
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/48x48/apps
  RENAME wlmaker.png)
INSTALL(FILES
  wlmaker-64x64.png
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps
  RENAME wlmaker.png)

INSTALL(FILES
  wlmclock-64x64.png
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps
  RENAME wlmclock.png)
INSTALL(FILES
  wlmcpugraph-64x64.png
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps
  RENAME wlmcpugraph.png)
INSTALL(FILES
  wlmmemgraph-64x64.png
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps
  RENAME wlmmemgraph.png)
INSTALL(FILES
  wlmnetgraph-64x64.png
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps
  RENAME wlmnetgraph.png)

INSTALL(FILES
  wlmeyes-64x64.png
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps
  RENAME wlmeyes.png)

INSTALL(FILES
  wlmaker/icons/chrome-48x48.png
  wlmaker/icons/clip-48x48.png
  wlmaker/icons/firefox-48x48.png
  wlmaker/icons/terminal-48x48.png
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/wlmaker/icons)
