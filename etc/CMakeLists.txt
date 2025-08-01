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
INCLUDE(EmbedBinary)

EmbedBinary_ADD_LIBRARY(
  embedded_root_menu
  "root_menu"
  "${CMAKE_CURRENT_SOURCE_DIR}/root-menu.plist")

EmbedBinary_ADD_LIBRARY(
  embedded_style
  "style"
  "${CMAKE_CURRENT_SOURCE_DIR}/style.plist")

EmbedBinary_ADD_LIBRARY(
  embedded_configuration
  "default_configuration"
  "${CMAKE_CURRENT_SOURCE_DIR}/wlmaker.plist")

EmbedBinary_ADD_LIBRARY(
  embedded_state
  "default_state"
  "${CMAKE_CURRENT_SOURCE_DIR}/wlmaker-state.plist")

# Found in Debian's gnustep-base-runtime. Using to keep plist format consistent
# with what GNUstep expects. Optional.
FIND_PROGRAM(PLDES pldes)
IF(PLDES)
  MESSAGE("Found ${PLDES}. Adding consistency tests for GNUstep plist syntax.")

  # Sadly, the gnustep-base plist utilies don't return EXIT_FAILURE upon
  # parsing an invalid plist file. But eg. `pldes` reports the parsing error,
  # so we capture this as a failure.
  SET(pldes_failure_regex ".*at\ line.*char")

  ADD_TEST(
    NAME root_menu_plist_test
    COMMAND pldes "${CMAKE_CURRENT_SOURCE_DIR}/root-menu.plist")
  SET_PROPERTY(
    TEST root_menu_plist_test
    PROPERTY FAIL_REGULAR_EXPRESSION "${pldes_failure_regex}")

  ADD_TEST(
    NAME style_default_plist_test
    COMMAND pldes "${CMAKE_CURRENT_SOURCE_DIR}/style.plist")
  SET_PROPERTY(
    TEST style_default_plist_test
    PROPERTY FAIL_REGULAR_EXPRESSION "${pldes_failure_regex}")

  ADD_TEST(
    NAME style_debian_plist_test
    COMMAND pldes "${CMAKE_CURRENT_SOURCE_DIR}/style-debian.plist")
  SET_PROPERTY(
    TEST style_debian_plist_test
    PROPERTY FAIL_REGULAR_EXPRESSION "${pldes_failure_regex}")

  ADD_TEST(
    NAME wlmaker_plist_test
    COMMAND pldes "${CMAKE_CURRENT_SOURCE_DIR}/wlmaker.plist")
  SET_PROPERTY(
    TEST wlmaker_plist_test
    PROPERTY FAIL_REGULAR_EXPRESSION "${pldes_failure_regex}")

  ADD_TEST(
    NAME wlmaker_example_plist_test
    COMMAND pldes "${CMAKE_CURRENT_SOURCE_DIR}/wlmaker-example.plist")
  SET_PROPERTY(
    TEST wlmaker_example_plist_test
    PROPERTY FAIL_REGULAR_EXPRESSION "${pldes_failure_regex}")

  ADD_TEST(
    NAME wlmaker_home_plist_test
    COMMAND pldes "${CMAKE_CURRENT_SOURCE_DIR}/wlmaker-home.plist")
  SET_PROPERTY(
    TEST wlmaker_home_plist_test
    PROPERTY FAIL_REGULAR_EXPRESSION "${pldes_failure_regex}")

  ADD_TEST(
    NAME wlmaker_state_plist_test
    COMMAND pldes "${CMAKE_CURRENT_SOURCE_DIR}/wlmaker-state.plist")
  SET_PROPERTY(
    TEST wlmaker_state_plist_test
    PROPERTY FAIL_REGULAR_EXPRESSION "${pldes_failure_regex}")

ENDIF(PLDES)

INSTALL(
  FILES
  root-menu.plist
  style.plist
  wlmaker-state.plist
  wlmaker.plist
  DESTINATION share/wlmaker)

IF(wmmenugen_executable)
  CONFIGURE_FILE(
    ${CMAKE_CURRENT_SOURCE_DIR}/root-menu-debian.plist.in
    ${CMAKE_CURRENT_BINARY_DIR}/root-menu-debian.plist
    @ONLY)
  INSTALL(
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/root-menu-debian.plist
    DESTINATION share/wlmaker)
ENDIF(wmmenugen_executable)
