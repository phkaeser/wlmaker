/* ========================================================================= */
/**
 * @file desktop.c
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
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

#include "desktop.h"

#include <libbase/libbase.h>
#include <stdlib.h>
#include <wayland-util.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/edges.h>
#undef WLR_USE_UNSTABLE

#include "container.h"
#include "input.h"
#include "panel.h"
#include "rectangle.h"
#include "test.h"  // IWYU pragma: keep
#include "tile.h"
#include "util.h"
#include "window.h"
#include "workspace.h"

struct wlr_keyboard_key_event;

/* == Declarations ========================================================= */

/** State of the desktop element. */
struct _wlmtk_desktop_t {
    /** The desktop's container: Holds workspaces, layers and the curtain. */
    wlmtk_container_t         container;
    /** Overwritten virtual method table before extending ig. */
    wlmtk_element_vmt_t       orig_super_element_vmt;
    /** Extents to be used by desktop. */
    struct wlr_box            extents;

    /** Events availabe of the desktop. */
    wlmtk_desktop_events_t    events;
    /** Desktop's style. */
    struct wlmtk_desktop_style style;

    /** Whether the desktop is currently locked. */
    bool                      locked;
    /**
     * The lock's element. Shown on top of
     * @ref wlmtk_desktop_t::curtain_rectangle_ptr.
     */
    wlmtk_element_t           *lock_element_ptr;

    /** Curtain element: Permit dimming or hiding everything. */
    wlmtk_rectangle_t         *curtain_rectangle_ptr;
    /** Background element: Behind @ref WLMTK_DESKTOP_LAYER_BACKGROUND. */
    wlmtk_rectangle_t         *background_rectangle_ptr;

    /** List of workspaces attached to desktop. @see wlmtk_workspace_t::dlnode. */
    bs_dllist_t               workspaces;
    /** Currently-active workspace. */
    wlmtk_workspace_t         *current_workspace_ptr;
    /** The window containers of all workspaces. */
    wlmtk_container_t         workspace_window_container;
    /** Holds the fullscreen elements of each workspace. */
    wlmtk_container_t         workspace_fullscreen_container;

    /** Listener for wlr_output_layout::events.change. */
    struct wl_listener        output_layout_change_listener;

    /** The wlr layers available in this desktop. */
    wlmtk_layer_t            *layer_ptrs[WLMTK_DESKTOP_LAYER_OVERLAY + 1];

    // Elements below not owned by wlmtk_desktop_t.
    /** wlroots output layout. */
    struct wlr_output_layout  *wlr_output_layout_ptr;
};

static void _wlmtk_desktop_element_layout(wlmtk_element_t *element_ptr);

static void _wlmtk_desktop_switch_to_workspace(
    wlmtk_desktop_t *desktop_ptr,
    wlmtk_workspace_t *workspace_ptr);
static void _wlmtk_desktop_enumerate_workspaces(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static void _wlmtk_desktop_destroy_workspace(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);

static bool _wlmtk_desktop_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);
static bool _wlmtk_desktop_element_pointer_axis(
    wlmtk_element_t *element_ptr,
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr);
static bool _wlmtk_desktop_element_keyboard_event(
    wlmtk_element_t *element_ptr,
    struct wlr_keyboard_key_event *wlr_keyboard_key_event_ptr);

