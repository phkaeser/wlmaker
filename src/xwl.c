/* ========================================================================= */
/**
 * @file xwl.c
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
 * @internal
 * The current XWayland implementation is not very cleanly designed and should
 * be considered *experimental*.
 * TODO(kaeser@gubbe.ch): Re-design, once object model is updated.
 *
 * Known issues:
 *
 * * Scene graph API nodes for toplevel windows are created early. This leads
 *   to issues with ownership (cleanup?), stacking order, and when properties
 *   (position) are set. It'd be better to only create them when mapping a
 *   window (and destroying when unmapping).
 *
 * * Windows with parents are created as plain surfaces and don't clearly show
 *   their stacking order. Decorations may not get applied in all cases.
 *
 * * Stacking order is not tackled, eg. popups may appear below. Reproduce:
 *   Open `emacs`, click a menu, and hover over a menu item for the tooltip to
 *   appear. When moving across menus, the tooltip sometimes appears below the
 *   menu window.
 *
 * * Popups or dialogs may not be activated or focussed correctly. Reproduce:
 *   Open `emacs`, open the `File` menu, and `Visit New File...`. The dialogue
 *   does not accept mouse events. Moving the dialogue window moves the entire
 *   emacs window.
 *
 * * `modal` windows are not identified and treated as such.
 *
 * * Positioning of windows: Applications such as `gimp` are setting the main
 *   window's position based on the earlier application's status. We currently
 *   don't translate this to the toplevel window's position, but apply it to
 *   the surface within the tree => leading to a title bar that's oddly offset.
 *   Reproduce: Open a gimp menu, and view the tooltip being off.
 *
 * * The window types are not well understood. Eg. `gimp` menu tooltips are
 *   created as windows without parent. We can identify them as TOOLTIP windows
 *   that won't have a border; but we don't have a well-understood set of
 *   properties for the window types.
 */


#include "xwl.h"

#if defined(WLMAKER_HAVE_XWAYLAND)

#define WLR_USE_UNSTABLE
#include <wlr/xwayland/xwayland.h>
#undef WLR_USE_UNSTABLE

#endif  // defined(WLMAKER_HAVE_XWAYLAND)

#include <inttypes.h>
#include <libbase/libbase.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <xcb/xcb.h>

#include "backend/backend.h"
#include "toolkit/toolkit.h"
#include "x11_cursor.xpm"
#include "xwl_content.h"

/* == Declarations ========================================================= */

/** XWayland interface state. */
struct _wlmaker_xwl_t {
    /** Back-link to server. */
    wlmaker_server_t          *server_ptr;

#if defined(WLMAKER_HAVE_XWAYLAND)
    /** XWayland server and XWM. */
    struct wlr_xwayland       *wlr_xwayland_ptr;

    /** Listener for the `ready` signal raised by `wlr_xwayland`. */
    struct wl_listener        ready_listener;
    /** Listener for the `new_surface` signal raised by `wlr_xwayland`. */
    struct wl_listener        new_surface_listener;

    /** XCB atoms we consider relevant. */
    xcb_atom_t                xcb_atoms[XWL_MAX_ATOM_ID];
#endif  // defined(WLMAKER_HAVE_XWAYLAND)
};

