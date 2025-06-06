/* ========================================================================= */
/**
 * @file xdg_shell.c
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

#include "xdg_shell.h"

#include <libbase/libbase.h>
#include <stdlib.h>
#include <wayland-util.h>
#define WLR_USE_UNSTABLE
#include <wlr/version.h>
#undef WLR_USE_UNSTABLE

#include "toolkit/toolkit.h"
#include "xdg_popup.h"
#include "xdg_toplevel.h"

/* == Declarations ========================================================= */

static void handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

#if WLR_VERSION_NUM >= (18 << 8)
static void handle_new_toplevel(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_new_popup(
    struct wl_listener *listener_ptr,
    void *data_ptr);
#else  // WLR_VERSION_NUM >= (18 << )8
static void handle_new_surface(
    struct wl_listener *listener_ptr,
    void *data_ptr);
#endif // WLR_VERSION_NUM >= (18 << 8)

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_xdg_shell_t *wlmaker_xdg_shell_create(wlmaker_server_t *server_ptr)
{
    wlmaker_xdg_shell_t *xdg_shell_ptr = logged_calloc(
        1, sizeof(wlmaker_xdg_shell_t));
    if (NULL == xdg_shell_ptr) return NULL;
    xdg_shell_ptr->server_ptr = server_ptr;

    xdg_shell_ptr->wlr_xdg_shell_ptr = wlr_xdg_shell_create(
        server_ptr->wl_display_ptr, 2);
    if (NULL == xdg_shell_ptr->wlr_xdg_shell_ptr) {
        wlmaker_xdg_shell_destroy(xdg_shell_ptr);
        return NULL;
    }

#if WLR_VERSION_NUM >= (18 << 8)
    wlmtk_util_connect_listener_signal(
        &xdg_shell_ptr->wlr_xdg_shell_ptr->events.new_toplevel,
        &xdg_shell_ptr->new_toplevel_listener,
        handle_new_toplevel);
    wlmtk_util_connect_listener_signal(
        &xdg_shell_ptr->wlr_xdg_shell_ptr->events.new_popup,
        &xdg_shell_ptr->new_popup_listener,
        handle_new_popup);
#else // WLR_VERSION_NUM >= (18 << 8)
    wlmtk_util_connect_listener_signal(
        &xdg_shell_ptr->wlr_xdg_shell_ptr->events.new_surface,
        &xdg_shell_ptr->new_surface_listener,
        handle_new_surface);
#endif // WLR_VERSION_NUM >= (18 << 8)
    wlmtk_util_connect_listener_signal(
        &xdg_shell_ptr->wlr_xdg_shell_ptr->events.destroy,
        &xdg_shell_ptr->destroy_listener,
        handle_destroy);

    return xdg_shell_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_xdg_shell_destroy(wlmaker_xdg_shell_t *xdg_shell_ptr)
{
    wl_list_remove(&xdg_shell_ptr->destroy_listener.link);
#if WLR_VERSION_NUM >= (18 << 8)
    wl_list_remove(&xdg_shell_ptr->new_popup_listener.link);
    wl_list_remove(&xdg_shell_ptr->new_toplevel_listener.link);
#else // WLR_VERSION_NUM >= (18 << 8)
    wl_list_remove(&xdg_shell_ptr->new_surface_listener.link);
#endif // WLR_VERSION_NUM >= (18 << 8)
    // Note: xdg_shell_ptr->wlr_xdg_shell_ptr is destroyed when the display
    // is destroyed.
    free(xdg_shell_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Event handler for the `destroy` signal raised by `wlr_xdg_shell`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_destroy(struct wl_listener *listener_ptr,
                    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_shell_t *xdg_shell_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xdg_shell_t, destroy_listener);

    wlmaker_xdg_shell_destroy(xdg_shell_ptr);
}

#if WLR_VERSION_NUM >= (18 << 8)
/* ------------------------------------------------------------------------- */
/**
 * Event handler for the `new_toplevel` signal raised by `wlr_xdg_shell`.
 *
 * @param listener_ptr
 * @param data_ptr            Points to the new struct wlr_xdg_toplevel.
 */
void handle_new_toplevel(struct wl_listener *listener_ptr,
                         void *data_ptr)
{
    wlmaker_xdg_shell_t *xdg_shell_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xdg_shell_t, new_toplevel_listener);
    struct wlr_xdg_toplevel *wlr_xdg_toplevel_ptr = data_ptr;

    wlmtk_window_t *window_ptr = wlmtk_window_create_from_xdg_toplevel(
        wlr_xdg_toplevel_ptr, xdg_shell_ptr->server_ptr);

    // TODO(kaeser@gubbe.ch): Handle errors.
    bs_log(BS_INFO, "XDG shell: Toolkit window %p for toplevel %p",
           window_ptr, wlr_xdg_toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Event handler for the `new_popup` signal raised by `wlr_xdg_shell`.
 *
 * @param listener_ptr
 * @param data_ptr            Points to the new struct wlr_xdg_popup.
 */
void handle_new_popup(struct wl_listener *listener_ptr,
                      void *data_ptr)
{
    wlmaker_xdg_shell_t *xdg_shell_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xdg_shell_t, new_popup_listener);
    struct wlr_xdg_popup *wlr_xdg_popup_ptr = data_ptr;

    if (NULL == wlr_xdg_popup_ptr->parent) {
        bs_log(BS_WARNING,
               "Unimplemented: XDG shell %p: Creating popup %p without parent",
               xdg_shell_ptr, wlr_xdg_popup_ptr);
    }
}

#else // WLR_VERSION_NUM >= (18 << 8)

/* ------------------------------------------------------------------------- */
/**
 * Event handler for the `new_surface` signal raised by `wlr_xdg_shell`.
  *
  * @param listener_ptr
  * @param data_ptr
  */
void handle_new_surface(struct wl_listener *listener_ptr,
                        void *data_ptr)
{
    struct wlr_xdg_surface *wlr_xdg_surface_ptr;
    wlmaker_xdg_shell_t *xdg_shell_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xdg_shell_t, new_surface_listener);
    wlr_xdg_surface_ptr = data_ptr;

    switch (wlr_xdg_surface_ptr->role) {
    case WLR_XDG_SURFACE_ROLE_POPUP:
        // We're dealing with popups separately -- via the `new_popup` signal
        // from the `wlr_xdg_surface` for popups as children of XDG shell
        // surfaces, respectively the `new_popup` signal of the
        // `wlr_scene_layer_surface` for popups of the WLR layer surface.
        break;

    case WLR_XDG_SURFACE_ROLE_TOPLEVEL:;
        wlmtk_window_t *window_ptr = wlmtk_window_create_from_xdg_toplevel(
            wlr_xdg_surface_ptr->toplevel, xdg_shell_ptr->server_ptr);
        bs_log(BS_INFO, "XDG shell: Toolkit window %p for surface %p",
               window_ptr, wlr_xdg_surface_ptr);
        break;

    default:
        bs_log(BS_ERROR, "Unhandled role: %d", wlr_xdg_surface_ptr->role);
    }
}

#endif // WLR_VERSION_NUM >= (18 << 8)

/* == End of xdg_shell.c =================================================== */
