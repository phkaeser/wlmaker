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

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** States of the pointer FSM. */
typedef enum {
    POINTER_STATE_PASSTHROUGH,
    POINTER_STATE_MOVE,
    POINTER_STATE_RESIZE
} pointer_state_t;

/** Events for the pointer FSM. */
typedef enum {
    POINTER_STATE_EVENT_BEGIN_MOVE,
    POINTER_STATE_EVENT_BEGIN_RESIZE,
    POINTER_STATE_EVENT_BTN_RELEASED,
    POINTER_STATE_EVENT_RESET,
    POINTER_STATE_EVENT_MOTION,
} pointer_state_event_t;

/** State of the workspace. */
struct _wlmtk_workspace_t {
    /** Superclass: Container. */
    wlmtk_container_t         super_container;
    /** Original virtual method table. We're overwriting parts. */
    wlmtk_element_impl_t      parent_element_impl;

    /** Current FSM state. */
    pointer_state_t current_state;

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

};

static void workspace_container_destroy(wlmtk_container_t *container_ptr);

static bool element_pointer_motion(
    wlmtk_element_t *element_ptr,
    double x, double y,
    uint32_t time_msec);
static bool element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);
static void element_pointer_leave(
    wlmtk_element_t *element_ptr);

/** Method table for the container's virtual methods. */
const wlmtk_container_impl_t  workspace_container_impl = {
    .destroy = workspace_container_destroy
};

/** State machine definition. */
typedef struct {
    /** Starting state. */
    pointer_state_t           state;
    /** Event. */
    pointer_state_event_t     event;
    /** Updated state. */
    pointer_state_t           new_state;
    /** Handler invoked by the (state, event) match. */
    void (*handler)(wlmtk_workspace_t *workspace_ptr, void *ud_ptr);
} state_transition_t;

static void begin_move(wlmtk_workspace_t *workspace_ptr, void *ud_ptr);
static void move_motion(wlmtk_workspace_t *workspace_ptr, void *ud_ptr);
static void move_end(wlmtk_workspace_t *workspace_ptr, void *ud_ptr);
static void handle_state(wlmtk_workspace_t *workspace_ptr,
                         pointer_state_event_t event,
                         void *ud_ptr);

/* == Data ================================================================= */

/** Finite state machine definition for pointer events. */
static const state_transition_t pointer_states[] = {
    {
        POINTER_STATE_PASSTHROUGH,
        POINTER_STATE_EVENT_BEGIN_MOVE,
        POINTER_STATE_MOVE,
        begin_move },
    {
        POINTER_STATE_MOVE,
        POINTER_STATE_EVENT_MOTION,
        POINTER_STATE_MOVE,
        move_motion
    },
    {
        POINTER_STATE_MOVE,
        POINTER_STATE_EVENT_BTN_RELEASED,
        POINTER_STATE_PASSTHROUGH,
        move_end,
    },
    { 0, 0, 0, NULL }  // sentinel.
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
                                       &workspace_container_impl,
                                       wlr_scene_tree_ptr)) {
        wlmtk_workspace_destroy(workspace_ptr);
        return NULL;
    }
    memcpy(&workspace_ptr->parent_element_impl,
           &workspace_ptr->super_container.super_element.impl,
           sizeof(wlmtk_element_impl_t));
    workspace_ptr->super_container.super_element.impl.pointer_motion =
        element_pointer_motion;
    workspace_ptr->super_container.super_element.impl.pointer_button =
        element_pointer_button;
    workspace_ptr->super_container.super_element.impl.pointer_leave =
        element_pointer_leave;
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

    // TODO(kaeser@gubbe.ch): Refine and test this.
    wlmtk_window_set_activated(window_ptr, true);
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_unmap_window(wlmtk_workspace_t *workspace_ptr,
                                  wlmtk_window_t *window_ptr)
{
    BS_ASSERT(workspace_ptr == wlmtk_workspace_from_container(
                  wlmtk_window_element(window_ptr)->parent_container_ptr));
    wlmtk_element_set_visible(wlmtk_window_element(window_ptr), false);
    wlmtk_container_remove_element(
        &workspace_ptr->super_container,
        wlmtk_window_element(window_ptr));
}

