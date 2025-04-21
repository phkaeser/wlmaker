/* ========================================================================= */
/**
 * @file window.c
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

#include <toolkit/rectangle.h>
#include <toolkit/window.h>
#include <toolkit/workspace.h>
#include <toolkit/test.h>

/// Include unstable interfaces of wlroots.
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/box.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** Maximum number of pending state updates. */
#define WLMTK_WINDOW_MAX_PENDING 64

/** Virtual method table for the window. */
typedef struct {
    /** Destructor. */
    void (*destroy)(wlmtk_window_t *window_ptr);
    /** Virtual method for @ref wlmtk_window_request_minimize. */
    void (*request_minimize)(wlmtk_window_t *window_ptr);
    /** Virtual method for @ref wlmtk_window_request_move. */
    void (*request_move)(wlmtk_window_t *window_ptr);
    /** Virtual method for @ref wlmtk_window_request_resize. */
    void (*request_resize)(wlmtk_window_t *window_ptr,
                           uint32_t edges);
} wlmtk_window_vmt_t;

/** Pending positional updates for @ref wlmtk_window_t::content_ptr. */
typedef struct {
    /** Node within @ref wlmtk_window_t::pending_updates. */
    bs_dllist_node_t          dlnode;
    /** Serial of the update. */
    uint32_t                  serial;
    /** Pending X position of the surface. */
    int                       x;
    /** Pending Y position of the surface. */
    int                       y;
    /** Surface's width that is to be committed at serial. */
    int                       width;
    /** Surface's hehight that is to be committed at serial. */
    int                       height;
} wlmtk_pending_update_t;

/** State of the window. */
struct _wlmtk_window_t {
    /** Superclass: Bordered. */
    wlmtk_bordered_t          super_bordered;
    /** Original virtual method table of the window's element superclass. */
    wlmtk_element_vmt_t       orig_super_element_vmt;
    /** Original virtual method table of the window' container superclass. */
    wlmtk_container_vmt_t     orig_super_container_vmt;

    /** Virtual method table. */
    wlmtk_window_vmt_t        vmt;

    /** Events for this window. */
    wlmtk_window_events_t     events;

    /** Box: In `super_bordered`, holds surface, title bar and resizebar. */
    wlmtk_box_t               box;

    /** FIXME: Element. */
    wlmtk_element_t           *element_ptr;
    /** Points to the workspace, if mapped. */
    wlmtk_workspace_t         *workspace_ptr;
    /** Environment, the argument passet to @ref wlmtk_window_create. */
    wlmtk_env_t               *env_ptr;
    /** Element in @ref wlmtk_workspace_t::windows, when mapped. */
    bs_dllist_node_t          dlnode;

    /** Content of the window. */
    wlmtk_content_t           *content_ptr;
    /** Titlebar. */
    wlmtk_titlebar_t          *titlebar_ptr;
    /** Resizebar. */
    wlmtk_resizebar_t         *resizebar_ptr;
    /** The window menu. */
    wlmtk_menu_t              *window_menu_ptr;
    /** Listener for then the popup menu requests to be closed. */
    struct wl_listener        menu_request_close_listener;

    /** Window title. Set through @ref wlmtk_window_set_title. */
    char                      *title_ptr;

    /** Pending updates. */
    bs_dllist_t               pending_updates;
    /** List of udpates currently available. */
    bs_dllist_t               available_updates;
    /** Pre-alloocated updates. */
    wlmtk_pending_update_t    pre_allocated_updates[WLMTK_WINDOW_MAX_PENDING];

    /** This window's properties. */
    uint32_t                  properties;

    /** Organic size of the window, ie. when not maximized. */
    struct wlr_box            organic_size;
    /** Whether the window has been requested as maximized. */
    bool                      maximized;
    /** Whether the window has been requested as fullscreen. */
    bool                      fullscreen;
    /**
     * Whether an "inorganic" sizing operation is in progress, and thus size
     * changes should not be recorded in @ref wlmtk_window_t::organic_size.
     *
     * This is eg. between @ref wlmtk_window_request_fullscreen and
     * @ref wlmtk_window_commit_fullscreen.
     */
    bool                      inorganic_sizing;

    /** Whether the window is currently shaded. */
    bool                      shaded;

    /**
     * Stores whether the window is server-side decorated.
     *
     * If the window is NOT fullscreen, then this is equivalent to
     * (titlebar_ptr != NULL && resizebar_ptr != NULL). For a fullscreen
     * window, titlebar and resizebar would be NULL, but the flag stores
     * whether decoration should be enabled on organic/maximized modes.
     */
    bool                      server_side_decorated;
    /** Stores whether the window is activated (keyboard focus). */
    bool                      activated;

    /** The style used for this window. */
    wlmtk_window_style_t      style;
};

/** State of a fake window: Includes the public record and the window. */
typedef struct {
    /** Window state. */
    wlmtk_window_t            window;
    /** Fake window - public state. */
    wlmtk_fake_window_t       fake_window;
} wlmtk_fake_window_state_t;

static bool _wlmtk_window_init(
    wlmtk_window_t *window_ptr,
    wlmtk_env_t *env_ptr,
    wlmtk_element_t *element_ptr);
static void _wlmtk_window_fini(wlmtk_window_t *window_ptr);
static wlmtk_window_vmt_t _wlmtk_window_extend(
    wlmtk_window_t *window_ptr,
    const wlmtk_window_vmt_t *window_vmt_ptr);

static bool _wlmtk_window_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);
static void _wlmtk_window_container_update_layout(
    wlmtk_container_t *container_ptr);

static void _wlmtk_window_request_minimize(wlmtk_window_t *window_ptr);
static void _wlmtk_window_request_move(wlmtk_window_t *window_ptr);
static void _wlmtk_window_request_resize(
    wlmtk_window_t *window_ptr,
    uint32_t edges);

static void _wlmtk_window_create_titlebar(wlmtk_window_t *window_ptr);
static void _wlmtk_window_create_resizebar(wlmtk_window_t *window_ptr);
static void _wlmtk_window_destroy_titlebar(wlmtk_window_t *window_ptr);
static void _wlmtk_window_destroy_resizebar(wlmtk_window_t *window_ptr);
static void _wlmtk_window_apply_decoration(wlmtk_window_t *window_ptr);
static void _wlmtk_window_request_position_and_size_decorated(
    wlmtk_window_t *window_ptr,
    int x,
    int y,
    int width,
    int height,
    bool include_titlebar,
    bool include_resizebar,
    bool include_extra);

static wlmtk_pending_update_t *_wlmtk_window_prepare_update(
    wlmtk_window_t *window_ptr);
static void _wlmtk_window_release_update(
    wlmtk_window_t *window_ptr,
    wlmtk_pending_update_t *update_ptr);

