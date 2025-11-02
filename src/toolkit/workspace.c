/* ========================================================================= */
/**
 * @file workspace.c
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

#include "workspace.h"

#include <libbase/libbase.h>
#include <linux/input-event-codes.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-util.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/edges.h>
#undef WLR_USE_UNSTABLE

#include "container.h"
#include "fsm.h"
#include "input.h"
#include "layer.h"
#include "surface.h"
#include "test.h"  // IWYU pragma: keep
#include "tile.h"
#include "util.h"

/* == Declarations ========================================================= */

/** State of the workspace. */
struct _wlmtk_workspace_t {
    /** Superclass: Container. */
    wlmtk_container_t         super_container;
    /** Original virtual method table. We're overwriting parts. */
    wlmtk_element_vmt_t       orig_super_element_vmt;

    /** Link to the @ref wlmtk_root_t this workspace is attached to. */
    wlmtk_root_t              *root_ptr;
    /** An element of @ref wlmtk_root_t::workspaces. */
    bs_dllist_node_t          dlnode;

    /** Name of the workspace. */
    char                      *name_ptr;
    /** Index of this workspace. */
    int                       index;

    /** Current FSM state. */
    wlmtk_fsm_t               fsm;

    /** Whether this workspace is enabled, ie. can have activated windows. */
    bool                      enabled;
    /** Container that holds the windows, ie. the window layer. */
    wlmtk_container_t         window_container;
    /** Container that holds the fullscreen elements. Should have only one. */
    wlmtk_container_t         fullscreen_container;

    /** List of toplevel windows. Via @ref wlmtk_window2_t::dlnode. */
    bs_dllist_t               window2s;

    /** The activated window. */
    wlmtk_window2_t           *activated_window_ptr;
    /** The most recent activated window, if none is activated now. */
    wlmtk_window2_t           *formerly_activated_window_ptr;

    /** The grabbed window. */
    wlmtk_window2_t           *grabbed_window_ptr;
    /** Motion X */
    int                       motion_x;
    /** Motion Y */
    int                       motion_y;
    /** Element's X position when initiating a move or resize. */
    int                       initial_x;
    /** Element's Y position when initiating a move or resize. */
    int                       initial_y;
    /** Window's width when initiazing the resize. */
    int                       initial_width;
    /** Window's height  when initiazing the resize. */
    int                       initial_height;
    /** Edges currently active for resizing: `enum wlr_edges`. */
    uint32_t                  resize_edges;

    /** Top left X coordinate of workspace. */
    int                       x1;
    /** Top left Y coordinate of workspace. */
    int                       y1;
    /** Bottom right X coordinate of workspace. */
    int                       x2;
    /** Bottom right Y coordinate of workspace. */
    int                       y2;

    /** Background layer. */
    wlmtk_layer_t             *background_layer_ptr;
    /** Bottom layer. */
    wlmtk_layer_t             *bottom_layer_ptr;
    /** Top layer. */
    wlmtk_layer_t             *top_layer_ptr;
    /** Overlay layer. */
    wlmtk_layer_t             *overlay_layer_ptr;

    /** Copy of the tile's style, for dimensions; */
    wlmtk_tile_style_t        tile_style;

    /** Listener for wlr_output_layout::events.change. */
    struct wl_listener        output_layout_change_listener;
    /** Listener for @ref wlmtk_element_events_t::pointer_leave. */
    struct wl_listener        element_pointer_leave_listener;
    /** Listener for @ref wlmtk_element_events_t::pointer_motion. */
    struct wl_listener        element_pointer_motion_listener;

    // Elements below not owned by wlmtk_workspace_t.
    /** Output layout. */
    struct wlr_output_layout *wlr_output_layout_ptr;
};

static void _wlmtk_workspace_element_destroy(wlmtk_element_t *element_ptr);
static void _wlmtk_workspace_element_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr);
static bool _wlmtk_workspace_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);

