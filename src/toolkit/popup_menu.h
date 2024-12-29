/* ========================================================================= */
/**
 * @file popup_menu.h
 *
 * @copyright
 * Copyright 2024 Google LLC
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
#ifndef __WLMTK_POPUP_MENU_H__
#define __WLMTK_POPUP_MENU_H__

/** Forward declaration: State of a popup menu. */
typedef struct _wlmtk_popup_menu_t wlmtk_popup_menu_t;

#include "env.h"
#include "menu.h"
#include "popup.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Events of the popup menu. */
typedef struct {
    /** Popup menu requests to be closed. */
    struct wl_signal          request_close;
} wlmtk_popup_menu_events_t;

/**
 * Creates a popup menu.
 *
 * @param style_ptr
 * @param env_ptr
 *
 * @return Pointer to the popup menu handle or NULL on error.
 */
wlmtk_popup_menu_t *wlmtk_popup_menu_create(
    const wlmtk_menu_style_t *style_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Destroys the popup menu.
 *
 * @param popup_menu_ptr
 */
void wlmtk_popup_menu_destroy(wlmtk_popup_menu_t *popup_menu_ptr);

/** @return Pointer to @ref wlmtk_popup_menu_t::events. */
wlmtk_popup_menu_events_t *wlmtk_popup_menu_events(
    wlmtk_popup_menu_t *popup_menu_ptr);

/** Returns pointer to the popup menu's @ref wlmtk_popup_t superclass. */
wlmtk_popup_t *wlmtk_popup_menu_popup(wlmtk_popup_menu_t *popup_menu_ptr);

/** Returns the contained @ref wlmtk_menu_t. */
wlmtk_menu_t *wlmtk_popup_menu_menu(wlmtk_popup_menu_t *popup_menu_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_POPUP_MENU_H__ */
/* == End of popup_menu.h ================================================== */
