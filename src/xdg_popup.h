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
 *
 * Handlers for creating XDG popup surfaces, children to a XDG shell toplevel
 * or a layer shell V1 surface.
 */
#ifndef __XDG_POPUP_H__
#define __XDG_POPUP_H__

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#undef WLR_USE_UNSTABLE

/** Forward declaration of the XDG popup handle. */
typedef struct _wlmaker_xdg_popup_t wlmaker_xdg_popup_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates an XDG popup. Both for XDG shell as also for layer shells.
 *
 * @param wlr_xdg_popup_ptr
 * @param parent_wlr_scene_tree_ptr
 *
 * @return Handle for the XDG popup.
 */
wlmaker_xdg_popup_t *wlmaker_xdg_popup_create(
    struct wlr_xdg_popup *wlr_xdg_popup_ptr,
    struct wlr_scene_tree *parent_wlr_scene_tree_ptr);

/**
 * Destroys the XDG popup.
 *
 * @param xdg_popup_ptr
 */
void wlmaker_xdg_popup_destroy(wlmaker_xdg_popup_t *xdg_popup_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __XDG_POPUP_H__ */
/* == End of xdg_popup.h =================================================== */
