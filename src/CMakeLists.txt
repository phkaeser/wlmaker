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

# Further dependency versions, as submodules:
# * drm at libdrm-2.4.117
# * hwdata at v0.371
# * libdisplay-info at 0.2.0
# * pixman at pixman-0.43.0

PKG_CHECK_MODULES(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server>=1.22.0)
# We build for wlroots 0.18.
PKG_CHECK_MODULES(WLROOTS IMPORTED_TARGET wlroots-0.18>=0.18)
PKG_CHECK_MODULES(XCB REQUIRED IMPORTED_TARGET xcb>=1.15)
# XWayland considered optional.
PKG_CHECK_MODULES(XWAYLAND xwayland>=22.1.9)

ADD_SUBDIRECTORY(backend)
ADD_SUBDIRECTORY(toolkit)

SET(PUBLIC_HEADER_FILES
  action.h
  action_item.h
  background.h
  backtrace.h
  clip.h
  config.h
  corner.h
  cursor.h
  dock.h
  icon_manager.h
  idle.h
  keyboard.h
  layer_panel.h
  layer_shell.h
  launcher.h
  lock_mgr.h
  root_menu.h
  server.h
  subprocess_monitor.h
  task_list.h
  tl_menu.h
  xdg_decoration.h
  xdg_popup.h
  xdg_shell.h
  xdg_toplevel.h
  xwl.h
  xwl_content.h
  xwl_popup.h
  xwl_toplevel.h)

ADD_LIBRARY(wlmaker_lib STATIC)
TARGET_SOURCES(wlmaker_lib PRIVATE
  action.c
  action_item.c
  background.c
  backtrace.c
  clip.c
  config.c
  corner.c
  cursor.c
  dock.c
  icon_manager.c
  idle.c
  keyboard.c
  launcher.c
  layer_panel.c
  layer_shell.c
  lock_mgr.c
  root_menu.c
  server.c
  subprocess_monitor.c
  task_list.c
  tl_menu.c
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
SET_TARGET_PROPERTIES(
  wlmaker_lib PROPERTIES
  VERSION 1.0
  PUBLIC_HEADER "${PUBLIC_HEADER_FILES}")
IF(iwyu_path_and_options)
  SET_TARGET_PROPERTIES(
    wlmaker_lib PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
ENDIF(iwyu_path_and_options)

ADD_DEPENDENCIES(
  wlmaker_lib
  backend
  embedded_configuration
  embedded_root_menu
  embedded_state
  embedded_style
  libbase
  libbase_plist
  protocol_headers
  toolkit
  wlmaker_lib)

TARGET_LINK_LIBRARIES(
  wlmaker_lib
  backend
  embedded_configuration
  embedded_root_menu
  embedded_state
  embedded_style
  libbase
  libbase_plist
  toolkit
  wlmaker_protocols
  PkgConfig::CAIRO
  PkgConfig::WAYLAND_SERVER
  PkgConfig::WLROOTS
  PkgConfig::XCB
  PkgConfig::XKBCOMMON)
TARGET_INCLUDE_DIRECTORIES(
  wlmaker_lib PUBLIC
  # Keep wlroots first -- multiple versions may interfere (#117).
  ${WLROOTS_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_BINARY_DIR}/third_party/protocols
  ${PROJECT_BINARY_DIR}/protocols
  ${CAIRO_INCLUDE_DIRS}
  ${WAYLAND_SERVER_INCLUDE_DIRS}
  ${XCB_INCLUDE_DIRS}
  ${XKBCOMMON_INCLUDE_DIRS}
)
TARGET_COMPILE_DEFINITIONS(
  wlmaker_lib PRIVATE
  WLMAKER_ICON_DATA_DIR="${CMAKE_INSTALL_FULL_DATAROOTDIR}/icons/wlmaker")
TARGET_COMPILE_DEFINITIONS(
  wlmaker_lib PRIVATE WLMAKER_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
TARGET_COMPILE_DEFINITIONS(
  wlmaker_lib PRIVATE WLMAKER_BINARY_DIR="${PROJECT_BINARY_DIR}")
TARGET_COMPILE_DEFINITIONS(
  wlmaker_lib PRIVATE WLMAKER_WMMENUGEN_PATH="${wmmenugen_path}")
IF(XWAYLAND_FOUND)
  TARGET_COMPILE_DEFINITIONS(wlmaker_lib PUBLIC WLMAKER_HAVE_XWAYLAND)
ENDIF(XWAYLAND_FOUND)

ADD_EXECUTABLE(wlmaker wlmaker.c)
ADD_DEPENDENCIES(wlmaker wlmaker_lib)
TARGET_COMPILE_OPTIONS(
  wlmaker PRIVATE
  ${WAYLAND_SERVER_CFLAGS}
  ${WAYLAND_SERVER_CFLAGS_OTHER})
TARGET_LINK_LIBRARIES(wlmaker PRIVATE libbase libbase_plist wlmaker_lib)
IF(LIBBACKTRACE)
  TARGET_COMPILE_DEFINITIONS(wlmaker_lib PRIVATE WLMAKER_HAVE_LIBBACKTRACE)
  TARGET_LINK_LIBRARIES(wlmaker PRIVATE ${LIBBACKTRACE})
ENDIF()
IF(iwyu_path_and_options)
  SET_TARGET_PROPERTIES(
    wlmaker PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
ENDIF(iwyu_path_and_options)

INSTALL(TARGETS wlmaker DESTINATION bin)
