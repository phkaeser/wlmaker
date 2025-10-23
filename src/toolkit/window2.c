/* ========================================================================= */
/**
 * @file window2.c
 *
 * @copyright
 * Copyright 2025 Google LLC
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

#include "window2.h"

#include <linux/input-event-codes.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/edges.h>
#undef WLR_USE_UNSTABLE

#include "bordered.h"
#include "container.h"
#include "titlebar.h"
#include "resizebar.h"
#include "test.h"

/* == Declarations ========================================================= */

/** Window handle. */
struct _wlmtk_window2_t {
    /** Bordered, wraps around the box. */
    wlmtk_bordered_t          bordered;
    /** Original virtual method table of the window's element superclass. */
    wlmtk_element_vmt_t       orig_super_element_vmt;

    /** Composed of a box: Holds decoration, popup container and content. */
    wlmtk_box_t               box;

    /** Events for this window. */
    wlmtk_window2_events_t    events;

    /** Element in @ref wlmtk_workspace_t::window2s, when mapped. */
    bs_dllist_node_t          dlnode;

    /** Container for the content. */
    wlmtk_container_t         content_container;
    /** Virtual method table of @ref wlmtk_window2_t::content_container. */
    wlmtk_container_vmt_t     orig_content_container_vmt;
    /** The content. */
    wlmtk_element_t           *content_element_ptr;

    /** The workspace, when mapped to a workspace. NULL otherwise. */
    wlmtk_workspace_t         *workspace_ptr;

    /** Preferred output. See @ref wlmtk_window2_set_wlr_output. */
    struct wlr_output         *wlr_output_ptr;

    /** Window style. */
    wlmtk_window_style_t      style;
    /** Menu style. */
    wlmtk_menu_style_t        menu_style;

    /** The titlebar, when server-side decorated. */
    wlmtk_titlebar_t          *titlebar_ptr;
    /** The resize-bar, when server-side decorated. */
    wlmtk_resizebar_t         *resizebar_ptr;

    /** The window's title. */
    char                      *title_ptr;

    /** Properties of the window. See @ref wlmtk_window_property_t. */
    uint32_t                  properties;

    /**
     * Position of the window, and size of the content window when not in
     * fullscreen or maximized state.
     */
    struct wlr_box            organic_bounding_box;

    /** Edges to anchor on when resizing. */
    uint32_t                  resize_edges;
    /** Current box size when resizing. */
    struct wlr_box            old_box;

    /**
     * Whether an "inorganic" sizing operation is in progress, and thus size
     * changes should not be recorded in @ref wlmtk_window_t::organic_size.
     *
     * This is eg. between @ref wlmtk_window2_request_fullscreen and
     * @ref wlmtk_window2_commit_fullscreen.
     */
    bool                      inorganic_sizing;
    /** Whether this window has server-side decorations. */
    bool                      server_side_decorated;
    /** Whether this windows is currently in fullscreen mode. */
    bool                      fullscreen;
    /** Whetehr this window is currently in maximized state. */
    bool                      maximized;
    /** Whether this window is currently activated (has keyboard focus). */
    bool                      activated;
};

static bool _wlmtk_window2_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);
static bool _wlmtk_window2_container_update_layout(
    wlmtk_container_t *container_ptr);

static void _wlmtk_window2_apply_decoration(wlmtk_window2_t *window_ptr);
static void _wlmtk_window2_create_titlebar(wlmtk_window2_t *window_ptr);
static void _wlmtk_window2_create_resizebar(wlmtk_window2_t *window_ptr);
static void _wlmtk_window2_destroy_titlebar(wlmtk_window2_t *window_ptr);
static void _wlmtk_window2_destroy_resizebar(wlmtk_window2_t *window_ptr);
static void _wlmtk_window2_set_decoration_width(wlmtk_window2_t *window_ptr);

/* == Data ================================================================= */

/** Virtual method table for the window's element superclass. */
static const wlmtk_element_vmt_t window_element_vmt = {
    .pointer_button = _wlmtk_window2_element_pointer_button,
};

