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

    /** Current FSM state. */
    wlmtk_fsm_t               fsm;

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
};

static void _wlmtk_workspace_element_destroy(wlmtk_element_t *element_ptr);
static bool element_pointer_motion(
    wlmtk_element_t *element_ptr,
    double x, double y,
    uint32_t time_msec);
static bool element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);
static void element_pointer_leave(
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
    .pointer_motion = element_pointer_motion,
    .pointer_button = element_pointer_button,
    .pointer_leave = element_pointer_leave,
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
    struct wlr_scene_tree *wlr_scene_tree_ptr)
{
    wlmtk_workspace_t *workspace_ptr =
        logged_calloc(1, sizeof(wlmtk_workspace_t));
    if (NULL == workspace_ptr) return NULL;

    if (!wlmtk_container_init_attached(&workspace_ptr->super_container,
                                       wlr_scene_tree_ptr)) {
        wlmtk_workspace_destroy(workspace_ptr);
        return NULL;
    }
    workspace_ptr->orig_super_element_vmt = wlmtk_element_extend(
        &workspace_ptr->super_container.super_element,
        &workspace_element_vmt);

    wlmtk_fsm_init(&workspace_ptr->fsm, pfsm_transitions, PFSMS_PASSTHROUGH);
    return workspace_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_destroy(wlmtk_workspace_t *workspace_ptr)
{
    wlmtk_container_fini(&workspace_ptr->super_container);
    free(workspace_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_map_window(wlmtk_workspace_t *workspace_ptr,
                                wlmtk_window_t *window_ptr)
{
    wlmtk_element_set_visible(wlmtk_window_element(window_ptr), true);
    wlmtk_container_add_element(
        &workspace_ptr->super_container,
        wlmtk_window_element(window_ptr));

    wlmtk_workspace_activate_window(workspace_ptr, window_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_unmap_window(wlmtk_workspace_t *workspace_ptr,
                                  wlmtk_window_t *window_ptr)
{
    bool need_activation = false;

    BS_ASSERT(workspace_ptr == wlmtk_workspace_from_container(
                  wlmtk_window_element(window_ptr)->parent_container_ptr));

    if (workspace_ptr->grabbed_window_ptr == window_ptr) {
        wlmtk_fsm_event(&workspace_ptr->fsm, PFSME_RESET, NULL);
        BS_ASSERT(NULL == workspace_ptr->grabbed_window_ptr);
    }

    if (workspace_ptr->activated_window_ptr == window_ptr) {
        wlmtk_workspace_activate_window(workspace_ptr, NULL);
        need_activation = true;
    }

    wlmtk_element_set_visible(wlmtk_window_element(window_ptr), false);
    wlmtk_container_remove_element(
        &workspace_ptr->super_container,
        wlmtk_window_element(window_ptr));

    if (need_activation) {
        // FIXME
        bs_dllist_node_t *dlnode_ptr =
            workspace_ptr->super_container.elements.head_ptr;
        if (NULL != dlnode_ptr) {
            wlmtk_element_t *element_ptr = wlmtk_element_from_dlnode(dlnode_ptr);
            wlmtk_window_t *window_ptr = wlmtk_window_from_element(element_ptr);
            wlmtk_workspace_activate_window(workspace_ptr, window_ptr);
        }
    }
}

/* ------------------------------------------------------------------------- */
wlmtk_workspace_t *wlmtk_workspace_from_container(
    wlmtk_container_t *container_ptr)
{
    BS_ASSERT(container_ptr->super_element.vmt.destroy ==
              _wlmtk_workspace_element_destroy);
    return BS_CONTAINER_OF(container_ptr, wlmtk_workspace_t, super_container);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_workspace_motion(
    wlmtk_workspace_t *workspace_ptr,
    double x,
    double y,
    uint32_t time_msec)
{
    return wlmtk_element_pointer_motion(
        &workspace_ptr->super_container.super_element, x, y, time_msec);
}

/* ------------------------------------------------------------------------- */
// TODO(kaeser@gubbe.ch): Improve this, has multiple bugs: It won't keep
// different buttons apart, and there's currently no test associated.
void wlmtk_workspace_button(
    wlmtk_workspace_t *workspace_ptr,
    const struct wlr_pointer_button_event *event_ptr)
{
    wlmtk_button_event_t event;

    // Guard clause: nothing to pass on if no element has the focus.
    event.button = event_ptr->button;
    event.time_msec = event_ptr->time_msec;
    if (WLR_BUTTON_PRESSED == event_ptr->state) {
        event.type = WLMTK_BUTTON_DOWN;
        wlmtk_element_pointer_button(
            &workspace_ptr->super_container.super_element, &event);

    } else if (WLR_BUTTON_RELEASED == event_ptr->state) {
        event.type = WLMTK_BUTTON_UP;
        wlmtk_element_pointer_button(
            &workspace_ptr->super_container.super_element, &event);
        event.type = WLMTK_BUTTON_CLICK;
        wlmtk_element_pointer_button(
            &workspace_ptr->super_container.super_element, &event);

    } else {
        bs_log(BS_WARNING,
               "Workspace %p: Unhandled state 0x%x for button 0x%x",
               workspace_ptr, event_ptr->state, event_ptr->button);
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
        wlmtk_window_set_activated(workspace_ptr->activated_window_ptr, false);
        workspace_ptr->activated_window_ptr = NULL;
    }

    if (NULL != window_ptr) {
        wlmtk_window_set_activated(window_ptr, true);
        workspace_ptr->activated_window_ptr = window_ptr;
    }
    // set activated.
    // keep track of activated. => so it can be deactivated.


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
 * @return Whether the motion encountered an active element.
 */
bool element_pointer_motion(
    wlmtk_element_t *element_ptr,
    double x, double y,
    uint32_t time_msec)
{
    wlmtk_workspace_t *workspace_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_workspace_t, super_container.super_element);
    bool rv = workspace_ptr->orig_super_element_vmt.pointer_motion(
        element_ptr, x, y, time_msec);
    wlmtk_fsm_event(&workspace_ptr->fsm, PFSME_MOTION, NULL);
    return rv;
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
bool element_pointer_button(
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
void element_pointer_leave(
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

    wlmtk_element_set_position(
        wlmtk_window_element(workspace_ptr->grabbed_window_ptr),
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
static void test_button(bs_test_t *test_ptr);
static void test_move(bs_test_t *test_ptr);
static void test_unmap_during_move(bs_test_t *test_ptr);
static void test_resize(bs_test_t *test_ptr);
static void test_activate(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_workspace_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "map_unmap", test_map_unmap },
    { 1, "button", test_button },
    { 1, "move", test_move },
    { 1, "unmap_during_move", test_unmap_during_move },
    { 1, "resize", test_resize },
    { 1, "activate", test_activate },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises workspace create & destroy methods. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_container_t *fake_parent_ptr = wlmtk_container_create_fake_parent();
    BS_ASSERT(NULL != fake_parent_ptr);

    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
        fake_parent_ptr->wlr_scene_tree_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, workspace_ptr);

    BS_TEST_VERIFY_EQ(
        test_ptr,
        workspace_ptr,
        wlmtk_workspace_from_container(&workspace_ptr->super_container));

    wlmtk_workspace_destroy(workspace_ptr);
    wlmtk_container_destroy_fake_parent(fake_parent_ptr);
}

/* ------------------------------------------------------------------------- */
/** Verifies that mapping and unmapping windows works. */
void test_map_unmap(bs_test_t *test_ptr)
{
    wlmtk_container_t *fake_parent_ptr = wlmtk_container_create_fake_parent();
    BS_ASSERT(NULL != fake_parent_ptr);
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
        fake_parent_ptr->wlr_scene_tree_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, workspace_ptr);

    wlmtk_fake_content_t *fake_content_ptr = wlmtk_fake_content_create();
    wlmtk_window_t *window_ptr = wlmtk_window_create(
        NULL, NULL, &fake_content_ptr->content);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, window_ptr);

    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window_element(window_ptr)->visible);
    wlmtk_workspace_map_window(workspace_ptr, window_ptr);
    BS_TEST_VERIFY_NEQ(
        test_ptr,
        NULL,
        wlmtk_window_element(window_ptr)->wlr_scene_node_ptr);
    BS_TEST_VERIFY_NEQ(
        test_ptr,
        NULL,
        fake_content_ptr->content.super_element.wlr_scene_node_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window_element(window_ptr)->visible);

    wlmtk_workspace_unmap_window(workspace_ptr, window_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        wlmtk_window_element(window_ptr)->wlr_scene_node_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        fake_content_ptr->content.super_element.wlr_scene_node_ptr);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window_element(window_ptr)->visible);

    wlmtk_window_destroy(window_ptr);
    wlmtk_workspace_destroy(workspace_ptr);
    wlmtk_container_destroy_fake_parent(fake_parent_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests wlmtk_workspace_button. */
void test_button(bs_test_t *test_ptr)
{
    wlmtk_container_t *fake_parent_ptr = wlmtk_container_create_fake_parent();
    BS_ASSERT(NULL != fake_parent_ptr);
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
        fake_parent_ptr->wlr_scene_tree_ptr);
    BS_ASSERT(NULL != workspace_ptr);
    wlmtk_fake_element_t *fake_element_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&fake_element_ptr->element, true);
    BS_ASSERT(NULL != fake_element_ptr);

    wlmtk_container_add_element(
        &workspace_ptr->super_container, &fake_element_ptr->element);

    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_workspace_motion(workspace_ptr, 0, 0, 1234));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        fake_element_ptr->pointer_motion_called);

    // Verify that a button down event is passed.
    struct wlr_pointer_button_event wlr_pointer_button_event = {
        .button = 42,
        .state = WLR_BUTTON_PRESSED,
        .time_msec = 4321,
    };
    wlmtk_workspace_button(workspace_ptr, &wlr_pointer_button_event);
    wlmtk_button_event_t expected_event = {
        .button = 42,
        .type = WLMTK_BUTTON_DOWN,
        .time_msec = 4321,
    };
    BS_TEST_VERIFY_MEMEQ(
        test_ptr,
        &expected_event,
        &fake_element_ptr->pointer_button_event,
        sizeof(wlmtk_button_event_t));

    // The button up event should trigger a click.
    wlr_pointer_button_event.state = WLR_BUTTON_RELEASED;
    wlmtk_workspace_button(workspace_ptr, &wlr_pointer_button_event);
    expected_event.type = WLMTK_BUTTON_CLICK;
    BS_TEST_VERIFY_MEMEQ(
        test_ptr,
        &expected_event,
        &fake_element_ptr->pointer_button_event,
        sizeof(wlmtk_button_event_t));

    wlmtk_container_remove_element(
        &workspace_ptr->super_container, &fake_element_ptr->element);

    wlmtk_element_destroy(&fake_element_ptr->element);
    wlmtk_workspace_destroy(workspace_ptr);
    wlmtk_container_destroy_fake_parent(fake_parent_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests moving a window. */
void test_move(bs_test_t *test_ptr)
{
    wlmtk_container_t *fake_parent_ptr = wlmtk_container_create_fake_parent();
    BS_ASSERT(NULL != fake_parent_ptr);
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
        fake_parent_ptr->wlr_scene_tree_ptr);
    BS_ASSERT(NULL != workspace_ptr);
    wlmtk_fake_content_t *fake_content_ptr = wlmtk_fake_content_create();
    wlmtk_window_t *window_ptr = wlmtk_window_create(
        NULL, NULL, &fake_content_ptr->content);
    BS_ASSERT(NULL != window_ptr);
    wlmtk_workspace_motion(workspace_ptr, 0, 0, 42);

    wlmtk_workspace_map_window(workspace_ptr, window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(window_ptr)->y);

    // Starts a move for the window. Will move it...
    wlmtk_workspace_begin_window_move(workspace_ptr, window_ptr);
    wlmtk_workspace_motion(workspace_ptr, 1, 2, 43);
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window_element(window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window_element(window_ptr)->y);

    // Releases the button. Should end the move.
    struct wlr_pointer_button_event wlr_pointer_button_event = {
        .button = BTN_LEFT,
        .state = WLR_BUTTON_RELEASED,
        .time_msec = 44,
    };
    wlmtk_workspace_button(workspace_ptr, &wlr_pointer_button_event);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, workspace_ptr->grabbed_window_ptr);

    // More motion, no longer updates the position.
    wlmtk_workspace_motion(workspace_ptr, 3, 4, 45);
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window_element(window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window_element(window_ptr)->y);

    wlmtk_workspace_unmap_window(workspace_ptr, window_ptr);

    wlmtk_window_destroy(window_ptr);
    wlmtk_workspace_destroy(workspace_ptr);
    wlmtk_container_destroy_fake_parent(fake_parent_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests moving a window that unmaps during the move. */
void test_unmap_during_move(bs_test_t *test_ptr)
{
    wlmtk_container_t *fake_parent_ptr = wlmtk_container_create_fake_parent();
    BS_ASSERT(NULL != fake_parent_ptr);
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
        fake_parent_ptr->wlr_scene_tree_ptr);
    BS_ASSERT(NULL != workspace_ptr);
    wlmtk_fake_content_t *fake_content_ptr = wlmtk_fake_content_create();
    wlmtk_window_t *window_ptr = wlmtk_window_create(
        NULL, NULL, &fake_content_ptr->content);
    BS_ASSERT(NULL != window_ptr);
    wlmtk_workspace_motion(workspace_ptr, 0, 0, 42);

    wlmtk_workspace_map_window(workspace_ptr, window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(window_ptr)->y);

    // Starts a move for the window. Will move it...
    wlmtk_workspace_begin_window_move(workspace_ptr, window_ptr);
    wlmtk_workspace_motion(workspace_ptr, 1, 2, 43);
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window_element(window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window_element(window_ptr)->y);

    wlmtk_workspace_unmap_window(workspace_ptr, window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, workspace_ptr->grabbed_window_ptr);

    // More motion, no longer updates the position.
    wlmtk_workspace_motion(workspace_ptr, 3, 4, 45);
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window_element(window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window_element(window_ptr)->y);


    // More motion, no longer updates the position.
    wlmtk_workspace_motion(workspace_ptr, 3, 4, 45);
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window_element(window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window_element(window_ptr)->y);

    wlmtk_window_destroy(window_ptr);
    wlmtk_workspace_destroy(workspace_ptr);
    wlmtk_container_destroy_fake_parent(fake_parent_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests resizing a window. */
void test_resize(bs_test_t *test_ptr)
{
    wlmtk_container_t *fake_parent_ptr = wlmtk_container_create_fake_parent();
    BS_ASSERT(NULL != fake_parent_ptr);
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
        fake_parent_ptr->wlr_scene_tree_ptr);
    BS_ASSERT(NULL != workspace_ptr);
    wlmtk_fake_content_t *fake_content_ptr = wlmtk_fake_content_create();
    wlmtk_content_commit_size(&fake_content_ptr->content, 1, 40, 20);
    wlmtk_window_t *window_ptr = wlmtk_window_create(
        NULL, NULL, &fake_content_ptr->content);
    BS_ASSERT(NULL != window_ptr);
    wlmtk_workspace_motion(workspace_ptr, 0, 0, 42);

    wlmtk_workspace_map_window(workspace_ptr, window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(window_ptr)->y);
    int width, height;
    wlmtk_window_get_size(window_ptr, &width, &height);
    BS_TEST_VERIFY_EQ(test_ptr, 40, width);
    BS_TEST_VERIFY_EQ(test_ptr, 20, height);

    // Starts a resize for the window. Will resize & move it...
    wlmtk_workspace_begin_window_resize(
        workspace_ptr, window_ptr, WLR_EDGE_TOP | WLR_EDGE_LEFT);
    fake_content_ptr->return_request_size = 1;  // The serial.
    wlmtk_workspace_motion(workspace_ptr, 1, 2, 43);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, wlmtk_window_element(window_ptr)->y);
    BS_TEST_VERIFY_EQ(test_ptr, 39, fake_content_ptr->requested_width);
    BS_TEST_VERIFY_EQ(test_ptr, 18, fake_content_ptr->requested_height);
    // This updates for the given serial.
    wlmtk_content_commit_size(&fake_content_ptr->content, 1, 39, 18);
    wlmtk_window_get_size(window_ptr, &width, &height);
    BS_TEST_VERIFY_EQ(test_ptr, 1, wlmtk_window_element(window_ptr)->x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, wlmtk_window_element(window_ptr)->y);
    BS_TEST_VERIFY_EQ(test_ptr, 39, width);
    BS_TEST_VERIFY_EQ(test_ptr, 18, height);

    // Releases the button. Should end the move.
    struct wlr_pointer_button_event wlr_pointer_button_event = {
        .button = BTN_LEFT,
        .state = WLR_BUTTON_RELEASED,
        .time_msec = 44,
    };
    wlmtk_workspace_button(workspace_ptr, &wlr_pointer_button_event);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, workspace_ptr->grabbed_window_ptr);

    wlmtk_workspace_unmap_window(workspace_ptr, window_ptr);
    wlmtk_window_destroy(window_ptr);
    wlmtk_workspace_destroy(workspace_ptr);
    wlmtk_container_destroy_fake_parent(fake_parent_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests window activation. */
void test_activate(bs_test_t *test_ptr)
{
    wlmtk_container_t *fake_parent_ptr = wlmtk_container_create_fake_parent();
    BS_ASSERT(NULL != fake_parent_ptr);
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
        fake_parent_ptr->wlr_scene_tree_ptr);
    BS_ASSERT(NULL != workspace_ptr);

    // Window 1: from (0, 0) to (100, 100)
    wlmtk_fake_window_t *fw1_ptr = wlmtk_fake_window_create();
    wlmtk_content_commit_size(&fw1_ptr->fake_content_ptr->content, 0, 100, 100);
    wlmtk_element_set_position(wlmtk_window_element(&fw1_ptr->window), 0, 0);
    BS_TEST_VERIFY_FALSE(test_ptr, fw1_ptr->activated);

    // Window 1 is mapped => it's activated.
    wlmtk_workspace_map_window(workspace_ptr, &fw1_ptr->window);
    BS_TEST_VERIFY_TRUE(test_ptr, fw1_ptr->activated);

    // Window 2: from (200, 0) to (300, 100).
    // Window 2 is mapped: Will get activated, and 1st one de-activated.
    wlmtk_fake_window_t *fw2_ptr = wlmtk_fake_window_create();
    wlmtk_content_commit_size(&fw2_ptr->fake_content_ptr->content, 0, 100, 100);
    wlmtk_element_set_position(wlmtk_window_element(&fw2_ptr->window), 200, 0);
    BS_TEST_VERIFY_FALSE(test_ptr, fw2_ptr->activated);
    wlmtk_workspace_map_window(workspace_ptr, &fw2_ptr->window);
    BS_TEST_VERIFY_FALSE(test_ptr, fw1_ptr->activated);
    BS_TEST_VERIFY_TRUE(test_ptr, fw2_ptr->activated);

    // Pointer move, over window 1. Nothing happens: We have click-to-focus.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_workspace_motion(workspace_ptr, 50, 50, 0));
    BS_TEST_VERIFY_FALSE(test_ptr, fw1_ptr->activated);
    BS_TEST_VERIFY_TRUE(test_ptr, fw2_ptr->activated);

    // Click on window 1: Gets activated.
    struct wlr_pointer_button_event wlr_button_event = {
        .button = BTN_RIGHT, .state = WLR_BUTTON_PRESSED
    };
    wlmtk_workspace_button(workspace_ptr, &wlr_button_event);
    BS_TEST_VERIFY_TRUE(test_ptr, fw1_ptr->activated);
    BS_TEST_VERIFY_FALSE(test_ptr, fw2_ptr->activated);

    // Unmap window 1. Now window 2 gets activated.
    wlmtk_workspace_unmap_window(workspace_ptr, &fw1_ptr->window);
    BS_TEST_VERIFY_FALSE(test_ptr, fw1_ptr->activated);
    BS_TEST_VERIFY_TRUE(test_ptr, fw2_ptr->activated);

    // Unmap the remaining window 2. Nothing is activated.
    wlmtk_workspace_unmap_window(workspace_ptr, &fw2_ptr->window);
    BS_TEST_VERIFY_FALSE(test_ptr, fw2_ptr->activated);

    wlmtk_fake_window_destroy(fw2_ptr);
    wlmtk_fake_window_destroy(fw1_ptr);
    wlmtk_workspace_destroy(workspace_ptr);
    wlmtk_container_destroy_fake_parent(fake_parent_ptr);
}

/* == End of workspace.c =================================================== */
