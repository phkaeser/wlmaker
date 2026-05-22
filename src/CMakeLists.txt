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

# Further dependency versions, as submodules:
# * drm at libdrm-2.4.117
# * hwdata at v0.371
# * libdisplay-info at 0.2.0
# * pixman at pixman-0.43.0
pkg_check_modules(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server>=1.22.0)
# We build for wlroots 0.18, 0.19 and (preparing for) 0.20.
pkg_check_modules(WLROOTS IMPORTED_TARGET wlroots-0.20>=0.20)
if(NOT WLROOTS_FOUND)
  pkg_check_modules(WLROOTS IMPORTED_TARGET wlroots-0.19>=0.19)
  if(NOT WLROOTS_FOUND)
    pkg_check_modules(WLROOTS REQUIRED IMPORTED_TARGET wlroots-0.18>=0.18)
  endif()
endif()
pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb>=1.15)
# XWayland considered optional.
pkg_check_modules(XWAYLAND xwayland>=22.1.9)
add_subdirectory(backend)
add_subdirectory(input)
add_subdirectory(toolkit)
add_subdirectory(util)
add_subdirectory(wlclient)
add_subdirectory(dock)
set(public_header_files
  action.h
  action_item.h
  background.h
  clip.h
  config.h
  corner.h
  dock.h
  icon_manager.h
  idle.h
  input_observation.h
  launcher.h
  layer_panel.h
  layer_shell.h
  lock_mgr.h
  root_menu.h
  server.h
  task_list.h
  tl_menu.h
  xdg_decoration.h
  xdg_popup.h
  xdg_shell.h
  xdg_toplevel.h
  xwl.h
  xwl_surface.h)
add_library(wlmaker_lib STATIC)
target_sources(wlmaker_lib PRIVATE
  action.c
  action_item.c
  background.c
  clip.c
  config.c
  corner.c
  dock.c
  icon_manager.c
  idle.c
  input_observation.c
  launcher.c
  layer_panel.c
  layer_shell.c
  lock_mgr.c
  root_menu.c
  server.c
  task_list.c
  tl_menu.c
  x11_cursor.xpm
  xdg_decoration.c
  xdg_popup.c
  xdg_shell.c
  xdg_toplevel.c
  xwl.c
  xwl_surface.c
)
set_target_properties(
  wlmaker_lib PROPERTIES
  VERSION 1.0
  PUBLIC_HEADER "${public_header_files}")
if(iwyu_path_and_options)
  set_target_properties(
    wlmaker_lib PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
endif()
add_dependencies(
  wlmaker_lib
  backend
  embedded_configuration
  embedded_root_menu
  embedded_state
  embedded_theme
  input
  libbase
  libbase_plist
  protocol_headers
  toolkit
  wlm_util_lib
  wlmaker_lib)
target_link_libraries(
  wlmaker_lib
  backend
  embedded_configuration
  embedded_root_menu
  embedded_state
  embedded_theme
  libbase
  libbase_plist
  input
  toolkit
  wlmaker_protocols
  wlm_util_lib
  PkgConfig::CAIRO
  PkgConfig::WAYLAND_SERVER
  PkgConfig::WLROOTS
  PkgConfig::XCB)
target_include_directories(
  wlmaker_lib PUBLIC
  # Keep wlroots first -- multiple versions may interfere (#117).
  "${WLROOTS_INCLUDE_DIRS}"
  "${PROJECT_SOURCE_DIR}/include"
  "${PROJECT_BINARY_DIR}/third_party/protocols"
  "${PROJECT_BINARY_DIR}/protocols"
  "${CAIRO_INCLUDE_DIRS}"
  "${WAYLAND_SERVER_INCLUDE_DIRS}"
  "${XCB_INCLUDE_DIRS}"
)
target_compile_definitions(
  wlmaker_lib PRIVATE "WLMAKER_SOURCE_DIR=\"${PROJECT_SOURCE_DIR}\"")
target_compile_definitions(
  wlmaker_lib PRIVATE "WLMAKER_BINARY_DIR=\"${PROJECT_BINARY_DIR}\"")
if(wmmenugen_executable)
  target_compile_definitions(
    wlmaker_lib PRIVATE "WLMAKER_WMMENUGEN_PATH=\"${wmmenugen_executable}\"")
endif()
if(XWAYLAND_FOUND)
  target_compile_definitions(wlmaker_lib PUBLIC WLMAKER_HAVE_XWAYLAND)
endif()
add_executable(
  wlmaker
  wlmaker.c)
add_dependencies(wlmaker wlmaker_lib)
target_compile_options(
  wlmaker PRIVATE
  "${WAYLAND_SERVER_CFLAGS}"
  "${WAYLAND_SERVER_CFLAGS_OTHER}")
target_compile_definitions(
  wlmaker PRIVATE
  "WLMAKER_VERSION_MAJOR=\"${WLMAKER_VERSION_MAJOR}\""
  "WLMAKER_VERSION_MINOR=\"${WLMAKER_VERSION_MINOR}\""
  "WLMAKER_VERSION_FULL=\"${WLMAKER_VERSION_FULL}\"")
if("${WLMAKER_VERSION_PATCH}")
  target_compile_definitions(
    wlmaker PRIVATE
    "WLMAKER_VERSION_PATCH=\"${WLMAKER_VERSION_PATCH}\"")
endif()
target_link_libraries(
  wlmaker
  PRIVATE
  libbase
  libbase_plist
  wlmaker_lib)
if(iwyu_path_and_options)
  set_target_properties(
    wlmaker PROPERTIES
    C_INCLUDE_WHAT_YOU_USE "${iwyu_path_and_options}")
endif()
install(TARGETS wlmaker DESTINATION "${CMAKE_INSTALL_BINDIR}")