/** Virtual method table for the window's container superclass. */
static const wlmtk_container_vmt_t _wlmtk_window2_container_vmt = {
    .update_layout = _wlmtk_window2_container_update_layout,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_window2_t *wlmtk_window2_create(
    wlmtk_element_t *content_element_ptr,
    const wlmtk_window_style_t *style_ptr,
    const wlmtk_menu_style_t *menu_style_ptr)
{
    wlmtk_window2_t *window_ptr = logged_calloc(1, sizeof(wlmtk_window2_t));
    if (NULL == window_ptr) return NULL;
    window_ptr->style = *style_ptr;
    window_ptr->menu_style = *menu_style_ptr;
    wl_signal_init(&window_ptr->events.state_changed);
    wl_signal_init(&window_ptr->events.request_close);
    wl_signal_init(&window_ptr->events.set_activated);
    wl_signal_init(&window_ptr->events.request_size);
    wl_signal_init(&window_ptr->events.request_fullscreen);
    wl_signal_init(&window_ptr->events.request_maximized);

    if (!wlmtk_window2_set_title(window_ptr, NULL)) goto error;

    if (!wlmtk_container_init(&window_ptr->content_container)) goto error;
    if (!wlmtk_box_init(
            &window_ptr->box,
            WLMTK_BOX_VERTICAL,
            &window_ptr->style.margin)) {
        goto error;
    }
    if (!wlmtk_bordered_init(
            &window_ptr->bordered,
            wlmtk_box_element(&window_ptr->box),
            &window_ptr->style.margin)) {
        goto error;
    }
    window_ptr->orig_super_element_vmt = wlmtk_element_extend(
        &window_ptr->bordered.super_container.super_element,
        &window_element_vmt);

    window_ptr->orig_content_container_vmt = wlmtk_container_extend(
        &window_ptr->content_container,
        &_wlmtk_window2_container_vmt);
    wlmtk_element_set_visible(
        &window_ptr->content_container.super_element, true);
    if (NULL != content_element_ptr) {
        wlmtk_element_set_visible(content_element_ptr, true);
        wlmtk_container_add_element(
            &window_ptr->content_container,
            content_element_ptr);
        window_ptr->content_element_ptr = content_element_ptr;
    }

    wlmtk_box_add_element_front(
        &window_ptr->box,
        &window_ptr->content_container.super_element);
    wlmtk_element_set_visible(wlmtk_box_element(&window_ptr->box), true);

    _wlmtk_window2_apply_decoration(window_ptr);
    return window_ptr;

error:
    wlmtk_window2_destroy(window_ptr);
    return NULL;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_destroy(wlmtk_window2_t *window_ptr)
{
    wlmtk_window2_set_server_side_decorated(window_ptr, false);

    if (window_ptr->content_container.super_element.parent_container_ptr) {
        wlmtk_box_remove_element(
            &window_ptr->box,
            &window_ptr->content_container.super_element);
    }

    if (NULL != window_ptr->content_element_ptr) {
        wlmtk_container_remove_element(
            &window_ptr->content_container,
            window_ptr->content_element_ptr);
        window_ptr->content_element_ptr = NULL;
    }

    wlmtk_bordered_fini(&window_ptr->bordered);
    wlmtk_box_fini(&window_ptr->box);
    wlmtk_container_fini(&window_ptr->content_container);

    if (NULL != window_ptr->title_ptr) {
        free(window_ptr->title_ptr);
        window_ptr->title_ptr = NULL;
    }
    free(window_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_window2_events_t *wlmtk_window2_events(wlmtk_window2_t *window_ptr)
{
    return &window_ptr->events;
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_window2_element(wlmtk_window2_t *window_ptr)
{
    return &window_ptr->bordered.super_container.super_element;
}

/* ------------------------------------------------------------------------- */
wlmtk_window2_t *wlmtk_window2_from_element(wlmtk_element_t *element_ptr)
{
    return BS_CONTAINER_OF(
        element_ptr, wlmtk_window2_t, bordered.super_container.super_element);
}

/* ------------------------------------------------------------------------- */
struct wlr_box wlmtk_window2_get_bounding_box(wlmtk_window2_t *window_ptr)
{
    struct wlr_box wbox = wlmtk_element_get_dimensions_box(
        wlmtk_window2_element(window_ptr));
    int x, y;
    wlmtk_element_get_position(wlmtk_window2_element(window_ptr), &x, &y);
    wbox.x += x;
    wbox.y += y;
    return wbox;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_position_changed(wlmtk_window2_t *window_ptr)
{
    if (window_ptr->inorganic_sizing) return;
    wlmtk_element_get_position(
        wlmtk_window2_element(window_ptr),
        &window_ptr->organic_bounding_box.x,
        &window_ptr->organic_bounding_box.y);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_set_properties(
    wlmtk_window2_t *window_ptr,
    uint32_t properties)
{
    if (window_ptr->properties == properties) return;

    window_ptr->properties = properties;
    _wlmtk_window2_apply_decoration(window_ptr);
}

/* ------------------------------------------------------------------------- */
const wlmtk_util_client_t *wlmtk_window2_get_client_ptr(
    __UNUSED__ wlmtk_window2_t *window_ptr)
{
    // TODO(kaeser@gubbe.ch): Wire this up.
    static wlmtk_util_client_t client = {};
    return &client;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_set_wlr_output(
    wlmtk_window2_t *window_ptr,
    struct wlr_output *wlr_output_ptr)
{
    window_ptr->wlr_output_ptr = wlr_output_ptr;
}

/* ------------------------------------------------------------------------- */
struct wlr_output *wlmtk_window2_get_wlr_output(wlmtk_window2_t *window_ptr)
{
    if (NULL != window_ptr->wlr_output_ptr) return window_ptr->wlr_output_ptr;

    wlmtk_workspace_t *workspace_ptr = wlmtk_window2_get_workspace(window_ptr);
    if (NULL == workspace_ptr) return NULL;
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlmtk_workspace_get_wlr_output_layout(workspace_ptr);
    if (NULL == wlr_output_layout_ptr) return NULL;

    struct wlr_box wbox = wlmtk_window2_get_bounding_box(window_ptr);
    double dest_x, dest_y;
    wlr_output_layout_closest_point(
        wlmtk_workspace_get_wlr_output_layout(workspace_ptr),
        NULL,  // struct wlr_output* reference. We don't need a reference.
        wbox.x + wbox.width / 2, wbox.y + wbox.height / 2,
        &dest_x, &dest_y);
    return wlr_output_layout_output_at(
        wlmtk_workspace_get_wlr_output_layout(workspace_ptr),
        dest_x, dest_y);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_window2_set_title(
    wlmtk_window2_t *window_ptr,
    const char *title_ptr)
{
    char *new_title_ptr = NULL;
    if (NULL != title_ptr) {
        new_title_ptr = logged_strdup(title_ptr);
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Unnamed window %p", window_ptr);
        new_title_ptr = logged_strdup(buf);
    }
    if (NULL == new_title_ptr) return false;

    if (NULL != window_ptr->title_ptr) {
        if (0 == strcmp(window_ptr->title_ptr, new_title_ptr)) {
            free(new_title_ptr);
            return true;
        }
        free(window_ptr->title_ptr);
    }
    window_ptr->title_ptr = new_title_ptr;

    if (NULL != window_ptr->titlebar_ptr) {
        wlmtk_titlebar_set_title(window_ptr->titlebar_ptr,
                                 window_ptr->title_ptr);
    }
    return true;
}

/* ------------------------------------------------------------------------- */
const char *wlmtk_window2_get_title(wlmtk_window2_t *window_ptr)
{
    BS_ASSERT(NULL != window_ptr->title_ptr);
    return window_ptr->title_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_set_activated(
    wlmtk_window2_t *window_ptr,
    bool activated)
{
    if (window_ptr->activated == activated) return;

    window_ptr->activated = activated;
    wl_signal_emit(&window_ptr->events.set_activated, NULL);

    if (NULL != window_ptr->titlebar_ptr) {
        wlmtk_titlebar_set_activated(window_ptr->titlebar_ptr, activated);
    }

    if (!activated) {
        wlmtk_window2_menu_set_enabled(window_ptr, false);

        // TODO(kaeser@gubbe.ch): Should test this behaviour.
        if (NULL != window_ptr->workspace_ptr) {
            wlmtk_workspace_activate_window(window_ptr->workspace_ptr, NULL);
        }
    }
}

/* ------------------------------------------------------------------------- */
bool wlmtk_window2_is_activated(wlmtk_window2_t *window_ptr)
{
    return window_ptr->activated;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_request_size(
    wlmtk_window2_t *window_ptr,
    const struct wlr_box *box_ptr)
{
    // The box includes current decoration around the content. Compute the
    // difference to determine what new size to request from the content.
    struct wlr_box wbox = wlmtk_element_get_dimensions_box(
        wlmtk_bordered_element(&window_ptr->bordered));
    struct wlr_box cbox = wlmtk_element_get_dimensions_box(
        window_ptr->content_element_ptr);

    struct wlr_box no_decorations_box = {
        .x = box_ptr->x,
        .y = box_ptr->y,
        .width = BS_MAX(0, box_ptr->width - (wbox.width - cbox.width)),
        .height = BS_MAX(0, box_ptr->height -  (wbox.height - cbox.height))
    };

    wl_signal_emit(&window_ptr->events.request_size, &no_decorations_box);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_resize_edges(
    wlmtk_window2_t *window_ptr,
    uint32_t edges)
{
    window_ptr->resize_edges = edges;
    window_ptr->old_box = wlmtk_element_get_dimensions_box(
        wlmtk_bordered_element(&window_ptr->bordered));
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_request_close(wlmtk_window2_t *window_ptr)
{
    if (!(window_ptr->properties & WLMTK_WINDOW_PROPERTY_CLOSABLE)) return;
    wl_signal_emit(&window_ptr->events.request_close, NULL);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_request_minimize(wlmtk_window2_t *window_ptr)
{
    bs_log(BS_ERROR, "TODO: Request minimize for window %p", window_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_request_fullscreen(
    wlmtk_window2_t *window_ptr,
    bool fullscreen)
{
    window_ptr->inorganic_sizing = fullscreen;

    struct wlr_box desired_size = window_ptr->organic_bounding_box;
    if (fullscreen) {
        desired_size = wlmtk_workspace_get_fullscreen_extents(
            window_ptr->workspace_ptr,
            wlmtk_window2_get_wlr_output(window_ptr));
        wl_signal_emit(&window_ptr->events.request_size, &desired_size);
    } else {
        desired_size.x = 0;
        desired_size.y = 0;
        wlmtk_window2_request_size(window_ptr, &desired_size);
    }

    wl_signal_emit(&window_ptr->events.request_fullscreen, &fullscreen);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_window2_is_fullscreen(wlmtk_window2_t *window_ptr)
{
    return window_ptr->fullscreen;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_commit_fullscreen(
    wlmtk_window2_t *window_ptr,
    bool fullscreen)
{
    // Guard clause: Nothing to do if already in expected state.
    if (window_ptr->fullscreen == fullscreen ) return;

    window_ptr->fullscreen = fullscreen;
    _wlmtk_window2_apply_decoration(window_ptr);

    wlmtk_workspace_window_to_fullscreen(
        window_ptr->workspace_ptr, window_ptr, fullscreen);

    struct wlr_box fsbox = wlmtk_workspace_get_fullscreen_extents(
            window_ptr->workspace_ptr,
            wlmtk_window2_get_wlr_output(window_ptr));
    wlmtk_workspace_set_window_position(
        window_ptr->workspace_ptr,
        window_ptr,
        fullscreen ? fsbox.x : window_ptr->organic_bounding_box.x,
        fullscreen ? fsbox.y : window_ptr->organic_bounding_box.y);

    window_ptr->inorganic_sizing = false;
    wl_signal_emit(&window_ptr->events.state_changed, NULL);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_request_maximized(
    wlmtk_window2_t *window_ptr,
    bool maximized)
{
    // Guard clause: No action needed if alreadz maximized or fullscreen.
    if (window_ptr->maximized == maximized ||
        window_ptr->fullscreen) return;
    window_ptr->inorganic_sizing = maximized;

    if (maximized) {
        struct wlr_box desired_size = wlmtk_workspace_get_maximize_extents(
            window_ptr->workspace_ptr,
            wlmtk_window2_get_wlr_output(window_ptr));
        wlmtk_window2_request_size(window_ptr, &desired_size);
    } else {
        struct wlr_box desired_size = window_ptr->organic_bounding_box;
        desired_size.x = 0;
        desired_size.y = 0;
        wl_signal_emit(&window_ptr->events.request_size, &desired_size);
    }
    wl_signal_emit(&window_ptr->events.request_maximized, &maximized);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_window2_is_maximized(wlmtk_window2_t *window_ptr)
{
    return window_ptr->maximized;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_commit_maximized(
    wlmtk_window2_t *window_ptr,
    bool maximized)
{
    if (window_ptr->maximized == maximized) return;
    window_ptr->maximized = maximized;

    struct wlr_box max_box = wlmtk_workspace_get_maximize_extents(
        window_ptr->workspace_ptr,
        wlmtk_window2_get_wlr_output(window_ptr));
    wlmtk_workspace_set_window_position(
        window_ptr->workspace_ptr,
        window_ptr,
        maximized ? max_box.x : window_ptr->organic_bounding_box.x,
        maximized ? max_box.y : window_ptr->organic_bounding_box.y);

    window_ptr->inorganic_sizing = false;
    wl_signal_emit(&window_ptr->events.state_changed, NULL);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_request_shaded(wlmtk_window2_t *window_ptr, bool shaded)
{
    bs_log(BS_ERROR, "TODO: Request shaded for window %p: %d",
           window_ptr, shaded);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_window2_is_shaded(wlmtk_window2_t *window_ptr)
{
    bs_log(BS_ERROR, "TODO: is_shaded for window %p", window_ptr);
    return false;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_menu_set_enabled(wlmtk_window2_t *window_ptr, bool enabled)
{
    bs_log(BS_ERROR, "TODO: Set menu for window %p %d", window_ptr, enabled);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_set_server_side_decorated(
    wlmtk_window2_t *window_ptr,
    bool decorated)
{
    if (window_ptr->server_side_decorated == decorated) return;

    window_ptr->server_side_decorated = decorated;
    _wlmtk_window2_apply_decoration(window_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_set_workspace(
    wlmtk_window2_t *window_ptr,
    wlmtk_workspace_t *workspace_ptr)
{
    if (window_ptr->workspace_ptr == workspace_ptr) return;
    window_ptr->workspace_ptr = workspace_ptr;

    wl_signal_emit(&window_ptr->events.state_changed, window_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_workspace_t *wlmtk_window2_get_workspace(wlmtk_window2_t *window_ptr)
{
    return window_ptr->workspace_ptr;
}

/* ------------------------------------------------------------------------- */
bs_dllist_node_t *wlmtk_dlnode_from_window2(wlmtk_window2_t *window_ptr)
{
    return &window_ptr->dlnode;
}

/* ------------------------------------------------------------------------- */
wlmtk_window2_t *wlmtk_window2_from_dlnode(bs_dllist_node_t *dlnode_ptr)
{
    return BS_CONTAINER_OF(dlnode_ptr, wlmtk_window2_t, dlnode);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Activates window on button press, and calls the parent's implementation. */
bool _wlmtk_window2_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmtk_window2_t *window_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_window2_t, bordered.super_container.super_element);

    // In right-click mode: Any out-of-window action will close it.
    // TODO(kaeser@gubbe.ch): This should be a specific window mode, and should
    // have a handler method when leaving that mode (eg. left release within
    // the window).
    // Also TODO(kaeser@gubbe.ch): Test this.
    if (window_ptr->properties & WLMTK_WINDOW_PROPERTY_RIGHTCLICK) {
        bool rv = window_ptr->orig_super_element_vmt.pointer_button(
            element_ptr, button_event_ptr);
        if (BTN_RIGHT == button_event_ptr->button &&
            WLMTK_BUTTON_UP == button_event_ptr->type &&
            !rv) {
            wlmtk_window2_request_close(window_ptr);
            return true;
        }
        return rv;
    }

#if 0
    // Permit drag-move with the (hardcoded) modifier.
    // TODO(kaeser@gubbe.ch): This should be changed to make use of "DRAG"
    // events, with corresponding modifiers. Do so, once added to toolkit.
    if (NULL != window_ptr->wlr_seat_ptr) {
        struct wlr_keyboard *wlr_keyboard_ptr = wlr_seat_get_keyboard(
            window_ptr->wlr_seat_ptr);
        uint32_t modifiers = wlr_keyboard_get_modifiers(wlr_keyboard_ptr);
        if (BTN_LEFT == button_event_ptr->button &&
            WLMTK_BUTTON_DOWN == button_event_ptr->type &&
            (WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO) == modifiers) {
            wlmtk_window_request_move(window_ptr);
            return true;
        }
    }
#endif

    // We shouldn't receive buttons when not mapped.
    wlmtk_workspace_t *workspace_ptr = wlmtk_window2_get_workspace(window_ptr);
    wlmtk_workspace_activate_window(workspace_ptr, window_ptr);
    if (!window_ptr->fullscreen) {
        wlmtk_workspace_raise_window(workspace_ptr, window_ptr);
    }

    return window_ptr->orig_super_element_vmt.pointer_button(
        element_ptr, button_event_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of @ref wlmtk_container_vmt_t::update_layout.
 *
 * Invoked when the window's contained elements triggered a layout update,
 * and will use this to trigger (potential) size updates to the window
 * decorations.
 *
 * @param container_ptr
 */
bool _wlmtk_window2_container_update_layout(wlmtk_container_t *container_ptr)
{
    wlmtk_window2_t *window_ptr = BS_CONTAINER_OF(
        container_ptr, wlmtk_window2_t, content_container);

    _wlmtk_window2_set_decoration_width(window_ptr);

    // Update layout for the parent. This is... hacky.
    if (NULL != container_ptr->super_element.parent_container_ptr) {
        wlmtk_container_update_layout_and_pointer_focus(
            container_ptr->super_element.parent_container_ptr);
    }

    // No updates if there is no content element (or not in the container).
    if (NULL == window_ptr->content_element_ptr ||
        NULL == window_ptr->content_element_ptr->parent_container_ptr) {
        return false;
    }
    struct wlr_box box = wlmtk_element_get_dimensions_box(
        window_ptr->content_element_ptr);

    // Store organic size. If we're in an organic mode.
    if (!window_ptr->inorganic_sizing &&
        !window_ptr->fullscreen &&
        !window_ptr->maximized) {
        window_ptr->organic_bounding_box.width = box.width;
        window_ptr->organic_bounding_box.height = box.height;
    }

    // new_box includes potential decorations.
    struct wlr_box new_box = wlmtk_element_get_dimensions_box(
        wlmtk_bordered_element(&window_ptr->bordered));
    int x, y;
    wlmtk_element_get_position(
        wlmtk_bordered_element(&window_ptr->bordered), &x, &y);
    if (window_ptr->resize_edges & WLR_EDGE_LEFT) {
        x += window_ptr->old_box.width - new_box.width;
    }
    if (window_ptr->resize_edges & WLR_EDGE_TOP) {
        y += window_ptr->old_box.height - new_box.height;
    }
    // TODO: Should we rather use wlmtk_workspace_set_window_position ?
    wlmtk_element_set_position(
        wlmtk_bordered_element(&window_ptr->bordered), x, y);
    window_ptr->old_box = new_box;

    // Hm. Something is broken -- there shouldn't be a delta other
    // than the border width. For now: Report.
    if (new_box.width - box.width !=
        2 * (int)window_ptr->bordered.style.width) {
        bs_log(BS_WARNING,
               "Resize jitter: box.width - new_box.width %d - %d,%d",
               box.width - new_box.width, box.x, new_box.x);
    }

    return true;
}

/* ------------------------------------------------------------------------- */
/** Applies window decoration depending on current state. */
void _wlmtk_window2_apply_decoration(wlmtk_window2_t *window_ptr)
{
    wlmtk_margin_style_t bstyle = window_ptr->style.border;

    if (window_ptr->server_side_decorated && !window_ptr->fullscreen) {
        _wlmtk_window2_create_titlebar(window_ptr);
    } else {
        bstyle.width = 0;
        _wlmtk_window2_destroy_titlebar(window_ptr);
    }

    if (NULL != window_ptr->titlebar_ptr) {
        uint32_t properties = 0;
        if (window_ptr->properties & WLMTK_WINDOW_PROPERTY_ICONIFIABLE) {
            properties |= WLMTK_TITLEBAR_PROPERTY_ICONIFY;
        }
        if (window_ptr->properties & WLMTK_WINDOW_PROPERTY_CLOSABLE) {
            properties |= WLMTK_TITLEBAR_PROPERTY_CLOSE;
        }
        wlmtk_titlebar_set_properties(window_ptr->titlebar_ptr, properties);
        wlmtk_titlebar_set_activated(
            window_ptr->titlebar_ptr, window_ptr->activated);
    }

    if (window_ptr->server_side_decorated &&
        !window_ptr->fullscreen &&
        (window_ptr->properties & WLMTK_WINDOW_PROPERTY_RESIZABLE)) {
        _wlmtk_window2_create_resizebar(window_ptr);
    } else {
        _wlmtk_window2_destroy_resizebar(window_ptr);
    }

    wlmtk_bordered_set_style(&window_ptr->bordered, &bstyle);

    _wlmtk_window2_set_decoration_width(window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Creates the titlebar. */
void _wlmtk_window2_create_titlebar(wlmtk_window2_t *window_ptr)
{
    BS_ASSERT(window_ptr->server_side_decorated && !window_ptr->fullscreen);

    // Guard clause: Don't add decoration.
    if (NULL != window_ptr->titlebar_ptr) return;

    // Create decoration.
    window_ptr->titlebar_ptr = wlmtk_titlebar2_create(
        window_ptr, &window_ptr->style.titlebar);
    BS_ASSERT(NULL != window_ptr->titlebar_ptr);

    wlmtk_element_set_visible(
        wlmtk_titlebar_element(window_ptr->titlebar_ptr), true);
    wlmtk_titlebar_set_activated(
        window_ptr->titlebar_ptr,
        window_ptr->activated);

    // Hm, if the content has a popup that extends over the titlebar area,
    // it'll be partially obscured. That will look odd... Well, let's
    // address that problem once there's a situation.
    wlmtk_box_add_element_front(
        &window_ptr->box,
        wlmtk_titlebar_element(window_ptr->titlebar_ptr));
}

/* ------------------------------------------------------------------------- */
/** Destroys the titlebar. */
void _wlmtk_window2_destroy_titlebar(wlmtk_window2_t *window_ptr)
{
    BS_ASSERT(!window_ptr->server_side_decorated || window_ptr->fullscreen);

    if (NULL == window_ptr->titlebar_ptr) return;

    wlmtk_box_remove_element(
        &window_ptr->box,
        wlmtk_titlebar_element(window_ptr->titlebar_ptr));
    wlmtk_titlebar_destroy(window_ptr->titlebar_ptr);
    window_ptr->titlebar_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
/** Creates the resizebar. */
void _wlmtk_window2_create_resizebar(wlmtk_window2_t *window_ptr)
{
    BS_ASSERT(window_ptr->server_side_decorated && !window_ptr->fullscreen);

    // Guard clause: Don't add decoration.
    if (NULL != window_ptr->resizebar_ptr) return;

    window_ptr->resizebar_ptr = wlmtk_resizebar2_create(
        window_ptr, &window_ptr->style.resizebar);
    BS_ASSERT(NULL != window_ptr->resizebar_ptr);
    wlmtk_element_set_visible(
        wlmtk_resizebar_element(window_ptr->resizebar_ptr), true);
    wlmtk_box_add_element_back(
        &window_ptr->box,
        wlmtk_resizebar_element(window_ptr->resizebar_ptr));
}

/* ------------------------------------------------------------------------- */
/** Destroys the resizebar. */
void _wlmtk_window2_destroy_resizebar(wlmtk_window2_t *window_ptr)
{
    BS_ASSERT(!window_ptr->server_side_decorated ||
              window_ptr->fullscreen ||
              !(window_ptr->properties & WLMTK_WINDOW_PROPERTY_RESIZABLE));

    if (NULL == window_ptr->resizebar_ptr) return;

    wlmtk_box_remove_element(
        &window_ptr->box,
        wlmtk_resizebar_element(window_ptr->resizebar_ptr));
    wlmtk_resizebar_destroy(window_ptr->resizebar_ptr);
    window_ptr->resizebar_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
/** Sets the decoration's width to match the content. */
void _wlmtk_window2_set_decoration_width(wlmtk_window2_t *window_ptr)
{
    if (NULL == window_ptr->content_element_ptr) return;

    struct wlr_box box = wlmtk_element_get_dimensions_box(
        window_ptr->content_element_ptr);
    if (NULL != window_ptr->titlebar_ptr) {
        wlmtk_titlebar_set_width(window_ptr->titlebar_ptr, box.width);
    }
    if (NULL != window_ptr->resizebar_ptr) {
        wlmtk_resizebar_set_width(window_ptr->resizebar_ptr, box.width);
    }
}

/* == Unit Tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);
static void test_decoration(bs_test_t *test_ptr);
static void test_events(bs_test_t *test_ptr);
static void test_set_activated(bs_test_t *test_ptr);
static void test_fullscreen(bs_test_t *test_ptr);
static void test_fullscreen_unmap(bs_test_t *test_ptr);
static void test_maximized(bs_test_t *test_ptr);

// TODO(kaeser@gubbe.ch): Add tests for ..
// * shade
// * maximize on multiple outputs
// * storing organic_bounding_box

const bs_test_case_t wlmtk_window2_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "decoration", test_decoration },
    { 1, "events", test_events },
    { 1, "set_activated", test_set_activated },
    { 1, "fullscreen", test_fullscreen },
    { 1, "fullscreen_unmap", test_fullscreen_unmap },
    { 1, "maxizimed", test_maximized },
    { 0, NULL, NULL }
};

/** Window style used as default for testing. */
static const wlmtk_window_style_t _wlmtk_window2_test_style = {
    .titlebar = { .height = 10 },
    .resizebar = { .height = 5 },
    .margin = { .width = 1 },
    .border = { .width = 2 },
};

/** Menu style used as default for testing windows. */
static const wlmtk_menu_style_t _wlmtk_window2_test_menu_style = {
};

/* ------------------------------------------------------------------------- */
wlmtk_window2_t *wlmtk_test_window2_create(
    wlmtk_element_t *content_element_ptr)
{
    return wlmtk_window2_create(
        content_element_ptr,
        &_wlmtk_window2_test_style,
        &_wlmtk_window2_test_menu_style);
}

/* ------------------------------------------------------------------------- */
/** Exercises ctor and dtor and some accessors. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fe = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe);

    wlmtk_window2_t *w = wlmtk_test_window2_create(&fe->element);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w);

    // Title. Must be set when unnamed, and override otherwise.
    BS_TEST_VERIFY_STRMATCH(
        test_ptr,
        wlmtk_window2_get_title(w),
        "Unnamed window .*");
    wlmtk_window2_set_title(w, "Title");
    BS_TEST_VERIFY_STREQ(test_ptr, "Title", wlmtk_window2_get_title(w));

    // Transform to element and dlnode and back.
    wlmtk_element_t *e = wlmtk_window2_element(w);
    BS_TEST_VERIFY_EQ(test_ptr, w, wlmtk_window2_from_element(e));
    bs_dllist_node_t *dlnode_ptr = wlmtk_dlnode_from_window2(w);
    BS_TEST_VERIFY_EQ(test_ptr, w, wlmtk_window2_from_dlnode(dlnode_ptr));

    // set_workspace and get_workspace.
    struct wl_display *display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(display_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_output_layout_ptr);
    wlmtk_tile_style_t ts = {};
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "t", &ts);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, workspace_ptr);

    wlmtk_util_test_listener_t l;
    wlmtk_util_connect_test_listener(
        &wlmtk_window2_events(w)->state_changed, &l);

    wlmtk_window2_set_workspace(w, workspace_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        workspace_ptr,
        wlmtk_window2_get_workspace(w));

    wlmtk_util_clear_test_listener(&l);
    wlmtk_window2_set_workspace(w, workspace_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, l.calls);

    wlmtk_workspace_destroy(workspace_ptr);
    wl_display_destroy(display_ptr);

    wlmtk_window2_destroy(w);
    wlmtk_element_destroy(&fe->element);
}

/* ------------------------------------------------------------------------- */
/** Tests server-side decoration. */
void test_decoration(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fe = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe);
    wlmtk_fake_element_set_dimensions(fe, 42, 20);

    wlmtk_window2_t *w = wlmtk_test_window2_create(&fe->element);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w);

    // Initially: no decoration.
    BS_TEST_VERIFY_EQ(test_ptr, NULL, w->titlebar_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, w->resizebar_ptr);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 42, 20,
        wlmtk_window2_get_bounding_box(w));

    // Enable: Default property is not resizable, so we only get titlebar.
    wlmtk_window2_set_server_side_decorated(w, true);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, w->titlebar_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, w->resizebar_ptr);

    // Set property. Now we must have resize bar.
    wlmtk_window2_set_properties(w, WLMTK_WINDOW_PROPERTY_RESIZABLE);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, w->resizebar_ptr);

    // Disable. Decoration vanishes.
    wlmtk_window2_set_server_side_decorated(w, false);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, w->titlebar_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, w->resizebar_ptr);

    // Re-enable. Properties are sticky, so we want all.
    wlmtk_window2_set_server_side_decorated(w, true);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, w->titlebar_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, w->resizebar_ptr);

    // Verify width: Must match the fake element's dimensions.
    wlmtk_element_t *te = wlmtk_titlebar_element(w->titlebar_ptr);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 42, 10,
        wlmtk_element_get_dimensions_box(te));
    wlmtk_element_t *re = wlmtk_resizebar_element(w->resizebar_ptr);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 42, 5,
        wlmtk_element_get_dimensions_box(re));
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 46, 41,
        wlmtk_window2_get_bounding_box(w));

    // An update in the element would call the paren's update_layout.
    wlmtk_fake_element_set_dimensions(fe, 52, 20);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 52, 10,
        wlmtk_element_get_dimensions_box(te));
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 52, 5,
        wlmtk_element_get_dimensions_box(re));

    // Update again, with top-left edges resizing.
    wlmtk_window2_resize_edges(w, WLR_EDGE_TOP | WLR_EDGE_LEFT);
    wlmtk_fake_element_set_dimensions(fe, 32, 10);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 32, 10,
        wlmtk_element_get_dimensions_box(te));
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 32, 5,
        wlmtk_element_get_dimensions_box(re));
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 20, 10, 36, 31,
        wlmtk_window2_get_bounding_box(w));

    wlmtk_window2_destroy(w);
    wlmtk_element_destroy(&fe->element);
}

/* ------------------------------------------------------------------------- */
/** Tests that desired events are firing. */
void test_events(bs_test_t *test_ptr)
{
    wlmtk_window2_t *w = wlmtk_test_window2_create(NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w);

    wlmtk_util_test_listener_t l;

    wlmtk_util_connect_test_listener(
        &wlmtk_window2_events(w)->request_close, &l);
    wlmtk_window2_request_close(w);
    BS_TEST_VERIFY_EQ(test_ptr, 0, l.calls);

    wlmtk_window2_set_properties(w, WLMTK_WINDOW_PROPERTY_CLOSABLE);
    wlmtk_window2_request_close(w);
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);

    wlmtk_window2_destroy(w);
}

/* ------------------------------------------------------------------------- */
/** Tests that activation updates decoration and callback. */
void test_set_activated(bs_test_t *test_ptr)
{
    wlmtk_window2_t *w = wlmtk_test_window2_create(NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w);

    wlmtk_util_test_listener_t l;
    wlmtk_util_connect_test_listener(
        &wlmtk_window2_events(w)->set_activated, &l);

    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window2_is_activated(w));

    wlmtk_window2_set_activated(w, true);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_is_activated(w));
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);

    wlmtk_window2_set_server_side_decorated(w, true);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_titlebar_is_activated(w->titlebar_ptr));

    wlmtk_util_clear_test_listener(&l);
    wlmtk_window2_set_activated(w, false);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window2_is_activated(w));
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_titlebar_is_activated(w->titlebar_ptr));

    wlmtk_window2_destroy(w);
}

/* ------------------------------------------------------------------------- */
/** Tests fullscreen mode of a window. */
void test_fullscreen(bs_test_t *test_ptr)
{
    struct wl_display *display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(display_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_output_layout_ptr);
    struct wlr_output output = { .width = 1024, .height = 768, .scale = 1 };
    wlmtk_test_wlr_output_init(&output);
    wlr_output_layout_add(wlr_output_layout_ptr, &output, 0, 0);

    wlmtk_tile_style_t ts = {};
    wlmtk_workspace_t *ws_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "t", &ts);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_workspace_enable(ws_ptr, true);

    wlmtk_fake_element_t *fe_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe_ptr);
    wlmtk_window2_t *w = wlmtk_test_window2_create(&fe_ptr->element);
    wlmtk_window2_set_properties(w, WLMTK_WINDOW_PROPERTY_RESIZABLE);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w);

    wlmtk_util_test_listener_t state_changed_listener;
    wlmtk_util_connect_test_listener(
        &wlmtk_window2_events(w)->state_changed,
        &state_changed_listener);

    wlmtk_util_test_listener_t request_fullscreen_listener;
    wlmtk_util_connect_test_listener(
        &wlmtk_window2_events(w)->request_fullscreen,
        &request_fullscreen_listener);

    wlmtk_util_test_wlr_box_listener_t request_size_listener;
    wlmtk_util_connect_test_wlr_box_listener(
        &wlmtk_window2_events(w)->request_size,
        &request_size_listener);

    // Map the window. Must be activated.
    wlmtk_window2_set_server_side_decorated(w, true);
    wlmtk_workspace_map_window2(ws_ptr, w);
    BS_TEST_VERIFY_EQ(test_ptr, 1, state_changed_listener.calls);
    wlmtk_util_clear_test_listener(&state_changed_listener);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_is_activated(w));

    // Setup initial size and position. Verify.
    wlmtk_fake_element_set_dimensions(fe_ptr, 200, 100);
    wlmtk_workspace_set_window_position(ws_ptr, w, 20, 10);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 20, 10, 204, 121,
        wlmtk_window2_get_bounding_box(w));

    // Request fullscreen. Size yet unchanged, but requests issued.
    wlmtk_window2_request_fullscreen(w, true);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 20, 10, 204, 121,
        wlmtk_window2_get_bounding_box(w));
    BS_TEST_VERIFY_EQ(test_ptr, 1, request_fullscreen_listener.calls);
    wlmtk_util_clear_test_listener(&request_fullscreen_listener);
    BS_TEST_VERIFY_EQ(test_ptr, 1, request_size_listener.calls);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 1024, 768, request_size_listener.box);
    wlmtk_util_clear_test_wlr_box_listener(&request_size_listener);

    // Only after commit: Be fullscreen.
    wlmtk_fake_element_set_dimensions(fe_ptr, 1024, 768);
    wlmtk_window2_commit_fullscreen(w, true);
    BS_TEST_VERIFY_EQ(test_ptr, 1, state_changed_listener.calls);
    wlmtk_util_clear_test_listener(&state_changed_listener);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 1024, 768,
        wlmtk_window2_get_bounding_box(w));
    BS_TEST_VERIFY_TRUE(test_ptr, w->server_side_decorated);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, w->titlebar_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, w->resizebar_ptr);

    // Request to end fullscreen. Also not taking immediate effect.
    wlmtk_window2_request_fullscreen(w, false);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 1024, 768,
        wlmtk_window2_get_bounding_box(w));
    BS_TEST_VERIFY_EQ(test_ptr, 1, request_fullscreen_listener.calls);
    wlmtk_util_clear_test_listener(&request_fullscreen_listener);
    BS_TEST_VERIFY_EQ(test_ptr, 1, request_size_listener.calls);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 200, 100, request_size_listener.box);
    wlmtk_util_clear_test_wlr_box_listener(&request_size_listener);

    // Only after commit: Be fullscreen.
    wlmtk_fake_element_set_dimensions(fe_ptr, 200, 100);
    wlmtk_window2_commit_fullscreen(w, false);
    BS_TEST_VERIFY_EQ(test_ptr, 1, state_changed_listener.calls);
    wlmtk_util_clear_test_listener(&state_changed_listener);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 20, 10, 204, 121,
        wlmtk_window2_get_bounding_box(w));
    BS_TEST_VERIFY_TRUE(test_ptr, w->server_side_decorated);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, w->titlebar_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, w->resizebar_ptr);

    wlmtk_workspace_unmap_window2(ws_ptr, w);

    wlmtk_util_disconnect_test_listener(&request_fullscreen_listener);
    wlmtk_util_disconnect_test_listener(&state_changed_listener);
    wlmtk_window2_destroy(w);
    wlmtk_element_destroy(&fe_ptr->element);
    wlmtk_workspace_destroy(ws_ptr);
    wl_display_destroy(display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests that unmapping a fullscreen window works. */
void test_fullscreen_unmap(bs_test_t *test_ptr)
{
    struct wl_display *display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(display_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_output_layout_ptr);
    struct wlr_output output = { .width = 1024, .height = 768, .scale = 1 };
    wlmtk_test_wlr_output_init(&output);
    wlr_output_layout_add(wlr_output_layout_ptr, &output, 0, 0);

    wlmtk_tile_style_t ts = {};
    wlmtk_workspace_t *ws_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "t", &ts);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_workspace_enable(ws_ptr, true);

    wlmtk_fake_element_t *fe_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe_ptr);
    wlmtk_window2_t *w = wlmtk_test_window2_create(&fe_ptr->element);
    wlmtk_window2_set_properties(w, WLMTK_WINDOW_PROPERTY_RESIZABLE);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w);

    wlmtk_util_test_listener_t state_changed_listener;
    wlmtk_util_connect_test_listener(
        &wlmtk_window2_events(w)->state_changed,
        &state_changed_listener);

    wlmtk_util_test_listener_t request_fullscreen_listener;
    wlmtk_util_connect_test_listener(
        &wlmtk_window2_events(w)->request_fullscreen,
        &request_fullscreen_listener);

    // Map the window. Must be activated.
    wlmtk_window2_set_server_side_decorated(w, true);
    wlmtk_workspace_map_window2(ws_ptr, w);
    BS_TEST_VERIFY_EQ(test_ptr, 1, state_changed_listener.calls);
    wlmtk_util_clear_test_listener(&state_changed_listener);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_is_activated(w));

    // Setup initial size and position. Verify.
    wlmtk_fake_element_set_dimensions(fe_ptr, 200, 100);
    wlmtk_workspace_set_window_position(ws_ptr, w, 20, 10);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 20, 10, 204, 121,
        wlmtk_window2_get_bounding_box(w));

    // Request fullscreen. Verify the signals to request size and fullscreen.
    wlmtk_window2_request_fullscreen(w, true);
    BS_TEST_VERIFY_EQ(test_ptr, 1, request_fullscreen_listener.calls);
    wlmtk_util_clear_test_listener(&request_fullscreen_listener);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 20, 10, 204, 121,
        wlmtk_window2_get_bounding_box(w));

    // Only after commit: Be back at organic size.
    wlmtk_fake_element_set_dimensions(fe_ptr, 1024, 768);
    wlmtk_window2_commit_fullscreen(w, true);
    BS_TEST_VERIFY_EQ(test_ptr, 1, state_changed_listener.calls);
    wlmtk_util_clear_test_listener(&state_changed_listener);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 1024, 768,
        wlmtk_window2_get_bounding_box(w));
    BS_TEST_VERIFY_TRUE(test_ptr, w->server_side_decorated);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, w->titlebar_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, w->resizebar_ptr);

    wlmtk_workspace_unmap_window2(ws_ptr, w);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window2_is_activated(w));

    wlmtk_util_disconnect_test_listener(&request_fullscreen_listener);
    wlmtk_util_disconnect_test_listener(&state_changed_listener);
    wlmtk_window2_destroy(w);
    wlmtk_element_destroy(&fe_ptr->element);
    wlmtk_workspace_destroy(ws_ptr);
    wl_display_destroy(display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests maximizing a window. */
void test_maximized(bs_test_t *test_ptr)
{
    struct wl_display *display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(display_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_output_layout_ptr);
    struct wlr_output output = { .width = 1024, .height = 768, .scale = 1 };
    wlmtk_test_wlr_output_init(&output);
    wlr_output_layout_add(wlr_output_layout_ptr, &output, 0, 0);

    wlmtk_tile_style_t ts = {};
    wlmtk_workspace_t *ws_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "t", &ts);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_workspace_enable(ws_ptr, true);

    wlmtk_fake_element_t *fe_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe_ptr);
    wlmtk_window2_t *w = wlmtk_test_window2_create(&fe_ptr->element);
    wlmtk_window2_set_properties(w, WLMTK_WINDOW_PROPERTY_RESIZABLE);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w);

    wlmtk_util_test_listener_t state_changed_listener;
    wlmtk_util_connect_test_listener(
        &wlmtk_window2_events(w)->state_changed,
        &state_changed_listener);

    wlmtk_util_test_listener_t request_maximized_listener;
    wlmtk_util_connect_test_listener(
        &wlmtk_window2_events(w)->request_maximized,
        &request_maximized_listener);

    wlmtk_util_test_wlr_box_listener_t request_size_listener;
    wlmtk_util_connect_test_wlr_box_listener(
        &wlmtk_window2_events(w)->request_size,
        &request_size_listener);

    // Map the window. Must be activated.
    wlmtk_window2_set_server_side_decorated(w, true);
    wlmtk_workspace_map_window2(ws_ptr, w);
    BS_TEST_VERIFY_EQ(test_ptr, 1, state_changed_listener.calls);
    wlmtk_util_clear_test_listener(&state_changed_listener);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_is_activated(w));

    // Setup initial size and position. Verify.
    wlmtk_fake_element_set_dimensions(fe_ptr, 200, 100);
    wlmtk_workspace_set_window_position(ws_ptr, w, 20, 10);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 20, 10, 204, 121,
        wlmtk_window2_get_bounding_box(w));

    // Request maximized. Size yet unchanged, but requests issued.
    wlmtk_window2_request_maximized(w, true);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 20, 10, 204, 121,
        wlmtk_window2_get_bounding_box(w));
    BS_TEST_VERIFY_EQ(test_ptr, 1, request_maximized_listener.calls);
    wlmtk_util_clear_test_listener(&request_maximized_listener);
    BS_TEST_VERIFY_EQ(test_ptr, 1, request_size_listener.calls);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 1020, 747, request_size_listener.box);
    wlmtk_util_clear_test_wlr_box_listener(&request_size_listener);

    // Only after commit: Be maximized.
    wlmtk_fake_element_set_dimensions(fe_ptr, 1020, 747);
    wlmtk_window2_commit_maximized(w, true);
    BS_TEST_VERIFY_EQ(test_ptr, 1, state_changed_listener.calls);
    wlmtk_util_clear_test_listener(&state_changed_listener);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 1024, 768,
        wlmtk_window2_get_bounding_box(w));
    BS_TEST_VERIFY_TRUE(test_ptr, w->server_side_decorated);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, w->titlebar_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, w->resizebar_ptr);

    // Request to end maximized. Also not taking immediate effect.
    wlmtk_window2_request_maximized(w, false);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 1024, 768,
        wlmtk_window2_get_bounding_box(w));
    BS_TEST_VERIFY_EQ(test_ptr, 1, request_maximized_listener.calls);
    wlmtk_util_clear_test_listener(&request_maximized_listener);
    BS_TEST_VERIFY_EQ(test_ptr, 1, request_size_listener.calls);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 200, 100, request_size_listener.box);
    wlmtk_util_clear_test_wlr_box_listener(&request_size_listener);

    // Only after commit: Be back at organic size.
    wlmtk_fake_element_set_dimensions(fe_ptr, 200, 100);
    wlmtk_window2_commit_maximized(w, false);
    BS_TEST_VERIFY_EQ(test_ptr, 1, state_changed_listener.calls);
    wlmtk_util_clear_test_listener(&state_changed_listener);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 20, 10, 204, 121,
        wlmtk_window2_get_bounding_box(w));
    BS_TEST_VERIFY_TRUE(test_ptr, w->server_side_decorated);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, w->titlebar_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, w->resizebar_ptr);

    wlmtk_workspace_unmap_window2(ws_ptr, w);

    wlmtk_util_disconnect_test_listener(&request_maximized_listener);
    wlmtk_util_disconnect_test_listener(&state_changed_listener);
    wlmtk_window2_destroy(w);
    wlmtk_element_destroy(&fe_ptr->element);
    wlmtk_workspace_destroy(ws_ptr);
    wl_display_destroy(display_ptr);
}

/* == End of window2.c ===================================================== */