static void _wlmtk_desktop_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static bool _wlmtk_desktop_window_set_style(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static bool _wlmtk_desktop_workspace_set_style(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);

/** Virtual method table for the container's super class: Element. */
static const wlmtk_element_vmt_t _wlmtk_desktop_element_vmt = {
    .layout = _wlmtk_desktop_element_layout,
    .pointer_button = _wlmtk_desktop_element_pointer_button,
    .pointer_axis = _wlmtk_desktop_element_pointer_axis,
    .keyboard_event = _wlmtk_desktop_element_keyboard_event,
};

/** Arg for iterators that are setting the style. */
struct _wlmtk_desktop_set_style_arg {
    /** Reference to the window's style. */
    wlmtk_window_style_ref_t  *window_style_ref_ptr;
    /** Reference to the menu's style. */
    wlmtk_menu_style_ref_t    *menu_style_ref_ptr;
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_desktop_t *wlmtk_desktop_create(
    struct wlr_scene *wlr_scene_ptr,
    const struct wlmtk_desktop_style *style_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr)
{
    wlmtk_desktop_t *desktop_ptr = logged_calloc(1, sizeof(wlmtk_desktop_t));
    if (NULL == desktop_ptr) return NULL;
    desktop_ptr->style = *style_ptr;
    desktop_ptr->wlr_output_layout_ptr = wlr_output_layout_ptr;

    if (NULL != wlr_scene_ptr) {
        if (!wlmtk_container_init_attached(
                &desktop_ptr->container,
                &wlr_scene_ptr->tree)) {
            wlmtk_desktop_destroy(desktop_ptr);
            return NULL;
        }
    } else {
        if (!wlmtk_container_init(&desktop_ptr->container)) {
            wlmtk_desktop_destroy(desktop_ptr);
            return NULL;
        }
    }
    wlmtk_element_set_visible(&desktop_ptr->container.super_element, true);
    desktop_ptr->orig_super_element_vmt = wlmtk_element_extend(
        &desktop_ptr->container.super_element,
        &_wlmtk_desktop_element_vmt);

    for (int i = 0; i <= WLMTK_DESKTOP_LAYER_OVERLAY; ++i) {
        wlmtk_layer_t *layer_ptr = wlmtk_layer_create(wlr_output_layout_ptr);
        if (NULL == layer_ptr) {
            wlmtk_desktop_destroy(desktop_ptr);
            return NULL;
        }
        wlmtk_element_set_visible(wlmtk_layer_element(layer_ptr), true);
        wlmtk_container_add_element(
            &desktop_ptr->container, wlmtk_layer_element(layer_ptr));
        desktop_ptr->layer_ptrs[i] = layer_ptr;
    }

    if (!wlmtk_container_init(&desktop_ptr->workspace_window_container)) {
        wlmtk_desktop_destroy(desktop_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        &desktop_ptr->workspace_window_container.super_element,
        true);
    wlmtk_layer_t *bg_layer_ptr = wlmtk_desktop_get_layer(
        desktop_ptr, WLMTK_DESKTOP_LAYER_BACKGROUND);
    wlmtk_container_add_element_atop(
        &desktop_ptr->container,
        wlmtk_layer_element(bg_layer_ptr),
        &desktop_ptr->workspace_window_container.super_element);

    if (!wlmtk_container_init(&desktop_ptr->workspace_fullscreen_container)) {
        wlmtk_desktop_destroy(desktop_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        &desktop_ptr->workspace_fullscreen_container.super_element,
        true);
    wlmtk_layer_t *overlay_layer_ptr = wlmtk_desktop_get_layer(
        desktop_ptr, WLMTK_DESKTOP_LAYER_OVERLAY);
    wlmtk_container_add_element_atop(
        &desktop_ptr->container,
        wlmtk_layer_element(overlay_layer_ptr),
        &desktop_ptr->workspace_fullscreen_container.super_element);

    desktop_ptr->curtain_rectangle_ptr = wlmtk_rectangle_create(
        desktop_ptr->extents.width, desktop_ptr->extents.height, 0xff000020);
    if (NULL == desktop_ptr->curtain_rectangle_ptr) {
        wlmtk_desktop_destroy(desktop_ptr);
        return NULL;
    }
    wlmtk_container_add_element(
        &desktop_ptr->container,
        wlmtk_rectangle_element(desktop_ptr->curtain_rectangle_ptr));

    desktop_ptr->background_rectangle_ptr = wlmtk_rectangle_create(
        desktop_ptr->extents.width,
        desktop_ptr->extents.height,
        desktop_ptr->style.background_color);
    if (NULL == desktop_ptr->background_rectangle_ptr) {
        wlmtk_desktop_destroy(desktop_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_rectangle_element(desktop_ptr->background_rectangle_ptr),
        true);
    wlmtk_container_add_element_atop(
        &desktop_ptr->container,
        NULL,
        wlmtk_rectangle_element(desktop_ptr->background_rectangle_ptr));

    wlmtk_util_connect_listener_signal(
        &wlr_output_layout_ptr->events.change,
        &desktop_ptr->output_layout_change_listener,
        _wlmtk_desktop_handle_output_layout_change);
    _wlmtk_desktop_handle_output_layout_change(
        &desktop_ptr->output_layout_change_listener,
        wlr_output_layout_ptr);

    wl_signal_init(&desktop_ptr->events.workspace_changed);
    wl_signal_init(&desktop_ptr->events.unlock_event);
    wl_signal_init(&desktop_ptr->events.window_mapped);
    wl_signal_init(&desktop_ptr->events.window_unmapped);
    return desktop_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_desktop_destroy(wlmtk_desktop_t *desktop_ptr)
{
    wlmtk_util_disconnect_listener(
        &desktop_ptr->output_layout_change_listener);

    bs_dllist_for_each(
        &desktop_ptr->workspaces,
        _wlmtk_desktop_destroy_workspace,
        desktop_ptr);
    if (desktop_ptr->workspace_window_container.super_element.parent_container_ptr) {
        wlmtk_container_remove_element(
            &desktop_ptr->container,
            &desktop_ptr->workspace_window_container.super_element);
    }
    wlmtk_container_fini(&desktop_ptr->workspace_window_container);
    if (desktop_ptr->workspace_fullscreen_container.super_element.parent_container_ptr) {
        wlmtk_container_remove_element(
            &desktop_ptr->container,
            &desktop_ptr->workspace_fullscreen_container.super_element);
    }
    wlmtk_container_fini(&desktop_ptr->workspace_fullscreen_container);

    for (int i = 0; i <= WLMTK_DESKTOP_LAYER_OVERLAY; ++i) {
        if (NULL == desktop_ptr->layer_ptrs[i]) continue;
        wlmtk_container_remove_element(
            &desktop_ptr->container,
            wlmtk_layer_element(desktop_ptr->layer_ptrs[i]));
        wlmtk_layer_destroy(desktop_ptr->layer_ptrs[i]);
        desktop_ptr->layer_ptrs[i] = NULL;
    }

    if (NULL != desktop_ptr->background_rectangle_ptr) {
        wlmtk_container_remove_element(
            &desktop_ptr->container,
            wlmtk_rectangle_element(desktop_ptr->background_rectangle_ptr));

        wlmtk_rectangle_destroy(desktop_ptr->background_rectangle_ptr);
        desktop_ptr->background_rectangle_ptr = NULL;
    }

    if (NULL != desktop_ptr->curtain_rectangle_ptr) {
        wlmtk_container_remove_element(
            &desktop_ptr->container,
            wlmtk_rectangle_element(desktop_ptr->curtain_rectangle_ptr));

        wlmtk_rectangle_destroy(desktop_ptr->curtain_rectangle_ptr);
        desktop_ptr->curtain_rectangle_ptr = NULL;
    }

    wlmtk_container_fini(&desktop_ptr->container);

    free(desktop_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_desktop_events_t *wlmtk_desktop_events(wlmtk_desktop_t *desktop_ptr)
{
    return &desktop_ptr->events;
}

/* ------------------------------------------------------------------------- */
void wlmtk_desktop_add_workspace(
    wlmtk_desktop_t *desktop_ptr,
    wlmtk_workspace_t *workspace_ptr)
{
    BS_ASSERT(NULL == wlmtk_workspace_get_desktop(workspace_ptr));

    bs_dllist_push_back(
        &desktop_ptr->workspaces,
        wlmtk_dlnode_from_workspace(workspace_ptr));
    wlmtk_container_add_element(
        &desktop_ptr->workspace_window_container,
        wlmtk_workspace_window_element(workspace_ptr));
    wlmtk_container_add_element(
        &desktop_ptr->workspace_fullscreen_container,
        wlmtk_workspace_fullscreen_element(workspace_ptr));

    wlmtk_element_set_visible(
        wlmtk_workspace_window_element(workspace_ptr), false);
    wlmtk_element_set_visible(
        wlmtk_workspace_fullscreen_element(workspace_ptr), false);

    wlmtk_workspace_set_details(
        workspace_ptr, bs_dllist_size(&desktop_ptr->workspaces));
    wlmtk_workspace_set_desktop(workspace_ptr, desktop_ptr);

    if (NULL == desktop_ptr->current_workspace_ptr) {
        _wlmtk_desktop_switch_to_workspace(desktop_ptr, workspace_ptr);
    } else {
        wl_signal_emit(&desktop_ptr->events.workspace_changed, workspace_ptr);
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_desktop_remove_workspace(
    wlmtk_desktop_t *desktop_ptr,
    wlmtk_workspace_t *workspace_ptr)
{
    BS_ASSERT(desktop_ptr == wlmtk_workspace_get_desktop(workspace_ptr));
    wlmtk_workspace_set_desktop(workspace_ptr, NULL);
    bs_dllist_remove(
        &desktop_ptr->workspaces,
        wlmtk_dlnode_from_workspace(workspace_ptr));
    wlmtk_container_remove_element(
        &desktop_ptr->workspace_fullscreen_container,
        wlmtk_workspace_fullscreen_element(workspace_ptr));
    wlmtk_container_remove_element(
        &desktop_ptr->workspace_window_container,
        wlmtk_workspace_window_element(workspace_ptr));
    wlmtk_element_set_visible(
        wlmtk_workspace_window_element(workspace_ptr), false);
    wlmtk_element_set_visible(
        wlmtk_workspace_fullscreen_element(workspace_ptr), false);

    int index = 1;
    bs_dllist_for_each(
        &desktop_ptr->workspaces,
        _wlmtk_desktop_enumerate_workspaces,
        &index);

    if (desktop_ptr->current_workspace_ptr == workspace_ptr) {
        _wlmtk_desktop_switch_to_workspace(
            desktop_ptr,
            wlmtk_workspace_from_dlnode(desktop_ptr->workspaces.head_ptr));
    } else {
        wl_signal_emit(&desktop_ptr->events.workspace_changed, NULL);
    }
}

/* ------------------------------------------------------------------------- */
wlmtk_workspace_t *wlmtk_desktop_get_current_workspace(wlmtk_desktop_t *desktop_ptr)
{
    return desktop_ptr->current_workspace_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_desktop_destroy_last_workspace(wlmtk_desktop_t *desktop_ptr)
{
    wlmtk_workspace_t *ws_ptr = wlmtk_workspace_from_dlnode(
        desktop_ptr->workspaces.tail_ptr);

    // Guard clause: Must have further workspaces, not be current workspace,
    // and not have windows on that workspace.
    if (1 >= bs_dllist_size(&desktop_ptr->workspaces) ||
        ws_ptr == desktop_ptr->current_workspace_ptr ||
        !bs_dllist_empty(wlmtk_workspace_get_windows_dllist(ws_ptr))) return;

    wlmtk_desktop_remove_workspace(desktop_ptr, ws_ptr);
    wlmtk_workspace_destroy(ws_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_desktop_switch_to_next_workspace(wlmtk_desktop_t *desktop_ptr)
{
    if (NULL == desktop_ptr->current_workspace_ptr) return;

    bs_dllist_node_t *dlnode_ptr = wlmtk_dlnode_from_workspace(
        desktop_ptr->current_workspace_ptr);
    if (NULL == dlnode_ptr->next_ptr) {
        dlnode_ptr = desktop_ptr->workspaces.head_ptr;
    } else {
        dlnode_ptr = dlnode_ptr->next_ptr;
    }
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_from_dlnode(dlnode_ptr);

    _wlmtk_desktop_switch_to_workspace(desktop_ptr, workspace_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_desktop_switch_to_previous_workspace(wlmtk_desktop_t *desktop_ptr)
{
    if (NULL == desktop_ptr->current_workspace_ptr) return;
    bs_dllist_node_t *dlnode_ptr = wlmtk_dlnode_from_workspace(
        desktop_ptr->current_workspace_ptr);
    if (NULL == dlnode_ptr->prev_ptr) {
        dlnode_ptr = desktop_ptr->workspaces.tail_ptr;
    } else {
        dlnode_ptr = dlnode_ptr->prev_ptr;
    }
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_from_dlnode(dlnode_ptr);

    _wlmtk_desktop_switch_to_workspace(desktop_ptr, workspace_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_desktop_for_each_workspace(
    wlmtk_desktop_t *desktop_ptr,
    void (*func)(bs_dllist_node_t *dlnode_ptr, void *ud_ptr),
    void *ud_ptr)
{
    bs_dllist_for_each(&desktop_ptr->workspaces, func, ud_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_desktop_lock(
    wlmtk_desktop_t *desktop_ptr,
    wlmtk_element_t *element_ptr)
{
    if (desktop_ptr->locked) {
        bs_log(BS_WARNING, "Desktop already locked by element %p",
               desktop_ptr->lock_element_ptr);
        return false;
    }
    desktop_ptr->lock_element_ptr = element_ptr;

    wlmtk_workspace_enable(desktop_ptr->current_workspace_ptr, false);

    wlmtk_rectangle_set_size(
        desktop_ptr->curtain_rectangle_ptr,
        desktop_ptr->extents.width, desktop_ptr->extents.height);
    wlmtk_element_set_visible(
        wlmtk_rectangle_element(desktop_ptr->curtain_rectangle_ptr),
        true);

    wlmtk_container_add_element(
        &desktop_ptr->container,
        desktop_ptr->lock_element_ptr);

    desktop_ptr->locked = true;
    return true;
}

/* ------------------------------------------------------------------------- */
bool wlmtk_desktop_unlock(
    wlmtk_desktop_t *desktop_ptr,
    wlmtk_element_t *element_ptr)
{
    // Guard clause: Not locked => nothing to do.
    if (!desktop_ptr->locked) return false;
    if (element_ptr != desktop_ptr->lock_element_ptr) {
        bs_log(BS_ERROR, "Lock held by element %p, attempted to unlock by %p",
               desktop_ptr->lock_element_ptr, element_ptr);
        return false;
    }

    wlmtk_desktop_lock_unreference(desktop_ptr, element_ptr);
    desktop_ptr->locked = false;

    wlmtk_element_set_visible(
        wlmtk_rectangle_element(desktop_ptr->curtain_rectangle_ptr),
        false);

    wl_signal_emit(&desktop_ptr->events.unlock_event, NULL);

    wlmtk_workspace_enable(desktop_ptr->current_workspace_ptr, true);
    return true;
}

/* ------------------------------------------------------------------------- */
bool wlmtk_desktop_locked(wlmtk_desktop_t *desktop_ptr)
{
    return desktop_ptr->locked;
}

/* ------------------------------------------------------------------------- */
void wlmtk_desktop_lock_unreference(
    wlmtk_desktop_t *desktop_ptr,
    wlmtk_element_t *element_ptr)
{
    if (element_ptr != desktop_ptr->lock_element_ptr) return;

    wlmtk_container_remove_element(
        &desktop_ptr->container,
        desktop_ptr->lock_element_ptr);
    desktop_ptr->lock_element_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_desktop_element(wlmtk_desktop_t *desktop_ptr)
{
    return &desktop_ptr->container.super_element;
}

/* ------------------------------------------------------------------------- */
bool wlmtk_desktop_set_style(
    wlmtk_desktop_t *desktop_ptr,
    const struct wlmtk_desktop_style *style_ptr,
    wlmtk_window_style_ref_t *window_style_ref_ptr,
    wlmtk_menu_style_ref_t *menu_style_ref_ptr)
{
    desktop_ptr->style = *style_ptr;
    wlmtk_rectangle_set_color(
        desktop_ptr->background_rectangle_ptr,
        desktop_ptr->style.background_color);

    struct _wlmtk_desktop_set_style_arg arg = {
        .window_style_ref_ptr = window_style_ref_ptr,
        .menu_style_ref_ptr = menu_style_ref_ptr
    };

    return bs_dllist_all(
        &desktop_ptr->workspaces,
        _wlmtk_desktop_workspace_set_style,
        &arg);
}

/* ------------------------------------------------------------------------- */
wlmtk_layer_t *wlmtk_desktop_get_layer(
    wlmtk_desktop_t *desktop_ptr,
    wlmtk_desktop_layer_t layer)
{
    BS_ASSERT(0 <= layer && layer <= WLMTK_DESKTOP_LAYER_OVERLAY);
    return desktop_ptr->layer_ptrs[layer];
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Lays out the desktop elements: (re)configures layers, and update extents.
 *
 * @param element_ptr
 */
void _wlmtk_desktop_element_layout(wlmtk_element_t *element_ptr)
{
    wlmtk_desktop_t *desktop_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_desktop_t, container.super_element);

    // Process each output.
    struct wlr_output_layout_output *wlr_output_layout_output_ptr;
    wl_list_for_each(
        wlr_output_layout_output_ptr,
        &desktop_ptr->wlr_output_layout_ptr->outputs,
        link) {

        // Process each layer: An updated panel may affect positioning within
        // other layers, and the workspace maximization area.
        struct wlr_box usable_area;
        wlr_output_layout_get_box(
            desktop_ptr->wlr_output_layout_ptr,
            wlr_output_layout_output_ptr->output, &usable_area);
        for (int i = 0; i < WLMTK_DESKTOP_LAYER_OVERLAY + 1; ++i) {
            wlmtk_layer_reconfigure_output(
                desktop_ptr->layer_ptrs[i],
                wlr_output_layout_output_ptr->output,
                &usable_area);
        }

        // TODO(kaeser@gubbe.ch): Set workspace maximize extents for output.
    }
    desktop_ptr->orig_super_element_vmt.layout(element_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Switches to `workspace_ptr` as the current workspace.
 *
 * @param desktop_ptr
 * @param workspace_ptr
 */
void _wlmtk_desktop_switch_to_workspace(
    wlmtk_desktop_t *desktop_ptr,
    wlmtk_workspace_t *workspace_ptr)
{
    if (desktop_ptr->current_workspace_ptr == workspace_ptr) return;

    if (NULL == workspace_ptr) {
        desktop_ptr->current_workspace_ptr = NULL;
    } else {
        BS_ASSERT(desktop_ptr = wlmtk_workspace_get_desktop(workspace_ptr));

        wlmtk_element_set_visible(
            wlmtk_workspace_window_element(workspace_ptr), true);
        wlmtk_element_set_visible(
            wlmtk_workspace_fullscreen_element(workspace_ptr), true);
        if (NULL != desktop_ptr->current_workspace_ptr) {
            wlmtk_element_set_visible(
                wlmtk_workspace_window_element(desktop_ptr->current_workspace_ptr),
                false);
            wlmtk_element_set_visible(
                wlmtk_workspace_fullscreen_element(
                    desktop_ptr->current_workspace_ptr), false);
            wlmtk_workspace_enable(desktop_ptr->current_workspace_ptr, false);
        }
        desktop_ptr->current_workspace_ptr = workspace_ptr;
        wlmtk_workspace_enable(desktop_ptr->current_workspace_ptr, true);
    }

    wl_signal_emit(
        &desktop_ptr->events.workspace_changed,
        desktop_ptr->current_workspace_ptr);
}

/* ------------------------------------------------------------------------- */
/** Callback for bs_dllist_for_each: Destroys the workspace. */
void _wlmtk_desktop_destroy_workspace(bs_dllist_node_t *dlnode_ptr, void *ud_ptr)
{
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_from_dlnode(dlnode_ptr);
    wlmtk_desktop_remove_workspace(ud_ptr, workspace_ptr);
    wlmtk_workspace_destroy(workspace_ptr);
}

/* ------------------------------------------------------------------------- */
/** Callback for bs_dllist_for_each: Enumerates the workspace. */
void _wlmtk_desktop_enumerate_workspaces(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    int *index_ptr = ud_ptr;
    wlmtk_workspace_set_details(
        wlmtk_workspace_from_dlnode(dlnode_ptr),
        *index_ptr);
    *index_ptr += 1;
}

/* ------------------------------------------------------------------------- */
/**
 * Implements @ref wlmtk_element_vmt_t::pointer_button. Handle button events.
 *
 * When locked, the desktop container will forward the events strictly only to
 * the lock container.
 *
 * @param element_ptr
 * @param button_event_ptr
 *
 * @return true if the button was handled.
 */
bool _wlmtk_desktop_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmtk_desktop_t *desktop_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_desktop_t, container.super_element);

    if (!desktop_ptr->locked) {
        // TODO(kaeser@gubbe.ch): We'll want to pass this on to the non-curtain
        // elements only.
        return desktop_ptr->orig_super_element_vmt.pointer_button(
            element_ptr, button_event_ptr);
    } else if (NULL != desktop_ptr->lock_element_ptr) {
        return wlmtk_element_pointer_button(
            desktop_ptr->lock_element_ptr,
            button_event_ptr);
    }

    // Fall-through.
    return false;
}

/* ------------------------------------------------------------------------- */
/**
 * Implements @ref wlmtk_element_vmt_t::pointer_axis. Handle axis events.
 *
 * When locked, the desktop container will forward the events strictly only to
 * the lock container.
 *
 * @param element_ptr
 * @param wlr_pointer_axis_event_ptr
 *
 * @return true if the axis event was handled.
 */
bool _wlmtk_desktop_element_pointer_axis(
    wlmtk_element_t *element_ptr,
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr)
{
    wlmtk_desktop_t *desktop_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_desktop_t, container.super_element);

    if (!desktop_ptr->locked) {
        // TODO(kaeser@gubbe.ch): We'll want to pass this on to the non-curtain
        // elements only.
        return desktop_ptr->orig_super_element_vmt.pointer_axis(
            element_ptr, wlr_pointer_axis_event_ptr);
    } else if (NULL != desktop_ptr->lock_element_ptr) {
        return wlmtk_element_pointer_axis(
            desktop_ptr->lock_element_ptr,
            wlr_pointer_axis_event_ptr);
    }

    // Fall-through.
    return false;
}

/* ------------------------------------------------------------------------- */
/**
 * Implements @ref wlmtk_element_vmt_t::keyboard_event. Handle keyboard events.
 *
 * When locked, the desktop container will forward the events strictly only to
 * the lock container.
 *
 * @param element_ptr
 * @param wlr_keyboard_key_event_ptr
 *
 * @return true if the axis event was handled.
 */
bool _wlmtk_desktop_element_keyboard_event(
    wlmtk_element_t *element_ptr,
    struct wlr_keyboard_key_event *wlr_keyboard_key_event_ptr)
{
    wlmtk_desktop_t *desktop_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_desktop_t, container.super_element);

    if (!desktop_ptr->locked) {
        // TODO(kaeser@gubbe.ch): We'll want to pass this on to the non-curtain
        // elements only.
        return desktop_ptr->orig_super_element_vmt.keyboard_event(
            element_ptr,
            wlr_keyboard_key_event_ptr);
    } else if (NULL != desktop_ptr->lock_element_ptr) {
        return wlmtk_element_keyboard_event(
            desktop_ptr->lock_element_ptr,
            wlr_keyboard_key_event_ptr);
    }

    // Fall-through: Too bad -- the screen is locked, but the lock element
    // disappeared (crashed?). No more handling of keys here...
    return false;
}

/* ------------------------------------------------------------------------- */
/**
 * Handles wlr_output_layout::events::change. Triggers when the output layout
 * changes, and we use this for updating the curtain and the layout in each
 * workspace.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmtk_desktop_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmtk_desktop_t *desktop_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_desktop_t, output_layout_change_listener);
    struct wlr_output_layout *wlr_output_layout_ptr = data_ptr;

    wlr_output_layout_get_box(wlr_output_layout_ptr, NULL, &desktop_ptr->extents);

    wlmtk_rectangle_t *rs[] = {
        desktop_ptr->curtain_rectangle_ptr,
        desktop_ptr->background_rectangle_ptr };
    for (size_t i = 0; i < sizeof(rs) / sizeof(wlmtk_rectangle_t*); ++i) {
        wlmtk_rectangle_set_size(
            rs[i], desktop_ptr->extents.width, desktop_ptr->extents.height);
        wlmtk_element_set_position(
            wlmtk_rectangle_element(rs[i]),
            desktop_ptr->extents.x,
            desktop_ptr->extents.y);
    }
}

/* ------------------------------------------------------------------------- */
/** Calls @ref wlmtk_window_set_style for the window at `dlnode_ptr`. */
bool _wlmtk_desktop_window_set_style(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    wlmtk_window_t *window_ptr = wlmtk_window_from_dlnode(dlnode_ptr);
    struct _wlmtk_desktop_set_style_arg *arg_ptr = ud_ptr;

    return wlmtk_window_set_style(
        window_ptr,
        arg_ptr->window_style_ref_ptr,
        arg_ptr->menu_style_ref_ptr);
}

/* ------------------------------------------------------------------------- */
/** Sets the style for each window of the workspace. */
bool _wlmtk_desktop_workspace_set_style(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_from_dlnode(dlnode_ptr);
    struct _wlmtk_desktop_set_style_arg *arg_ptr = ud_ptr;

    return bs_dllist_all(
        wlmtk_workspace_get_windows_dllist(workspace_ptr),
        _wlmtk_desktop_window_set_style,
        arg_ptr);
}

/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);
static void test_workspaces(bs_test_t *test_ptr);
static void test_layout(bs_test_t *test_ptr);

/** Test cases */
static const bs_test_case_t _wlmtk_desktop_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "workspaces", test_workspaces },
    { 1, "layout", test_layout },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmtk_desktop_test_set = BS_TEST_SET(
    true, "desktop", _wlmtk_desktop_test_cases);

/* ------------------------------------------------------------------------- */
/** Exercises ctor and dtor. */
void test_create_destroy(bs_test_t *test_ptr)
{
    struct wlr_scene *wlr_scene_ptr = wlr_scene_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_scene_ptr);
    struct wl_display *wl_display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wl_display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr = wlr_output_layout_create(
        wl_display_ptr);

    struct wlmtk_desktop_style ds = {};
    wlmtk_desktop_t *desktop_ptr = wlmtk_desktop_create(
        wlr_scene_ptr, &ds, wlr_output_layout_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, desktop_ptr);

    BS_TEST_VERIFY_EQ(
        test_ptr, &desktop_ptr->events, wlmtk_desktop_events(desktop_ptr));

    wlmtk_desktop_destroy(desktop_ptr);
    wl_display_destroy(wl_display_ptr);
    wlr_scene_node_destroy(&wlr_scene_ptr->tree.node);
}

/* ------------------------------------------------------------------------- */
/** Exercises workspace adding and removal. */
void test_workspaces(bs_test_t *test_ptr)
{
    wlmtk_util_test_listener_t l;
    struct wlr_scene *wlr_scene_ptr = wlr_scene_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_scene_ptr);
    struct wl_display *wl_display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wl_display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(wl_display_ptr);
    struct wlmtk_desktop_style ds = {};
    wlmtk_desktop_t *desktop_ptr = wlmtk_desktop_create(
        wlr_scene_ptr, &ds, wlr_output_layout_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, desktop_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr, NULL, wlmtk_desktop_get_current_workspace(desktop_ptr));

    wlmtk_util_connect_test_listener(
        &wlmtk_desktop_events(desktop_ptr)->workspace_changed, &l);
    // Empty? A no-op.
    wlmtk_desktop_destroy_last_workspace(desktop_ptr);

    static const struct wlmtk_tile_style tstyle = {};
    wlmtk_workspace_t *ws1_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "1", &tstyle);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws1_ptr);
    wlmtk_desktop_add_workspace(desktop_ptr, ws1_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr, ws1_ptr, wlmtk_desktop_get_current_workspace(desktop_ptr));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_workspace_window_element(ws1_ptr)->visible);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_workspace_fullscreen_element(ws1_ptr)->visible);
    BS_TEST_VERIFY_EQ(test_ptr, ws1_ptr, l.last_data_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);
    wlmtk_util_clear_test_listener(&l);
    // Will not destroy the last workspace.
    wlmtk_desktop_destroy_last_workspace(desktop_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 1, bs_dllist_size(&desktop_ptr->workspaces));
    BS_TEST_VERIFY_EQ(test_ptr, 0, l.calls);

    wlmtk_workspace_t *ws2_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "2", &tstyle);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws2_ptr);
    wlmtk_desktop_add_workspace(desktop_ptr, ws2_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr, ws1_ptr, wlmtk_desktop_get_current_workspace(desktop_ptr));
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_workspace_window_element(ws2_ptr)->visible);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_workspace_fullscreen_element(ws2_ptr)->visible);
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);
    BS_TEST_VERIFY_EQ(test_ptr, ws2_ptr, l.last_data_ptr);
    wlmtk_util_clear_test_listener(&l);

    wlmtk_desktop_remove_workspace(desktop_ptr, ws1_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);
    wlmtk_util_clear_test_listener(&l);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_workspace_window_element(ws1_ptr)->visible);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_workspace_fullscreen_element(ws1_ptr)->visible);
    BS_TEST_VERIFY_EQ(
        test_ptr, ws2_ptr, wlmtk_desktop_get_current_workspace(desktop_ptr));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_workspace_window_element(ws2_ptr)->visible);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_workspace_fullscreen_element(ws2_ptr)->visible);
    // Again: not destroying the workspace.
    wlmtk_desktop_destroy_last_workspace(desktop_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 1, bs_dllist_size(&desktop_ptr->workspaces));
    BS_TEST_VERIFY_EQ(test_ptr, 0, l.calls);

    // Now add ws1 again. Deleting last workspace
    wlmtk_desktop_add_workspace(desktop_ptr, ws1_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);
    wlmtk_util_clear_test_listener(&l);
    BS_TEST_VERIFY_EQ(test_ptr, 2, bs_dllist_size(&desktop_ptr->workspaces));
    wlmtk_desktop_destroy_last_workspace(desktop_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 1, bs_dllist_size(&desktop_ptr->workspaces));
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);
    wlmtk_util_clear_test_listener(&l);

    wlmtk_desktop_remove_workspace(desktop_ptr, ws2_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);
    wlmtk_util_clear_test_listener(&l);
    wlmtk_workspace_destroy(ws2_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr, NULL, wlmtk_desktop_get_current_workspace(desktop_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, NULL, l.last_data_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, l.calls);

    wlmtk_util_disconnect_test_listener(&l);
    wlmtk_desktop_destroy(desktop_ptr);
    wlr_output_layout_destroy(wlr_output_layout_ptr);
    wl_display_destroy(wl_display_ptr);
    wlr_scene_node_destroy(&wlr_scene_ptr->tree.node);
}

/* ------------------------------------------------------------------------- */
/** Tests that panel positions in different layers define workspace extents. */
void test_layout(bs_test_t *test_ptr)
{
    wlmtk_layer_t *l;

    // Test setup: Two outputs.
    struct wlr_scene *wlr_scene_ptr = wlr_scene_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_scene_ptr);
    struct wl_display *wl_display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wl_display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(wl_display_ptr);
    struct wlmtk_desktop_style ds = {};
    wlmtk_desktop_t *desktop_ptr = wlmtk_desktop_create(
        wlr_scene_ptr, &ds, wlr_output_layout_ptr);
    struct wlr_output o1 = { .width = 1024, .height = 768, .scale = 1 };
    wlmtk_test_wlr_output_init(&o1);
    wlr_output_layout_add_auto(wlr_output_layout_ptr, &o1);
    struct wlr_output o2 = { .width = 640, .height = 480, .scale = 1 };
    wlmtk_test_wlr_output_init(&o2);
    wlr_output_layout_add_auto(wlr_output_layout_ptr, &o2);

    struct wlmtk_tile_style ts = {};
    wlmtk_workspace_t *ws_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "name", &ts);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_desktop_add_workspace(desktop_ptr, ws_ptr);

    // Adds a panel to the bottom layer, output 2.
    wlmtk_panel_positioning_t p1 = {
        .anchor = WLR_EDGE_LEFT,
        .exclusive_zone = 64
    };
    wlmtk_fake_panel_t *fp1_ptr = wlmtk_fake_panel_create(&p1);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fp1_ptr);
    wlmtk_element_set_visible(wlmtk_panel_element(&fp1_ptr->panel), true);
    l  = wlmtk_desktop_get_layer(desktop_ptr, WLMTK_DESKTOP_LAYER_BOTTOM);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_layer_add_panel(l, &fp1_ptr->panel, &o2));

    // Adds a panel to the top layer, output 2.
    wlmtk_panel_positioning_t p2 = {
        .anchor = WLR_EDGE_LEFT,
        .exclusive_zone = 80
    };
    wlmtk_fake_panel_t *fp2_ptr = wlmtk_fake_panel_create(&p2);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fp2_ptr);
    wlmtk_element_set_visible(wlmtk_panel_element(&fp2_ptr->panel), true);
    l = wlmtk_desktop_get_layer(desktop_ptr, WLMTK_DESKTOP_LAYER_TOP);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_layer_add_panel(l, &fp2_ptr->panel, &o2));

    // When computing the full layout, p1's position shifts p2.
    wlmtk_element_layout(wlmtk_desktop_element(desktop_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, 1024, wlmtk_panel_element(&fp1_ptr->panel)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 1088, wlmtk_panel_element(&fp2_ptr->panel)->x);
    // Maximize extents on o2 are without p1 and p2.
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 1024, 768,
        wlmtk_workspace_get_maximize_extents(ws_ptr, &o1));
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 1168, 0, 496, 480,
        wlmtk_workspace_get_maximize_extents(ws_ptr, &o2));

    wlr_output_layout_remove(wlr_output_layout_ptr, &o2);
    wlr_output_layout_remove(wlr_output_layout_ptr, &o1);
    wlmtk_desktop_destroy(desktop_ptr);
    wlr_output_layout_destroy(wlr_output_layout_ptr);
    wl_display_destroy(wl_display_ptr);
    wlr_scene_node_destroy(&wlr_scene_ptr->tree.node);
}

/* == End of desktop.c ====================================================== */