/* ------------------------------------------------------------------------- */
wlmtk_workspace_t *wlmtk_workspace_from_container(
    wlmtk_container_t *container_ptr)
{
    BS_ASSERT(0 == memcmp(
                  &container_ptr->impl,
                  &workspace_container_impl,
                  sizeof(wlmtk_container_impl_t)));
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
// TODO(kaeser@gubbe.ch): Improve this, and add tests to tatch UP to CLICK.
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
    handle_state(workspace_ptr, POINTER_STATE_EVENT_BEGIN_MOVE, window_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Virtual destructor, in case called from container. Wraps to our dtor. */
void workspace_container_destroy(wlmtk_container_t *container_ptr)
{
    wlmtk_workspace_t *workspace_ptr = BS_CONTAINER_OF(
        container_ptr, wlmtk_workspace_t, super_container);
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

    handle_state(workspace_ptr, POINTER_STATE_EVENT_MOTION, NULL);

    return workspace_ptr->parent_element_impl.pointer_motion(
        element_ptr, x, y, time_msec);
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

    if (button_event_ptr->type == WLMTK_BUTTON_UP) {
        handle_state(workspace_ptr,
                     POINTER_STATE_EVENT_BTN_RELEASED,
                     NULL);
    }

    return workspace_ptr->parent_element_impl.pointer_button(
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

    handle_state(workspace_ptr, POINTER_STATE_EVENT_RESET, NULL);

    workspace_ptr->parent_element_impl.pointer_leave(element_ptr);
}

/* ------------------------------------------------------------------------- */
/** State machine handler. */
void handle_state(wlmtk_workspace_t *workspace_ptr,
                  pointer_state_event_t event,
                  void *ud_ptr)
{
    for (const state_transition_t *transition_ptr = &pointer_states[0];
         NULL != transition_ptr->handler;
         transition_ptr++) {
        if (transition_ptr->state == workspace_ptr->current_state &&
            transition_ptr->event == event) {
            transition_ptr->handler(workspace_ptr, ud_ptr);
            workspace_ptr->current_state = transition_ptr->new_state;
            return;
        }
    }
}


/* ------------------------------------------------------------------------- */
/** Initiates a move. */
void begin_move(wlmtk_workspace_t *workspace_ptr, void *ud_ptr)
{
    workspace_ptr->grabbed_window_ptr = ud_ptr;
    workspace_ptr->motion_x =
        workspace_ptr->super_container.super_element.last_pointer_x;
    workspace_ptr->motion_y =
        workspace_ptr->super_container.super_element.last_pointer_y;

    wlmtk_element_get_position(
        wlmtk_window_element(workspace_ptr->grabbed_window_ptr),
        &workspace_ptr->initial_x,
        &workspace_ptr->initial_y);
}

/* ------------------------------------------------------------------------- */
/** Handles motion during a move. */
void move_motion(wlmtk_workspace_t *workspace_ptr,
                 __UNUSED__ void *ud_ptr)
{
    double rel_x = workspace_ptr->super_container.super_element.last_pointer_x -
        workspace_ptr->motion_x;
    double rel_y = workspace_ptr->super_container.super_element.last_pointer_y -
        workspace_ptr->motion_y;

    wlmtk_element_set_position(
        wlmtk_window_element(workspace_ptr->grabbed_window_ptr),
        workspace_ptr->initial_x + rel_x,
        workspace_ptr->initial_y + rel_y);
}

/* ------------------------------------------------------------------------- */
/** Ends the move. */
void move_end(__UNUSED__ wlmtk_workspace_t *workspace_ptr,
              __UNUSED__ void *ud_ptr)
{
    // Nothing to do.
}

/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);
static void test_map_unmap(bs_test_t *test_ptr);
static void test_button(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_workspace_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "map_unmap", test_map_unmap },
    { 1, "button", test_button },
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
    wlmtk_container_destroy(fake_parent_ptr);
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
        &fake_content_ptr->content);
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
    wlmtk_container_destroy(fake_parent_ptr);
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
    wlmtk_container_destroy(fake_parent_ptr);
}

/* == End of workspace.c =================================================== */