static void _wlmtk_window_menu_request_close_handler(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/** Virtual method table for the window's element superclass. */
static const wlmtk_element_vmt_t window_element_vmt = {
    .pointer_button = _wlmtk_window_element_pointer_button,
};
/** Virtual method table for the window's container superclass. */
static const wlmtk_container_vmt_t window_container_vmt = {
    .update_layout = _wlmtk_window_container_update_layout,
};
/** Virtual method table for the window itself. */
static const wlmtk_window_vmt_t _wlmtk_window_vmt = {
    .request_minimize = _wlmtk_window_request_minimize,
    .request_move = _wlmtk_window_request_move,
    .request_resize = _wlmtk_window_request_resize,
};

/** Default properties. Override by @ref wlmtk_window_set_properties. */
static const uint32_t _wlmtk_window_default_properties =
    WLMTK_WINDOW_PROPERTY_RESIZABLE |
    WLMTK_WINDOW_PROPERTY_ICONIFIABLE |
    WLMTK_WINDOW_PROPERTY_CLOSABLE;

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_window_t *wlmtk_window_create(
    wlmtk_content_t *content_ptr,
    const wlmtk_window_style_t *style_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmtk_window_t *window_ptr = logged_calloc(1, sizeof(wlmtk_window_t));
    if (NULL == window_ptr) return NULL;
    window_ptr->style = *style_ptr;

    if (!_wlmtk_window_init(
            window_ptr,
            env_ptr,
            wlmtk_content_element(content_ptr))) {
        wlmtk_window_destroy(window_ptr);
        return NULL;
    }
    window_ptr->content_ptr = content_ptr;
    wlmtk_content_set_window(content_ptr, window_ptr);

    // Create the window menu. It is kept hidden until invoked.
    window_ptr->window_menu_ptr = wlmtk_menu_create(
        menu_style_ptr,
        env_ptr);
    if (NULL == window_ptr->window_menu_ptr) {
        _wlmtk_window_fini(window_ptr);
        return false;
    }
    // TODO(kaeser@gubbe.ch): Do not access content element directly.
    wlmtk_container_add_element(
        &window_ptr->content_ptr->popup_container,
        wlmtk_menu_element(window_ptr->window_menu_ptr));
    wlmtk_util_connect_listener_signal(
        &wlmtk_menu_events(window_ptr->window_menu_ptr)->request_close,
        &window_ptr->menu_request_close_listener,
        _wlmtk_window_menu_request_close_handler);

    return window_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_destroy(wlmtk_window_t *window_ptr)
{
    _wlmtk_window_fini(window_ptr);
    free(window_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_window_events_t *wlmtk_window_events(
    wlmtk_window_t *window_ptr)
{
    return &window_ptr->events;
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_window_element(wlmtk_window_t *window_ptr)
{
    return &window_ptr->super_bordered.super_container.super_element;
}

/* ------------------------------------------------------------------------- */
wlmtk_window_t *wlmtk_window_from_element(wlmtk_element_t *element_ptr)
{
    wlmtk_window_t *window_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_window_t, super_bordered.super_container.super_element);
    BS_ASSERT(_wlmtk_window_container_update_layout ==
              window_ptr->super_bordered.super_container.vmt.update_layout);
    return window_ptr;
}

/* ------------------------------------------------------------------------- */
wlmtk_window_t *wlmtk_window_from_dlnode(bs_dllist_node_t *dlnode_ptr)
{
    return BS_CONTAINER_OF(dlnode_ptr, wlmtk_window_t, dlnode);
}

/* ------------------------------------------------------------------------- */
bs_dllist_node_t *wlmtk_dlnode_from_window(wlmtk_window_t *window_ptr)
{
    return &window_ptr->dlnode;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_set_activated(
    wlmtk_window_t *window_ptr,
    bool activated)
{
    if (window_ptr->activated == activated) return;

    window_ptr->activated = activated;
    wlmtk_content_set_activated(window_ptr->content_ptr, activated);
    if (NULL != window_ptr->titlebar_ptr) {
        wlmtk_titlebar_set_activated(window_ptr->titlebar_ptr, activated);
    }

    if (!activated) {
        wlmtk_window_menu_set_enabled(window_ptr, false);

        // TODO(kaeser@gubbe.ch): Should test this behaviour.
        if (NULL != window_ptr->workspace_ptr) {
            wlmtk_workspace_activate_window(window_ptr->workspace_ptr, NULL);
        }
    }
}

/* ------------------------------------------------------------------------- */
bool wlmtk_window_is_activated(wlmtk_window_t *window_ptr)
{
    return window_ptr->activated;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_set_server_side_decorated(
    wlmtk_window_t *window_ptr,
    bool decorated)
{
    if (window_ptr->server_side_decorated == decorated) return;

    if (!decorated && wlmtk_window_is_shaded(window_ptr)) {
        wlmtk_window_request_shaded(window_ptr, false);
    }

    window_ptr->server_side_decorated = decorated;
    _wlmtk_window_apply_decoration(window_ptr);

}

/* ------------------------------------------------------------------------- */
void wlmtk_window_set_properties(
    wlmtk_window_t *window_ptr,
    uint32_t properties)
{
    if (window_ptr->properties == properties) return;
    window_ptr->properties = properties;
    _wlmtk_window_apply_decoration(window_ptr);

    if (NULL != window_ptr->titlebar_ptr) {
        uint32_t properties = 0;
        if (window_ptr->properties & WLMTK_WINDOW_PROPERTY_ICONIFIABLE) {
            properties |= WLMTK_TITLEBAR_PROPERTY_ICONIFY;
        }
        if (window_ptr->properties & WLMTK_WINDOW_PROPERTY_CLOSABLE) {
            properties |= WLMTK_TITLEBAR_PROPERTY_CLOSE;
        }
        wlmtk_titlebar_set_properties(window_ptr->titlebar_ptr, properties);
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_set_title(
    wlmtk_window_t *window_ptr,
    const char *title_ptr)
{
    char *new_title_ptr = NULL;
    if (NULL != title_ptr) {
        new_title_ptr = logged_strdup(title_ptr);
        BS_ASSERT(NULL != new_title_ptr);
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Unnamed window %p", window_ptr);
        new_title_ptr = logged_strdup(buf);
        BS_ASSERT(NULL != new_title_ptr);
    }

    if (NULL != window_ptr->title_ptr) {
        if (0 == strcmp(window_ptr->title_ptr, new_title_ptr)) {
            free(new_title_ptr);
            return;
        }
        free(window_ptr->title_ptr);
    }
    window_ptr->title_ptr = new_title_ptr;

    if (NULL != window_ptr->titlebar_ptr) {
        wlmtk_titlebar_set_title(window_ptr->titlebar_ptr,
                                 window_ptr->title_ptr);
    }
}

/* ------------------------------------------------------------------------- */
const char *wlmtk_window_get_title(wlmtk_window_t *window_ptr)
{
    BS_ASSERT(NULL != window_ptr->title_ptr);
    return window_ptr->title_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_request_close(wlmtk_window_t *window_ptr)
{
    wlmtk_content_request_close(window_ptr->content_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_request_minimize(wlmtk_window_t *window_ptr)
{
    window_ptr->vmt.request_minimize(window_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_request_maximized(
    wlmtk_window_t *window_ptr,
    bool maximized)
{
    wlmtk_workspace_t *workspace_ptr = wlmtk_window_get_workspace(window_ptr);
    // We don't permit maximizing unmapped windows. Don't know the extents.
    if (NULL == workspace_ptr) maximized = false;

    if (window_ptr->shaded) return;
    if (window_ptr->maximized == maximized) return;
    if (window_ptr->fullscreen) return;

    window_ptr->inorganic_sizing = maximized;

    struct wlr_box box;
    if (maximized) {
        box = wlmtk_workspace_get_maximize_extents(workspace_ptr, NULL);
    } else {
        box = window_ptr->organic_size;
    }

    wlmtk_content_request_maximized(window_ptr->content_ptr, maximized);

    _wlmtk_window_request_position_and_size_decorated(
        window_ptr, box.x, box.y, box.width, box.height,
        window_ptr->server_side_decorated,
        window_ptr->server_side_decorated,
        !maximized);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_commit_maximized(
    wlmtk_window_t *window_ptr,
    bool maximized)
{
    // Guard clause: Nothing to do if already as committed.
    if (window_ptr->maximized == maximized) return;

    window_ptr->maximized = maximized;
    wl_signal_emit(&window_ptr->events.state_changed, window_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_window_is_maximized(wlmtk_window_t *window_ptr)
{
    return window_ptr->maximized;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_request_fullscreen(
    wlmtk_window_t *window_ptr,
    bool fullscreen)
{
    struct wlr_box box;
    uint32_t serial;
    wlmtk_pending_update_t *pending_update_ptr;

    if (window_ptr->shaded) return;

    // Must be mapped.
    BS_ASSERT(NULL != wlmtk_window_get_workspace(window_ptr));

    window_ptr->inorganic_sizing = fullscreen;

    // Will not line up another pending update.
    wlmtk_content_request_fullscreen(window_ptr->content_ptr, fullscreen);

    if (fullscreen) {
        box = wlmtk_workspace_get_fullscreen_extents(
            wlmtk_window_get_workspace(window_ptr), NULL);
        serial = wlmtk_content_request_size(
            window_ptr->content_ptr, box.width, box.height);
        pending_update_ptr = _wlmtk_window_prepare_update(window_ptr);
        pending_update_ptr->serial = serial;
        pending_update_ptr->x = box.x;
        pending_update_ptr->y = box.y;
        pending_update_ptr->width = box.width;
        pending_update_ptr->height = box.height;

    } else {

        box = window_ptr->organic_size;
        _wlmtk_window_request_position_and_size_decorated(
            window_ptr, box.x, box.y, box.width, box.height,
            window_ptr->server_side_decorated,
            window_ptr->server_side_decorated,
            true);
    }

}

/* ------------------------------------------------------------------------- */
void wlmtk_window_commit_fullscreen(
    wlmtk_window_t *window_ptr,
    bool fullscreen)
{
    // Guard clause: Nothing to do if we're already there.
    if (window_ptr->fullscreen == fullscreen) return;

    window_ptr->fullscreen = fullscreen;
    _wlmtk_window_apply_decoration(window_ptr);

    wlmtk_workspace_window_to_fullscreen(
        wlmtk_window_get_workspace(window_ptr), window_ptr, fullscreen);
    wl_signal_emit(&window_ptr->events.state_changed, window_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_window_is_fullscreen(wlmtk_window_t *window_ptr)
{
    return window_ptr->fullscreen;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_request_shaded(wlmtk_window_t *window_ptr, bool shaded)
{
    if (window_ptr->fullscreen ||
        !window_ptr->server_side_decorated ||
        window_ptr->shaded == shaded) return;

    wlmtk_element_set_visible(
        wlmtk_content_element(window_ptr->content_ptr), !shaded);
    if (NULL != window_ptr->resizebar_ptr) {
        wlmtk_element_set_visible(
            wlmtk_resizebar_element(window_ptr->resizebar_ptr), !shaded);
    }

    window_ptr->shaded = shaded;
    wl_signal_emit(&window_ptr->events.state_changed, window_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_window_is_shaded(wlmtk_window_t *window_ptr)
{
    return window_ptr->shaded;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_menu_set_enabled(
    wlmtk_window_t *window_ptr,
    bool enabled)
{
    if (!window_ptr->activated) enabled = false;

    // For convenience: Get the menu's element. Note: It must have a parent,
    // since it's contained within the window.
    wlmtk_element_t *menu_element_ptr = wlmtk_menu_element(
        window_ptr->window_menu_ptr);
    BS_ASSERT(NULL != menu_element_ptr->parent_container_ptr);

    wlmtk_menu_set_open(window_ptr->window_menu_ptr, enabled);

    if (enabled) {
        wlmtk_menu_set_mode(
            window_ptr->window_menu_ptr,
            WLMTK_MENU_MODE_RIGHTCLICK);
        wlmtk_container_raise_element_to_top(
            menu_element_ptr->parent_container_ptr,
            menu_element_ptr);
        wlmtk_container_pointer_grab(
            menu_element_ptr->parent_container_ptr,
            menu_element_ptr);

        if (wlmtk_window_element(window_ptr)->pointer_inside) {
            wlmtk_element_set_position(
                menu_element_ptr,
                wlmtk_window_element(window_ptr)->last_pointer_x, 0);
        } else {
            wlmtk_element_set_position(menu_element_ptr, 0, 0);
        }
    } else {
        wlmtk_container_pointer_grab_release(
            menu_element_ptr->parent_container_ptr,
            menu_element_ptr);
    }
}

/* ------------------------------------------------------------------------- */
wlmtk_menu_t *wlmtk_window_menu(wlmtk_window_t *window_ptr)
{
    return window_ptr->window_menu_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_request_move(wlmtk_window_t *window_ptr)
{
    window_ptr->vmt.request_move(window_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_request_resize(wlmtk_window_t *window_ptr,
                                 uint32_t edges)
{
    window_ptr->vmt.request_resize(window_ptr, edges);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_set_position(wlmtk_window_t *window_ptr, int x, int y)
{
    window_ptr->organic_size.x = x;
    window_ptr->organic_size.y = y;
    wlmtk_element_set_position(wlmtk_window_element(window_ptr), x, y);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_get_size(
    wlmtk_window_t *window_ptr,
    int *width_ptr,
    int *height_ptr)
{
    struct wlr_box dimensions = wlmtk_element_get_dimensions_box(
        wlmtk_content_element(window_ptr->content_ptr));
    *width_ptr = dimensions.width;
    *height_ptr = dimensions.height;

    if (NULL != window_ptr->titlebar_ptr) {
        *height_ptr += window_ptr->style.titlebar.height +
            window_ptr->style.margin.width;
    }
    if (NULL != window_ptr->resizebar_ptr) {
        *height_ptr += window_ptr->style.resizebar.height +
            window_ptr->style.margin.width;
    }
    *height_ptr += 2 * window_ptr->super_bordered.style.width;

    *width_ptr += 2 * window_ptr->super_bordered.style.width;
}

/* ------------------------------------------------------------------------- */
struct wlr_box wlmtk_window_get_position_and_size(
    wlmtk_window_t *window_ptr)
{
    struct wlr_box box;

    wlmtk_element_get_position(
        wlmtk_window_element(window_ptr), &box.x, &box.y);
    wlmtk_window_get_size(window_ptr, &box.width, &box.height);
    return box;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_request_position_and_size(
    wlmtk_window_t *window_ptr,
    int x,
    int y,
    int width,
    int height)
{
    _wlmtk_window_request_position_and_size_decorated(
        window_ptr, x, y, width, height,
        NULL != window_ptr->titlebar_ptr,
        NULL != window_ptr->resizebar_ptr,
        true);

    window_ptr->organic_size.x = x;
    window_ptr->organic_size.y = y;
    window_ptr->organic_size.width = width;
    window_ptr->organic_size.height = height;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_serial(wlmtk_window_t *window_ptr, uint32_t serial)
{
    bs_dllist_node_t *dlnode_ptr;

    if (!window_ptr->inorganic_sizing &&
        NULL == window_ptr->pending_updates.head_ptr) {
        wlmtk_window_get_size(window_ptr,
                              &window_ptr->organic_size.width,
                              &window_ptr->organic_size.height);
        return;
    }

    while (NULL != (dlnode_ptr = window_ptr->pending_updates.head_ptr)) {
        wlmtk_pending_update_t *pending_update_ptr = BS_CONTAINER_OF(
            dlnode_ptr, wlmtk_pending_update_t, dlnode);

        int32_t delta = pending_update_ptr->serial - serial;
        if (0 < delta) break;

        wlmtk_element_set_position(
            wlmtk_window_element(window_ptr),
            pending_update_ptr->x,
            pending_update_ptr->y);
        _wlmtk_window_release_update(window_ptr, pending_update_ptr);
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_set_workspace(
    wlmtk_window_t *window_ptr,
    wlmtk_workspace_t *workspace_ptr)
{
    window_ptr->workspace_ptr = workspace_ptr;

    wl_signal_emit(&window_ptr->events.state_changed, window_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_workspace_t *wlmtk_window_get_workspace(wlmtk_window_t *window_ptr)
{
    return window_ptr->workspace_ptr;
}

/* ------------------------------------------------------------------------- */
const wlmtk_util_client_t *wlmtk_window_get_client_ptr(
    wlmtk_window_t *window_ptr)
{
    if (NULL == window_ptr->content_ptr) return NULL;
    return &window_ptr->content_ptr->client;
}


/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Initializes an (allocated) window.
 *
 * @param window_ptr
 * @param env_ptr
 * @param element_ptr
 *
 * @return true on success.
 */
bool _wlmtk_window_init(
    wlmtk_window_t *window_ptr,
    wlmtk_env_t *env_ptr,
    wlmtk_element_t *element_ptr)
{
    BS_ASSERT(NULL != window_ptr);
    memcpy(&window_ptr->vmt, &_wlmtk_window_vmt, sizeof(wlmtk_window_vmt_t));
    window_ptr->env_ptr = env_ptr;

    for (size_t i = 0; i < WLMTK_WINDOW_MAX_PENDING; ++i) {
        bs_dllist_push_back(&window_ptr->available_updates,
                            &window_ptr->pre_allocated_updates[i].dlnode);
    }

    if (!wlmtk_box_init(&window_ptr->box, env_ptr,
                        WLMTK_BOX_VERTICAL,
                        &window_ptr->style.margin)) {
        _wlmtk_window_fini(window_ptr);
        return false;
    }
    wlmtk_element_set_visible(
        &window_ptr->box.super_container.super_element, true);

    if (!wlmtk_bordered_init(&window_ptr->super_bordered,
                             env_ptr,
                             &window_ptr->box.super_container.super_element,
                             &window_ptr->style.border)) {
        _wlmtk_window_fini(window_ptr);
        return false;
    }

    window_ptr->orig_super_element_vmt = wlmtk_element_extend(
        &window_ptr->super_bordered.super_container.super_element,
        &window_element_vmt);
    window_ptr->orig_super_container_vmt = wlmtk_container_extend(
        &window_ptr->super_bordered.super_container, &window_container_vmt);
    window_ptr->element_ptr = element_ptr;

    wlmtk_window_set_title(window_ptr, NULL);
    // Also triggers _wlmtk_window_apply_decoration(window_ptr).
    wlmtk_window_set_properties(window_ptr, _wlmtk_window_default_properties);

    wlmtk_box_add_element_front(&window_ptr->box, element_ptr);
    wlmtk_element_set_visible(element_ptr, true);

    wl_signal_init(&window_ptr->events.state_changed);
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Uninitializes the window.
 *
 * @param window_ptr
 */
void _wlmtk_window_fini(wlmtk_window_t *window_ptr)
{
    if (NULL != window_ptr->window_menu_ptr) {
        wlmtk_util_disconnect_listener(
            &window_ptr->menu_request_close_listener);

        wlmtk_container_remove_element(
            &window_ptr->content_ptr->popup_container,
            wlmtk_menu_element(window_ptr->window_menu_ptr));
        wlmtk_menu_destroy(window_ptr->window_menu_ptr);
        window_ptr->window_menu_ptr = NULL;
    }

    wlmtk_window_set_server_side_decorated(window_ptr, false);

    if (NULL != window_ptr->content_ptr) {
        wlmtk_content_set_window(window_ptr->content_ptr, NULL);
    }

    if (NULL != window_ptr->element_ptr) {
        wlmtk_box_remove_element(
            &window_ptr->box, window_ptr->element_ptr);
        wlmtk_element_set_visible(window_ptr->element_ptr, false);
        window_ptr->element_ptr = NULL;
    }

    if (NULL != window_ptr->title_ptr) {
        free(window_ptr->title_ptr);
        window_ptr->title_ptr = NULL;
    }

    wlmtk_bordered_fini(&window_ptr->super_bordered);
    wlmtk_box_fini(&window_ptr->box);
}

/* ------------------------------------------------------------------------- */
/**
 * Extends the window's virtual methods.
 *
 * @param window_ptr
 * @param window_vmt_ptr
 *
 * @return The previous virtual method table.
 */
wlmtk_window_vmt_t _wlmtk_window_extend(
    wlmtk_window_t *window_ptr,
    const wlmtk_window_vmt_t *window_vmt_ptr)
{
    wlmtk_window_vmt_t orig_vmt = window_ptr->vmt;

    if (NULL != window_vmt_ptr->request_minimize) {
        window_ptr->vmt.request_minimize = window_vmt_ptr->request_minimize;
    }
    if (NULL != window_vmt_ptr->request_move) {
        window_ptr->vmt.request_move = window_vmt_ptr->request_move;
    }
    if (NULL != window_vmt_ptr->request_resize) {
        window_ptr->vmt.request_resize = window_vmt_ptr->request_resize;
    }

    return orig_vmt;
}

/* ------------------------------------------------------------------------- */
/** Activates window on button press, and calls the parent's implementation. */
bool _wlmtk_window_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmtk_window_t *window_ptr = BS_CONTAINER_OF(
        element_ptr,
        wlmtk_window_t,
        super_bordered.super_container.super_element);

    // In right-click mode: Any out-of-window action will close it.
    // TODO(kaeser@gubbe.ch): This should be a specific window mode, and should
    // have a handler method when leaving that mode (eg. left release within
    // the window).
    if (window_ptr->properties & WLMTK_WINDOW_PROPERTY_RIGHTCLICK) {
        bool rv = window_ptr->orig_super_element_vmt.pointer_button(
            element_ptr, button_event_ptr);
        if (BTN_RIGHT == button_event_ptr->button &&
            WLMTK_BUTTON_UP == button_event_ptr->type &&
            !rv) {
            wlmtk_window_request_close(window_ptr);
            return true;
        }
        return rv;
    }

    // Permit drag-move with the (hardcoded) modifier.
    // TODO(kaeser@gubbe.ch): This should be changed to make use of "DRAG"
    // events, with corresponding modifiers. Do so, once added to toolkit.
    if (NULL != window_ptr->env_ptr) {
        struct wlr_keyboard *wlr_keyboard_ptr = wlr_seat_get_keyboard(
            wlmtk_env_wlr_seat(window_ptr->env_ptr));
        uint32_t modifiers = wlr_keyboard_get_modifiers(wlr_keyboard_ptr);
        if (BTN_LEFT == button_event_ptr->button &&
            WLMTK_BUTTON_DOWN == button_event_ptr->type &&
            (WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO) == modifiers) {
            wlmtk_window_request_move(window_ptr);
            return true;
        }
    }

    // We shouldn't receive buttons when not mapped.
    wlmtk_workspace_t *workspace_ptr = wlmtk_window_get_workspace(window_ptr);
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
void _wlmtk_window_container_update_layout(wlmtk_container_t *container_ptr)
{
    wlmtk_window_t *window_ptr = BS_CONTAINER_OF(
        container_ptr, wlmtk_window_t, super_bordered.super_container);

    window_ptr->orig_super_container_vmt.update_layout(container_ptr);

    if (NULL != window_ptr->content_ptr) {
        int width;
        wlmtk_content_get_size(window_ptr->content_ptr, &width, NULL);
        if (NULL != window_ptr->titlebar_ptr) {
            wlmtk_titlebar_set_width(window_ptr->titlebar_ptr, width);
        }
        if (NULL != window_ptr->resizebar_ptr) {
            wlmtk_resizebar_set_width(window_ptr->resizebar_ptr, width);
        }
    }
}

/* ------------------------------------------------------------------------- */
/** Default implementation of @ref wlmtk_window_request_minimize. */
void _wlmtk_window_request_minimize(wlmtk_window_t *window_ptr)
{
    bs_log(BS_INFO, "Requesting window %p to minimize.", window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Default implementation of @ref wlmtk_window_request_move. */
void _wlmtk_window_request_move(wlmtk_window_t *window_ptr)
{
    BS_ASSERT(NULL != wlmtk_window_get_workspace(window_ptr));
    wlmtk_workspace_begin_window_move(
        wlmtk_window_get_workspace(window_ptr), window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Default implementation of @ref wlmtk_window_request_resize. */
void _wlmtk_window_request_resize(wlmtk_window_t *window_ptr, uint32_t edges)
{
    BS_ASSERT(NULL != wlmtk_window_get_workspace(window_ptr));
    wlmtk_workspace_begin_window_resize(
        wlmtk_window_get_workspace(window_ptr), window_ptr, edges);
}

/* ------------------------------------------------------------------------- */
/** Creates the titlebar. Expects server_side_decorated to be set. */
void _wlmtk_window_create_titlebar(wlmtk_window_t *window_ptr)
{
    BS_ASSERT(window_ptr->server_side_decorated && !window_ptr->fullscreen);

    // Guard clause: Don't add decoration.
    if (NULL != window_ptr->titlebar_ptr) return;

    // Create decoration.
    window_ptr->titlebar_ptr = wlmtk_titlebar_create(
        window_ptr->super_bordered.super_container.super_element.env_ptr,
        window_ptr, &window_ptr->style.titlebar);
    BS_ASSERT(NULL != window_ptr->titlebar_ptr);
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
    wlmtk_element_set_visible(
        wlmtk_titlebar_element(window_ptr->titlebar_ptr), true);
    // Hm, if the content has a popup that extends over the titlebar area,
    // it'll be partially obscured. That will look odd... Well, let's
    // address that problem once there's a situation.
    wlmtk_box_add_element_front(
        &window_ptr->box,
        wlmtk_titlebar_element(window_ptr->titlebar_ptr));
}

/* ------------------------------------------------------------------------- */
/** Creates the resizebar. Expects server_side_decorated to be set. */
void _wlmtk_window_create_resizebar(wlmtk_window_t *window_ptr)
{
    BS_ASSERT(window_ptr->server_side_decorated && !window_ptr->fullscreen);

    // Guard clause: Don't add decoration.
    if (NULL != window_ptr->resizebar_ptr) return;

    window_ptr->resizebar_ptr = wlmtk_resizebar_create(
        window_ptr->super_bordered.super_container.super_element.env_ptr,
        window_ptr, &window_ptr->style.resizebar);
    BS_ASSERT(NULL != window_ptr->resizebar_ptr);
    wlmtk_element_set_visible(
        wlmtk_resizebar_element(window_ptr->resizebar_ptr), true);
    wlmtk_box_add_element_back(
        &window_ptr->box,
        wlmtk_resizebar_element(window_ptr->resizebar_ptr));
}

/* ------------------------------------------------------------------------- */
/** Destroys the titlebar. */
void _wlmtk_window_destroy_titlebar(wlmtk_window_t *window_ptr)
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
/** Destroys the resizebar. */
void _wlmtk_window_destroy_resizebar(wlmtk_window_t *window_ptr)
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
/** Applies window decoration depending on current state. */
void _wlmtk_window_apply_decoration(wlmtk_window_t *window_ptr)
{
    wlmtk_margin_style_t bstyle = window_ptr->style.border;

    if (window_ptr->server_side_decorated && !window_ptr->fullscreen) {
        _wlmtk_window_create_titlebar(window_ptr);
    } else {
        bstyle.width = 0;
        _wlmtk_window_destroy_titlebar(window_ptr);
    }

    if (window_ptr->server_side_decorated &&
        !window_ptr->fullscreen &&
        (window_ptr->properties & WLMTK_WINDOW_PROPERTY_RESIZABLE)) {
        _wlmtk_window_create_resizebar(window_ptr);
    } else {
        _wlmtk_window_destroy_resizebar(window_ptr);
    }

    wlmtk_bordered_set_style(&window_ptr->super_bordered, &bstyle);
}

/* ------------------------------------------------------------------------- */
/**
 * Helper: Requests position and size, factoring in decoration.
 *
 * @param window_ptr
 * @param x
 * @param y
 * @param width
 * @param height
 * @param include_titlebar
 * @param include_resizebar
 * @param include_extra
 */
void _wlmtk_window_request_position_and_size_decorated(
    wlmtk_window_t *window_ptr,
    int x,
    int y,
    int width,
    int height,
    bool include_titlebar,
    bool include_resizebar,
    bool include_extra)
{
    // Correct for borders, margin and decoration.
    if (include_titlebar) {
        height -= window_ptr->style.titlebar.height +
            window_ptr->style.margin.width;
    }
    if (include_resizebar) {
        height -= window_ptr->style.resizebar.height +
            window_ptr->style.margin.width;
    }
    if (include_titlebar || include_resizebar) {
        height -= 2 * window_ptr->style.border.width;
        width -= 2 * window_ptr->style.border.width;
    }

    // Account for potential extra size beyond the content: For example, by
    // sub-surfaces that clients use for borders or resize-areas.
    if (include_extra) {
        struct wlr_box dimensions = wlmtk_element_get_dimensions_box(
            wlmtk_content_element(window_ptr->content_ptr));
        int w, h;
        wlmtk_content_get_size(window_ptr->content_ptr, &w, &h);
        width += w - dimensions.width;
        height += h - dimensions.height;
    }

    height = BS_MAX(0, height);
    width = BS_MAX(0, width);

    uint32_t serial = wlmtk_content_request_size(
        window_ptr->content_ptr, width, height);

    wlmtk_pending_update_t *pending_update_ptr =
        _wlmtk_window_prepare_update(window_ptr);
    pending_update_ptr->serial = serial;
    pending_update_ptr->x = x;
    pending_update_ptr->y = y;
    pending_update_ptr->width = width;
    pending_update_ptr->height = height;

    // TODO(kaeser@gubbe.ch): Handle synchronous case: @ref wlmtk_window_serial
    // may have been called early, so we should check if serial had just been
    // called before (or is below the last @wlmt_window_serial). In that case,
    // the pending state should be applied right away.
}

/* ------------------------------------------------------------------------- */
/**
 * Prepares a positional update: Allocates an item and attach it to the end
 * of the list of pending updates.
 *
 * @param window_ptr
 *
 * @return A pointer to a @ref wlmtk_pending_update_t, already positioned at the
 *     back of @ref wlmtk_window_t::pending_updates.
 */
wlmtk_pending_update_t *_wlmtk_window_prepare_update(
    wlmtk_window_t *window_ptr)
{
    bs_dllist_node_t *dlnode_ptr = bs_dllist_pop_front(
        &window_ptr->available_updates);
    if (NULL == dlnode_ptr) {
        dlnode_ptr = bs_dllist_pop_front(&window_ptr->pending_updates);
        bs_log(BS_WARNING, "Window %p: No updates available.", window_ptr);
        // TODO(kaeser@gubbe.ch): Hm, should we apply this (old) update?
    }
    wlmtk_pending_update_t *update_ptr = BS_CONTAINER_OF(
        dlnode_ptr, wlmtk_pending_update_t, dlnode);
    bs_dllist_push_back(&window_ptr->pending_updates, &update_ptr->dlnode);
    return update_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Releases a pending positional update. Moves it to the list of
 * @ref wlmtk_window_t::available_updates.
 *
 * @param window_ptr
 * @param update_ptr
 */
void _wlmtk_window_release_update(
    wlmtk_window_t *window_ptr,
    wlmtk_pending_update_t *update_ptr)
{
    bs_dllist_remove(&window_ptr->pending_updates, &update_ptr->dlnode);
    bs_dllist_push_front(&window_ptr->available_updates, &update_ptr->dlnode);
}

/* ------------------------------------------------------------------------- */
/** Handles @ref wlmtk_menu_events_t::request_close signals. */
void _wlmtk_window_menu_request_close_handler(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_window_t *window_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_window_t, menu_request_close_listener);
    wlmtk_window_menu_set_enabled(window_ptr, false);
}

/* == Implementation of the fake window ==================================== */

static void _wlmtk_fake_window_request_minimize(wlmtk_window_t *window_ptr);
static void _wlmtk_fake_window_request_move(wlmtk_window_t *window_ptr);
static void _wlmtk_fake_window_request_resize(
    wlmtk_window_t *window_ptr,
    uint32_t edges);

/** Virtual method table for the fake window itself. */
static const wlmtk_window_vmt_t _wlmtk_fake_window_vmt = {
    .request_minimize = _wlmtk_fake_window_request_minimize,
    .request_move = _wlmtk_fake_window_request_move,
    .request_resize = _wlmtk_fake_window_request_resize,

};

/* ------------------------------------------------------------------------- */
wlmtk_fake_window_t *wlmtk_fake_window_create(void)
{
    wlmtk_fake_window_state_t *fake_window_state_ptr = logged_calloc(
        1, sizeof(wlmtk_fake_window_state_t));
    if (NULL == fake_window_state_ptr) return NULL;

    fake_window_state_ptr->fake_window.fake_surface_ptr =
        wlmtk_fake_surface_create();
    if (NULL == fake_window_state_ptr->fake_window.fake_surface_ptr) {
        wlmtk_fake_window_destroy(&fake_window_state_ptr->fake_window);
        return NULL;
    }

    fake_window_state_ptr->fake_window.fake_content_ptr =
        wlmtk_fake_content_create(
            fake_window_state_ptr->fake_window.fake_surface_ptr);
    if (NULL == fake_window_state_ptr->fake_window.fake_content_ptr) {
        wlmtk_fake_window_destroy(&fake_window_state_ptr->fake_window);
        return NULL;
    }

    fake_window_state_ptr->window.style.border.width = 1;
    if (!_wlmtk_window_init(
            &fake_window_state_ptr->window,
            NULL,
            wlmtk_content_element(
                &fake_window_state_ptr->fake_window.fake_content_ptr->content)
            )) {
        wlmtk_fake_window_destroy(&fake_window_state_ptr->fake_window);
        return NULL;
    }
    fake_window_state_ptr->fake_window.window_ptr =
        &fake_window_state_ptr->window;
    fake_window_state_ptr->fake_window.window_ptr->content_ptr =
        &fake_window_state_ptr->fake_window.fake_content_ptr->content;

    wlmtk_content_set_window(
        &fake_window_state_ptr->fake_window.fake_content_ptr->content,
        fake_window_state_ptr->fake_window.window_ptr);

    wlmtk_menu_style_t ms = {};
    fake_window_state_ptr->fake_window.window_ptr->window_menu_ptr =
        wlmtk_menu_create(&ms, NULL);

    wlmtk_container_add_element(
        &fake_window_state_ptr->fake_window.window_ptr->content_ptr->popup_container,
        wlmtk_menu_element(fake_window_state_ptr->fake_window.window_ptr->window_menu_ptr));
    fake_window_state_ptr->fake_window.window_menu_ptr =
        fake_window_state_ptr->fake_window.window_ptr->window_menu_ptr;

    // Extend. We don't save the VMT, since it's for fake only.
    _wlmtk_window_extend(&fake_window_state_ptr->window,
                         &_wlmtk_fake_window_vmt);
    return &fake_window_state_ptr->fake_window;
}

/* ------------------------------------------------------------------------- */
void wlmtk_fake_window_destroy(wlmtk_fake_window_t *fake_window_ptr)
{
    wlmtk_fake_window_state_t *fake_window_state_ptr = BS_CONTAINER_OF(
        fake_window_ptr, wlmtk_fake_window_state_t, fake_window);

    _wlmtk_window_fini(&fake_window_state_ptr->window);

    if (NULL != fake_window_state_ptr->fake_window.fake_content_ptr) {
        wlmtk_fake_content_destroy(
            fake_window_state_ptr->fake_window.fake_content_ptr);
        fake_window_state_ptr->fake_window.fake_content_ptr = NULL;
    }

    if (NULL != fake_window_state_ptr->fake_window.fake_surface_ptr) {
        wlmtk_fake_surface_destroy(
            fake_window_state_ptr->fake_window.fake_surface_ptr);
        fake_window_state_ptr->fake_window.fake_surface_ptr = NULL;
    }

    free(fake_window_state_ptr);
}

/* ------------------------------------------------------------------------- */
/** Calls commit_size with the fake surface's serial and dimensions. */
void wlmtk_fake_window_commit_size(wlmtk_fake_window_t *fake_window_ptr)
{
    wlmtk_fake_content_commit(fake_window_ptr->fake_content_ptr);
}

/* ------------------------------------------------------------------------- */
/** Fake implementation of @ref wlmtk_window_request_minimize. Records call. */
void _wlmtk_fake_window_request_minimize(wlmtk_window_t *window_ptr)
{
    wlmtk_fake_window_state_t *fake_window_state_ptr = BS_CONTAINER_OF(
        window_ptr, wlmtk_fake_window_state_t, window);
    fake_window_state_ptr->fake_window.request_minimize_called = true;
}

/* ------------------------------------------------------------------------- */
/** Fake implementation of @ref wlmtk_window_request_move. Records call */
void _wlmtk_fake_window_request_move(wlmtk_window_t *window_ptr)
{
    wlmtk_fake_window_state_t *fake_window_state_ptr = BS_CONTAINER_OF(
        window_ptr, wlmtk_fake_window_state_t, window);
    fake_window_state_ptr->fake_window.request_move_called = true;
}

/* ------------------------------------------------------------------------- */
/** Fake implementation of @ref wlmtk_window_request_resize. Records call. */
void _wlmtk_fake_window_request_resize(
    wlmtk_window_t *window_ptr,
    uint32_t edges)
{
    wlmtk_fake_window_state_t *fake_window_state_ptr = BS_CONTAINER_OF(
        window_ptr, wlmtk_fake_window_state_t, window);
    fake_window_state_ptr->fake_window.request_resize_called = true;
    fake_window_state_ptr->fake_window.request_resize_edges = edges;
}

/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);
static void test_set_title(bs_test_t *test_ptr);
static void test_request_close(bs_test_t *test_ptr);
static void test_set_activated(bs_test_t *test_ptr);
static void test_server_side_decorated(bs_test_t *test_ptr);
static void test_server_side_decorated_properties(bs_test_t *test_ptr);
static void test_maximize(bs_test_t *test_ptr);
static void test_fullscreen(bs_test_t *test_ptr);
static void test_fullscreen_unmap(bs_test_t *test_ptr);
static void test_shade(bs_test_t *test_ptr);
static void test_fake(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_window_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "set_title", test_set_title },
    { 1, "request_close", test_request_close },
    { 1, "set_activated", test_set_activated },
    { 1, "set_server_side_decorated", test_server_side_decorated },
    { 1, "set_server_side_decorated_properties",
      test_server_side_decorated_properties },
    { 1, "maximize", test_maximize },
    { 1, "fullscreen", test_fullscreen },
    { 1, "fullscreen_unmap", test_fullscreen_unmap },
    { 1, "shade", test_shade },
    { 1, "fake", test_fake },
    { 0, NULL, NULL }
};

/** Tile style used in tests. */
static const wlmtk_tile_style_t _wlmtk_workspace_test_tile_style = {
    .size = 64
};

/* ------------------------------------------------------------------------- */
/** Tests setup and teardown. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_fake_surface_t *fake_surface_ptr = wlmtk_fake_surface_create();
    wlmtk_window_style_t s = {};
    wlmtk_menu_style_t ms = {};
    wlmtk_content_t content;
    wlmtk_content_init(
        &content,
        wlmtk_surface_element(&fake_surface_ptr->surface),
        NULL);
    wlmtk_window_t *window_ptr = wlmtk_window_create(&content, &s, &ms, NULL);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, window_ptr, content.window_ptr);

    bs_dllist_node_t *dln_ptr = wlmtk_dlnode_from_window(window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, dln_ptr, &window_ptr->dlnode);
    BS_TEST_VERIFY_EQ(test_ptr, window_ptr, wlmtk_window_from_dlnode(dln_ptr));

    wlmtk_window_destroy(window_ptr);
    wlmtk_content_fini(&content);
    wlmtk_fake_surface_destroy(fake_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests title. */
void test_set_title(bs_test_t *test_ptr)
{
    wlmtk_fake_window_t *fw_ptr = wlmtk_fake_window_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fw_ptr);

    wlmtk_window_set_title(fw_ptr->window_ptr, "Title");
    BS_TEST_VERIFY_STREQ(
        test_ptr,
        "Title",
        wlmtk_window_get_title(fw_ptr->window_ptr));

    wlmtk_window_set_title(fw_ptr->window_ptr, NULL);
    BS_TEST_VERIFY_STRMATCH(
        test_ptr,
        wlmtk_window_get_title(fw_ptr->window_ptr),
        "Unnamed window .*");

    wlmtk_fake_window_destroy(fw_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests activation. */
void test_request_close(bs_test_t *test_ptr)
{
    wlmtk_fake_window_t *fw_ptr = wlmtk_fake_window_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fw_ptr);

    wlmtk_window_request_close(fw_ptr->window_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        fw_ptr->fake_content_ptr->request_close_called);

    wlmtk_fake_window_destroy(fw_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests activation. */
void test_set_activated(bs_test_t *test_ptr)
{
    wlmtk_fake_window_t *fw_ptr = wlmtk_fake_window_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fw_ptr);

    wlmtk_window_set_activated(fw_ptr->window_ptr, true);
    BS_TEST_VERIFY_TRUE(test_ptr, fw_ptr->fake_content_ptr->activated);

    wlmtk_window_set_activated(fw_ptr->window_ptr, false);
    BS_TEST_VERIFY_FALSE(test_ptr, fw_ptr->fake_content_ptr->activated);

    wlmtk_fake_window_destroy(fw_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests enabling and disabling server-side decoration. */
void test_server_side_decorated(bs_test_t *test_ptr)
{
    struct wl_display *display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(display_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_output_layout_ptr);
    struct wlr_output output = { .width = 1024, .height = 768, .scale = 1 };
    wlmtk_test_wlr_output_init(&output);
    wlr_output_layout_add(wlr_output_layout_ptr, &output, 0, 0);

    wlmtk_workspace_t *ws_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "test", &_wlmtk_workspace_test_tile_style, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_fake_window_t *fw_ptr = wlmtk_fake_window_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fw_ptr);

    wlmtk_workspace_map_window(ws_ptr, fw_ptr->window_ptr);

    BS_TEST_VERIFY_EQ(test_ptr, NULL, fw_ptr->window_ptr->titlebar_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, fw_ptr->window_ptr->resizebar_ptr);

    wlmtk_window_set_server_side_decorated(fw_ptr->window_ptr, true);
    BS_TEST_VERIFY_TRUE(test_ptr, fw_ptr->window_ptr->server_side_decorated);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fw_ptr->window_ptr->titlebar_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fw_ptr->window_ptr->resizebar_ptr);

    // Maximize the window: We keep the decoration.
    wlmtk_window_request_maximized(fw_ptr->window_ptr, true);
    wlmtk_fake_window_commit_size(fw_ptr);
    wlmtk_window_commit_maximized(fw_ptr->window_ptr, true);
    BS_TEST_VERIFY_TRUE(test_ptr, fw_ptr->window_ptr->server_side_decorated);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fw_ptr->window_ptr->titlebar_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fw_ptr->window_ptr->resizebar_ptr);

    // Make the window fullscreen: Hide the decoration.
    wlmtk_window_request_fullscreen(fw_ptr->window_ptr, true);
    wlmtk_fake_window_commit_size(fw_ptr);
    wlmtk_window_commit_maximized(fw_ptr->window_ptr, false);
    wlmtk_window_commit_fullscreen(fw_ptr->window_ptr, true);
    BS_TEST_VERIFY_TRUE(test_ptr, fw_ptr->window_ptr->server_side_decorated);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, fw_ptr->window_ptr->titlebar_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, fw_ptr->window_ptr->resizebar_ptr);

    // Back to organic size: Decoration is on.
    wlmtk_window_request_fullscreen(fw_ptr->window_ptr, false);
    wlmtk_fake_window_commit_size(fw_ptr);
    wlmtk_window_commit_fullscreen(fw_ptr->window_ptr, false);
    BS_TEST_VERIFY_TRUE(test_ptr, fw_ptr->window_ptr->server_side_decorated);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fw_ptr->window_ptr->titlebar_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fw_ptr->window_ptr->resizebar_ptr);

    // Disable decoration.
    wlmtk_window_set_server_side_decorated(fw_ptr->window_ptr, false);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, fw_ptr->window_ptr->titlebar_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, fw_ptr->window_ptr->resizebar_ptr);

    wlmtk_workspace_unmap_window(ws_ptr, fw_ptr->window_ptr);

    wlmtk_fake_window_destroy(fw_ptr);
    wlmtk_workspace_destroy(ws_ptr);
    wl_display_destroy(display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests server-side decoration depending on properties. */
void test_server_side_decorated_properties(bs_test_t *test_ptr)
{
    struct wl_display *display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(display_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_output_layout_ptr);
    struct wlr_output output = { .width = 1024, .height = 768, .scale = 1 };
    wlmtk_test_wlr_output_init(&output);
    wlr_output_layout_add(wlr_output_layout_ptr, &output, 0, 0);

    wlmtk_workspace_t *ws_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "test", &_wlmtk_workspace_test_tile_style, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_fake_window_t *fw_ptr = wlmtk_fake_window_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fw_ptr);

    wlmtk_workspace_map_window(ws_ptr, fw_ptr->window_ptr);
    wlmtk_window_set_server_side_decorated(fw_ptr->window_ptr, true);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fw_ptr->window_ptr->titlebar_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fw_ptr->window_ptr->resizebar_ptr);

    wlmtk_window_set_properties(
        fw_ptr->window_ptr,
        WLMTK_WINDOW_PROPERTY_ICONIFIABLE);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fw_ptr->window_ptr->titlebar_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, fw_ptr->window_ptr->resizebar_ptr);

    wlmtk_window_set_properties(
        fw_ptr->window_ptr,
        WLMTK_WINDOW_PROPERTY_RESIZABLE);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fw_ptr->window_ptr->titlebar_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fw_ptr->window_ptr->resizebar_ptr);

    wlmtk_workspace_unmap_window(ws_ptr, fw_ptr->window_ptr);
    wlmtk_fake_window_destroy(fw_ptr);
    wlmtk_workspace_destroy(ws_ptr);
    wl_display_destroy(display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests maximizing and un-maximizing a window. */
void test_maximize(bs_test_t *test_ptr)
{
    struct wlr_box box;
    wlmtk_util_test_listener_t l;

    struct wl_display *display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(display_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_output_layout_ptr);
    struct wlr_output output = { .width = 1024, .height = 768, .scale = 1 };
    wlmtk_test_wlr_output_init(&output);
    wlr_output_layout_add(wlr_output_layout_ptr, &output, 0, 0);

    wlmtk_workspace_t *ws_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "test", &_wlmtk_workspace_test_tile_style, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_fake_window_t *fw_ptr = wlmtk_fake_window_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fw_ptr);

    wlmtk_util_connect_test_listener(
        &wlmtk_window_events(fw_ptr->window_ptr)->state_changed, &l);

    // Window must be mapped to get maximized: Need workspace dimensions.
    wlmtk_workspace_map_window(ws_ptr, fw_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);
    wlmtk_util_clear_test_listener(&l);

    // Set up initial organic size, and verify.
    wlmtk_window_request_position_and_size(fw_ptr->window_ptr, 20, 10, 200, 100);
    wlmtk_fake_window_commit_size(fw_ptr);
    box = wlmtk_window_get_position_and_size(fw_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 20, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, 10, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 200, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 100, box.height);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window_is_maximized(fw_ptr->window_ptr));

    // Re-position the window.
    wlmtk_window_set_position(fw_ptr->window_ptr, 50, 30);
    box = wlmtk_window_get_position_and_size(fw_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 50, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, 30, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 200, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 100, box.height);

    // Trigger another serial update. Should not change position nor size.
    wlmtk_window_serial(fw_ptr->window_ptr, 1234);
    box = wlmtk_window_get_position_and_size(fw_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 50, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, 30, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 200, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 100, box.height);

    // Maximize.
    wlmtk_window_request_maximized(fw_ptr->window_ptr, true);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window_is_maximized(fw_ptr->window_ptr));
    wlmtk_fake_window_commit_size(fw_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, l.calls);
    wlmtk_window_commit_maximized(fw_ptr->window_ptr, true);
    box = wlmtk_window_get_position_and_size(fw_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 960, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 704, box.height);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window_is_maximized(fw_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);
    wlmtk_util_clear_test_listener(&l);

    // A second commit: should not overwrite the organic dimension.
    wlmtk_fake_window_commit_size(fw_ptr);

    // Unmaximize. Restore earlier organic size and position.
    wlmtk_window_request_maximized(fw_ptr->window_ptr, false);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window_is_maximized(fw_ptr->window_ptr));
    wlmtk_fake_window_commit_size(fw_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, l.calls);
    wlmtk_window_commit_maximized(fw_ptr->window_ptr, false);
    box = wlmtk_window_get_position_and_size(fw_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 50, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, 30, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 200, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 100, box.height);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window_is_maximized(fw_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);

    // TODO(kaeser@gubbe.ch): Define what should happen when a maximized
    // window is moved. Should it lose maximization? Should it not move?
    // Or just move on?
    // Window Maker keeps maximization, but it's ... odd.

    wlmtk_workspace_unmap_window(ws_ptr, fw_ptr->window_ptr);
    wlmtk_util_disconnect_test_listener(&l);
    wlmtk_fake_window_destroy(fw_ptr);
    wlmtk_workspace_destroy(ws_ptr);
    wl_display_destroy(display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests turning a window to fullscreen and back. */
void test_fullscreen(bs_test_t *test_ptr)
{
    struct wlr_box box;
    wlmtk_util_test_listener_t l;

    struct wl_display *display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(display_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_output_layout_ptr);
    struct wlr_output output = { .width = 1024, .height = 768, .scale = 1 };
    wlmtk_test_wlr_output_init(&output);
    wlr_output_layout_add(wlr_output_layout_ptr, &output, 0, 0);

    wlmtk_workspace_t *ws_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "test", &_wlmtk_workspace_test_tile_style, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_workspace_enable(ws_ptr, true);
    wlmtk_fake_window_t *fw_ptr = wlmtk_fake_window_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fw_ptr);

    wlmtk_util_connect_test_listener(
        &wlmtk_window_events(fw_ptr->window_ptr)->state_changed, &l);

    wlmtk_window_set_server_side_decorated(fw_ptr->window_ptr, true);
    wlmtk_workspace_map_window(ws_ptr, fw_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);
    wlmtk_util_clear_test_listener(&l);

    BS_TEST_VERIFY_TRUE(test_ptr, fw_ptr->fake_content_ptr->activated);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        fw_ptr->window_ptr,
        wlmtk_workspace_get_activated_window(ws_ptr));

    // Set up initial organic size, and verify.
    wlmtk_window_request_position_and_size(fw_ptr->window_ptr, 20, 10, 200, 100);
    wlmtk_fake_window_commit_size(fw_ptr);
    box = wlmtk_window_get_position_and_size(fw_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 20, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, 10, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 200, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 100, box.height);
    BS_TEST_VERIFY_FALSE(test_ptr, fw_ptr->window_ptr->inorganic_sizing);

    // Request fullscreen. Does not take immediate effect.
    wlmtk_window_request_fullscreen(fw_ptr->window_ptr, true);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window_is_fullscreen(fw_ptr->window_ptr));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_titlebar_is_activated(fw_ptr->window_ptr->titlebar_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, 0, l.calls);

    // Only after "commit", it will take effect.
    wlmtk_fake_window_commit_size(fw_ptr);
    wlmtk_window_commit_fullscreen(fw_ptr->window_ptr, true);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window_is_fullscreen(fw_ptr->window_ptr));
    box = wlmtk_window_get_position_and_size(fw_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 1024, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 768, box.height);
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);
    wlmtk_util_clear_test_listener(&l);

    BS_TEST_VERIFY_TRUE(test_ptr, fw_ptr->fake_content_ptr->activated);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        fw_ptr->window_ptr,
        wlmtk_workspace_get_activated_window(ws_ptr));

    BS_TEST_VERIFY_TRUE(test_ptr, fw_ptr->window_ptr->server_side_decorated);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, fw_ptr->window_ptr->titlebar_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, fw_ptr->window_ptr->resizebar_ptr);

    // Request to end fullscreen. Not taking immediate effect.
    wlmtk_window_request_fullscreen(fw_ptr->window_ptr, false);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window_is_fullscreen(fw_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, 0, l.calls);

    // Takes effect after commit. We'll want the same position as before.
    wlmtk_fake_window_commit_size(fw_ptr);
    wlmtk_window_commit_fullscreen(fw_ptr->window_ptr, false);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window_is_fullscreen(fw_ptr->window_ptr));
    box = wlmtk_window_get_position_and_size(fw_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 20, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, 10, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 200, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 100, box.height);

    BS_TEST_VERIFY_TRUE(test_ptr, fw_ptr->fake_content_ptr->activated);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_titlebar_is_activated(fw_ptr->window_ptr->titlebar_ptr));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        fw_ptr->window_ptr,
        wlmtk_workspace_get_activated_window(ws_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);

    BS_TEST_VERIFY_TRUE(test_ptr, fw_ptr->window_ptr->server_side_decorated);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fw_ptr->window_ptr->titlebar_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fw_ptr->window_ptr->resizebar_ptr);

    wlmtk_workspace_unmap_window(ws_ptr, fw_ptr->window_ptr);
    wlmtk_util_disconnect_test_listener(&l);

    wlmtk_fake_window_destroy(fw_ptr);
    wlmtk_workspace_destroy(ws_ptr);
    wl_display_destroy(display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests that unmapping a fullscreen window works. */
void test_fullscreen_unmap(bs_test_t *test_ptr)
{
    struct wlr_box box;

    struct wl_display *display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(display_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_output_layout_ptr);
    struct wlr_output output = { .width = 1024, .height = 768, .scale = 1 };
    wlmtk_test_wlr_output_init(&output);
    wlr_output_layout_add(wlr_output_layout_ptr, &output, 0, 0);

    wlmtk_workspace_t *ws_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "test", &_wlmtk_workspace_test_tile_style, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_workspace_enable(ws_ptr, true);
    wlmtk_fake_window_t *fw_ptr = wlmtk_fake_window_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fw_ptr);
    wlmtk_workspace_map_window(ws_ptr, fw_ptr->window_ptr);

    BS_TEST_VERIFY_TRUE(test_ptr, fw_ptr->fake_content_ptr->activated);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        fw_ptr->window_ptr,
        wlmtk_workspace_get_activated_window(ws_ptr));

    // Request fullscreen. Does not take immediate effect.
    wlmtk_window_request_fullscreen(fw_ptr->window_ptr, true);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window_is_fullscreen(fw_ptr->window_ptr));

    // Only after "commit", it will take effect.
    wlmtk_fake_window_commit_size(fw_ptr);
    wlmtk_window_commit_fullscreen(fw_ptr->window_ptr, true);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window_is_fullscreen(fw_ptr->window_ptr));
    box = wlmtk_window_get_position_and_size(fw_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 1024, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 768, box.height);
    BS_TEST_VERIFY_TRUE(test_ptr, fw_ptr->fake_content_ptr->activated);

    wlmtk_workspace_unmap_window(ws_ptr, fw_ptr->window_ptr);
    BS_TEST_VERIFY_FALSE(test_ptr, fw_ptr->fake_content_ptr->activated);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        wlmtk_workspace_get_activated_window(ws_ptr));

    wlmtk_fake_window_destroy(fw_ptr);

    wlmtk_workspace_destroy(ws_ptr);
    wl_display_destroy(display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Verifies that window shading hides the element and raises signal. */
void test_shade(bs_test_t *test_ptr)
{
    wlmtk_util_test_listener_t l;

    wlmtk_fake_window_t *fw_ptr = wlmtk_fake_window_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fw_ptr);
    wlmtk_util_connect_test_listener(
        &wlmtk_window_events(fw_ptr->window_ptr)->state_changed, &l);

    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window_is_shaded(fw_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, 0, l.calls);

    // Shading only works on server-side-decorated windows.
    wlmtk_window_set_server_side_decorated(fw_ptr->window_ptr, true);

    wlmtk_window_request_shaded(fw_ptr->window_ptr, true);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window_is_shaded(fw_ptr->window_ptr));
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_content_element(&fw_ptr->fake_content_ptr->content)->visible);
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);
    wlmtk_util_clear_test_listener(&l);

    wlmtk_window_request_shaded(fw_ptr->window_ptr, false);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window_is_shaded(fw_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);
    wlmtk_util_clear_test_listener(&l);

    // Shading not supported on client-side decoration. Must be disabled.
    wlmtk_window_request_shaded(fw_ptr->window_ptr, true);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window_is_shaded(fw_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);
    wlmtk_util_clear_test_listener(&l);

    wlmtk_window_set_server_side_decorated(fw_ptr->window_ptr, false);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window_is_shaded(fw_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);
    wlmtk_util_clear_test_listener(&l);

    // Verify that 'shading' on client decorations does not do anything.
    wlmtk_window_set_server_side_decorated(fw_ptr->window_ptr, false);
    wlmtk_window_request_shaded(fw_ptr->window_ptr, true);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window_is_shaded(fw_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, 0, l.calls);

    wlmtk_util_disconnect_test_listener(&l);
    wlmtk_fake_window_destroy(fw_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests fake window ctor and dtor. */
void test_fake(bs_test_t *test_ptr)
{
    wlmtk_fake_window_t *fake_window_ptr = wlmtk_fake_window_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fake_window_ptr);
    wlmtk_fake_window_destroy(fake_window_ptr);
}

/* == End of window.c ====================================================== */
