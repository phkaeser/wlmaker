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

SET(SOURCES
  button.c
  cairo_util.c
  clip.c
  config.c
  cursor.c
  decorations.c
  dock_app.c
  dock.c
  icon_manager.c
  iconified.c
  idle.c
  interactive.c
  keyboard.c
  layer_panel.c
  layer_shell.c
  layer_surface.c
  lock_mgr.c
  menu.c
  menu_item.c
  output.c
  root.c
  server.c
  subprocess_monitor.c
  task_list.c
  tile.c
  tile_container.c
  view.c
  workspace.c
  xdg_decoration.c
  xdg_popup.c
  xdg_shell.c
  xdg_toplevel.c
  x11_cursor.xpm
  xwl.c
  xwl_content.c
  xwl_popup.c
  xwl_toplevel.c
)

SET(HEADERS
  button.h
  cairo_util.h
  clip.h
  config.h
  cursor.h
  decorations.h
  dock_app.h
  dock.h
  icon_manager.h
  iconified.h
  idle.h
  interactive.h
  keyboard.h
  layer_panel.h
  layer_shell.h
  layer_surface.h
  lock_mgr.h
  menu.h
  menu_item.h
  output.h
  root.h
  server.h
  subprocess_monitor.h
  task_list.h
  tile_container.h
  tile.h
  view.h
  workspace.h
  xdg_decoration.h
  xdg_popup.h
  xdg_shell.h
  xdg_toplevel.h
  xwl.h
  xwl_content.h
  xwl_popup.h
  xwl_toplevel.h
)

EmbedBinary_ADD_LIBRARY(
  embedded_configuration
  "default_configuration"
  "${PROJECT_SOURCE_DIR}/etc/wlmaker.plist")

ADD_EXECUTABLE(wlmaker wlmaker.c ${SOURCES} ${HEADERS})
ADD_DEPENDENCIES(wlmaker protocol_headers conf toolkit embedded_configuration)

TARGET_COMPILE_DEFINITIONS(
  wlmaker PRIVATE WLMAKER_ICON_DATA_DIR="${CMAKE_INSTALL_FULL_DATAROOTDIR}/icons/wlmaker")
TARGET_COMPILE_DEFINITIONS(
  wlmaker PRIVATE WLMAKER_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

TARGET_COMPILE_OPTIONS(
  wlmaker PRIVATE
  ${WAYLAND_CFLAGS}
  ${WAYLAND_CFLAGS_OTHER}
)
TARGET_INCLUDE_DIRECTORIES(
  wlmaker PRIVATE
  ${PROJECT_BINARY_DIR}/third_party/protocols
  ${PROJECT_BINARY_DIR}/protocols
  ${CAIRO_INCLUDE_DIRS}
  ${WAYLAND_INCLUDE_DIRS}
  ${WLROOTS_INCLUDE_DIRS}
  ${XCB_INCLUDE_DIRS}
  ${XKBCOMMON_INCLUDE_DIRS}
)

TARGET_LINK_LIBRARIES(
  wlmaker PRIVATE
  base
  conf
  toolkit
  embedded_configuration
  wlmaker_protocols
  PkgConfig::CAIRO
  PkgConfig::WAYLAND
  PkgConfig::WLROOTS
  PkgConfig::XCB
  PkgConfig::XKBCOMMON
)

ADD_EXECUTABLE(wlmaker_test wlmaker_test.c ${SOURCES} ${HEADERS})
ADD_DEPENDENCIES(wlmaker_test protocol_headers toolkit embedded_configuration)
TARGET_INCLUDE_DIRECTORIES(
  wlmaker_test PRIVATE
  ${PROJECT_BINARY_DIR}/third_party/protocols
  ${PROJECT_BINARY_DIR}/protocols
  ${CAIRO_INCLUDE_DIRS}
  ${WAYLAND_INCLUDE_DIRS}
  ${WLROOTS_INCLUDE_DIRS}
  ${XCB_INCLUDE_DIRS}
  ${XKBCOMMON_INCLUDE_DIRS}
)

TARGET_LINK_LIBRARIES(
  wlmaker_test PRIVATE
  base
  conf
  toolkit
  embedded_configuration
  wlmaker_protocols
  PkgConfig::CAIRO
  PkgConfig::WAYLAND
  PkgConfig::WLROOTS
  PkgConfig::XCB
  PkgConfig::XKBCOMMON
)
TARGET_COMPILE_DEFINITIONS(
  wlmaker_test PUBLIC TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/testdata")
TARGET_COMPILE_DEFINITIONS(
  wlmaker_test PRIVATE WLMAKER_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

ADD_TEST(NAME wlmaker_test COMMAND wlmaker_test)

INSTALL(TARGETS wlmaker DESTINATION bin)
