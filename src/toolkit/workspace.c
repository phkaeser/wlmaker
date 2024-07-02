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

#include "fsm.h"
#include "layer.h"

#define WLR_USE_UNSTABLE
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

    /** Container that holds the windows, ie. the window layer. */
    wlmtk_container_t         window_container;
    /** Container that holds the fullscreen elements. Should have only one. */
    wlmtk_container_t         fullscreen_container;

    /** List of toplevel windows. Via @ref wlmtk_window_t::dlnode. */
    bs_dllist_t               windows;

    /** The activated window. */
    wlmtk_window_t            *activated_window_ptr;

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

    /** Points to signal that triggers when a window is mapped. */
    struct wl_signal          *window_mapped_event_ptr;
    /** Points to signal that triggers when a window is unmapped. */
    struct wl_signal          *window_unmapped_event_ptr;

    /** Background layer. */
    wlmtk_layer_t             *background_layer_ptr;
    /** Bottom layer. */
    wlmtk_layer_t             *bottom_layer_ptr;
    /** Top layer. */
    wlmtk_layer_t             *top_layer_ptr;
    /** Overlay layer. */
    wlmtk_layer_t             *overlay_layer_ptr;
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
    const char *name_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmtk_workspace_t *workspace_ptr =
        logged_calloc(1, sizeof(wlmtk_workspace_t));
    if (NULL == workspace_ptr) return NULL;
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

    workspace_ptr->background_layer_ptr = wlmtk_layer_create(env_ptr);
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

    workspace_ptr->bottom_layer_ptr = wlmtk_layer_create(env_ptr);
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

    workspace_ptr->top_layer_ptr = wlmtk_layer_create(env_ptr);
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

    workspace_ptr->overlay_layer_ptr = wlmtk_layer_create(env_ptr);
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
    return workspace_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_destroy(wlmtk_workspace_t *workspace_ptr)
{
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
void wlmtk_workspace_get_details(
    wlmtk_workspace_t *workspace_ptr,
    const char **name_ptr_ptr,
    int *index_ptr)
{
    *index_ptr = workspace_ptr->index;
    *name_ptr_ptr = workspace_ptr->name_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_set_signals(
    wlmtk_workspace_t *workspace_ptr,
    struct wl_signal *mapped_event_ptr,
    struct wl_signal *unmapped_event_ptr)
{
    workspace_ptr->window_mapped_event_ptr = mapped_event_ptr;
    workspace_ptr->window_unmapped_event_ptr = unmapped_event_ptr;
}

/* ------------------------------------------------------------------------- */
// TODO(kaeser@gubbe.ch): Add test to verify layers are reconfigured.
void wlmtk_workspace_set_extents(wlmtk_workspace_t *workspace_ptr,
                                 const struct wlr_box *extents_ptr)
{
    workspace_ptr->x1 = extents_ptr->x;
    workspace_ptr->y1 = extents_ptr->y;
    workspace_ptr->x2 = extents_ptr->x + extents_ptr->width;
    workspace_ptr->y2 = extents_ptr->y + extents_ptr->height;

    if (NULL != workspace_ptr->background_layer_ptr) {
        wlmtk_layer_reconfigure(workspace_ptr->background_layer_ptr);
    }
    if (NULL != workspace_ptr->bottom_layer_ptr) {
        wlmtk_layer_reconfigure(workspace_ptr->bottom_layer_ptr);
    }
    if (NULL != workspace_ptr->top_layer_ptr) {
        wlmtk_layer_reconfigure(workspace_ptr->top_layer_ptr);
    }
    if (NULL != workspace_ptr->overlay_layer_ptr) {
        wlmtk_layer_reconfigure(workspace_ptr->overlay_layer_ptr);
    }
}

/* ------------------------------------------------------------------------- */
struct wlr_box wlmtk_workspace_get_maximize_extents(
    wlmtk_workspace_t *workspace_ptr)
{
    // TODO(kaeser@gubbe.ch): Well, actually compute something sensible.
    struct wlr_box box = {
        .x = workspace_ptr->x1,
        .y = workspace_ptr->y1,
        .width = workspace_ptr->x2 - workspace_ptr->x1 - 64,
        .height = workspace_ptr->y2 - workspace_ptr->y1 - 64 };
    return box;
}

/* ------------------------------------------------------------------------- */
struct wlr_box wlmtk_workspace_get_fullscreen_extents(
    wlmtk_workspace_t *workspace_ptr)
{
    struct wlr_box box = {
        .x = workspace_ptr->x1,
        .y = workspace_ptr->y1,
        .width = workspace_ptr->x2 - workspace_ptr->x1,
        .height = workspace_ptr->y2 - workspace_ptr->y1 };
    return box;
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

    if (NULL != workspace_ptr->window_mapped_event_ptr) {
        wl_signal_emit(workspace_ptr->window_mapped_event_ptr, window_ptr);
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
    if (NULL != workspace_ptr->window_unmapped_event_ptr) {
        wl_signal_emit(workspace_ptr->window_unmapped_event_ptr, window_ptr);
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
bool wlmtk_workspace_axis(
    wlmtk_workspace_t *workspace_ptr,
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr)
{
    return wlmtk_element_pointer_axis(
        &workspace_ptr->super_container.super_element,
        wlr_pointer_axis_event_ptr);
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
        wlmtk_window_set_activated(workspace_ptr->activated_window_ptr, false);
        workspace_ptr->activated_window_ptr = NULL;
    }

    if (NULL != window_ptr) {
        wlmtk_window_set_activated(window_ptr, true);
        workspace_ptr->activated_window_ptr = window_ptr;
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
    return BS_CONTAINER_OF(dlnode_ptr, wlmtk_workspace_t, dlnode);
}

/* == Fake workspace methods, useful for tests ============================= */

static void wlmtk_fake_workspace_handle_window_mapped(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr);
static void wlmtk_fake_workspace_handle_window_unmapped(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr);

/* ------------------------------------------------------------------------- */
wlmtk_fake_workspace_t *wlmtk_fake_workspace_create(int width, int height)
{
    wlmtk_fake_workspace_t *fw_ptr = logged_calloc(
        1, sizeof(wlmtk_fake_workspace_t));
    if (NULL == fw_ptr) return NULL;

    fw_ptr->workspace_ptr = wlmtk_workspace_create("fake", NULL);
    if (NULL == fw_ptr->workspace_ptr) {
        wlmtk_fake_workspace_destroy(fw_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_workspace_element(fw_ptr->workspace_ptr), true);

    fw_ptr->fake_parent_ptr = wlmtk_container_create_fake_parent();
    if (NULL == fw_ptr->fake_parent_ptr) {
        wlmtk_fake_workspace_destroy(fw_ptr);
        return NULL;
    }
    wlmtk_container_add_element(
        fw_ptr->fake_parent_ptr,
        wlmtk_workspace_element(fw_ptr->workspace_ptr));

    struct wlr_box extents = { .width = width, .height = height };
    wlmtk_workspace_set_extents(fw_ptr->workspace_ptr, &extents);

    wl_signal_init(&fw_ptr->window_mapped_event);
    wl_signal_init(&fw_ptr->window_unmapped_event);
    wlmtk_workspace_set_signals(
        fw_ptr->workspace_ptr,
        &fw_ptr->window_mapped_event,
        &fw_ptr->window_unmapped_event);

    wlmtk_util_connect_listener_signal(
        &fw_ptr->window_mapped_event,
        &fw_ptr->window_mapped_listener,
        wlmtk_fake_workspace_handle_window_mapped);
    wlmtk_util_connect_listener_signal(
        &fw_ptr->window_unmapped_event,
        &fw_ptr->window_unmapped_listener,
        wlmtk_fake_workspace_handle_window_unmapped);

    return fw_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_fake_workspace_destroy(wlmtk_fake_workspace_t *fw_ptr)
{
    wl_list_remove(&fw_ptr->window_unmapped_listener.link);
    wl_list_remove(&fw_ptr->window_mapped_listener.link);

    if (NULL != fw_ptr->fake_parent_ptr) {
        wlmtk_container_remove_element(
            fw_ptr->fake_parent_ptr,
            wlmtk_workspace_element(fw_ptr->workspace_ptr));
        wlmtk_container_destroy_fake_parent(fw_ptr->fake_parent_ptr);
        fw_ptr->fake_parent_ptr = NULL;
    }

    if (NULL != fw_ptr->workspace_ptr) {
        wlmtk_workspace_destroy(fw_ptr->workspace_ptr);
        fw_ptr->workspace_ptr = NULL;
    }

    free(fw_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handler for the fake workspace's "window mapped" signal. */
void wlmtk_fake_workspace_handle_window_mapped(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_fake_workspace_t *fw_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_fake_workspace_t, window_mapped_listener);
    fw_ptr->window_mapped_listener_invoked = true;
}

/* ------------------------------------------------------------------------- */
/** Handler for the fake workspace's "window unmapped" signal. */
void wlmtk_fake_workspace_handle_window_unmapped(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_fake_workspace_t *fw_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_fake_workspace_t, window_unmapped_listener);
    fw_ptr->window_unmapped_listener_invoked = true;
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

    bs_log(BS_ERROR, "Came here: %f, %f", x, y);

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
    bs_log(BS_ERROR, "FIXME: reqeust %d, %d, %d, %d",
           left, top, right - left, bottom - top);
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
static void test_activate(bs_test_t *test_ptr);
static void test_activate_cycling(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_workspace_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "map_unmap", test_map_unmap },
    { 1, "move", test_move },
    { 1, "unmap_during_move", test_unmap_during_move },
    { 1, "resize", test_resize },
    { 1, "activate", test_activate },
    { 1, "activate_cycling", test_activate_cycling },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises workspace create & destroy methods. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_container_t *fake_parent_ptr = wlmtk_container_create_fake_parent();
    BS_ASSERT(NULL != fake_parent_ptr);

    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create("test", NULL);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, workspace_ptr);

    struct wlr_box box = { .x = -10, .y = -20, .width = 100, .height = 200 };
    wlmtk_workspace_set_extents(workspace_ptr, &box);
    int x1, y1, x2, y2;
    wlmtk_element_get_pointer_area(
        &workspace_ptr->super_container.super_element, &x1, &y1, &x2, &y2);
    BS_TEST_VERIFY_EQ(test_ptr, -10, x1);
    BS_TEST_VERIFY_EQ(test_ptr, -20, y1);
    BS_TEST_VERIFY_EQ(test_ptr, 90, x2);
    BS_TEST_VERIFY_EQ(test_ptr, 180, y2);

    box = wlmtk_workspace_get_maximize_extents(workspace_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, -10, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, -20, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 36, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 136, box.height);

    box = wlmtk_workspace_get_fullscreen_extents(workspace_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, -10, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, -20, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 100, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 200, box.height);

    const char *name_ptr;
    int index;
    wlmtk_workspace_get_details(workspace_ptr, &name_ptr, &index);
    BS_TEST_VERIFY_STREQ(test_ptr, "test", name_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, index);

    wlmtk_workspace_destroy(workspace_ptr);
    wlmtk_container_destroy_fake_parent(fake_parent_ptr);
}

/* ------------------------------------------------------------------------- */
/** Verifies that mapping and unmapping windows works. */
void test_map_unmap(bs_test_t *test_ptr)
{
    wlmtk_fake_workspace_t *fws_ptr = wlmtk_fake_workspace_create(1024, 768);
    bs_dllist_t *wdl_ptr = wlmtk_workspace_get_windows_dllist(
        fws_ptr->workspace_ptr);
    BS_ASSERT(NULL != fws_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, bs_dllist_size(wdl_ptr));

    wlmtk_fake_window_t *fw_ptr = wlmtk_fake_window_create();
    BS_ASSERT(NULL != fw_ptr);

    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window_element(fw_ptr->window_ptr)->visible);
    wlmtk_workspace_map_window(fws_ptr->workspace_ptr, fw_ptr->window_ptr);
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
    BS_TEST_VERIFY_TRUE(test_ptr, fws_ptr->window_mapped_listener_invoked);

    wlmtk_workspace_unmap_window(fws_ptr->workspace_ptr, fw_ptr->window_ptr);
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
    BS_TEST_VERIFY_TRUE(test_ptr, fws_ptr->window_unmapped_listener_invoked);

    wlmtk_fake_window_destroy(fw_ptr);
    wlmtk_fake_workspace_destroy(fws_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests moving a window. */
void test_move(bs_test_t *test_ptr)
{
    wlmtk_fake_workspace_t *fws_ptr = wlmtk_fake_workspace_create(1024, 768);
    BS_ASSERT(NULL != fws_ptr);
    wlmtk_fake_window_t *fw_ptr = wlmtk_fake_window_create();
    BS_ASSERT(NULL != fw_ptr);

    wlmtk_element_pointer_motion(
        wlmtk_workspace_element(fws_ptr->workspace_ptr), 0, 0, 42);

    wlmtk_workspace_map_window(fws_ptr->workspace_ptr, fw_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(fw_ptr->window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(fw_ptr->window_ptr)->y);

    // Starts a move for the window. Will move it...
    wlmtk_workspace_begin_window_move(fws_ptr->workspace_ptr, fw_ptr->window_ptr);
    wlmtk_element_pointer_motion(
        wlmtk_workspace_element(fws_ptr->workspace_ptr), 1, 2, 43);
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window_element(fw_ptr->window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window_element(fw_ptr->window_ptr)->y);

    // Releases the button. Should end the move.
    wlmtk_button_event_t button_event = {
        .button = BTN_LEFT,
        .type = WLMTK_BUTTON_UP,
        .time_msec = 44,
    };
    wlmtk_element_pointer_button(
        wlmtk_workspace_element(fws_ptr->workspace_ptr),
        &button_event);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, fws_ptr->workspace_ptr->grabbed_window_ptr);

    // More motion, no longer updates the position.
    wlmtk_element_pointer_motion(
        wlmtk_workspace_element(fws_ptr->workspace_ptr), 3, 4, 45);
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window_element(fw_ptr->window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window_element(fw_ptr->window_ptr)->y);

    wlmtk_workspace_unmap_window(fws_ptr->workspace_ptr, fw_ptr->window_ptr);

    wlmtk_fake_window_destroy(fw_ptr);
    wlmtk_fake_workspace_destroy(fws_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests moving a window that unmaps during the move. */
void test_unmap_during_move(bs_test_t *test_ptr)
{
    wlmtk_fake_workspace_t *fws_ptr = wlmtk_fake_workspace_create(1024, 768);
    BS_ASSERT(NULL != fws_ptr);
    wlmtk_fake_window_t *fw_ptr = wlmtk_fake_window_create();
    BS_ASSERT(NULL != fw_ptr);

    wlmtk_element_pointer_motion(
        wlmtk_workspace_element(fws_ptr->workspace_ptr), 0, 0, 42);

    wlmtk_workspace_map_window(fws_ptr->workspace_ptr, fw_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(fw_ptr->window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(fw_ptr->window_ptr)->y);

    // Starts a move for the window. Will move it...
    wlmtk_workspace_begin_window_move(fws_ptr->workspace_ptr, fw_ptr->window_ptr);
    wlmtk_element_pointer_motion(
        wlmtk_workspace_element(fws_ptr->workspace_ptr), 1, 2, 43);
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window_element(fw_ptr->window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window_element(fw_ptr->window_ptr)->y);

    wlmtk_workspace_unmap_window(fws_ptr->workspace_ptr, fw_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, fws_ptr->workspace_ptr->grabbed_window_ptr);

    // More motion, no longer updates the position.
    wlmtk_element_pointer_motion(
        wlmtk_workspace_element(fws_ptr->workspace_ptr), 3, 4, 45);
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window_element(fw_ptr->window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window_element(fw_ptr->window_ptr)->y);


    // More motion, no longer updates the position.
    wlmtk_element_pointer_motion(
        wlmtk_workspace_element(fws_ptr->workspace_ptr), 3, 4, 45);
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window_element(fw_ptr->window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window_element(fw_ptr->window_ptr)->y);

    wlmtk_fake_window_destroy(fw_ptr);
    wlmtk_fake_workspace_destroy(fws_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests resizing a window. */
void test_resize(bs_test_t *test_ptr)
{
    wlmtk_fake_workspace_t *fws_ptr = wlmtk_fake_workspace_create(1024, 768);
    BS_ASSERT(NULL != fws_ptr);

    wlmtk_fake_window_t *fw_ptr = wlmtk_fake_window_create();
    BS_ASSERT(NULL != fw_ptr);
    wlmtk_window_request_position_and_size(fw_ptr->window_ptr, 0, 0, 40, 20);
    wlmtk_fake_window_commit_size(fw_ptr);
    bs_log(BS_ERROR, "FIXME: Start");
    wlmtk_element_pointer_motion(
        &fws_ptr->fake_parent_ptr->super_element, 0, 0, 42);

    wlmtk_workspace_map_window(fws_ptr->workspace_ptr, fw_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(fw_ptr->window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(fw_ptr->window_ptr)->y);
    int width, height;
    wlmtk_window_get_size(fw_ptr->window_ptr, &width, &height);
    BS_TEST_VERIFY_EQ(test_ptr, 40, width);
    BS_TEST_VERIFY_EQ(test_ptr, 20, height);

    // Starts a resize for the window. Will resize & move it...
    wlmtk_workspace_begin_window_resize(
        fws_ptr->workspace_ptr, fw_ptr->window_ptr, WLR_EDGE_TOP | WLR_EDGE_LEFT);
    fw_ptr->fake_content_ptr->serial = 1;  // The serial.
    bs_log(BS_ERROR, "FIXME: Motion start");
    wlmtk_element_pointer_motion(
        &fws_ptr->fake_parent_ptr->super_element, 1, 2, 44);
    bs_log(BS_ERROR, "FIXME: Motion End");
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
    // FIXME: became 38, 16. Moved twice?

    bs_log(BS_ERROR, "FIXME: End");

    // Releases the button. Should end the move.
    wlmtk_button_event_t button_event = {
        .button = BTN_LEFT,
        .type = WLMTK_BUTTON_UP,
        .time_msec = 44,
    };
    wlmtk_element_pointer_button(
        wlmtk_workspace_element(fws_ptr->workspace_ptr),
        &button_event);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, fws_ptr->workspace_ptr->grabbed_window_ptr);

    wlmtk_workspace_unmap_window(fws_ptr->workspace_ptr, fw_ptr->window_ptr);
    wlmtk_fake_window_destroy(fw_ptr);
    wlmtk_fake_workspace_destroy(fws_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests window activation. */
void test_activate(bs_test_t *test_ptr)
{
    wlmtk_fake_workspace_t *fws_ptr = wlmtk_fake_workspace_create(1024, 768);
    BS_ASSERT(NULL != fws_ptr);

    // Window 1: from (0, 0) to (100, 100)
    wlmtk_fake_window_t *fw1_ptr = wlmtk_fake_window_create();
    wlmtk_content_request_size(&fw1_ptr->fake_content_ptr->content, 100, 100);
    wlmtk_fake_window_commit_size(fw1_ptr);
    wlmtk_window_set_position(fw1_ptr->window_ptr, 0, 0);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_window_is_activated(fw1_ptr->window_ptr));

    // Window 1 is mapped => it's activated.
    wlmtk_workspace_map_window(fws_ptr->workspace_ptr, fw1_ptr->window_ptr);
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
    wlmtk_workspace_map_window(fws_ptr->workspace_ptr, fw2_ptr->window_ptr);
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
            &fws_ptr->fake_parent_ptr->super_element,  50, 50, 0));
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
        wlmtk_workspace_element(fws_ptr->workspace_ptr),
        &button_event);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw1_ptr->window_ptr));
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_window_is_activated(fw2_ptr->window_ptr));

    // Unmap window 1. Now window 2 gets activated.
    wlmtk_workspace_unmap_window(fws_ptr->workspace_ptr, fw1_ptr->window_ptr);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_window_is_activated(fw1_ptr->window_ptr));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw2_ptr->window_ptr));

    // Unmap the remaining window 2. Nothing is activated.
    wlmtk_workspace_unmap_window(fws_ptr->workspace_ptr, fw2_ptr->window_ptr);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_window_is_activated(fw2_ptr->window_ptr));

    wlmtk_fake_window_destroy(fw2_ptr);
    wlmtk_fake_window_destroy(fw1_ptr);
    wlmtk_fake_workspace_destroy(fws_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests cycling through windows. */
void test_activate_cycling(bs_test_t *test_ptr)
{
    wlmtk_fake_workspace_t *fws_ptr = wlmtk_fake_workspace_create(1024, 768);
    BS_ASSERT(NULL != fws_ptr);
    bs_dllist_t *windows_ptr = wlmtk_workspace_get_windows_dllist(
        fws_ptr->workspace_ptr);

    // Window 1 gets mapped: Activated and on top.
    wlmtk_fake_window_t *fw1_ptr = wlmtk_fake_window_create();
    wlmtk_workspace_map_window(fws_ptr->workspace_ptr, fw1_ptr->window_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw1_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_window(fw1_ptr->window_ptr),
        windows_ptr->head_ptr);

    // Window 2 gets mapped: Activated and on top.
    wlmtk_fake_window_t *fw2_ptr = wlmtk_fake_window_create();
    wlmtk_workspace_map_window(fws_ptr->workspace_ptr, fw2_ptr->window_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw2_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_window(fw2_ptr->window_ptr),
        windows_ptr->head_ptr);

    // Window 3 gets mapped: Activated and on top.
    wlmtk_fake_window_t *fw3_ptr = wlmtk_fake_window_create();
    wlmtk_workspace_map_window(fws_ptr->workspace_ptr, fw3_ptr->window_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw3_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_window(fw3_ptr->window_ptr),
        windows_ptr->head_ptr);

    // From mapping sequence: We have 3 -> 2 -> 1. Cycling brings us to
    // window 2, but must not change the top window.
    wlmtk_workspace_activate_next_window(fws_ptr->workspace_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw2_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_window(fw3_ptr->window_ptr),
        windows_ptr->head_ptr);

    // One more cycle: 1.
    wlmtk_workspace_activate_next_window(fws_ptr->workspace_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw1_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_window(fw3_ptr->window_ptr),
        windows_ptr->head_ptr);

    // One more cycle: Back at 3.
    wlmtk_workspace_activate_next_window(fws_ptr->workspace_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw3_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_window(fw3_ptr->window_ptr),
        windows_ptr->head_ptr);

    // Cycle backward: Gets us to 1.
    wlmtk_workspace_activate_previous_window(fws_ptr->workspace_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_window_is_activated(fw1_ptr->window_ptr));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_window(fw3_ptr->window_ptr),
        windows_ptr->head_ptr);

    // Raise: Must come to top.
    wlmtk_workspace_raise_window(fws_ptr->workspace_ptr,
                                 fw1_ptr->window_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_window(fw1_ptr->window_ptr),
        windows_ptr->head_ptr);

    wlmtk_workspace_unmap_window(fws_ptr->workspace_ptr, fw3_ptr->window_ptr);
    wlmtk_workspace_unmap_window(fws_ptr->workspace_ptr, fw2_ptr->window_ptr);
    wlmtk_workspace_unmap_window(fws_ptr->workspace_ptr, fw1_ptr->window_ptr);
    wlmtk_fake_window_destroy(fw3_ptr);
    wlmtk_fake_window_destroy(fw2_ptr);
    wlmtk_fake_window_destroy(fw1_ptr);
    wlmtk_fake_workspace_destroy(fws_ptr);
}

/* == End of workspace.c =================================================== */
