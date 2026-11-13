/* ========================================================================= */
/**
 * @file jetty.c
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser <kaeser@gubbe.ch>
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

#include "jetty.h"

#include "tilebox.h"
#include "subcompositor.h"


/* == Declarations ========================================================= */

/** State of the jetty. */
struct wlmdock_jetty {
    /** Tilebox, where tiles are docked in. */
    // TODO(kaeser@gubbe.ch): Consider merging wlmdock_tilebox_t into here.
    wlmdock_tilebox_t         *tilebox_ptr;

    /** The subcompositor running this jetty. */
    wlmdock_subcompositor_t   *subcompositor_ptr;
};

/* == Data ================================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
struct wlmdock_jetty *wlmdock_jetty_create(
    struct wl_display *wl_display_ptr,
    struct wlr_backend *wlr_backend_ptr,
    wlmcl_client_t *client_ptr,
    struct zwlr_layer_shell_v1 *zwlr_layer_shell_ptr,
    const struct wlmtk_dock_style *dock_style_ptr,
    struct wlmim_cursor_style *cursor_style_ptr,
    const wlmtk_dock_positioning_t *positioning_ptr)
{
    struct wlmdock_jetty *jetty_ptr = logged_calloc(1, sizeof(*jetty_ptr));
    if (NULL == jetty_ptr) return NULL;

    // FIXME: orientation and positioning must be configured.
    wlmtk_box_orientation_t orientation = WLMTK_BOX_VERTICAL;
    if (positioning_ptr->edge == WLR_EDGE_TOP ||
        positioning_ptr->edge == WLR_EDGE_BOTTOM) {
        orientation = WLMTK_BOX_HORIZONTAL;
    }
    jetty_ptr->tilebox_ptr = wlmdock_tilebox_create(
        orientation,
        (positioning_ptr->anchor == WLR_EDGE_BOTTOM ||
         positioning_ptr->anchor == WLR_EDGE_RIGHT),
        dock_style_ptr);
    if (NULL == jetty_ptr->tilebox_ptr) {
        wlmdock_jetty_destroy(jetty_ptr);
        return NULL;
    }

    jetty_ptr->subcompositor_ptr = wlmdock_subcompositor_create(
        wl_display_ptr, wlr_backend_ptr, client_ptr, zwlr_layer_shell_ptr,
        cursor_style_ptr, wlmdock_tilebox_container(jetty_ptr->tilebox_ptr),
        positioning_ptr);
    if (NULL == jetty_ptr->subcompositor_ptr) {
        wlmdock_jetty_destroy(jetty_ptr);
        return NULL;
    }

    return jetty_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmdock_jetty_destroy(struct wlmdock_jetty *jetty_ptr)
{
    if (NULL != jetty_ptr->subcompositor_ptr) {
        wlmdock_subcompositor_destroy(jetty_ptr->subcompositor_ptr);
        jetty_ptr->subcompositor_ptr = NULL;
    }

    if (NULL != jetty_ptr->tilebox_ptr) {
        wlmdock_tilebox_destroy(jetty_ptr->tilebox_ptr);
        jetty_ptr->tilebox_ptr = NULL;
    }

    free(jetty_ptr);
}

/* == Local (static) methods =============================================== */

/* == Unit Tests =========================================================== */

/* == End of jetty.c ======================================================= */
