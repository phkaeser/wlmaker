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
include(EmbedBinary)
embedbinary_add_library(
  embedded_root_menu
  "root_menu"
  "${CMAKE_CURRENT_SOURCE_DIR}/RootMenu.plist")
embedbinary_add_library(
  embedded_configuration
  "default_configuration"
  "${CMAKE_CURRENT_SOURCE_DIR}/Config.plist")
embedbinary_add_library(
  embedded_state
  "default_state"
  "${CMAKE_CURRENT_SOURCE_DIR}/State.plist")

# Found in Debian's gnustep-base-runtime. Using to keep plist format consistent
# with what GNUstep expects. Optional.
find_program(PLDES pldes)
if(PLDES)
  message("Found ${PLDES}. Adding consistency tests for GNUstep plist syntax.")

  # Sadly, the gnustep-base plist utilies don't return EXIT_FAILURE upon
  # parsing an invalid plist file. But eg. `pldes` reports the parsing error,
  # so we capture this as a failure.
  set(pldes_failure_regex ".*at\ line.*char")
  add_test(
    NAME root_menu_plist_test
    COMMAND pldes "${CMAKE_CURRENT_SOURCE_DIR}/RootMenu.plist")
  set_property(
    TEST root_menu_plist_test
    PROPERTY FAIL_REGULAR_EXPRESSION "${pldes_failure_regex}")
  add_test(
    NAME wlmaker_plist_test
    COMMAND pldes "${CMAKE_CURRENT_SOURCE_DIR}/Config.plist")
  set_property(
    TEST wlmaker_plist_test
    PROPERTY FAIL_REGULAR_EXPRESSION "${pldes_failure_regex}")
  add_test(
    NAME wlmaker_example_plist_test
    COMMAND pldes "${CMAKE_CURRENT_SOURCE_DIR}/ExampleConfig.plist")
  set_property(
    TEST wlmaker_example_plist_test
    PROPERTY FAIL_REGULAR_EXPRESSION "${pldes_failure_regex}")
  add_test(
    NAME wlmaker_home_plist_test
    COMMAND pldes "${CMAKE_CURRENT_SOURCE_DIR}/HomeConfig.plist")
  set_property(
    TEST wlmaker_home_plist_test
    PROPERTY FAIL_REGULAR_EXPRESSION "${pldes_failure_regex}")
  add_test(
    NAME wlmaker_state_plist_test
    COMMAND pldes "${CMAKE_CURRENT_SOURCE_DIR}/State.plist")
  set_property(
    TEST wlmaker_state_plist_test
    PROPERTY FAIL_REGULAR_EXPRESSION "${pldes_failure_regex}")
endif()
install(
  FILES
  RootMenu.plist
  State.plist
  Config.plist
  DESTINATION "${WLMAKER_INSTALL_CONFIG_DIR}")
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/RootMenuDebian.plist.in
  ${CMAKE_CURRENT_BINARY_DIR}/RootMenuDebian.plist
  @ONLY)
install(
  FILES
  ${CMAKE_CURRENT_BINARY_DIR}/RootMenuDebian.plist
  DESTINATION "${WLMAKER_INSTALL_CONFIG_DIR}")
