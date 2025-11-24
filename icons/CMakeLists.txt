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

INSTALL(FILES
  wlmaker.svg
  DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps)
INSTALL(FILES
  wlmaker-48x48.png
  DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/48x48/apps
  RENAME wlmaker.png)
INSTALL(FILES
  wlmaker-64x64.png
  DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/64x64/apps
  RENAME wlmaker.png)

INSTALL(FILES
  chrome-48x48.png
  clip-48x48.png
  firefox-48x48.png
  terminal-48x48.png
  wlmclock-64x64.png
  DESTINATION ${CMAKE_INSTALL_PREFIX}/share/wlmaker)