static void _wlmtk_workspace_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmtk_workspace_handle_element_pointer_leave(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmtk_workspace_handle_element_pointer_motion(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmtk_window_reposition_window(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);

static bool pfsm_move_begin(wlmtk_fsm_t *fsm_ptr, void *ud_ptr);
static bool pfsm_move_motion(wlmtk_fsm_t *fsm_ptr, void *ud_ptr);
static bool pfsm_resize_begin(wlmtk_fsm_t *fsm_ptr, void *ud_ptr);
static bool pfsm_resize_motion(wlmtk_fsm_t *fsm_ptr, void *ud_ptr);
static bool pfsm_reset(wlmtk_fsm_t *fsm_ptr, void *ud_ptr);

/* == Data ================================================================= */

/** States of the pointer FSM. */
typedef enum {
    PFSMS_PASSTHROUGH,
    PFSMS_MOVE,
    PFSMS_RESIZE
} pointer_state_t;

/** Events for the pointer FSM. */
typedef enum {
    PFSME_BEGIN_MOVE,
    PFSME_BEGIN_RESIZE,
    PFSME_RELEASED,
    PFSME_MOTION,
    PFSME_RESET,
} pointer_state_event_t;

/** Extensions to the workspace's super element's virtual methods. */
const wlmtk_element_vmt_t     workspace_element_vmt = {
    .destroy = _wlmtk_workspace_element_destroy,
    .get_dimensions = _wlmtk_workspace_element_get_dimensions,
    .pointer_button = _wlmtk_workspace_element_pointer_button,
};

/** Finite state machine definition for pointer events. */
static const wlmtk_fsm_transition_t pfsm_transitions[] = {
    { PFSMS_PASSTHROUGH, PFSME_BEGIN_MOVE, PFSMS_MOVE, pfsm_move_begin },
    { PFSMS_MOVE, PFSME_MOTION, PFSMS_MOVE, pfsm_move_motion },
    { PFSMS_MOVE, PFSME_RELEASED, PFSMS_PASSTHROUGH, pfsm_reset },
    { PFSMS_MOVE, PFSME_RESET, PFSMS_PASSTHROUGH, pfsm_reset },
    { PFSMS_PASSTHROUGH, PFSME_BEGIN_RESIZE, PFSMS_RESIZE, pfsm_resize_begin },
    { PFSMS_RESIZE, PFSME_MOTION, PFSMS_RESIZE, pfsm_resize_motion },
    { PFSMS_RESIZE, PFSME_RELEASED, PFSMS_PASSTHROUGH, pfsm_reset },
    { PFSMS_RESIZE, PFSME_RESET, PFSMS_PASSTHROUGH, pfsm_reset },
    WLMTK_FSM_TRANSITION_SENTINEL,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_workspace_t *wlmtk_workspace_create(
    struct wlr_output_layout *wlr_output_layout_ptr,
    const char *name_ptr,
    const wlmtk_tile_style_t *tile_style_ptr)
{
    wlmtk_workspace_t *workspace_ptr =
        logged_calloc(1, sizeof(wlmtk_workspace_t));
    if (NULL == workspace_ptr) return NULL;
    workspace_ptr->wlr_output_layout_ptr = wlr_output_layout_ptr;
    workspace_ptr->tile_style = *tile_style_ptr;
    workspace_ptr->name_ptr = logged_strdup(name_ptr);
    if (NULL == workspace_ptr->name_ptr) {
        wlmtk_workspace_destroy(workspace_ptr);
        return NULL;
    }

    if (!wlmtk_container_init(&workspace_ptr->super_container)) {
        wlmtk_workspace_destroy(workspace_ptr);
        return NULL;
    }
    workspace_ptr->orig_super_element_vmt = wlmtk_element_extend(
        &workspace_ptr->super_container.super_element,
        &workspace_element_vmt);
    wlmtk_util_connect_listener_signal(
        &workspace_ptr->super_container.super_element.events.pointer_leave,
        &workspace_ptr->element_pointer_leave_listener,
        _wlmtk_workspace_handle_element_pointer_leave);
    wlmtk_util_connect_listener_signal(
        &workspace_ptr->super_container.super_element.events.pointer_motion,
        &workspace_ptr->element_pointer_motion_listener,
        _wlmtk_workspace_handle_element_pointer_motion);

    if (!wlmtk_container_init(&workspace_ptr->window_container)) {
        wlmtk_workspace_destroy(workspace_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        &workspace_ptr->window_container.super_element,
        true);
    wlmtk_container_add_element(
        &workspace_ptr->super_container,
        &workspace_ptr->window_container.super_element);

    if (!wlmtk_container_init(&workspace_ptr->fullscreen_container)) {
        wlmtk_workspace_destroy(workspace_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        &workspace_ptr->fullscreen_container.super_element,
        true);
    wlmtk_container_add_element(
        &workspace_ptr->super_container,
        &workspace_ptr->fullscreen_container.super_element);

    workspace_ptr->background_layer_ptr = wlmtk_layer_create(
        wlr_output_layout_ptr);
    if (NULL == workspace_ptr->background_layer_ptr) {
        wlmtk_workspace_destroy(workspace_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_layer_element(workspace_ptr->background_layer_ptr),
        true);
    wlmtk_container_add_element_atop(
        &workspace_ptr->super_container,
        NULL,
        wlmtk_layer_element(workspace_ptr->background_layer_ptr));
    wlmtk_layer_set_workspace(
        workspace_ptr->background_layer_ptr,
        workspace_ptr);

    workspace_ptr->bottom_layer_ptr = wlmtk_layer_create(
        wlr_output_layout_ptr);
    if (NULL == workspace_ptr->bottom_layer_ptr) {
        wlmtk_workspace_destroy(workspace_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_layer_element(workspace_ptr->bottom_layer_ptr),
        true);
    wlmtk_container_add_element_atop(
        &workspace_ptr->super_container,
        wlmtk_layer_element(workspace_ptr->background_layer_ptr),
        wlmtk_layer_element(workspace_ptr->bottom_layer_ptr));
    wlmtk_layer_set_workspace(workspace_ptr->bottom_layer_ptr, workspace_ptr);

    workspace_ptr->top_layer_ptr = wlmtk_layer_create(
        wlr_output_layout_ptr);
    if (NULL == workspace_ptr->top_layer_ptr) {
        wlmtk_workspace_destroy(workspace_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_layer_element(workspace_ptr->top_layer_ptr),
        true);
    wlmtk_container_add_element_atop(
        &workspace_ptr->super_container,
        &workspace_ptr->window_container.super_element,
        wlmtk_layer_element(workspace_ptr->top_layer_ptr));
    wlmtk_layer_set_workspace(workspace_ptr->top_layer_ptr, workspace_ptr);

    workspace_ptr->overlay_layer_ptr = wlmtk_layer_create(
        wlr_output_layout_ptr);
    if (NULL == workspace_ptr->overlay_layer_ptr) {
        wlmtk_workspace_destroy(workspace_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_layer_element(workspace_ptr->overlay_layer_ptr),
        true);
    wlmtk_container_add_element_atop(
        &workspace_ptr->super_container,
        wlmtk_layer_element(workspace_ptr->top_layer_ptr),
        wlmtk_layer_element(workspace_ptr->overlay_layer_ptr));
    wlmtk_layer_set_workspace(workspace_ptr->overlay_layer_ptr, workspace_ptr);

    wlmtk_fsm_init(&workspace_ptr->fsm, pfsm_transitions, PFSMS_PASSTHROUGH);

    wlmtk_util_connect_listener_signal(
        &workspace_ptr->wlr_output_layout_ptr->events.change,
        &workspace_ptr->output_layout_change_listener,
        _wlmtk_workspace_handle_output_layout_change);
    _wlmtk_workspace_handle_output_layout_change(
        &workspace_ptr->output_layout_change_listener,
        workspace_ptr->wlr_output_layout_ptr);

    return workspace_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_destroy(wlmtk_workspace_t *workspace_ptr)
{
    wlmtk_util_disconnect_listener(
        &workspace_ptr->output_layout_change_listener);

    if (NULL != workspace_ptr->overlay_layer_ptr) {
        wlmtk_layer_set_workspace(workspace_ptr->overlay_layer_ptr, NULL);
        wlmtk_container_remove_element(
            &workspace_ptr->super_container,
            wlmtk_layer_element(workspace_ptr->overlay_layer_ptr));
        wlmtk_layer_destroy(workspace_ptr->overlay_layer_ptr);
        workspace_ptr->overlay_layer_ptr = NULL;
    }
    if (NULL != workspace_ptr->top_layer_ptr) {
        wlmtk_layer_set_workspace(workspace_ptr->top_layer_ptr, NULL);
        wlmtk_container_remove_element(
            &workspace_ptr->super_container,
            wlmtk_layer_element(workspace_ptr->top_layer_ptr));
        wlmtk_layer_destroy(workspace_ptr->top_layer_ptr);
        workspace_ptr->top_layer_ptr = NULL;
    }
    if (NULL != workspace_ptr->bottom_layer_ptr) {
        wlmtk_layer_set_workspace(workspace_ptr->bottom_layer_ptr, NULL);
        wlmtk_container_remove_element(
            &workspace_ptr->super_container,
            wlmtk_layer_element(workspace_ptr->bottom_layer_ptr));
        wlmtk_layer_destroy(workspace_ptr->bottom_layer_ptr);
        workspace_ptr->bottom_layer_ptr = NULL;
    }
    if (NULL != workspace_ptr->background_layer_ptr) {
        wlmtk_layer_set_workspace(workspace_ptr->background_layer_ptr, NULL);
        wlmtk_container_remove_element(
            &workspace_ptr->super_container,
            wlmtk_layer_element(workspace_ptr->background_layer_ptr));
        wlmtk_layer_destroy(workspace_ptr->background_layer_ptr);
        workspace_ptr->background_layer_ptr = NULL;
    }

    if (NULL != workspace_ptr->fullscreen_container.super_element.parent_container_ptr) {
        wlmtk_container_remove_element(
            &workspace_ptr->super_container,
            &workspace_ptr->fullscreen_container.super_element);
    }
    wlmtk_util_disconnect_listener(
        &workspace_ptr->element_pointer_leave_listener);
    wlmtk_util_disconnect_listener(
        &workspace_ptr->element_pointer_motion_listener);
    wlmtk_container_fini(&workspace_ptr->fullscreen_container);

    if (NULL != workspace_ptr->window_container.super_element.parent_container_ptr) {
        wlmtk_container_remove_element(
            &workspace_ptr->super_container,
            &workspace_ptr->window_container.super_element);
    }
    wlmtk_container_fini(&workspace_ptr->window_container);

    wlmtk_container_fini(&workspace_ptr->super_container);

    if (NULL != workspace_ptr->name_ptr) {
        free(workspace_ptr->name_ptr);
        workspace_ptr->name_ptr = NULL;
    }
    free(workspace_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_set_details(
    wlmtk_workspace_t *workspace_ptr,
    int index)
{
    workspace_ptr->index = index;
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_get_details(
    wlmtk_workspace_t *workspace_ptr,
    const char **name_ptr_ptr,
    int *index_ptr)
{
    *index_ptr = workspace_ptr->index;
    *name_ptr_ptr = workspace_ptr->name_ptr;
}

/* ------------------------------------------------------------------------- */
struct wlr_box wlmtk_workspace_get_maximize_extents(
    wlmtk_workspace_t *workspace_ptr,
    struct wlr_output *wlr_output_ptr)
{
    struct wlr_box extents = {};

    // No output provided. Pick the primary (first one in layout).
    if (NULL == wlr_output_ptr) {
        if (wl_list_empty(&workspace_ptr->wlr_output_layout_ptr->outputs)) {
            return extents;
        }

        struct wlr_output_layout_output *wol_output_ptr = BS_CONTAINER_OF(
            workspace_ptr->wlr_output_layout_ptr->outputs.next,
            struct wlr_output_layout_output,
            link);
        wlr_output_ptr = wol_output_ptr->output;
    }

    wlr_output_layout_get_box(
        workspace_ptr->wlr_output_layout_ptr,
        wlr_output_ptr,
        &extents);

    if (!wlr_box_empty(&extents)) {
        // TODO(kaeser@gubbe.ch): Well, actually compute something sensible.
        extents.width -= workspace_ptr->tile_style.size;
        extents.height -= workspace_ptr->tile_style.size;

    }
    return extents;
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_confine_within(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_window2_t *window_ptr)
{
    // Only act if the window belongs to this workspace.
    if (workspace_ptr != wlmtk_window2_get_workspace(window_ptr)) return;

    struct wlr_box box = {
        .x = workspace_ptr->x1,
        .y = workspace_ptr->y1,
        .width = workspace_ptr->x2 - workspace_ptr->x1,
        .height = workspace_ptr->y2 - workspace_ptr->y1 };

    struct wlr_box elem_box = wlmtk_element_get_dimensions_box(
        wlmtk_window2_element(window_ptr));
    int x, y;
    wlmtk_element_get_position(wlmtk_window2_element(window_ptr), &x, &y);

    int max_x = x - elem_box.x + elem_box.width;
    if (max_x > box.width) x -= max_x - box.width;

    int max_y = y - elem_box.y + elem_box.height;
    if (max_y > box.height) y -= max_y - box.height;

    wlmtk_workspace_set_window_position(workspace_ptr, window_ptr, x, y);
}

/* ------------------------------------------------------------------------- */
struct wlr_output_layout *wlmtk_workspace_get_wlr_output_layout(
    wlmtk_workspace_t *workspace_ptr)
{
    return workspace_ptr->wlr_output_layout_ptr;
}

/* ------------------------------------------------------------------------- */
struct wlr_box wlmtk_workspace_get_fullscreen_extents(
    wlmtk_workspace_t *workspace_ptr,
    struct wlr_output *wlr_output_ptr)
{
    struct wlr_box extents = {};

    // No output provided. Pick the primary (first one in layout).
    if (NULL == wlr_output_ptr) {
        if (wl_list_empty(&workspace_ptr->wlr_output_layout_ptr->outputs)) {
            return extents;
        }

        struct wlr_output_layout_output *wol_output_ptr = BS_CONTAINER_OF(
            workspace_ptr->wlr_output_layout_ptr->outputs.next,
            struct wlr_output_layout_output,
            link);
        wlr_output_ptr = wol_output_ptr->output;
    }

    wlr_output_layout_get_box(
        workspace_ptr->wlr_output_layout_ptr,
        wlr_output_ptr,
        &extents);
    return extents;
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_enable(wlmtk_workspace_t *workspace_ptr, bool enabled)
{
    if (workspace_ptr->enabled == enabled) return;

    workspace_ptr->enabled = enabled;
    if (!enabled) {
        wlmtk_workspace_activate_window(workspace_ptr, NULL);
    } else {
        wlmtk_workspace_activate_window(
            workspace_ptr,
            workspace_ptr->formerly_activated_window_ptr);
    }
}

/* ------------------------------------------------------------------------- */
bool wlmtk_workspace_enabled(wlmtk_workspace_t *workspace_ptr)
{
    return workspace_ptr->enabled;
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_map_window2(wlmtk_workspace_t *workspace_ptr,
                                 wlmtk_window2_t *window_ptr)
{
    BS_ASSERT(NULL == wlmtk_window2_get_workspace(window_ptr));

    wlmtk_element_set_visible(wlmtk_window2_element(window_ptr), true);
    wlmtk_container_add_element(
        &workspace_ptr->window_container,
        wlmtk_window2_element(window_ptr));
    bs_dllist_push_front(&workspace_ptr->window2s,
                         wlmtk_dlnode_from_window2(window_ptr));
    wlmtk_window2_set_workspace(window_ptr, workspace_ptr);

    wlmtk_workspace_activate_window(workspace_ptr, window_ptr);

    if (NULL != workspace_ptr->root_ptr) {
        wl_signal_emit(
            &wlmtk_root_events(workspace_ptr->root_ptr)->window_mapped,
            window_ptr);
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_unmap_window2(wlmtk_workspace_t *workspace_ptr,
                                   wlmtk_window2_t *window_ptr)
{
    bool need_activation = false;

    BS_ASSERT(workspace_ptr == wlmtk_window2_get_workspace(window_ptr));

    if (workspace_ptr->grabbed_window_ptr == window_ptr) {
        wlmtk_fsm_event(&workspace_ptr->fsm, PFSME_RESET, NULL);
        BS_ASSERT(NULL == workspace_ptr->grabbed_window_ptr);
    }

    if (workspace_ptr->activated_window_ptr == window_ptr) {
        wlmtk_workspace_activate_window(workspace_ptr, NULL);
        need_activation = true;
    }
    if (workspace_ptr->formerly_activated_window_ptr == window_ptr) {
        workspace_ptr->formerly_activated_window_ptr = NULL;
        need_activation = true;
    }

    wlmtk_element_set_visible(wlmtk_window2_element(window_ptr), false);

    if (wlmtk_window2_is_fullscreen(window_ptr)) {
        wlmtk_container_remove_element(
            &workspace_ptr->fullscreen_container,
            wlmtk_window2_element(window_ptr));
    } else {
        wlmtk_container_remove_element(
            &workspace_ptr->window_container,
            wlmtk_window2_element(window_ptr));
    }
    bs_dllist_remove(&workspace_ptr->window2s,
                     wlmtk_dlnode_from_window2(window_ptr));
    wlmtk_window2_set_workspace(window_ptr, NULL);
    if (NULL != workspace_ptr->root_ptr) {
        wl_signal_emit(
            &wlmtk_root_events(workspace_ptr->root_ptr)->window_unmapped,
            window_ptr);
    }

    if (need_activation) {
        // FIXME: What about raising?
        bs_dllist_node_t *dlnode_ptr =
            workspace_ptr->window_container.elements.head_ptr;
        if (NULL != dlnode_ptr) {
            wlmtk_element_t *element_ptr = wlmtk_element_from_dlnode(dlnode_ptr);
            wlmtk_window2_t *window_ptr = wlmtk_window2_from_element(element_ptr);
            wlmtk_workspace_activate_window(workspace_ptr, window_ptr);
        }
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_set_window_position(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_window2_t *window_ptr,
    int x,
    int y)
{
    if (workspace_ptr != wlmtk_window2_get_workspace(window_ptr)) return;
    wlmtk_element_set_position(wlmtk_window2_element(window_ptr), x, y);
    wlmtk_window2_position_changed(window_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_layer_t *wlmtk_workspace_get_layer(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_workspace_layer_t layer)
{
    wlmtk_layer_t *layer_ptr = NULL;
    switch (layer) {
    case WLMTK_WORKSPACE_LAYER_BACKGROUND:
        layer_ptr = workspace_ptr->background_layer_ptr;
        break;
    case WLMTK_WORKSPACE_LAYER_BOTTOM:
        layer_ptr = workspace_ptr->bottom_layer_ptr;
        break;
    case WLMTK_WORKSPACE_LAYER_TOP:
        layer_ptr = workspace_ptr->top_layer_ptr;
        break;
    case WLMTK_WORKSPACE_LAYER_OVERLAY:
        layer_ptr = workspace_ptr->overlay_layer_ptr;
        break;
    default:
        break;
    }

    return layer_ptr;
}

/* ------------------------------------------------------------------------- */
bs_dllist_t *wlmtk_workspace_get_window2s_dllist(
    wlmtk_workspace_t *workspace_ptr)
{
    return &workspace_ptr->window2s;
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_window_to_fullscreen(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_window2_t *window_ptr,
    bool fullscreen)
{
    BS_ASSERT(workspace_ptr == wlmtk_window2_get_workspace(window_ptr));
    BS_ASSERT(fullscreen == wlmtk_window2_is_fullscreen(window_ptr));

    if (fullscreen) {
        BS_ASSERT(
            bs_dllist_contains(
                &workspace_ptr->window_container.elements,
                wlmtk_dlnode_from_element(wlmtk_window2_element(window_ptr))));

        // TODO(kaeser@gubbe.ch): Add a method to just reparent an element. The
        // current implementation will destroy, then re-create each of the
        // scene nodes.
        wlmtk_container_remove_element(
            &workspace_ptr->window_container,
            wlmtk_window2_element(window_ptr));
        wlmtk_container_add_element(
            &workspace_ptr->fullscreen_container,
            wlmtk_window2_element(window_ptr));
        // TODO(kaeser@gubbe.ch): Removing the element appears to not reset
        // the keyboard focus, hence the unset/set combo. Debug & fix this.
        wlmtk_workspace_activate_window(workspace_ptr, NULL);
        wlmtk_workspace_activate_window(workspace_ptr, window_ptr);

    } else {
        BS_ASSERT(
            bs_dllist_contains(
                &workspace_ptr->fullscreen_container.elements,
                wlmtk_dlnode_from_element(wlmtk_window2_element(window_ptr))));

        wlmtk_container_remove_element(
            &workspace_ptr->fullscreen_container,
            wlmtk_window2_element(window_ptr));
        wlmtk_container_add_element(
            &workspace_ptr->window_container,
            wlmtk_window2_element(window_ptr));
        wlmtk_workspace_activate_window(workspace_ptr, NULL);
        wlmtk_workspace_activate_window(workspace_ptr, window_ptr);

        // The un-fullscreened window will come on top of the container. Also
        // reflect that in @ref wlmtk_workspace_t::window2s.
        bs_dllist_remove(&workspace_ptr->window2s,
                         wlmtk_dlnode_from_window2(window_ptr));
        bs_dllist_push_front(&workspace_ptr->window2s,
                             wlmtk_dlnode_from_window2(window_ptr));
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_begin_window_move(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_window2_t *window_ptr)
{
    BS_ASSERT(NULL != workspace_ptr);
    BS_ASSERT(workspace_ptr == wlmtk_window2_get_workspace(window_ptr));
    wlmtk_fsm_event(&workspace_ptr->fsm, PFSME_BEGIN_MOVE, window_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_begin_window_resize(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_window2_t *window_ptr,
    uint32_t edges)
{
    BS_ASSERT(NULL != workspace_ptr);
    BS_ASSERT(workspace_ptr == wlmtk_window2_get_workspace(window_ptr));
    workspace_ptr->resize_edges = edges;
    wlmtk_fsm_event(&workspace_ptr->fsm, PFSME_BEGIN_RESIZE, window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Acticates `window_ptr`. Will de-activate an earlier window. */
void wlmtk_workspace_activate_window(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_window2_t *window_ptr)
{
    // Nothing to do.
    if (workspace_ptr->activated_window_ptr == window_ptr) return;

    if (NULL != workspace_ptr->activated_window_ptr) {
        wlmtk_window2_t *w_ptr = workspace_ptr->activated_window_ptr;
        workspace_ptr->formerly_activated_window_ptr =
            workspace_ptr->activated_window_ptr;
        workspace_ptr->activated_window_ptr = NULL;
        wlmtk_window2_set_activated(w_ptr, false);
    }

    if (NULL != window_ptr) {
        if (workspace_ptr->enabled) {
            workspace_ptr->activated_window_ptr = window_ptr;
            wlmtk_window2_set_activated(window_ptr, true);
        }
        workspace_ptr->formerly_activated_window_ptr = window_ptr;
    }
}

/* ------------------------------------------------------------------------- */
wlmtk_window2_t *wlmtk_workspace_get_activated_window(
    wlmtk_workspace_t *workspace_ptr)
{
    return workspace_ptr->activated_window_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_activate_previous_window(
    wlmtk_workspace_t *workspace_ptr)
{
    bs_dllist_node_t *dlnode_ptr = NULL;
    if (NULL != workspace_ptr->activated_window_ptr) {
        dlnode_ptr = wlmtk_dlnode_from_window2(
            workspace_ptr->activated_window_ptr);
        dlnode_ptr = dlnode_ptr->prev_ptr;
    }
    if (NULL == dlnode_ptr) {
        dlnode_ptr = workspace_ptr->window2s.tail_ptr;
    }

    if (NULL == dlnode_ptr) return;

    wlmtk_workspace_activate_window(
        workspace_ptr, wlmtk_window2_from_dlnode(dlnode_ptr));
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_activate_next_window(
    wlmtk_workspace_t *workspace_ptr)
{
    bs_dllist_node_t *dlnode_ptr = NULL;
    if (NULL != workspace_ptr->activated_window_ptr) {
        dlnode_ptr = wlmtk_dlnode_from_window2(
            workspace_ptr->activated_window_ptr);
        dlnode_ptr = dlnode_ptr->next_ptr;
    }
    if (NULL == dlnode_ptr) {
        dlnode_ptr = workspace_ptr->window2s.head_ptr;
    }

    if (NULL == dlnode_ptr) return;

    wlmtk_workspace_activate_window(
        workspace_ptr, wlmtk_window2_from_dlnode(dlnode_ptr));
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_raise_window(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_window2_t *window_ptr)
{
    BS_ASSERT(workspace_ptr == wlmtk_window2_get_workspace(window_ptr));
    bs_dllist_remove(&workspace_ptr->window2s,
                     wlmtk_dlnode_from_window2(window_ptr));
    bs_dllist_push_front(&workspace_ptr->window2s,
                         wlmtk_dlnode_from_window2(window_ptr));
    wlmtk_container_raise_element_to_top(&workspace_ptr->window_container,
                                         wlmtk_window2_element(window_ptr));
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_workspace_element(wlmtk_workspace_t *workspace_ptr)
{
    return &workspace_ptr->super_container.super_element;
}

/* ------------------------------------------------------------------------- */
wlmtk_root_t *wlmtk_workspace_get_root(wlmtk_workspace_t *workspace_ptr)
{
    return workspace_ptr->root_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_set_root(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_root_t *root_ptr)
{
    workspace_ptr->root_ptr = root_ptr;
}

/* ------------------------------------------------------------------------- */
bs_dllist_node_t *wlmtk_dlnode_from_workspace(
    wlmtk_workspace_t *workspace_ptr)
{
    return &workspace_ptr->dlnode;
}

/* ------------------------------------------------------------------------- */
wlmtk_workspace_t *wlmtk_workspace_from_dlnode(
    bs_dllist_node_t *dlnode_ptr)
{
    if (NULL == dlnode_ptr) return NULL;
    return BS_CONTAINER_OF(dlnode_ptr, wlmtk_workspace_t, dlnode);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Virtual destructor, wraps to our dtor. */
void _wlmtk_workspace_element_destroy(wlmtk_element_t *element_ptr)
{
    wlmtk_workspace_t *workspace_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_workspace_t, super_container.super_element);
    wlmtk_workspace_destroy(workspace_ptr);
}

/* ------------------------------------------------------------------------- */
/** Returns the workspace area. */
void _wlmtk_workspace_element_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr)
{
    wlmtk_workspace_t *workspace_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_workspace_t, super_container.super_element);

    if (NULL != left_ptr) *left_ptr = workspace_ptr->x1;
    if (NULL != top_ptr) *top_ptr = workspace_ptr->y1;
    if (NULL != right_ptr) *right_ptr = workspace_ptr->x2;
    if (NULL != bottom_ptr) *bottom_ptr = workspace_ptr->y2;
}

/* ------------------------------------------------------------------------- */
/**
 * Extends wlmtk_container_t::pointer_button.
 *
 * @param element_ptr
 * @param button_event_ptr
 *
 * @return Whether the button event was consumed.
 */
bool _wlmtk_workspace_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmtk_workspace_t *workspace_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_workspace_t, super_container.super_element);

    // TODO(kaeser@gubbe.ch): We should retract as to which event had triggered
    // the move, and then figure out the exit condition (button up? key? ...)
    // from there.
    // See xdg_toplevel::move doc at https://wayland.app/protocols/xdg-shell.
    if (button_event_ptr->button == BTN_LEFT &&
        button_event_ptr->type == WLMTK_BUTTON_UP) {
        wlmtk_fsm_event(&workspace_ptr->fsm, PFSME_RELEASED, NULL);
    }

    return workspace_ptr->orig_super_element_vmt.pointer_button(
        element_ptr, button_event_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles output changes: Updates own extents, updates layers. */
void _wlmtk_workspace_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmtk_workspace_t *workspace_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_workspace_t, output_layout_change_listener);
    struct wlr_output_layout *wlr_output_layout_ptr = data_ptr;

    struct wlr_box extents;
    wlr_output_layout_get_box(wlr_output_layout_ptr, NULL, &extents);
    workspace_ptr->x1 = extents.x;
    workspace_ptr->y1 = extents.y;
    workspace_ptr->x2 = extents.x + extents.width;
    workspace_ptr->y2 = extents.y + extents.height;

    bs_dllist_for_each(
        &workspace_ptr->window2s,
        _wlmtk_window_reposition_window,
        workspace_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles @ref wlmtk_element_events_t::pointer_leave. Reset state machine. */
void _wlmtk_workspace_handle_element_pointer_leave(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_workspace_t *workspace_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_workspace_t, element_pointer_leave_listener);
    wlmtk_fsm_event(&workspace_ptr->fsm, PFSME_RESET, NULL);
}

/* ------------------------------------------------------------------------- */
/** @ref wlmtk_element_events_t::pointer_motion. Feeds into state machine. */
void _wlmtk_workspace_handle_element_pointer_motion(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_workspace_t *workspace_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_workspace_t, element_pointer_motion_listener);
    wlmtk_fsm_event(&workspace_ptr->fsm, PFSME_MOTION, NULL);
}

/* ------------------------------------------------------------------------- */
/** Repositions the window. To be called when output layout changes. */
void _wlmtk_window_reposition_window(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    wlmtk_window2_t *window_ptr = wlmtk_window2_from_dlnode(dlnode_ptr);
    wlmtk_workspace_t *workspace_ptr = ud_ptr;

    // Fullscreen window? Re-position it. We commit right away, to re-position
    // the element.
    if (wlmtk_window2_is_fullscreen(window_ptr)) {
        wlmtk_window2_request_fullscreen(window_ptr, true);
        struct wlr_box fsbox = wlmtk_workspace_get_fullscreen_extents(
            workspace_ptr,
            wlmtk_window2_get_wlr_output(window_ptr));
        wlmtk_workspace_set_window_position(
            workspace_ptr, window_ptr, fsbox.x, fsbox.y);
        return;
    }

    // Maximized window? Re-request maximized, will polll the size.
    if (wlmtk_window2_is_maximized(window_ptr)) {
        wlmtk_window2_request_maximized(window_ptr, true);
        struct wlr_box box = wlmtk_workspace_get_maximize_extents(
            workspace_ptr,
            wlmtk_window2_get_wlr_output(window_ptr));
        wlmtk_workspace_set_window_position(
            workspace_ptr, window_ptr, box.x, box.y);
        return;
    }

    // Otherwise: See if the window dimensions (still) intersect. If yes: OK.
    struct wlr_box wbox = wlmtk_window2_get_bounding_box(window_ptr);
    if (wlr_output_layout_intersects(
            workspace_ptr->wlr_output_layout_ptr, NULL, &wbox)) return;

    // Otherwise: Re-position.
    double closest_x, closest_y;
    wlr_output_layout_closest_point(
        workspace_ptr->wlr_output_layout_ptr,
        NULL,  // reference.
        wbox.x + wbox.width / 2.0, wbox.y + wbox.height / 2.0,
        &closest_x, &closest_y);

    // May return NULL, but that's handled by wlr_output_layout_get_box().
    struct wlr_output *closest_wlr_output_ptr = wlr_output_layout_output_at(
        workspace_ptr->wlr_output_layout_ptr, closest_x, closest_y);
    struct wlr_box output_box;
    wlr_output_layout_get_box(
        workspace_ptr->wlr_output_layout_ptr,
        closest_wlr_output_ptr,
        &output_box);
    if (wlr_box_empty(&output_box)) {
        bs_log(BS_WARNING, "No nearby output found to re-position window %p",
               window_ptr);
        return;
    }

    wlmtk_workspace_set_window_position(
        workspace_ptr, window_ptr, output_box.x, output_box.y);
    wlmtk_window2_request_size(window_ptr, &wbox);
}


/* ------------------------------------------------------------------------- */
/** Initiates a move. */
bool pfsm_move_begin(wlmtk_fsm_t *fsm_ptr, void *ud_ptr)
{
    wlmtk_workspace_t *workspace_ptr = BS_CONTAINER_OF(
        fsm_ptr, wlmtk_workspace_t, fsm);

    workspace_ptr->grabbed_window_ptr = ud_ptr;
    workspace_ptr->motion_x =
        workspace_ptr->super_container.super_element.last_pointer_motion_event.x;
    workspace_ptr->motion_y =
        workspace_ptr->super_container.super_element.last_pointer_motion_event.y;

    wlmtk_element_get_position(
        wlmtk_window2_element(workspace_ptr->grabbed_window_ptr),
        &workspace_ptr->initial_x,
        &workspace_ptr->initial_y);

    // TODO(kaeser@gubbe.ch): When in move mode, set (and keep) a corresponding
    // cursor image.
    return true;
}

/* ------------------------------------------------------------------------- */
/** Handles motion during a move. */
bool pfsm_move_motion(wlmtk_fsm_t *fsm_ptr, __UNUSED__ void *ud_ptr)
{
    wlmtk_workspace_t *workspace_ptr = BS_CONTAINER_OF(
        fsm_ptr, wlmtk_workspace_t, fsm);

    double rel_x =
        workspace_ptr->super_container.super_element.last_pointer_motion_event.x -
        workspace_ptr->motion_x;
    double rel_y =
        workspace_ptr->super_container.super_element.last_pointer_motion_event.y -
        workspace_ptr->motion_y;

    wlmtk_workspace_set_window_position(
        workspace_ptr,
        workspace_ptr->grabbed_window_ptr,
        workspace_ptr->initial_x + rel_x,
        workspace_ptr->initial_y + rel_y);

    return true;
}

/* ------------------------------------------------------------------------- */
/** Initiates a resize. */
bool pfsm_resize_begin(wlmtk_fsm_t *fsm_ptr, void *ud_ptr)
{
    wlmtk_workspace_t *workspace_ptr = BS_CONTAINER_OF(
        fsm_ptr, wlmtk_workspace_t, fsm);

    workspace_ptr->grabbed_window_ptr = ud_ptr;
    workspace_ptr->motion_x =
        workspace_ptr->super_container.super_element.last_pointer_motion_event.x;
    workspace_ptr->motion_y =
        workspace_ptr->super_container.super_element.last_pointer_motion_event.y;

    wlmtk_element_get_position(
        wlmtk_window2_element(workspace_ptr->grabbed_window_ptr),
        &workspace_ptr->initial_x,
        &workspace_ptr->initial_y);

    struct wlr_box box = wlmtk_element_get_dimensions_box(
        wlmtk_window2_element(workspace_ptr->grabbed_window_ptr));
    workspace_ptr->initial_width = box.width;
    workspace_ptr->initial_height = box.height;

    wlmtk_window2_set_resize_edges(
        workspace_ptr->grabbed_window_ptr,
        workspace_ptr->resize_edges);

    return true;
}

/* ------------------------------------------------------------------------- */
/** Handles motion during a resize. */
bool pfsm_resize_motion(wlmtk_fsm_t *fsm_ptr, __UNUSED__ void *ud_ptr)
{
    wlmtk_workspace_t *workspace_ptr = BS_CONTAINER_OF(
        fsm_ptr, wlmtk_workspace_t, fsm);

    double rel_x =
        workspace_ptr->super_container.super_element.last_pointer_motion_event.x -
        workspace_ptr->motion_x;
    double rel_y =
        workspace_ptr->super_container.super_element.last_pointer_motion_event.y -
        workspace_ptr->motion_y;

    // Update new boundaries by the relative movement.
    int top = workspace_ptr->initial_y;
    int bottom = workspace_ptr->initial_y + workspace_ptr->initial_height;
    if (workspace_ptr->resize_edges & WLR_EDGE_TOP) {
        top += rel_y;
        if (top >= bottom) top = bottom - 1;
    } else if (workspace_ptr->resize_edges & WLR_EDGE_BOTTOM) {
        bottom += rel_y;
        if (bottom <= top) bottom = top + 1;
    }

    int left = workspace_ptr->initial_x;
    int right = workspace_ptr->initial_x + workspace_ptr->initial_width;
    if (workspace_ptr->resize_edges & WLR_EDGE_LEFT) {
        left += rel_x;
        if (left >= right) left = right - 1 ;
    } else if (workspace_ptr->resize_edges & WLR_EDGE_RIGHT) {
        right += rel_x;
        if (right <= left) right = left + 1;
    }

    struct wlr_box box = {
        .width = right - left, .height = bottom - top
    };
    wlmtk_window2_request_size(
        workspace_ptr->grabbed_window_ptr, &box);
    return true;
}

/* ------------------------------------------------------------------------- */
/** Resets the state machine. */
bool pfsm_reset(wlmtk_fsm_t *fsm_ptr, __UNUSED__ void *ud_ptr)
{
    wlmtk_workspace_t *workspace_ptr = BS_CONTAINER_OF(
        fsm_ptr, wlmtk_workspace_t, fsm);

    wlmtk_window2_set_resize_edges(workspace_ptr->grabbed_window_ptr, 0);

    workspace_ptr->grabbed_window_ptr = NULL;
    return true;
}

/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);
static void test_map_unmap(bs_test_t *test_ptr);
static void test_move(bs_test_t *test_ptr);
static void test_unmap_during_move(bs_test_t *test_ptr);
static void test_resize(bs_test_t *test_ptr);
static void test_enable(bs_test_t *test_ptr);
static void test_activate(bs_test_t *test_ptr);
static void test_activate_cycling(bs_test_t *test_ptr);
static void test_multi_output_extents(bs_test_t *test_ptr);
static void test_multi_output_reposition(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_workspace_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "map_unmap", test_map_unmap },
    { 1, "move", test_move },
    { 1, "unmap_during_move", test_unmap_during_move },
    { 1, "resize", test_resize },
    { 1, "enable", test_enable } ,
    { 1, "activate", test_activate },
    { 1, "activate_cycling", test_activate_cycling },
    { 1, "multi_output_extents", test_multi_output_extents },
    { 1, "multi_output_reposition", test_multi_output_reposition },
    { 0, NULL, NULL }
};

/** Tile style used in tests. */
static const wlmtk_tile_style_t _wlmtk_workspace_test_tile_style = {
    .size = 64
};

/* ------------------------------------------------------------------------- */
/** Exercises workspace create & destroy methods. */
void test_create_destroy(bs_test_t *test_ptr)
{
    struct wl_display *display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(display_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_output_layout_ptr);
    struct wlr_output output = { .width = 100, .height = 200, .scale = 1 };
    wlmtk_test_wlr_output_init(&output);
    wlr_output_layout_add(wlr_output_layout_ptr, &output, -10, -20);

    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "t", &_wlmtk_workspace_test_tile_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, workspace_ptr);

    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlr_output_layout_ptr,
        wlmtk_workspace_get_wlr_output_layout(workspace_ptr));

    struct wlr_box box = { .x = -10, .y = -20, .width = 100, .height = 200 };
    workspace_ptr->x1 = -10;
    workspace_ptr->y1 = -20;
    workspace_ptr->x2 = 90;
    workspace_ptr->y2 = 180;

    box = wlmtk_workspace_get_maximize_extents(workspace_ptr, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, -10, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, -20, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 36, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 136, box.height);

    box = wlmtk_workspace_get_fullscreen_extents(workspace_ptr, NULL);
    BS_TEST_VERIFY_EQ(test_ptr, -10, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, -20, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 100, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 200, box.height);

    const char *name_ptr;
    int index;
    wlmtk_workspace_set_details(workspace_ptr, 42);
    wlmtk_workspace_get_details(workspace_ptr, &name_ptr, &index);
    BS_TEST_VERIFY_STREQ(test_ptr, "t", name_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 42, index);

    wlmtk_workspace_destroy(workspace_ptr);
    wl_display_destroy(display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Verifies that mapping and unmapping windows works. */
void test_map_unmap(bs_test_t *test_ptr)
{
    struct wlr_scene *wlr_scene_ptr = wlr_scene_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_scene_ptr);
    struct wl_display *wl_display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wl_display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(wl_display_ptr);
    wlmtk_root_t *root_ptr = wlmtk_root_create(
        wlr_scene_ptr, wlr_output_layout_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, root_ptr);
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "test", &_wlmtk_workspace_test_tile_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, workspace_ptr);
    wlmtk_root_add_workspace(root_ptr, workspace_ptr);

    wlmtk_util_test_listener_t mapped, unmapped;
    wlmtk_util_connect_test_listener(
        &wlmtk_root_events(root_ptr)->window_mapped, &mapped);
    wlmtk_util_connect_test_listener(
        &wlmtk_root_events(root_ptr)->window_unmapped, &unmapped);

    bs_dllist_t *wdl_ptr = wlmtk_workspace_get_window2s_dllist(workspace_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, bs_dllist_size(wdl_ptr));

    wlmtk_window2_t *w = wlmtk_test_window2_create(NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w);

    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window2_element(w)->visible);
    wlmtk_workspace_map_window2(workspace_ptr, w);
    BS_TEST_VERIFY_NEQ(
        test_ptr, NULL, wlmtk_window2_element(w)->wlr_scene_node_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_element(w)->visible);
    BS_TEST_VERIFY_EQ(test_ptr, 1, bs_dllist_size(wdl_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, 1, mapped.calls);
    BS_TEST_VERIFY_EQ(test_ptr, w, mapped.last_data_ptr);

    wlmtk_workspace_unmap_window2(workspace_ptr, w);
    BS_TEST_VERIFY_EQ(
        test_ptr, NULL, wlmtk_window2_element(w)->wlr_scene_node_ptr);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window2_element(w)->visible);
    BS_TEST_VERIFY_EQ(test_ptr, 0, bs_dllist_size(wdl_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, 1, unmapped.calls);
    BS_TEST_VERIFY_EQ(test_ptr, w, mapped.last_data_ptr);

    wlmtk_util_disconnect_test_listener(&mapped);
    wlmtk_util_disconnect_test_listener(&unmapped);
    wlmtk_window2_destroy(w);
    wlmtk_root_remove_workspace(root_ptr, workspace_ptr);
    wlmtk_workspace_destroy(workspace_ptr);
    wlmtk_root_destroy(root_ptr);
    wl_display_destroy(wl_display_ptr);
    wlr_scene_node_destroy(&wlr_scene_ptr->tree.node);
}

/* ------------------------------------------------------------------------- */
/** Tests moving a window. */
void test_move(bs_test_t *test_ptr)
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
        wlr_output_layout_ptr, "t", &_wlmtk_workspace_test_tile_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_element_set_visible(wlmtk_workspace_element(ws_ptr), true);

    wlmtk_fake_element_t *fe_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe_ptr);
    wlmtk_fake_element_set_dimensions(fe_ptr, 10, 2);
    wlmtk_window2_t *w = wlmtk_test_window2_create(&fe_ptr->element);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w);
    wlmtk_workspace_map_window2(ws_ptr, w);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window2_element(w)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window2_element(w)->y);

    wlmtk_pointer_motion_event_t mev = { .x = 0, .y = 0 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(wlmtk_workspace_element(ws_ptr), &mev));

    // Starts a move for the window. Will move it...
    wlmtk_workspace_begin_window_move(ws_ptr, w);
    mev = (wlmtk_pointer_motion_event_t){ .x = 1, .y = 2 };
    wlmtk_element_pointer_motion(wlmtk_workspace_element(ws_ptr), &mev);
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window2_element(w)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window2_element(w)->y);

    // Releases the button. Should end the move.
    wlmtk_button_event_t button_event = {
        .button = BTN_LEFT,
        .type = WLMTK_BUTTON_UP,
        .time_msec = 44,
    };
    wlmtk_element_pointer_button(
        wlmtk_workspace_element(ws_ptr),
        &button_event);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, ws_ptr->grabbed_window_ptr);

    // More motion, no longer updates the position.
    mev = (wlmtk_pointer_motion_event_t){ .x = 3, .y = 4 };
    wlmtk_element_pointer_motion(wlmtk_workspace_element(ws_ptr), &mev);
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window2_element(w)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window2_element(w)->y);

    wlmtk_workspace_unmap_window2(ws_ptr, w);

    wlmtk_window2_destroy(w);
    wlmtk_element_destroy(&fe_ptr->element);
    wlmtk_workspace_destroy(ws_ptr);
    wl_display_destroy(display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests moving a window that unmaps during the move. */
void test_unmap_during_move(bs_test_t *test_ptr)
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
        wlr_output_layout_ptr, "t", &_wlmtk_workspace_test_tile_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_element_set_visible(wlmtk_workspace_element(ws_ptr), true);

    wlmtk_fake_element_t *fe_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe_ptr);
    wlmtk_fake_element_set_dimensions(fe_ptr, 40, 20);
    wlmtk_window2_t *w = wlmtk_test_window2_create(&fe_ptr->element);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w);
    wlmtk_workspace_map_window2(ws_ptr, w);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window2_element(w)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window2_element(w)->y);

    wlmtk_pointer_motion_event_t mev = { .x = 0, .y = 0 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(
            wlmtk_workspace_element(ws_ptr), &mev));

    // Starts a move for the window. Will move it...
    wlmtk_workspace_begin_window_move(ws_ptr, w);
    mev = (wlmtk_pointer_motion_event_t){ .x = 1, .y = 2 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(
            wlmtk_workspace_element(ws_ptr), &mev));
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window2_element(w)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window2_element(w)->y);

    wlmtk_workspace_unmap_window2(ws_ptr, w);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, ws_ptr->grabbed_window_ptr);

    // More motion, no longer updates the position.
    mev = (wlmtk_pointer_motion_event_t){ .x = 3, .y = 4 };
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_element_pointer_motion(
            wlmtk_workspace_element(ws_ptr), &mev));
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window2_element(w)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window2_element(w)->y);

    // More motion, no longer updates the position.
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_element_pointer_motion(
            wlmtk_workspace_element(ws_ptr), &mev));
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window2_element(w)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window2_element(w)->y);

    wlmtk_window2_destroy(w);
    wlmtk_element_destroy(&fe_ptr->element);
    wlmtk_workspace_destroy(ws_ptr);
    wl_display_destroy(display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests resizing a window. */
void test_resize(bs_test_t *test_ptr)
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
        wlr_output_layout_ptr, "t", &_wlmtk_workspace_test_tile_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_element_set_visible(wlmtk_workspace_element(ws_ptr), true);

    wlmtk_fake_element_t *fe_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe_ptr);
    wlmtk_fake_element_set_dimensions(fe_ptr, 40, 20);
    wlmtk_window2_t *w = wlmtk_test_window2_create(&fe_ptr->element);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w);
    wlmtk_workspace_map_window2(ws_ptr, w);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window2_element(w)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window2_element(w)->y);
    wlmtk_util_test_listener_t l;
    wlmtk_util_connect_test_listener(
        &wlmtk_window2_events(w)->request_size, &l);

    wlmtk_pointer_motion_event_t mev = { .x = 0, .y = 0 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(
            wlmtk_workspace_element(ws_ptr),  &mev));
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 40, 20,
        wlmtk_element_get_dimensions_box(wlmtk_window2_element(w)));

    // Starts a resize for the window. Must call the listener.
    wlmtk_workspace_begin_window_resize(ws_ptr, w, WLR_EDGE_TOP|WLR_EDGE_LEFT);
    mev = (wlmtk_pointer_motion_event_t){ .x = 1, .y = 2 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(
            wlmtk_workspace_element(ws_ptr), &mev));
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 40, 20,
        wlmtk_element_get_dimensions_box(wlmtk_window2_element(w)));

    // Now, apply the dimension.
    wlmtk_fake_element_set_dimensions(fe_ptr, 39, 18);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 0, 0, 39, 18,
        wlmtk_element_get_dimensions_box(wlmtk_window2_element(w)));
    int x, y;
    wlmtk_element_get_position(wlmtk_window2_element(w), &x, &y);
    BS_TEST_VERIFY_EQ(test_ptr, 1, x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, y);

    // Releases the button. Should end the move.
    wlmtk_button_event_t button_event = {
        .button = BTN_LEFT,
        .type = WLMTK_BUTTON_UP,
        .time_msec = 44,
    };
    wlmtk_element_pointer_button(
        wlmtk_workspace_element(ws_ptr),
        &button_event);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, ws_ptr->grabbed_window_ptr);

    wlmtk_workspace_unmap_window2(ws_ptr, w);
    wlmtk_window2_destroy(w);
    wlmtk_element_destroy(&fe_ptr->element);
    wlmtk_workspace_destroy(ws_ptr);
    wl_display_destroy(display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests enabling or disabling the workspace. */
void test_enable(bs_test_t *test_ptr)
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
        wlr_output_layout_ptr, "t", &_wlmtk_workspace_test_tile_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_workspace_enable(ws_ptr, false);

    // Map while disabled: Not activated.
    wlmtk_window2_t *w1 = wlmtk_test_window2_create(NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w1);
    wlmtk_workspace_map_window2(ws_ptr, w1);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window2_is_activated(w1));

    // Enable: Activates the earlier-mapped window.
    wlmtk_workspace_enable(ws_ptr, true);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_is_activated(w1));

    // Disable: De-activate the window
    wlmtk_workspace_enable(ws_ptr, false);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window2_is_activated(w1));

    // Maps another window, while de-activated: Also don't activate.
    wlmtk_window2_t *w2 = wlmtk_test_window2_create(NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w2);
    wlmtk_workspace_map_window2(ws_ptr, w2);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window2_is_activated(w2));

    // Enable: Activates the window just mapped before.
    wlmtk_workspace_enable(ws_ptr, true);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_is_activated(w2));

    // Unmaps while de-activated. Enabling after should still activate fw1.
    wlmtk_workspace_enable(ws_ptr, false);
    wlmtk_workspace_unmap_window2(ws_ptr, w2);
    wlmtk_window2_destroy(w2);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window2_is_activated(w1));
    wlmtk_workspace_enable(ws_ptr, true);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_is_activated(w1));

    wlmtk_workspace_unmap_window2(ws_ptr, w1);
    wlmtk_window2_destroy(w1);
    wlmtk_workspace_destroy(ws_ptr);
    wl_display_destroy(display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests window activation. */
void test_activate(bs_test_t *test_ptr)
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
        wlr_output_layout_ptr, "t", &_wlmtk_workspace_test_tile_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_element_set_visible(wlmtk_workspace_element(ws_ptr), true);
    wlmtk_workspace_enable(ws_ptr, true);

    // Window 1: from (0, 0) to (100, 100)
    wlmtk_fake_element_t *fe1_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe1_ptr);
    wlmtk_fake_element_set_dimensions(fe1_ptr, 100, 100);
    wlmtk_window2_t *w1 = wlmtk_test_window2_create(&fe1_ptr->element);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w1);
    wlmtk_workspace_set_window_position(ws_ptr, w1, 0, 0);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window2_is_activated(w1));

    // Window 1 is mapped => it's activated.
    wlmtk_workspace_map_window2(ws_ptr, w1);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_is_activated(w1));

    // Window 2: from (200, 0) to (300, 100).
    // Window 2 is mapped: Will get activated, and 1st one de-activated.
    wlmtk_fake_element_t *fe2_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe2_ptr);
    wlmtk_fake_element_set_dimensions(fe2_ptr, 100, 100);
    wlmtk_window2_t *w2 = wlmtk_test_window2_create(&fe2_ptr->element);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w2);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window2_is_activated(w2));
    wlmtk_workspace_map_window2(ws_ptr, w2);
    wlmtk_workspace_set_window_position(ws_ptr, w2, 200, 0);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window2_is_activated(w1));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_is_activated(w2));

    // Pointer move, over window 1. Nothing happens: We have click-to-focus.
    wlmtk_pointer_motion_event_t mev = { .x = 50, .y = 50 };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(wlmtk_workspace_element(ws_ptr), &mev));
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window2_is_activated(w1));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_is_activated(w2));

    // Click on window 1: Gets activated.
    wlmtk_button_event_t bev = {
        .button = BTN_RIGHT,
        .type = WLMTK_BUTTON_DOWN,
    };

    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(wlmtk_workspace_element(ws_ptr), &bev));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_is_activated(w1));
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window2_is_activated(w2));

    // Unmap window 1. Now window 2 gets activated.
    wlmtk_workspace_unmap_window2(ws_ptr, w1);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window2_is_activated(w1));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_is_activated(w2));

    // Unmap the remaining window 2. Nothing is activated.
    wlmtk_workspace_unmap_window2(ws_ptr, w2);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window2_is_activated(w2));

    wlmtk_window2_destroy(w2);
    wlmtk_element_destroy(&fe2_ptr->element);
    wlmtk_window2_destroy(w1);
    wlmtk_element_destroy(&fe1_ptr->element);
    wlmtk_workspace_destroy(ws_ptr);
    wl_display_destroy(display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests cycling through windows. */
void test_activate_cycling(bs_test_t *test_ptr)
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
        wlr_output_layout_ptr, "t", &_wlmtk_workspace_test_tile_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_workspace_enable(ws_ptr, true);
    bs_dllist_t *window2s_ptr = wlmtk_workspace_get_window2s_dllist(
        ws_ptr);

    // Window 1 gets mapped: Activated and on top.
    wlmtk_window2_t *w1 = wlmtk_test_window2_create(NULL);
    wlmtk_workspace_map_window2(ws_ptr, w1);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_is_activated(w1));
    BS_TEST_VERIFY_EQ(
        test_ptr, wlmtk_dlnode_from_window2(w1), window2s_ptr->head_ptr);

    // Window 2 gets mapped: Activated and on top.
    wlmtk_window2_t *w2 = wlmtk_test_window2_create(NULL);
    wlmtk_workspace_map_window2(ws_ptr, w2);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_is_activated(w2));
    BS_TEST_VERIFY_EQ(
        test_ptr, wlmtk_dlnode_from_window2(w2), window2s_ptr->head_ptr);

    // Window 3 gets mapped: Activated and on top.
    wlmtk_window2_t *w3 = wlmtk_test_window2_create(NULL);
    wlmtk_workspace_map_window2(ws_ptr, w3);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_is_activated(w3));
    BS_TEST_VERIFY_EQ(
        test_ptr, wlmtk_dlnode_from_window2(w3), window2s_ptr->head_ptr);

    // From mapping sequence: We have 3 -> 2 -> 1. Cycling brings us to
    // window 2, but must not change the top window.
    wlmtk_workspace_activate_next_window(ws_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_is_activated(w2));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_window2(w3),
        window2s_ptr->head_ptr);

    // One more cycle: 1.
    wlmtk_workspace_activate_next_window(ws_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_is_activated(w1));
    BS_TEST_VERIFY_EQ(
        test_ptr, wlmtk_dlnode_from_window2(w3), window2s_ptr->head_ptr);

    // One more cycle: Back at 3.
    wlmtk_workspace_activate_next_window(ws_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_is_activated(w3));
    BS_TEST_VERIFY_EQ(
        test_ptr, wlmtk_dlnode_from_window2(w3), window2s_ptr->head_ptr);

    // Cycle backward: Gets us to 1.
    wlmtk_workspace_activate_previous_window(ws_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_is_activated(w1));
    BS_TEST_VERIFY_EQ(
        test_ptr, wlmtk_dlnode_from_window2(w3), window2s_ptr->head_ptr);

    // Raise: Must come to top.
    wlmtk_workspace_raise_window(ws_ptr, w1);
    BS_TEST_VERIFY_EQ(
        test_ptr, wlmtk_dlnode_from_window2(w1), window2s_ptr->head_ptr);

    wlmtk_workspace_unmap_window2(ws_ptr, w3);
    wlmtk_workspace_unmap_window2(ws_ptr, w2);
    wlmtk_workspace_unmap_window2(ws_ptr, w1);
    wlmtk_window2_destroy(w3);
    wlmtk_window2_destroy(w2);
    wlmtk_window2_destroy(w1);
    wlmtk_workspace_destroy(ws_ptr);
    wl_display_destroy(display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests extents with multiple outputs. */
void test_multi_output_extents(bs_test_t *test_ptr)
{
    struct wlr_box result;
    struct wl_display *display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(display_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_output_layout_ptr);

    wlmtk_workspace_t *ws_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "t", &_wlmtk_workspace_test_tile_style);

    struct wlr_output o1 = { .width = 100, .height = 200, .scale = 1 };
    wlmtk_test_wlr_output_init(&o1);
    struct wlr_output o2 = { .width = 300, .height = 250, .scale = 1 };
    wlmtk_test_wlr_output_init(&o2);

    // (1): Get extents without any output. Must be empty.
    result = wlmtk_workspace_get_maximize_extents(ws_ptr, NULL);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 0, 0, 0, 0, result);
    result = wlmtk_workspace_get_maximize_extents(ws_ptr, &o1);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 0, 0, 0, 0, result);

    result = wlmtk_workspace_get_fullscreen_extents(ws_ptr, NULL);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 0, 0, 0, 0, result);
    result = wlmtk_workspace_get_fullscreen_extents(ws_ptr, &o1);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 0, 0, 0, 0, result);

    // (2): Add one output. Return extents on NULL or for &o1.
    wlr_output_layout_add(wlr_output_layout_ptr, &o1, -10, -20);
    result = wlmtk_workspace_get_maximize_extents(ws_ptr, NULL);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, -10, -20, 36, 136, result);
    result = wlmtk_workspace_get_maximize_extents(ws_ptr, &o1);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, -10, -20, 36, 136, result);
    result = wlmtk_workspace_get_maximize_extents(ws_ptr, &o2);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 0, 0, 0, 0, result);

    result = wlmtk_workspace_get_fullscreen_extents(ws_ptr, NULL);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, -10, -20, 100, 200, result);
    result = wlmtk_workspace_get_fullscreen_extents(ws_ptr, &o1);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, -10, -20, 100, 200, result);
    result = wlmtk_workspace_get_fullscreen_extents(ws_ptr, &o2);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 0, 0, 0, 0, result);

    // (3): Add second output. Must return extents on all.
    wlr_output_layout_add(wlr_output_layout_ptr, &o2, 400, 0);
    result = wlmtk_workspace_get_maximize_extents(ws_ptr, NULL);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, -10, -20, 36, 136, result);
    result = wlmtk_workspace_get_maximize_extents(ws_ptr, &o1);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, -10, -20, 36, 136, result);
    result = wlmtk_workspace_get_maximize_extents(ws_ptr, &o2);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 400, 0, 236, 186, result);

    result = wlmtk_workspace_get_fullscreen_extents(ws_ptr, NULL);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, -10, -20, 100, 200, result);
    result = wlmtk_workspace_get_fullscreen_extents(ws_ptr, &o1);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, -10, -20, 100, 200, result);
    result = wlmtk_workspace_get_fullscreen_extents(ws_ptr, &o2);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 400, 0, 300, 250, result);

    // (4): Remove first output. Must now default to 2nd output.
    wlr_output_layout_remove(wlr_output_layout_ptr, &o1);
    result = wlmtk_workspace_get_maximize_extents(ws_ptr, NULL);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 400, 0, 236, 186, result);
    result = wlmtk_workspace_get_maximize_extents(ws_ptr, &o1);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 0, 0, 0, 0, result);
    result = wlmtk_workspace_get_maximize_extents(ws_ptr, &o2);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 400, 0, 236, 186, result);

    result = wlmtk_workspace_get_fullscreen_extents(ws_ptr, NULL);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 400, 0, 300, 250, result);
    result = wlmtk_workspace_get_fullscreen_extents(ws_ptr, &o1);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 0, 0, 0, 0, result);
    result = wlmtk_workspace_get_fullscreen_extents(ws_ptr, &o2);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 400, 0, 300, 250, result);

    wlmtk_workspace_destroy(ws_ptr);
    wlr_output_layout_destroy(wlr_output_layout_ptr);
    wl_display_destroy(display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Verifies that windows are re-positioned when output is removed. */
void test_multi_output_reposition(bs_test_t *test_ptr)
{
    struct wl_display *display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(display_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_output_layout_ptr);

    wlmtk_workspace_t *ws_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "t", &_wlmtk_workspace_test_tile_style);

    struct wlr_output o1 = { .width = 100, .height = 200, .scale = 1 };
    wlmtk_test_wlr_output_init(&o1);
    wlr_output_layout_add(wlr_output_layout_ptr, &o1, -10, -20);
    struct wlr_output o2 = { .width = 300, .height = 250, .scale = 1 };
    wlmtk_test_wlr_output_init(&o2);
    wlr_output_layout_add(wlr_output_layout_ptr, &o2, 400, 0);

    // A fullscreen window w1, on o1.
    wlmtk_fake_element_t *fe1_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe1_ptr);
    wlmtk_window2_t *w1 = wlmtk_test_window2_create(&fe1_ptr->element);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w1);
    wlmtk_workspace_map_window2(ws_ptr, w1);
    wlmtk_window2_commit_fullscreen(w1, true);
    wlmtk_fake_element_set_dimensions(fe1_ptr, 100, 200);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, -10, -20, 100, 200,
        wlmtk_window2_get_bounding_box(w1));

    wlmtk_util_test_wlr_box_listener_t l1 = {};
    wlmtk_util_connect_test_wlr_box_listener(
        &wlmtk_window2_events(w1)->request_size, &l1);

    // A normal window w2, on o1.
    wlmtk_fake_element_t *fe2_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe2_ptr);
    wlmtk_fake_element_set_dimensions(fe2_ptr, 30, 40);
    wlmtk_window2_t *w2 = wlmtk_test_window2_create(&fe2_ptr->element);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w2);
    wlmtk_workspace_map_window2(ws_ptr, w2);
    wlmtk_workspace_set_window_position(ws_ptr, w2, 10, 20);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 10, 20, 30, 40,
        wlmtk_window2_get_bounding_box(w2));

    // A maximized window w3, on o1.
    wlmtk_fake_element_t *fe3_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe2_ptr);
    wlmtk_window2_t *w3 = wlmtk_test_window2_create(&fe3_ptr->element);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w3);
    wlmtk_workspace_map_window2(ws_ptr, w3);
    wlmtk_window2_commit_maximized(w3, true);
    wlmtk_fake_element_set_dimensions(fe3_ptr, 100, 200);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(  // FIXME: but.. why?
        test_ptr, -10, -20, 100, 200,
        wlmtk_window2_get_bounding_box(w3));
    wlmtk_util_test_wlr_box_listener_t l3 = {};
    wlmtk_util_connect_test_wlr_box_listener(
        &wlmtk_window2_events(w3)->request_size, &l3);

    // Remove o1.
    wlr_output_layout_remove(wlr_output_layout_ptr, &o1);

    // The fullscreen window must have received a `request_size` call. Verify,
    // then let element take that dimension
    BS_TEST_VERIFY_EQ(test_ptr, 1, l1.calls);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 400, 0, 300, 250, l1.box);
    wlmtk_fake_element_set_dimensions(fe1_ptr, l1.box.width, l1.box.height);

    // Also, the maximimzed window must have receied a request_size call; now
    // suitable for o2. And be placed there.
    BS_TEST_VERIFY_EQ(test_ptr, 1, l3.calls);
    WLMTK_TEST_VERIFY_WLRBOX_EQ(test_ptr, 400, 0, 236, 186, l3.box);
    wlmtk_fake_element_set_dimensions(fe3_ptr, l3.box.width, l3.box.height);

    // Now both windows must be on o2. w1 must have a new size.
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 400, 0, 300, 250,
        wlmtk_window2_get_bounding_box(w1));
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 400, 0, 30, 40,
        wlmtk_window2_get_bounding_box(w2));
    WLMTK_TEST_VERIFY_WLRBOX_EQ(
        test_ptr, 400, 0, 236, 186,
        wlmtk_window2_get_bounding_box(w3));

    wlmtk_workspace_unmap_window2(ws_ptr, w3);
    wlmtk_window2_destroy(w3);
    wlmtk_element_destroy(&fe3_ptr->element);
    wlmtk_workspace_unmap_window2(ws_ptr, w2);
    wlmtk_window2_destroy(w2);
    wlmtk_element_destroy(&fe2_ptr->element);
    wlmtk_workspace_unmap_window2(ws_ptr, w1);
    wlmtk_window2_destroy(w1);
    wlmtk_element_destroy(&fe1_ptr->element);
    wlmtk_workspace_destroy(ws_ptr);
    wlr_output_layout_destroy(wlr_output_layout_ptr);
    wl_display_destroy(display_ptr);
}
/* == End of workspace.c =================================================== */
