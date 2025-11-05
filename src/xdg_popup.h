/* ========================================================================= */
/**
 * @file xdg_popup.h
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
#ifndef __XDG_POPUP_H__
#define __XDG_POPUP_H__

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_xdg_shell.h>
#undef WLR_USE_UNSTABLE

#include "toolkit/toolkit.h"

struct wlr_seat;
struct wlr_xdg_popup;

/** Forward declaration: State of the toolkit's XDG popup. */
typedef struct _wlmaker_xdg_popup_t wlmaker_xdg_popup_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a popup.
 *
 * @param wlr_xdg_popup_ptr
 * @param wlr_seat_ptr
 *
 * @return Popup handle or NULL on error.
 */
wlmaker_xdg_popup_t *wlmaker_xdg_popup_create(
    struct wlr_xdg_popup *wlr_xdg_popup_ptr,
    struct wlr_seat *wlr_seat_ptr);

/**
 * Destroys the popup.
 *
 * @param wlmaker_xdg_popup_ptr
 */
void wlmaker_xdg_popup_destroy(
    wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr);

/** Returns the superclass element. */
wlmtk_element_t *wlmaker_xdg_popup_element(
    wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __XDG_POPUP_H__ */
/* == End of xdg_popup.h =================================================== */