#if defined(WLMAKER_HAVE_XWAYLAND)
static void handle_ready(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_new_surface(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/** Lookup map for some of XCB atom identifiers. */
static const char *xwl_atom_name_map[XWL_MAX_ATOM_ID] = {
    [NET_WM_WINDOW_TYPE_NORMAL] = "_NET_WM_WINDOW_TYPE_NORMAL",
    [NET_WM_WINDOW_TYPE_DIALOG] = "_NET_WM_WINDOW_TYPE_DIALOG",
    [NET_WM_WINDOW_TYPE_UTILITY] = "_NET_WM_WINDOW_TYPE_UTILITY",
    [NET_WM_WINDOW_TYPE_TOOLBAR] = "_NET_WM_WINDOW_TYPE_TOOLBAR",
    [NET_WM_WINDOW_TYPE_SPLASH] = "_NET_WM_WINDOW_TYPE_SPLASH",
    [NET_WM_WINDOW_TYPE_MENU] = "_NET_WM_WINDOW_TYPE_MENU",
    [NET_WM_WINDOW_TYPE_DROPDOWN_MENU] = "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
    [NET_WM_WINDOW_TYPE_POPUP_MENU] = "_NET_WM_WINDOW_TYPE_POPUP_MENU",
    [NET_WM_WINDOW_TYPE_TOOLTIP] = "_NET_WM_WINDOW_TYPE_TOOLTIP",
    [NET_WM_WINDOW_TYPE_NOTIFICATION] = "_NET_WM_WINDOW_TYPE_NOTIFICATION",
};

/* == Exported methods ===================================================== */

#endif  // defined(WLMAKER_HAVE_XWAYLAND)

/* ------------------------------------------------------------------------- */
wlmaker_xwl_t *wlmaker_xwl_create(wlmaker_server_t *server_ptr)
{
    wlmaker_xwl_t *xwl_ptr = logged_calloc(1, sizeof(wlmaker_xwl_t));
    if (NULL == xwl_ptr) return NULL;
    xwl_ptr->server_ptr = server_ptr;

#if defined(WLMAKER_HAVE_XWAYLAND)
    xwl_ptr->wlr_xwayland_ptr = wlr_xwayland_create(
        server_ptr->wl_display_ptr,
        wlmbe_backend_compositor(server_ptr->backend_ptr),
        false);
    if (NULL == xwl_ptr->wlr_xwayland_ptr) {
        bs_log(BS_ERROR, "Failed wlr_xwayland_create(%p, %p, false).",
               server_ptr->wl_display_ptr,
               wlmbe_backend_compositor(server_ptr->backend_ptr));
        wlmaker_xwl_destroy(xwl_ptr);
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &xwl_ptr->wlr_xwayland_ptr->events.ready,
        &xwl_ptr->ready_listener,
        handle_ready);
    wlmtk_util_connect_listener_signal(
        &xwl_ptr->wlr_xwayland_ptr->events.new_surface,
        &xwl_ptr->new_surface_listener,
        handle_new_surface);

    // TODO(kaeser@gubbe.ch): That's a bit ugly. We should only do a setenv
    // as we create & fork the subprocesses. Needs infrastructure, though.
    setenv("DISPLAY", xwl_ptr->wlr_xwayland_ptr->display_name, true);
#endif  // defined(WLMAKER_HAVE_XWAYLAND)

    return xwl_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_xwl_destroy(wlmaker_xwl_t *xwl_ptr)
{
#if defined(WLMAKER_HAVE_XWAYLAND)
    if (NULL != xwl_ptr->wlr_xwayland_ptr) {
        wlr_xwayland_destroy(xwl_ptr->wlr_xwayland_ptr);
        xwl_ptr->wlr_xwayland_ptr = NULL;
    }
#endif  // defined(WLMAKER_HAVE_XWAYLAND)

    free(xwl_ptr);
}

#if defined(WLMAKER_HAVE_XWAYLAND)
/* ------------------------------------------------------------------------- */
/**
 * Returns whether the XWayland surface has any of the window types.
 *
 * @param xwl_ptr
 * @param wlr_xwayland_surface_ptr
 * @param atom_identifiers    NULL-terminated set of window type we're looking
 *                            for.
 *
 * @return Whether `atom_identifiers` is in any of the window types.
 */
bool xwl_is_window_type(
    wlmaker_xwl_t *xwl_ptr,
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr,
    const xwl_atom_identifier_t *atom_identifiers)
{
    for (; *atom_identifiers < XWL_MAX_ATOM_ID; ++atom_identifiers) {
        for (size_t i = 0;
             i < wlr_xwayland_surface_ptr->window_type_len;
             ++i) {
            if (wlr_xwayland_surface_ptr->window_type[i] ==
                xwl_ptr->xcb_atoms[*atom_identifiers]) {
                return true;
            }
        }
    }
    return false;
}

/* ------------------------------------------------------------------------- */
const char *xwl_atom_name(
    wlmaker_xwl_t *xwl_ptr,
    xcb_atom_t atom)
{
    for (size_t atom_idx = 0; atom_idx < XWL_MAX_ATOM_ID; ++atom_idx) {
        if (xwl_ptr->xcb_atoms[atom_idx] == atom) {
            return xwl_atom_name_map[atom_idx];
        }
    }
    return NULL;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Event handler for the `ready` signal raised by `wlr_xwayland`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_ready(struct wl_listener *listener_ptr,
                  __UNUSED__ void *data_ptr)
{
    wlmaker_xwl_t *xwl_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_t, ready_listener);

    xcb_connection_t *xcb_connection_ptr = xcb_connect(
        xwl_ptr->wlr_xwayland_ptr->display_name, NULL);
    int error = xcb_connection_has_error(xcb_connection_ptr);
    if (0 != error) {
        bs_log(BS_ERROR, "Failed xcb_connect(%s, NULL): %d",
               xwl_ptr->wlr_xwayland_ptr->display_name, error);
        return;
    }

    xcb_intern_atom_cookie_t atom_cookies[XWL_MAX_ATOM_ID];
    for (size_t i = 0; i < XWL_MAX_ATOM_ID; ++i) {
        const char *name_ptr = xwl_atom_name_map[i];
        atom_cookies[i] = xcb_intern_atom(
            xcb_connection_ptr, 0, strlen(name_ptr), name_ptr);
    }

    for (size_t i = 0; i < XWL_MAX_ATOM_ID; ++i) {
        xcb_generic_error_t *error_ptr = NULL;
        xcb_intern_atom_reply_t *atom_reply_ptr = xcb_intern_atom_reply(
            xcb_connection_ptr, atom_cookies[i], &error_ptr);
        if (NULL != atom_reply_ptr) {
            if (NULL == error_ptr) {
                xwl_ptr->xcb_atoms[i] = atom_reply_ptr->atom;
                bs_log(BS_DEBUG, "XCB lookup on %s: atom %s = 0x%"PRIx32,
                       xwl_ptr->wlr_xwayland_ptr->display_name,
                       xwl_atom_name_map[i],
                       atom_reply_ptr->atom);
            }
            free(atom_reply_ptr);
        }

        if (NULL != error_ptr) {
            bs_log(BS_ERROR, "Failed xcb_intern_atom_reply(%p, %s, %p): %d",
                   xcb_connection_ptr, xwl_atom_name_map[i],
                   &error_ptr, error_ptr->error_code);
            free(error_ptr);
            break;
        }
    }

    xcb_disconnect(xcb_connection_ptr);

    // Sets the default cursor to use for XWayland surfaces, unless overrideen.
    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_xpm_create_from_data(x11_cursor_xpm);
    if (NULL != gfxbuf_ptr) {
        wlr_xwayland_set_cursor(
            xwl_ptr->wlr_xwayland_ptr,
            (uint8_t*)gfxbuf_ptr->data_ptr,
            gfxbuf_ptr->pixels_per_line * sizeof(uint32_t),
            gfxbuf_ptr->width, gfxbuf_ptr->height,
            0, 0);
        bs_gfxbuf_destroy(gfxbuf_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Event handler for the `new_surface` signal raised by `wlr_xwayland`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_new_surface(struct wl_listener *listener_ptr,
                        void *data_ptr)
{
    wlmaker_xwl_t *xwl_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xwl_t, new_surface_listener);
    struct wlr_xwayland_surface *wlr_xwayland_surface_ptr = data_ptr;

    wlmaker_xwl_content_t *xwl_content_ptr = wlmaker_xwl_content_create(
        wlr_xwayland_surface_ptr,
        xwl_ptr,
        xwl_ptr->server_ptr);
    if (NULL == xwl_content_ptr) {
        bs_log(BS_ERROR, "Failed wlmaker_xwl_content_create(%p)",
               wlr_xwayland_surface_ptr);
    }
}

#endif  // defined(WLMAKER_HAVE_XWAYLAND)
/* == End of xwl.c ========================================================= */
