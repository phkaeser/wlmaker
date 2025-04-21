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

#include <toolkit/fsm.h>
#include <toolkit/layer.h>
#include <toolkit/workspace.h>
#include <toolkit/test.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/edges.h>
#undef WLR_USE_UNSTABLE

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

    /** List of toplevel windows. Via @ref wlmtk_window_t::dlnode. */
    bs_dllist_t               windows;

    /** The activated window. */
    wlmtk_window_t            *activated_window_ptr;
    /** The most recent activated window, if none is activated now. */
    wlmtk_window_t            *formerly_activated_window_ptr;

    /** The grabbed window. */
    wlmtk_window_t            *grabbed_window_ptr;
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
static void _wlmtk_workspace_element_get_pointer_area(
    wlmtk_element_t *element_ptr,
    int *x1_ptr,
    int *y1_ptr,
    int *x2_ptr,
    int *y2_ptr);
static bool _wlmtk_workspace_element_pointer_motion(
    wlmtk_element_t *element_ptr,
    double x, double y,
    uint32_t time_msec);
static bool _wlmtk_workspace_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);
static void _wlmtk_workspace_element_pointer_leave(
    wlmtk_element_t *element_ptr);

static void _wlmtk_workspace_handle_output_layout_change(
    struct wl_listener *listener_ptr,
    void *data_ptr);

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
    .get_pointer_area = _wlmtk_workspace_element_get_pointer_area,
    .pointer_motion = _wlmtk_workspace_element_pointer_motion,
    .pointer_button = _wlmtk_workspace_element_pointer_button,
    .pointer_leave = _wlmtk_workspace_element_pointer_leave,
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
    const wlmtk_tile_style_t *tile_style_ptr,
    wlmtk_env_t *env_ptr)
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

    if (!wlmtk_container_init(&workspace_ptr->super_container, env_ptr)) {
        wlmtk_workspace_destroy(workspace_ptr);
        return NULL;
    }
    workspace_ptr->orig_super_element_vmt = wlmtk_element_extend(
        &workspace_ptr->super_container.super_element,
        &workspace_element_vmt);

    if (!wlmtk_container_init(&workspace_ptr->window_container, env_ptr)) {
        wlmtk_workspace_destroy(workspace_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        &workspace_ptr->window_container.super_element,
        true);
    wlmtk_container_add_element(
        &workspace_ptr->super_container,
        &workspace_ptr->window_container.super_element);

    if (!wlmtk_container_init(&workspace_ptr->fullscreen_container, env_ptr)) {
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
        wlr_output_layout_ptr, env_ptr);
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
        wlr_output_layout_ptr, env_ptr);
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
        wlr_output_layout_ptr, env_ptr);
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
        wlr_output_layout_ptr, env_ptr);
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
    wlmtk_window_t *window_ptr)
{
    // Only act if the window belongs to this workspace.
    if (workspace_ptr != wlmtk_window_get_workspace(window_ptr)) return;

    struct wlr_box box = {
        .x = workspace_ptr->x1,
        .y = workspace_ptr->y1,
        .width = workspace_ptr->x2 - workspace_ptr->x1,
        .height = workspace_ptr->y2 - workspace_ptr->y1 };

    struct wlr_box elem_box = wlmtk_element_get_dimensions_box(
        wlmtk_window_element(window_ptr));
    int x, y;
    wlmtk_element_get_position(wlmtk_window_element(window_ptr), &x, &y);


    int max_x = x - elem_box.x + elem_box.width;
    if (max_x > box.width) x -= max_x - box.width;

    int max_y = y - elem_box.y + elem_box.height;
    if (max_y > box.height) y -= max_y - box.height;

    wlmtk_element_set_position(wlmtk_window_element(window_ptr), x, y);
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
void wlmtk_workspace_map_window(wlmtk_workspace_t *workspace_ptr,
                                wlmtk_window_t *window_ptr)
{
    BS_ASSERT(NULL == wlmtk_window_get_workspace(window_ptr));

    wlmtk_element_set_visible(wlmtk_window_element(window_ptr), true);
    wlmtk_container_add_element(
        &workspace_ptr->window_container,
        wlmtk_window_element(window_ptr));
    bs_dllist_push_front(&workspace_ptr->windows,
                         wlmtk_dlnode_from_window(window_ptr));
    wlmtk_window_set_workspace(window_ptr, workspace_ptr);

    wlmtk_workspace_activate_window(workspace_ptr, window_ptr);

    if (NULL != workspace_ptr->root_ptr) {
        wl_signal_emit(
            &wlmtk_root_events(workspace_ptr->root_ptr)->window_mapped,
            window_ptr);
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_unmap_window(wlmtk_workspace_t *workspace_ptr,
                                  wlmtk_window_t *window_ptr)
{
    bool need_activation = false;

    BS_ASSERT(workspace_ptr == wlmtk_window_get_workspace(window_ptr));

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

    wlmtk_element_set_visible(wlmtk_window_element(window_ptr), false);

    if (wlmtk_window_is_fullscreen(window_ptr)) {
        wlmtk_container_remove_element(
            &workspace_ptr->fullscreen_container,
            wlmtk_window_element(window_ptr));
    } else {
        wlmtk_container_remove_element(
            &workspace_ptr->window_container,
            wlmtk_window_element(window_ptr));
    }
    bs_dllist_remove(&workspace_ptr->windows,
                     wlmtk_dlnode_from_window(window_ptr));
    wlmtk_window_set_workspace(window_ptr, NULL);
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
            wlmtk_window_t *window_ptr = wlmtk_window_from_element(element_ptr);
            wlmtk_workspace_activate_window(workspace_ptr, window_ptr);
        }
    }
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
bs_dllist_t *wlmtk_workspace_get_windows_dllist(
    wlmtk_workspace_t *workspace_ptr)
{
    return &workspace_ptr->windows;
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_window_to_fullscreen(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_window_t *window_ptr,
    bool fullscreen)
{
    BS_ASSERT(workspace_ptr == wlmtk_window_get_workspace(window_ptr));
    BS_ASSERT(fullscreen == wlmtk_window_is_fullscreen(window_ptr));

    if (fullscreen) {
        BS_ASSERT(
            bs_dllist_contains(
                &workspace_ptr->window_container.elements,
                wlmtk_dlnode_from_element(wlmtk_window_element(window_ptr))));

        // TODO(kaeser@gubbe.ch): Add a method to just reparent an element. The
        // current implementation will destroy, then re-create each of the
        // scene nodes.
        wlmtk_container_remove_element(
            &workspace_ptr->window_container,
            wlmtk_window_element(window_ptr));
        wlmtk_container_add_element(
            &workspace_ptr->fullscreen_container,
            wlmtk_window_element(window_ptr));
        wlmtk_workspace_activate_window(workspace_ptr, window_ptr);
    } else {
        BS_ASSERT(
            bs_dllist_contains(
                &workspace_ptr->fullscreen_container.elements,
                wlmtk_dlnode_from_element(wlmtk_window_element(window_ptr))));

        wlmtk_container_remove_element(
            &workspace_ptr->fullscreen_container,
            wlmtk_window_element(window_ptr));
        wlmtk_container_add_element(
            &workspace_ptr->window_container,
            wlmtk_window_element(window_ptr));
        wlmtk_workspace_activate_window(workspace_ptr, window_ptr);

        // The un-fullscreened window will come on top of the container. Also
        // reflect that in @ref wlmtk_workspace_t::windows.
        bs_dllist_remove(&workspace_ptr->windows,
                         wlmtk_dlnode_from_window(window_ptr));
        bs_dllist_push_front(&workspace_ptr->windows,
                             wlmtk_dlnode_from_window(window_ptr));
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_begin_window_move(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_window_t *window_ptr)
{
    wlmtk_fsm_event(&workspace_ptr->fsm, PFSME_BEGIN_MOVE, window_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_begin_window_resize(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_window_t *window_ptr,
    uint32_t edges)
{
    workspace_ptr->resize_edges = edges;
    wlmtk_fsm_event(&workspace_ptr->fsm, PFSME_BEGIN_RESIZE, window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Acticates `window_ptr`. Will de-activate an earlier window. */
void wlmtk_workspace_activate_window(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_window_t *window_ptr)
{
    // Nothing to do.
    if (workspace_ptr->activated_window_ptr == window_ptr) return;

    if (NULL != workspace_ptr->activated_window_ptr) {
        wlmtk_window_t *w_ptr = workspace_ptr->activated_window_ptr;
        workspace_ptr->formerly_activated_window_ptr =
            workspace_ptr->activated_window_ptr;
        workspace_ptr->activated_window_ptr = NULL;
        wlmtk_window_set_activated(w_ptr, false);
    }

    if (NULL != window_ptr) {
        if (workspace_ptr->enabled) {
            workspace_ptr->activated_window_ptr = window_ptr;
            wlmtk_window_set_activated(window_ptr, true);
        }
        workspace_ptr->formerly_activated_window_ptr = window_ptr;
    }
}

/* ------------------------------------------------------------------------- */
wlmtk_window_t *wlmtk_workspace_get_activated_window(
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
        dlnode_ptr = wlmtk_dlnode_from_window(
            workspace_ptr->activated_window_ptr);
        dlnode_ptr = dlnode_ptr->prev_ptr;
    }
    if (NULL == dlnode_ptr) {
        dlnode_ptr = workspace_ptr->windows.tail_ptr;
    }

    if (NULL == dlnode_ptr) return;

    wlmtk_workspace_activate_window(
        workspace_ptr, wlmtk_window_from_dlnode(dlnode_ptr));
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_activate_next_window(
    wlmtk_workspace_t *workspace_ptr)
{
    bs_dllist_node_t *dlnode_ptr = NULL;
    if (NULL != workspace_ptr->activated_window_ptr) {
        dlnode_ptr = wlmtk_dlnode_from_window(
            workspace_ptr->activated_window_ptr);
        dlnode_ptr = dlnode_ptr->next_ptr;
    }
    if (NULL == dlnode_ptr) {
        dlnode_ptr = workspace_ptr->windows.head_ptr;
    }

    if (NULL == dlnode_ptr) return;

    wlmtk_workspace_activate_window(
        workspace_ptr, wlmtk_window_from_dlnode(dlnode_ptr));
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_raise_window(
    wlmtk_workspace_t *workspace_ptr,
    wlmtk_window_t *window_ptr)
{
    BS_ASSERT(workspace_ptr == wlmtk_window_get_workspace(window_ptr));
    bs_dllist_remove(&workspace_ptr->windows,
                     wlmtk_dlnode_from_window(window_ptr));
    bs_dllist_push_front(&workspace_ptr->windows,
                         wlmtk_dlnode_from_window(window_ptr));
    wlmtk_container_raise_element_to_top(&workspace_ptr->window_container,
                                         wlmtk_window_element(window_ptr));
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
/** Returns workspace area: @ref _wlmtk_workspace_element_get_dimensions. */
void _wlmtk_workspace_element_get_pointer_area(
    wlmtk_element_t *element_ptr,
    int *x1_ptr,
    int *y1_ptr,
    int *x2_ptr,
    int *y2_ptr)
{
    return _wlmtk_workspace_element_get_dimensions(
        element_ptr, x1_ptr, y1_ptr, x2_ptr, y2_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Extends wlmtk_container_t::pointer_button: Feeds the motion into the
 * workspace's pointer state machine, and only passes it to the container's
 * handler if the event isn't consumed yet.
 *
 * @param element_ptr
 * @param x
 * @param y
 * @param time_msec
 *
 * @return Always true.
 */
bool _wlmtk_workspace_element_pointer_motion(
    wlmtk_element_t *element_ptr,
    double x, double y,
    uint32_t time_msec)
{
    wlmtk_workspace_t *workspace_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_workspace_t, super_container.super_element);

    // Note: Workspace ignores the return value. All motion events are whitin.
    bool rv = workspace_ptr->orig_super_element_vmt.pointer_motion(
        element_ptr, x, y, time_msec);
    wlmtk_fsm_event(&workspace_ptr->fsm, PFSME_MOTION, NULL);

    // Focus wasn't claimed, so we'll set the cursor here.
    if (!rv) {
        wlmtk_env_set_cursor(
            workspace_ptr->super_container.super_element.env_ptr,
            WLMTK_CURSOR_DEFAULT);
    }

    return true;
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
/**
 * Extends wlmtk_container_t::leave.
 *
 * @param element_ptr
 */
void _wlmtk_workspace_element_pointer_leave(
    wlmtk_element_t *element_ptr)
{
    wlmtk_workspace_t *workspace_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_workspace_t, super_container.super_element);
    wlmtk_fsm_event(&workspace_ptr->fsm, PFSME_RESET, NULL);
    workspace_ptr->orig_super_element_vmt.pointer_leave(element_ptr);
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
}

/* ------------------------------------------------------------------------- */
/** Initiates a move. */
bool pfsm_move_begin(wlmtk_fsm_t *fsm_ptr, void *ud_ptr)
{
    wlmtk_workspace_t *workspace_ptr = BS_CONTAINER_OF(
        fsm_ptr, wlmtk_workspace_t, fsm);

    workspace_ptr->grabbed_window_ptr = ud_ptr;
    workspace_ptr->motion_x =
        workspace_ptr->super_container.super_element.last_pointer_x;
    workspace_ptr->motion_y =
        workspace_ptr->super_container.super_element.last_pointer_y;

    wlmtk_element_get_position(
        wlmtk_window_element(workspace_ptr->grabbed_window_ptr),
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

    double rel_x = workspace_ptr->super_container.super_element.last_pointer_x -
        workspace_ptr->motion_x;
    double rel_y = workspace_ptr->super_container.super_element.last_pointer_y -
        workspace_ptr->motion_y;

    wlmtk_window_set_position(
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
        workspace_ptr->super_container.super_element.last_pointer_x;
    workspace_ptr->motion_y =
        workspace_ptr->super_container.super_element.last_pointer_y;

    wlmtk_element_get_position(
        wlmtk_window_element(workspace_ptr->grabbed_window_ptr),
        &workspace_ptr->initial_x,
        &workspace_ptr->initial_y);

    wlmtk_window_get_size(
        workspace_ptr->grabbed_window_ptr,
        &workspace_ptr->initial_width,
        &workspace_ptr->initial_height);

    return true;
}

/* ------------------------------------------------------------------------- */
/** Handles motion during a resize. */
bool pfsm_resize_motion(wlmtk_fsm_t *fsm_ptr, __UNUSED__ void *ud_ptr)
{
    wlmtk_workspace_t *workspace_ptr = BS_CONTAINER_OF(
        fsm_ptr, wlmtk_workspace_t, fsm);

    double rel_x = workspace_ptr->super_container.super_element.last_pointer_x -
        workspace_ptr->motion_x;
    double rel_y = workspace_ptr->super_container.super_element.last_pointer_y -
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

    wlmtk_window_request_position_and_size(
        workspace_ptr->grabbed_window_ptr,
        left, top,
        right - left, bottom - top);
    return true;
}

/* ------------------------------------------------------------------------- */
/** Resets the state machine. */
bool pfsm_reset(wlmtk_fsm_t *fsm_ptr, __UNUSED__ void *ud_ptr)
{
    wlmtk_workspace_t *workspace_ptr = BS_CONTAINER_OF(
        fsm_ptr, wlmtk_workspace_t, fsm);
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
        wlr_output_layout_ptr, "t", &_wlmtk_workspace_test_tile_style, NULL);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, workspace_ptr);

    struct wlr_box box = { .x = -10, .y = -20, .width = 100, .height = 200 };
    workspace_ptr->x1 = -10;
    workspace_ptr->y1 = -20;
    workspace_ptr->x2 = 90;
    workspace_ptr->y2 = 180;

    int x1, y1, x2, y2;
    wlmtk_element_get_pointer_area(
        &workspace_ptr->super_container.super_element, &x1, &y1, &x2, &y2);
    BS_TEST_VERIFY_EQ(test_ptr, -10, x1);
    BS_TEST_VERIFY_EQ(test_ptr, -20, y1);
    BS_TEST_VERIFY_EQ(test_ptr, 90, x2);
    BS_TEST_VERIFY_EQ(test_ptr, 180, y2);

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
        wlr_scene_ptr, wlr_output_layout_ptr, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, root_ptr);
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "test", &_wlmtk_workspace_test_tile_style, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, workspace_ptr);
    wlmtk_root_add_workspace(root_ptr, workspace_ptr);

    wlmtk_util_test_listener_t mapped, unmapped;
    wlmtk_util_connect_test_listener(
        &wlmtk_root_events(root_ptr)->window_mapped, &mapped);
    wlmtk_util_connect_test_listener(
        &wlmtk_root_events(root_ptr)->window_unmapped, &unmapped);

    bs_dllist_t *wdl_ptr = wlmtk_workspace_get_windows_dllist(workspace_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, bs_dllist_size(wdl_ptr));

    wlmtk_fake_window_t *fw_ptr = wlmtk_fake_window_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fw_ptr);

    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window_element(fw_ptr->window_ptr)->visible);
    wlmtk_workspace_map_window(workspace_ptr, fw_ptr->window_ptr);
    BS_TEST_VERIFY_NEQ(
        test_ptr,
        NULL,
        wlmtk_window_element(fw_ptr->window_ptr)->wlr_scene_node_ptr);
    BS_TEST_VERIFY_NEQ(
        test_ptr,
        NULL,
        fw_ptr->fake_surface_ptr->surface.super_element.wlr_scene_node_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window_element(fw_ptr->window_ptr)->visible);
    BS_TEST_VERIFY_EQ(test_ptr, 1, bs_dllist_size(wdl_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, 1, mapped.calls);

    wlmtk_workspace_unmap_window(workspace_ptr, fw_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        wlmtk_window_element(fw_ptr->window_ptr)->wlr_scene_node_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        fw_ptr->fake_surface_ptr->surface.super_element.wlr_scene_node_ptr);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window_element(fw_ptr->window_ptr)->visible);
    BS_TEST_VERIFY_EQ(test_ptr, 0, bs_dllist_size(wdl_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, 1, unmapped.calls);

    wlmtk_util_disconnect_test_listener(&mapped);
    wlmtk_util_disconnect_test_listener(&unmapped);
    wlmtk_fake_window_destroy(fw_ptr);
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
        wlr_output_layout_ptr, "t", &_wlmtk_workspace_test_tile_style, NULL);

    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_fake_window_t *fw_ptr = wlmtk_fake_window_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fw_ptr);

    wlmtk_element_pointer_motion(
        wlmtk_workspace_element(ws_ptr), 0, 0, 42);

    wlmtk_workspace_map_window(ws_ptr, fw_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(fw_ptr->window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(fw_ptr->window_ptr)->y);

    // Starts a move for the window. Will move it...
    wlmtk_workspace_begin_window_move(ws_ptr, fw_ptr->window_ptr);
    wlmtk_element_pointer_motion(
        wlmtk_workspace_element(ws_ptr), 1, 2, 43);
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window_element(fw_ptr->window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window_element(fw_ptr->window_ptr)->y);

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
    wlmtk_element_pointer_motion(
        wlmtk_workspace_element(ws_ptr), 3, 4, 45);
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window_element(fw_ptr->window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window_element(fw_ptr->window_ptr)->y);

    wlmtk_workspace_unmap_window(ws_ptr, fw_ptr->window_ptr);

    wlmtk_fake_window_destroy(fw_ptr);
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
        wlr_output_layout_ptr, "t", &_wlmtk_workspace_test_tile_style, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_fake_window_t *fw_ptr = wlmtk_fake_window_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fw_ptr);

    wlmtk_element_pointer_motion(
        wlmtk_workspace_element(ws_ptr), 0, 0, 42);

    wlmtk_workspace_map_window(ws_ptr, fw_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(fw_ptr->window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(fw_ptr->window_ptr)->y);

    // Starts a move for the window. Will move it...
    wlmtk_workspace_begin_window_move(ws_ptr, fw_ptr->window_ptr);
    wlmtk_element_pointer_motion(
        wlmtk_workspace_element(ws_ptr), 1, 2, 43);
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window_element(fw_ptr->window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window_element(fw_ptr->window_ptr)->y);

    wlmtk_workspace_unmap_window(ws_ptr, fw_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, ws_ptr->grabbed_window_ptr);

    // More motion, no longer updates the position.
    wlmtk_element_pointer_motion(
        wlmtk_workspace_element(ws_ptr), 3, 4, 45);
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window_element(fw_ptr->window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window_element(fw_ptr->window_ptr)->y);


    // More motion, no longer updates the position.
    wlmtk_element_pointer_motion(
        wlmtk_workspace_element(ws_ptr), 3, 4, 45);
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window_element(fw_ptr->window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window_element(fw_ptr->window_ptr)->y);

    wlmtk_fake_window_destroy(fw_ptr);
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
        wlr_output_layout_ptr, "t", &_wlmtk_workspace_test_tile_style, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);

    wlmtk_fake_window_t *fw_ptr = wlmtk_fake_window_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fw_ptr);
    wlmtk_window_request_position_and_size(fw_ptr->window_ptr, 0, 0, 40, 20);
    wlmtk_fake_window_commit_size(fw_ptr);

    wlmtk_element_pointer_motion(wlmtk_workspace_element(ws_ptr),  0, 0, 42);

    wlmtk_workspace_map_window(ws_ptr, fw_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(fw_ptr->window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(fw_ptr->window_ptr)->y);
    int width, height;
    wlmtk_window_get_size(fw_ptr->window_ptr, &width, &height);
    BS_TEST_VERIFY_EQ(test_ptr, 40, width);
    BS_TEST_VERIFY_EQ(test_ptr, 20, height);

    // Starts a resize for the window. Will resize & move it...
    wlmtk_workspace_begin_window_resize(
        ws_ptr, fw_ptr->window_ptr, WLR_EDGE_TOP | WLR_EDGE_LEFT);
    fw_ptr->fake_content_ptr->serial = 1;  // The serial.
    wlmtk_element_pointer_motion(wlmtk_workspace_element(ws_ptr), 1, 2, 44);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(fw_ptr->window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(fw_ptr->window_ptr)->y);
    BS_TEST_VERIFY_EQ(test_ptr, 39, fw_ptr->fake_content_ptr->requested_width);
    BS_TEST_VERIFY_EQ(test_ptr, 18, fw_ptr->fake_content_ptr->requested_height);
    // This updates for the given serial.
    wlmtk_fake_window_commit_size(fw_ptr);
    wlmtk_window_get_size(fw_ptr->window_ptr, &width, &height);
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window_element(fw_ptr->window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window_element(fw_ptr->window_ptr)->y);
    BS_TEST_VERIFY_EQ(test_ptr, 39, width);
    BS_TEST_VERIFY_EQ(test_ptr, 18, height);

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

    wlmtk_workspace_unmap_window(ws_ptr, fw_ptr->window_ptr);
    wlmtk_fake_window_destroy(fw_ptr);
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
        wlr_output_layout_ptr, "t", &_wlmtk_workspace_test_tile_style, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_workspace_enable(ws_ptr, false);

    // Map while disabled: Not activated.
    wlmtk_fake_window_t *fw1_ptr = wlmtk_fake_window_create();
    wlmtk_workspace_map_window(ws_ptr, fw1_ptr->window_ptr);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_window_is_activated(fw1_ptr->window_ptr));

    // Enable: Activates the earlier-mapped window.
    wlmtk_workspace_enable(ws_ptr, true);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw1_ptr->window_ptr));

    // Disable: De-activate the window
    wlmtk_workspace_enable(ws_ptr, false);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_window_is_activated(fw1_ptr->window_ptr));

    // Maps another window, while de-activated: Also don't activate.
    wlmtk_fake_window_t *fw2_ptr = wlmtk_fake_window_create();
    wlmtk_workspace_map_window(ws_ptr, fw2_ptr->window_ptr);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_window_is_activated(fw2_ptr->window_ptr));

    // Enable: Activates the window just mapped before.
    wlmtk_workspace_enable(ws_ptr, true);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw2_ptr->window_ptr));

    // Unmaps while de-activated. Enabling after should still activate fw1.
    wlmtk_workspace_enable(ws_ptr, false);
    wlmtk_workspace_unmap_window(ws_ptr, fw2_ptr->window_ptr);
    wlmtk_fake_window_destroy(fw2_ptr);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_window_is_activated(fw1_ptr->window_ptr));
    wlmtk_workspace_enable(ws_ptr, true);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw1_ptr->window_ptr));

    wlmtk_workspace_unmap_window(ws_ptr, fw1_ptr->window_ptr);
    wlmtk_fake_window_destroy(fw1_ptr);
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
        wlr_output_layout_ptr, "t", &_wlmtk_workspace_test_tile_style, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_workspace_enable(ws_ptr, true);

    // Window 1: from (0, 0) to (100, 100)
    wlmtk_fake_window_t *fw1_ptr = wlmtk_fake_window_create();
    wlmtk_content_request_size(&fw1_ptr->fake_content_ptr->content, 100, 100);
    wlmtk_fake_window_commit_size(fw1_ptr);
    wlmtk_window_set_position(fw1_ptr->window_ptr, 0, 0);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_window_is_activated(fw1_ptr->window_ptr));

    // Window 1 is mapped => it's activated.
    wlmtk_workspace_map_window(ws_ptr, fw1_ptr->window_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw1_ptr->window_ptr));

    // Window 2: from (200, 0) to (300, 100).
    // Window 2 is mapped: Will get activated, and 1st one de-activated.
    wlmtk_fake_window_t *fw2_ptr = wlmtk_fake_window_create();
    wlmtk_content_request_size(&fw2_ptr->fake_content_ptr->content, 100, 100);
    wlmtk_fake_window_commit_size(fw2_ptr);
    wlmtk_window_set_position(fw2_ptr->window_ptr, 200, 0);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_window_is_activated(fw2_ptr->window_ptr));
    wlmtk_workspace_map_window(ws_ptr, fw2_ptr->window_ptr);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_window_is_activated(fw1_ptr->window_ptr));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw2_ptr->window_ptr));

    // Pointer move, over window 1. Nothing happens: We have click-to-focus.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(
            wlmtk_workspace_element(ws_ptr), 50, 50, 0));
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_window_is_activated(fw1_ptr->window_ptr));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw2_ptr->window_ptr));

    // Click on window 1: Gets activated.
    wlmtk_button_event_t button_event = {
        .button = BTN_RIGHT,
        .type = WLMTK_BUTTON_DOWN,
    };
    wlmtk_element_pointer_button(
        wlmtk_workspace_element(ws_ptr),
        &button_event);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw1_ptr->window_ptr));
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_window_is_activated(fw2_ptr->window_ptr));

    // Unmap window 1. Now window 2 gets activated.
    wlmtk_workspace_unmap_window(ws_ptr, fw1_ptr->window_ptr);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_window_is_activated(fw1_ptr->window_ptr));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw2_ptr->window_ptr));

    // Unmap the remaining window 2. Nothing is activated.
    wlmtk_workspace_unmap_window(ws_ptr, fw2_ptr->window_ptr);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_window_is_activated(fw2_ptr->window_ptr));

    wlmtk_fake_window_destroy(fw2_ptr);
    wlmtk_fake_window_destroy(fw1_ptr);
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
        wlr_output_layout_ptr, "t", &_wlmtk_workspace_test_tile_style, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws_ptr);
    wlmtk_workspace_enable(ws_ptr, true);
    bs_dllist_t *windows_ptr = wlmtk_workspace_get_windows_dllist(
        ws_ptr);

    // Window 1 gets mapped: Activated and on top.
    wlmtk_fake_window_t *fw1_ptr = wlmtk_fake_window_create();
    wlmtk_workspace_map_window(ws_ptr, fw1_ptr->window_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw1_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_window(fw1_ptr->window_ptr),
        windows_ptr->head_ptr);

    // Window 2 gets mapped: Activated and on top.
    wlmtk_fake_window_t *fw2_ptr = wlmtk_fake_window_create();
    wlmtk_workspace_map_window(ws_ptr, fw2_ptr->window_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw2_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_window(fw2_ptr->window_ptr),
        windows_ptr->head_ptr);

    // Window 3 gets mapped: Activated and on top.
    wlmtk_fake_window_t *fw3_ptr = wlmtk_fake_window_create();
    wlmtk_workspace_map_window(ws_ptr, fw3_ptr->window_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw3_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_window(fw3_ptr->window_ptr),
        windows_ptr->head_ptr);

    // From mapping sequence: We have 3 -> 2 -> 1. Cycling brings us to
    // window 2, but must not change the top window.
    wlmtk_workspace_activate_next_window(ws_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw2_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_window(fw3_ptr->window_ptr),
        windows_ptr->head_ptr);

    // One more cycle: 1.
    wlmtk_workspace_activate_next_window(ws_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw1_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_window(fw3_ptr->window_ptr),
        windows_ptr->head_ptr);

    // One more cycle: Back at 3.
    wlmtk_workspace_activate_next_window(ws_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw3_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_window(fw3_ptr->window_ptr),
        windows_ptr->head_ptr);

    // Cycle backward: Gets us to 1.
    wlmtk_workspace_activate_previous_window(ws_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw1_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_window(fw3_ptr->window_ptr),
        windows_ptr->head_ptr);

    // Raise: Must come to top.
    wlmtk_workspace_raise_window(ws_ptr,
                                 fw1_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_window(fw1_ptr->window_ptr),
        windows_ptr->head_ptr);

    wlmtk_workspace_unmap_window(ws_ptr, fw3_ptr->window_ptr);
    wlmtk_workspace_unmap_window(ws_ptr, fw2_ptr->window_ptr);
    wlmtk_workspace_unmap_window(ws_ptr, fw1_ptr->window_ptr);
    wlmtk_fake_window_destroy(fw3_ptr);
    wlmtk_fake_window_destroy(fw2_ptr);
    wlmtk_fake_window_destroy(fw1_ptr);
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
        wlr_output_layout_ptr, "t", &_wlmtk_workspace_test_tile_style, NULL);

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

/* == End of workspace.c =================================================== */
