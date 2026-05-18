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
include(EmbedBinary)
embedbinary_add_library(
  embedded_theme
  "theme"
  "${CMAKE_CURRENT_SOURCE_DIR}/Themes/Default.plist")
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/wlmaker.desktop.in
  ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.desktop
  @ONLY)
# Install to both wayland-sessions/ and applications/ -- it can run in windowed
# mode.
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.desktop
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.desktop
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/wayland-sessions")
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/wlmaker.wlmbattery.desktop.in
  ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmbattery.desktop
  @ONLY)
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmbattery.desktop
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/wlmaker.wlmclock.desktop.in
  ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmclock.desktop
  @ONLY)
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmclock.desktop
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/wlmaker.wlmcpugraph.desktop.in
  ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmcpugraph.desktop
  @ONLY)
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmcpugraph.desktop
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/wlmaker.wlmnetgraph.desktop.in
  ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmnetgraph.desktop
  @ONLY)
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmnetgraph.desktop
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/wlmaker.wlmmemgraph.desktop.in
  ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmmemgraph.desktop
  @ONLY)
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmmemgraph.desktop
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/wlmaker.wlmeyes.desktop.in
  ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmeyes.desktop
  @ONLY)
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmeyes.desktop
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
find_program(DESKTOP_FILE_VALIDATOR NAMES desktop-file-validate OPTIONAL)
if(DESKTOP_FILE_VALIDATOR)
  add_test(
    NAME wlmaker_desktop_test
    COMMAND ${DESKTOP_FILE_VALIDATOR} "${CMAKE_CURRENT_BINARY_DIR}/wlmaker.desktop")
  # We also want to report errors that are not "fatal" yet.
  set_property(
    TEST wlmaker_desktop_test
    PROPERTY FAIL_REGULAR_EXPRESSION " error: ")
  add_test(
    NAME wlmbattery_desktop_test
    COMMAND ${DESKTOP_FILE_VALIDATOR} "${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmbattery.desktop")
  # We also want to report errors that are not "fatal" yet.
  set_property(
    TEST wlmbattery_desktop_test
    PROPERTY FAIL_REGULAR_EXPRESSION " error: ")
  add_test(
    NAME wlmclock_desktop_test
    COMMAND ${DESKTOP_FILE_VALIDATOR} "${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmclock.desktop")
  # We also want to report errors that are not "fatal" yet.
  set_property(
    TEST wlmclock_desktop_test
    PROPERTY FAIL_REGULAR_EXPRESSION " error: ")
  add_test(
    NAME wlmcpugraph_desktop_test
    COMMAND ${DESKTOP_FILE_VALIDATOR} "${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmcpugraph.desktop")
  # We also want to report errors that are not "fatal" yet.
  set_property(
    TEST wlmcpugraph_desktop_test
    PROPERTY FAIL_REGULAR_EXPRESSION " error: ")
  add_test(
    NAME wlmnetgraph_desktop_test
    COMMAND ${DESKTOP_FILE_VALIDATOR} "${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmnetgraph.desktop")
  # We also want to report errors that are not "fatal" yet.
  set_property(
    TEST wlmnetgraph_desktop_test
    PROPERTY FAIL_REGULAR_EXPRESSION " error: ")
  add_test(
    NAME wlmmemgraph_desktop_test
    COMMAND ${DESKTOP_FILE_VALIDATOR} "${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmmemgraph.desktop")
  # We also want to report errors that are not "fatal" yet.
  set_property(
    TEST wlmmemgraph_desktop_test
    PROPERTY FAIL_REGULAR_EXPRESSION " error: ")
  add_test(
    NAME wlmeyes_desktop_test
    COMMAND ${DESKTOP_FILE_VALIDATOR} "${CMAKE_CURRENT_BINARY_DIR}/wlmaker.wlmeyes.desktop")
  # We also want to report errors that are not "fatal" yet.
  set_property(
    TEST wlmeyes_desktop_test
    PROPERTY FAIL_REGULAR_EXPRESSION " error: ")
else()
  message(STATUS "Skipping .desktop file validation -- `desktop-file-validate` not found")
endif()

# Found in Debian's gnustep-base-runtime. Using to keep plist format consistent
# with what GNUstep expects. Optional.
find_program(PLDES pldes)
if(PLDES)
  add_test(
    NAME theme_default_plist_test
    COMMAND pldes "${CMAKE_CURRENT_SOURCE_DIR}/Themes/Default.plist")
  set_property(
    TEST theme_default_plist_test
    PROPERTY FAIL_REGULAR_EXPRESSION "${pldes_failure_regex}")
  add_test(
    NAME theme_debian_plist_test
    COMMAND pldes "${CMAKE_CURRENT_SOURCE_DIR}/Themes/Debian.plist")
  set_property(
    TEST theme_debian_plist_test
    PROPERTY FAIL_REGULAR_EXPRESSION "${pldes_failure_regex}")
  add_test(
    NAME theme_debian_larger_plist_test
    COMMAND pldes "${CMAKE_CURRENT_SOURCE_DIR}/Themes/DebianLarger.plist")
  set_property(
    TEST theme_debian_larger_plist_test
    PROPERTY FAIL_REGULAR_EXPRESSION "${pldes_failure_regex}")
  add_test(
    NAME theme_debian_double_plist_test
    COMMAND pldes "${CMAKE_CURRENT_SOURCE_DIR}/Themes/DebianDouble.plist")
  set_property(
    TEST theme_debian_double_plist_test
    PROPERTY FAIL_REGULAR_EXPRESSION "${pldes_failure_regex}")
endif()
install(FILES
  wlmaker.svg
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps")
install(FILES
  wlmaker-48x48.png
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/48x48/apps"
  RENAME wlmaker.png)
install(FILES
  wlmaker-64x64.png
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps"
  RENAME wlmaker.png)
install(FILES
  wlmbattery-64x64.png
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps"
  RENAME wlmbattery.png)
install(FILES
  wlmclock-64x64.png
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps"
  RENAME wlmclock.png)
install(FILES
  wlmcpugraph-64x64.png
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps"
  RENAME wlmcpugraph.png)
install(FILES
  wlmmemgraph-64x64.png
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps"
  RENAME wlmmemgraph.png)
install(FILES
  wlmnetgraph-64x64.png
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps"
  RENAME wlmnetgraph.png)
install(FILES
  wlmeyes-64x64.png
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps"
  RENAME wlmeyes.png)
install(FILES
  wlmaker/icons/chrome-56x56.png
  wlmaker/icons/clip-56x56.png
  wlmaker/icons/firefox-56x56.png
  wlmaker/icons/terminal-56x56.png
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/wlmaker/icons")
install(
  FILES
  Themes/Default.plist
  Themes/Debian.plist
  Themes/DebianLarger.plist
  Themes/DebianDouble.plist
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/wlmaker/Themes")
