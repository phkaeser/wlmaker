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
  interactive.c
  keyboard.c
  layer_shell.c
  layer_surface.c
  menu.c
  menu_item.c
  output.c
  resizebar.c
  server.c
  subprocess_monitor.c
  task_list.c
  tile.c
  tile_container.c
  titlebar.c
  view.c
  wlmtk_xdg_popup.c
  wlmtk_xdg_toplevel.c
  workspace.c
  xdg_decoration.c
  xdg_popup.c
  xdg_shell.c
  xdg_toplevel.c
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
  interactive.h
  keyboard.h
  layer_shell.h
  layer_surface.h
  menu.h
  menu_item.h
  output.h
  resizebar.h
  server.h
  subprocess_monitor.h
  task_list.h
  tile_container.h
  tile.h
  titlebar.h
  view.h
  wlmtk_xdg_popup.h
  wlmtk_xdg_toplevel.h
  workspace.h
  xdg_decoration.h
  xdg_popup.h
  xdg_shell.h
  xdg_toplevel.h
)

ADD_EXECUTABLE(wlmaker wlmaker.c ${SOURCES} ${HEADERS})
ADD_DEPENDENCIES(wlmaker protocol_headers decorations toolkit)

TARGET_COMPILE_DEFINITIONS(
  wlmaker PRIVATE WLMAKER_ICON_DATA_DIR="${CMAKE_INSTALL_FULL_DATAROOTDIR}/icons/wlmaker")
TARGET_COMPILE_DEFINITIONS(
  wlmaker PRIVATE WLMAKER_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

TARGET_COMPILE_OPTIONS(
  wlmaker PRIVATE
  ${WAYLAND_CFLAGS}
  ${WAYLAND_CFLAGS_OTHER}
)
IF(config_TOOLKIT_PROTOTYPE)
  TARGET_COMPILE_DEFINITIONS(wlmaker PUBLIC ENABLE_TOOLKIT_PROTOTYPE)
ENDIF(config_TOOLKIT_PROTOTYPE)
TARGET_INCLUDE_DIRECTORIES(
  wlmaker PRIVATE
  ${PROJECT_BINARY_DIR}/third_party/protocols
  ${PROJECT_BINARY_DIR}/protocols
  ${CAIRO_INCLUDE_DIRS}
  ${LIBDRM_INCLUDE_DIRS}
  ${PIXMAN_INCLUDE_DIRS}
  ${WAYLAND_INCLUDE_DIRS}
  ${WLROOTS_INCLUDE_DIRS}
  ${XKBCOMMON_INCLUDE_DIRS}
)

TARGET_LINK_LIBRARIES(
  wlmaker PRIVATE
  base
  decorations
  wlmaker_protocols
  PkgConfig::CAIRO
  PkgConfig::LIBDRM
  PkgConfig::PIXMAN
  PkgConfig::WAYLAND
  PkgConfig::WLROOTS
  PkgConfig::XKBCOMMON
)

ADD_EXECUTABLE(wlmaker_test wlmaker_test.c ${SOURCES} ${HEADERS})
ADD_DEPENDENCIES(wlmaker_test protocol_headers decorations toolkit)
TARGET_INCLUDE_DIRECTORIES(
  wlmaker_test PRIVATE
  ${PROJECT_BINARY_DIR}/third_party/protocols
  ${PROJECT_BINARY_DIR}/protocols
  ${CAIRO_INCLUDE_DIRS}
  ${LIBDRM_INCLUDE_DIRS}
  ${PIXMAN_INCLUDE_DIRS}
  ${WAYLAND_INCLUDE_DIRS}
  ${WLROOTS_INCLUDE_DIRS}
  ${XKBCOMMON_INCLUDE_DIRS}
)

TARGET_LINK_LIBRARIES(
  wlmaker_test PRIVATE
  base
  decorations
  wlmaker_protocols
  PkgConfig::CAIRO
  PkgConfig::LIBDRM
  PkgConfig::PIXMAN
  PkgConfig::WAYLAND
  PkgConfig::WLROOTS
  PkgConfig::XKBCOMMON
)
TARGET_COMPILE_DEFINITIONS(
  wlmaker_test PUBLIC TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/testdata")
TARGET_COMPILE_DEFINITIONS(
  wlmaker_test PRIVATE WLMAKER_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

ADD_TEST(NAME wlmaker_test COMMAND wlmaker_test)

INSTALL(TARGETS wlmaker DESTINATION bin)
