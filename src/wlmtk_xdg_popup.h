/* ========================================================================= */
/**
 * @file wlmtk_xdg_popup.h
 *
 * @copyright
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __WLMTK_XDG_POPUP_H__
#define __WLMTK_XDG_POPUP_H__

#include "toolkit/toolkit.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_xdg_shell.h>
#undef WLR_USE_UNSTABLE

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: State of the toolkit's XDG popup. */
typedef struct _wlmtk_xdg_popup_t wlmtk_xdg_popup_t;

/** State of toolkit popup. */
struct _wlmtk_xdg_popup_t {
    /** Super class: Content. */
    wlmtk_content_t           super_content;

    /** Surface of the popup. */
    wlmtk_surface_t           surface;
    /** The WLR popup. */
    struct wlr_xdg_popup      *wlr_xdg_popup_ptr;
};

/**
 * Creates a popup.
 *
 * @param wlr_xdg_popup_ptr
 * @param env_ptr
 */
wlmtk_xdg_popup_t *wlmtk_xdg_popup_create(
    struct wlr_xdg_popup *wlr_xdg_popup_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Destroys the popup.
 *
 * @param xdg_popup_ptr
 */
void wlmtk_xdg_popup_destroy(
    wlmtk_xdg_popup_t *xdg_popup_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_XDG_POPUP_H__ */
/* == End of wlmtk_xdg_popup.h ============================================= */
