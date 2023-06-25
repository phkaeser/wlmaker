/* ========================================================================= */
/**
 * @file iconified.c
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

#include "iconified.h"

#include "config.h"
#include "decorations.h"
#include "interactive.h"
#include "toolkit/gfxbuf.h"

#include <libbase/libbase.h>
#include <linux/input-event-codes.h>

/* == Declarations ========================================================= */

/** State of an iconified. */
struct _wlmaker_iconified_t {
    /** Links to the @ref wlmaker_view_t that is shown as iconified. */
    wlmaker_view_t            *view_ptr;

    /** As an element of @ref wlmaker_tile_container_t `tiles`. */
    bs_dllist_node_t          dlnode;

    /** WLR gfx buffer, where the iconified tile is drawn into. */
    struct wlr_buffer         *wlr_buffer_ptr;

    /** Buffer scene node. Visualization of the iconified app. */
    struct wlr_scene_buffer   *wlr_scene_buffer_ptr;
    /**
     * Helper: Which node to use for interaction. For the iconified, this
     * is &wlr_scene_buffer_ptr->node. For the prototype dockapp, it's the
     * node of the wlr_scene_tree_ptr.
     * TODO(kaeser@gubbe.ch): Elinminate, once prototype gone.
     */
    struct wlr_scene_node     *node_ptr;

    /** Corresponding iteractive. */
    wlmaker_interactive_t     interactive;
};

/** Prototype: A DockApp, camouflaged as iconified. TODO: eliminate. */
struct _wlmaker_dockapp_iconified_t {
    /** The iconified it camouflages. */
    wlmaker_iconified_t       iconified;
    /** Scene tree, holding the tile, and the surface. */
    struct wlr_scene_tree     *wlr_scene_tree_ptr;
};

static wlmaker_iconified_t *iconified_from_interactive(
    wlmaker_interactive_t *interactive_ptr);

static void _iconified_enter(
    wlmaker_interactive_t *interactive_ptr);
static void _iconified_leave(
    wlmaker_interactive_t *interactive_ptr);
static void _iconified_motion(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y);
static void _iconified_focus(
    wlmaker_interactive_t *interactive_ptr);
static void _iconified_button(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y,
    struct wlr_pointer_button_event *wlr_pointer_button_event_ptr);
static void _iconified_destroy(wlmaker_interactive_t *interactive_ptr);

/* == Data ================================================================= */

/** Handler implementation of the @ref wlmaker_interactive_t for iconified. */
const wlmaker_interactive_impl_t iconified_interactive_impl = {
    .enter = _iconified_enter,
    .leave = _iconified_leave,
    .motion = _iconified_motion,
    .focus = _iconified_focus,
    .button = _iconified_button,
    .destroy = _iconified_destroy
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
/** Prototype: Creates an iconified as DockApp. */
// TODO(kaeser@gubbe.ch): Remove, once designed and implemented properly.  */
wlmaker_dockapp_iconified_t *wlmaker_dockapp_iconified_create(
    wlmaker_server_t *server_ptr)
{
    wlmaker_dockapp_iconified_t *dai_ptr = logged_calloc(
        1, sizeof(wlmaker_dockapp_iconified_t));
    if (NULL == dai_ptr) return NULL;
    dai_ptr->iconified.view_ptr = NULL;

    dai_ptr->iconified.wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(64, 64);
    if (NULL == dai_ptr->iconified.wlr_buffer_ptr) {
        wlmaker_dockapp_iconified_destroy(dai_ptr);
        return NULL;
    }
    cairo_t *cairo_ptr = cairo_create_from_wlr_buffer(
        dai_ptr->iconified.wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        wlmaker_dockapp_iconified_destroy(dai_ptr);
        return NULL;
    }

    const wlmaker_style_fill_t fill = {
        .type = WLMAKER_STYLE_COLOR_DGRADIENT,
        .param = { .hgradient = { .from = 0xff767686,.to = 0xff313541 }}
    };
    wlmaker_decorations_draw_tile(cairo_ptr, &fill, false);
    cairo_destroy(cairo_ptr);

    dai_ptr->wlr_scene_tree_ptr = wlr_scene_tree_create(
        &server_ptr->void_wlr_scene_ptr->tree);
    if (NULL == dai_ptr->wlr_scene_tree_ptr) {
        wlmaker_dockapp_iconified_destroy(dai_ptr);
        return NULL;
    }
    dai_ptr->iconified.node_ptr = &dai_ptr->wlr_scene_tree_ptr->node;

    // We'll want to create a node. And add this node to ... a "tile_holder".
    dai_ptr->iconified.wlr_scene_buffer_ptr = wlr_scene_buffer_create(
        dai_ptr->wlr_scene_tree_ptr,
        dai_ptr->iconified.wlr_buffer_ptr);
    if (NULL == dai_ptr->iconified.wlr_scene_buffer_ptr) {
        wlmaker_dockapp_iconified_destroy(dai_ptr);
        return NULL;
    }

    wlr_scene_node_set_enabled(
        &dai_ptr->iconified.wlr_scene_buffer_ptr->node,
        true);

    wlmaker_interactive_init(
        &dai_ptr->iconified.interactive,
        &iconified_interactive_impl,
        dai_ptr->iconified.wlr_scene_buffer_ptr,
        server_ptr->cursor_ptr,
        dai_ptr->iconified.wlr_buffer_ptr);

    return dai_ptr;
}

/* ------------------------------------------------------------------------- */
/** Prototype: Destroys the iconified as DockApp. */
// TODO(kaeser@gubbe.ch): Remove, once designed and implemented properly.  */
void wlmaker_dockapp_iconified_destroy(wlmaker_dockapp_iconified_t *dai_ptr)
{
    if (NULL != dai_ptr->iconified.wlr_scene_buffer_ptr) {
        wlr_scene_node_destroy(
            &dai_ptr->iconified.wlr_scene_buffer_ptr->node);
        dai_ptr->iconified.wlr_scene_buffer_ptr = NULL;
    }

    if (NULL != dai_ptr->wlr_scene_tree_ptr) {
        wlr_scene_node_destroy(
            &dai_ptr->wlr_scene_tree_ptr->node);
        dai_ptr->wlr_scene_tree_ptr = NULL;
    }

    if (NULL != dai_ptr->iconified.wlr_buffer_ptr) {
        wlr_buffer_drop(dai_ptr->iconified.wlr_buffer_ptr);
        dai_ptr->iconified.wlr_buffer_ptr = NULL;
    }

    free(dai_ptr);
}

/* ------------------------------------------------------------------------- */
/** Prototype: Gets the iconified from the DockApp. */
// TODO(kaeser@gubbe.ch): Remove, once designed and implemented properly.  */
wlmaker_iconified_t *wlmaker_iconified_from_dockapp(
    wlmaker_dockapp_iconified_t *dai_ptr)
{
    return &dai_ptr->iconified;
}

/* ------------------------------------------------------------------------- */
/** Prototype: Attaches a surface to the DockApp. */
// TODO(kaeser@gubbe.ch): Remove, once designed and implemented properly.  */
void wlmaker_dockapp_iconified_attach(
    wlmaker_dockapp_iconified_t *dai_ptr,
    struct wlr_surface *wlr_surface_ptr)
{
    struct wlr_scene_surface *wlr_scene_surface_ptr = wlr_scene_surface_create(
        dai_ptr->wlr_scene_tree_ptr,
        wlr_surface_ptr);

    wlr_scene_surface_ptr = wlr_scene_surface_ptr;
}

/* ------------------------------------------------------------------------- */
wlmaker_iconified_t *wlmaker_iconified_create(
    wlmaker_view_t *view_ptr)
{
    wlmaker_iconified_t *iconified_ptr = logged_calloc(
        1, sizeof(wlmaker_iconified_t));
    if (NULL == iconified_ptr) return NULL;
    iconified_ptr->view_ptr = view_ptr;
    // TODO(kaeser@gubbe.ch): Ugly, need to refactor.
    view_ptr->iconified_ptr = iconified_ptr;

    iconified_ptr->wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(64, 64);
    if (NULL == iconified_ptr->wlr_buffer_ptr) {
        wlmaker_iconified_destroy(iconified_ptr);
        return NULL;
    }
    cairo_t *cairo_ptr = cairo_create_from_wlr_buffer(
        iconified_ptr->wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        wlmaker_iconified_destroy(iconified_ptr);
        return NULL;
    }

    wlmaker_decorations_draw_tile(
        cairo_ptr,
        &wlmaker_config_theme.tile_fill,
        false);
    const char *title_ptr = wlmaker_view_get_title(view_ptr);
    wlmaker_decorations_draw_iconified(
        cairo_ptr,
        &wlmaker_config_theme.iconified_title_fill,
        wlmaker_config_theme.iconified_title_color,
        title_ptr ? title_ptr : "Unnamed Window");

    cairo_destroy(cairo_ptr);

    // We'll want to create a node. And add this node to ... a "tile_holder".
    iconified_ptr->wlr_scene_buffer_ptr = wlr_scene_buffer_create(
        &view_ptr->server_ptr->void_wlr_scene_ptr->tree,
        iconified_ptr->wlr_buffer_ptr);
    if (NULL == iconified_ptr->wlr_scene_buffer_ptr) {
        wlmaker_iconified_destroy(iconified_ptr);
        return NULL;
    }
    iconified_ptr->node_ptr = &iconified_ptr->wlr_scene_buffer_ptr->node;

    wlr_scene_node_set_enabled(
        &iconified_ptr->wlr_scene_buffer_ptr->node,
        true);

    wlmaker_interactive_init(
        &iconified_ptr->interactive,
        &iconified_interactive_impl,
        iconified_ptr->wlr_scene_buffer_ptr,
        view_ptr->server_ptr->cursor_ptr,
        iconified_ptr->wlr_buffer_ptr);

    view_ptr->iconified_ptr = iconified_ptr;
    return iconified_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_iconified_destroy(wlmaker_iconified_t *iconified_ptr)
{
    if (NULL != iconified_ptr->wlr_scene_buffer_ptr) {
        wlr_scene_node_destroy(
            &iconified_ptr->wlr_scene_buffer_ptr->node);
        iconified_ptr->wlr_scene_buffer_ptr = NULL;
    }

    if (NULL != iconified_ptr->wlr_buffer_ptr) {
        wlr_buffer_drop(iconified_ptr->wlr_buffer_ptr);
        iconified_ptr->wlr_buffer_ptr = NULL;
    }

    if (NULL != iconified_ptr->view_ptr) {
        iconified_ptr->view_ptr->iconified_ptr = NULL;
    }
    free(iconified_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_iconified_set_position(
    wlmaker_iconified_t *iconified_ptr,
    uint32_t x, uint32_t y)
{
    wlr_scene_node_set_position(iconified_ptr->node_ptr, x, y);
}

/* ------------------------------------------------------------------------- */
wlmaker_view_t *wlmaker_view_from_iconified(
    wlmaker_iconified_t *iconified_ptr)
{
    return iconified_ptr->view_ptr;
}

/* ------------------------------------------------------------------------- */
bs_dllist_node_t *wlmaker_dlnode_from_iconified(
    wlmaker_iconified_t *iconified_ptr)
{
    return &iconified_ptr->dlnode;
}

/* ------------------------------------------------------------------------- */
bs_avltree_node_t *wlmaker_avlnode_from_iconified(
    wlmaker_iconified_t *iconified_ptr)
{
    return &iconified_ptr->interactive.avlnode;
}

/* ------------------------------------------------------------------------- */
struct wlr_scene_node *wlmaker_wlr_scene_node_from_iconified(
    wlmaker_iconified_t *iconified_ptr)
{
    return iconified_ptr->node_ptr;
}

/* ------------------------------------------------------------------------- */
struct wlr_scene_node *wlmaker_wlr_scene_node_from_iconified_scene_buffer(
    wlmaker_iconified_t *iconified_ptr)
{
    return &iconified_ptr->wlr_scene_buffer_ptr->node;
}

/* ------------------------------------------------------------------------- */
wlmaker_iconified_t *wlmaker_iconified_from_dlnode(
    bs_dllist_node_t *dlnode_ptr)
{
    return BS_CONTAINER_OF(dlnode_ptr, wlmaker_iconified_t, dlnode);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Cast: Get the @ref wlmaker_iconified_t from the pointer to `interactive`.
 *
 * @param interactive_ptr
 *
 * @return Pointer to the @ref wlmaker_iconified_t.
 */
wlmaker_iconified_t *iconified_from_interactive(
    wlmaker_interactive_t *interactive_ptr)
{
    return BS_CONTAINER_OF(interactive_ptr, wlmaker_iconified_t, interactive);
}

/* ------------------------------------------------------------------------- */
/** Handler: Pointer enters the interactive. */
void _iconified_enter(
    wlmaker_interactive_t *interactive_ptr)
{
    wlmaker_iconified_t *iconified_ptr = iconified_from_interactive(
        interactive_ptr);
    bs_log(BS_INFO, "Enter iconified %p", iconified_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handler: Pointer leaves the interactive. */
void _iconified_leave(
    wlmaker_interactive_t *interactive_ptr)
{
    wlmaker_iconified_t *iconified_ptr = iconified_from_interactive(
        interactive_ptr);
    bs_log(BS_INFO, "Leave iconified %p", iconified_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handler: Pointer motion. */
void _iconified_motion(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y)
{
    wlmaker_iconified_t *iconified_ptr = iconified_from_interactive(
        interactive_ptr);
    bs_log(BS_INFO, "Motion iconified %p: %.2f, %.2f", iconified_ptr, x, y);
}

/* ------------------------------------------------------------------------- */
/** Handler, unused: Focus the iconified. There is no focus. */
void _iconified_focus(
    __UNUSED__ wlmaker_interactive_t *interactive_ptr)
{
    // No focus supported.
}

/* ------------------------------------------------------------------------- */
/**
 * Handles button events for the iconified.
 *
 * Will un-minimize (restore) the view shown by the iconified.
 *
 * @param interactive_ptr
 * @param x
 * @param y
 * @param wlr_pointer_button_event_ptr
 */
void _iconified_button(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y,
    struct wlr_pointer_button_event *wlr_pointer_button_event_ptr)
{
    wlmaker_iconified_t *iconified_ptr = iconified_from_interactive(
        interactive_ptr);
    bs_log(BS_INFO, "Button iconified %p: %.2f, %.2f, %p",
           iconified_ptr, x, y, wlr_pointer_button_event_ptr);

    if (wlr_pointer_button_event_ptr->button != BTN_LEFT) return;
    if (!wlmaker_interactive_contains(interactive_ptr, x, y)) return;
    if (wlr_pointer_button_event_ptr->state != WLR_BUTTON_PRESSED) return;

    wlmaker_workspace_iconified_set_as_view(
        iconified_ptr->view_ptr->workspace_ptr,
        iconified_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handler: Destroy interactive, wraps to @ref wlmaker_iconified_destroy. */
void _iconified_destroy(wlmaker_interactive_t *interactive_ptr)
{
    wlmaker_iconified_t *iconified_ptr = iconified_from_interactive(
        interactive_ptr);
    wlmaker_iconified_destroy(iconified_ptr);
}

/* == End of iconified.c =================================================== */
