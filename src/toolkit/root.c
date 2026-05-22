/* ========================================================================= */
/**
 * @file root.c
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

#include "root.h"

#include <libbase/libbase.h>
#include <stdlib.h>
#include <wayland-server-protocol.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

#include "desktop.h"
#include "element.h"
#include "input.h"
#include "output_tracker.h"
#include "test.h"  // IWYU pragma: keep
#include "tile.h"
#include "util.h"
#include "window.h"
#include "workspace.h"

/* == Declarations ========================================================= */

/** State of the root wrapper. */
struct _wlmtk_root_t {
    /** The wrapped element. */
    wlmtk_element_t           *element_ptr;
    /** Last pointer motion event context. */
    wlmtk_pointer_motion_event_t mev;
    /** Events available from the root wrapper. */
    wlmtk_root_events_t       events;

    /** wlroots output layout. */
    struct wlr_output_layout  *wlr_output_layout_ptr;
    /** Output tracker. */
    wlmtk_output_tracker_t    *output_tracker_ptr;
};

/** Listeners for a specific output tracked by root. */
typedef struct {
    /** Backlink. */
    wlmtk_root_t              *root_ptr;
    /** Frame listener. */
    struct wl_listener        frame_listener;
} wlmtk_root_output_t;

static void _wlmtk_root_output_handle_frame(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void *_wlmtk_root_output_tracker_create(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr);

static void _wlmtk_root_output_tracker_destroy(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr,
    void *output_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_root_t *wlmtk_root_create(
    wlmtk_element_t *element_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr)
{
    wlmtk_root_t *root_ptr = logged_calloc(1, sizeof(wlmtk_root_t));
    if (NULL == root_ptr) return NULL;
    root_ptr->element_ptr = element_ptr;
    root_ptr->wlr_output_layout_ptr = wlr_output_layout_ptr;

    wl_signal_init(&root_ptr->events.unclaimed_button_event);

    root_ptr->output_tracker_ptr = wlmtk_output_tracker_create(
        wlr_output_layout_ptr,
        root_ptr,
        _wlmtk_root_output_tracker_create,
        NULL,
        _wlmtk_root_output_tracker_destroy);
    if (NULL == root_ptr->output_tracker_ptr) {
        wlmtk_root_destroy(root_ptr);
        return NULL;
    }

    return root_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_root_destroy(wlmtk_root_t *root_ptr)
{
    if (NULL != root_ptr->output_tracker_ptr) {
        wlmtk_output_tracker_destroy(root_ptr->output_tracker_ptr);
        root_ptr->output_tracker_ptr = NULL;
    }
    free(root_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_root_events_t *wlmtk_root_events(wlmtk_root_t *root_ptr)
{
    return &root_ptr->events;
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_root_element(wlmtk_root_t *root_ptr)
{
    return root_ptr->element_ptr;
}

/* ------------------------------------------------------------------------- */
bool wlmtk_root_pointer_motion(
    wlmtk_root_t *root_ptr,
    double x,
    double y,
    uint32_t time_msec,
    wlmtk_pointer_t *pointer_ptr)
{
    root_ptr->mev = (wlmtk_pointer_motion_event_t){
        .x = x, .y = y, .time_msec = time_msec, .pointer_ptr = pointer_ptr
    };

    return wlmtk_element_pointer_motion(root_ptr->element_ptr, &root_ptr->mev);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_root_pointer_button(
    wlmtk_root_t *root_ptr,
    const struct wlr_pointer_button_event *event_ptr,
    uint32_t modifiers)
{
    wlmtk_button_event_t event;
    bool rv;

    event.button = event_ptr->button;
    event.time_msec = event_ptr->time_msec;
    event.keyboard_modifiers = modifiers;
    switch (event_ptr->state) {
    case WL_POINTER_BUTTON_STATE_PRESSED:
        event.type = WLMTK_BUTTON_DOWN;
        rv = wlmtk_element_pointer_button(root_ptr->element_ptr, &event);
        break;

    case WL_POINTER_BUTTON_STATE_RELEASED:
        event.type = WLMTK_BUTTON_UP;
        wlmtk_element_pointer_button(root_ptr->element_ptr, &event);
        event.type = WLMTK_BUTTON_CLICK;
        rv = wlmtk_element_pointer_button(root_ptr->element_ptr, &event);

        // Hack: Fix lost focus when a menu releases the pointer grab. This
        // should not be needed here, but needs a better means of updating
        // the pointer focus.
        // TODO(kaeser@gubbe.ch): Have a test for this with a window + menu,
        // and ensure focus computation is done well there.
        wlmtk_element_pointer_motion(root_ptr->element_ptr, &root_ptr->mev);
        break;

    default:
        bs_log(BS_WARNING,
               "Root %p: Unhandled state 0x%x for button 0x%x",
               root_ptr, event_ptr->state, event_ptr->button);
        return false;
    }

    if (!rv) {
        wl_signal_emit(&root_ptr->events.unclaimed_button_event, &event);
    }
    return rv;
}

/* ------------------------------------------------------------------------- */
bool wlmtk_root_pointer_axis(
    wlmtk_root_t *root_ptr,
    struct wlr_pointer_axis_event *wlr_pointer_axis_event_ptr)
{
    return wlmtk_element_pointer_axis(
        root_ptr->element_ptr,
        wlr_pointer_axis_event_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Ctor for struct wlmtk_root_output. */
void *_wlmtk_root_output_tracker_create(
    struct wlr_output *wlr_output_ptr,
    void *ud_ptr)
{
    wlmtk_root_output_t *root_output_ptr = logged_calloc(
        1, sizeof(wlmtk_root_output_t));
    if (NULL == root_output_ptr) return NULL;
    root_output_ptr->root_ptr = ud_ptr;
    wlmtk_util_connect_listener_signal(
        &wlr_output_ptr->events.frame,
        &root_output_ptr->frame_listener,
        _wlmtk_root_output_handle_frame);
    return root_output_ptr;
}

/* ------------------------------------------------------------------------- */
/** Dtor for struct wlmtk_root_output. */
void _wlmtk_root_output_tracker_destroy(
    __UNUSED__ struct wlr_output *wlr_output_ptr,
    __UNUSED__ void *ud_ptr,
    void *output_ptr)
{
    wlmtk_root_output_t *root_output_ptr = output_ptr;
    wlmtk_util_disconnect_listener(&root_output_ptr->frame_listener);
    free(root_output_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles frame: Updates layout. */
void _wlmtk_root_output_handle_frame(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_root_output_t *root_output_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_root_output_t, frame_listener);
    wlmtk_element_layout(root_output_ptr->root_ptr->element_ptr);
    // Once redrawn, recompute pointer focus.
    wlmtk_element_pointer_motion(
        root_output_ptr->root_ptr->element_ptr,
        &root_output_ptr->root_ptr->mev);
}

/* == Unit Tests =========================================================== */

static void test_pointer_button(bs_test_t *test_ptr);
static void test_pointer_move(bs_test_t *test_ptr);

/** Test cases for the root wrapper. */
static const bs_test_case_t   _wlmtk_root_test_cases[] = {
    { true, "pointer_button", test_pointer_button },
    { true, "pointer_move", test_pointer_move },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmtk_root_test_set = BS_TEST_SET(
    true, "root", _wlmtk_root_test_cases);

/* ------------------------------------------------------------------------- */
/** Tests wlmtk_root_pointer_button. */
void test_pointer_button(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fake_element_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fake_element_ptr);
    wlmtk_element_set_visible(&fake_element_ptr->element, true);

    struct wl_display *wl_display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wl_display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr = wlr_output_layout_create(
        wl_display_ptr);

    wlmtk_root_t *root_ptr = wlmtk_root_create(
        &fake_element_ptr->element, wlr_output_layout_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, root_ptr);

    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_root_pointer_motion(root_ptr, 0, 0, 0, NULL));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        fake_element_ptr->pointer_accepts_motion_called);

    // Verify that a button down event is passed.
    struct wlr_pointer_button_event wlr_pointer_button_event = {
        .button = 42,
        .state = WL_POINTER_BUTTON_STATE_PRESSED,
        .time_msec = 4321,
    };
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_root_pointer_button(root_ptr, &wlr_pointer_button_event, 0));
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
    wlr_pointer_button_event.state = WL_POINTER_BUTTON_STATE_RELEASED;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_root_pointer_button(root_ptr, &wlr_pointer_button_event, 0));
    expected_event.type = WLMTK_BUTTON_CLICK;
    BS_TEST_VERIFY_MEMEQ(
        test_ptr,
        &expected_event,
        &fake_element_ptr->pointer_button_event,
        sizeof(wlmtk_button_event_t));

    wlmtk_root_destroy(root_ptr);
    wlmtk_element_destroy(&fake_element_ptr->element);

    wl_display_destroy(wl_display_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests pointer moves. Finicky, because ws switch renders them invisible. */
void test_pointer_move(bs_test_t *test_ptr)
{
    struct wlr_scene *wlr_scene_ptr = wlr_scene_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_scene_ptr);
    struct wl_display *wl_display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wl_display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr = wlr_output_layout_create(
        wl_display_ptr);
    struct wlr_output output = { .width = 1024, .height = 768, .scale = 1 };
    wlmtk_test_wlr_output_init(&output);
    wlr_output_layout_add_auto(wlr_output_layout_ptr, &output);
    wlmtk_desktop_t *desktop_ptr = wlmtk_desktop_create(
        wlr_scene_ptr, wlr_output_layout_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, desktop_ptr);

    wlmtk_root_t *root_ptr = wlmtk_root_create(
        wlmtk_desktop_element(desktop_ptr), wlr_output_layout_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, root_ptr);

    static const struct wlmtk_tile_style tstyle = {};
    wlmtk_workspace_t *ws1_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "1", &tstyle);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws1_ptr);
    wlmtk_desktop_add_workspace(desktop_ptr, ws1_ptr);
    wlmtk_workspace_t *ws2_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "2", &tstyle);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, ws2_ptr);
    wlmtk_desktop_add_workspace(desktop_ptr, ws2_ptr);

    wlmtk_fake_element_t *fe1 = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe1);
    wlmtk_fake_element_set_dimensions(fe1, 40, 20);
    wlmtk_window_t *w1 = wlmtk_test_window_create(&fe1->element);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w1);
    wlmtk_workspace_map_window(ws1_ptr, w1);

    wlmtk_fake_element_t *fe2 = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe2);
    wlmtk_fake_element_set_dimensions(fe2, 40, 20);
    wlmtk_window_t *w2 = wlmtk_test_window_create(&fe2->element);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w2);
    wlmtk_workspace_map_window(ws2_ptr, w2);

    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_root_pointer_motion(root_ptr, 5, 10, 42, NULL));
    wlmtk_desktop_switch_to_next_workspace(desktop_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_root_pointer_motion(root_ptr, 6, 11, 42, NULL));

    wlmtk_desktop_remove_workspace(desktop_ptr, ws2_ptr);
    wlmtk_desktop_remove_workspace(desktop_ptr, ws1_ptr);

    wlmtk_workspace_unmap_window(ws2_ptr, w2);
    wlmtk_window_destroy(w2);
    wlmtk_element_destroy(&fe2->element);
    wlmtk_workspace_unmap_window(ws1_ptr, w1);
    wlmtk_window_destroy(w1);
    wlmtk_element_destroy(&fe1->element);

    wlmtk_workspace_destroy(ws2_ptr);
    wlmtk_workspace_destroy(ws1_ptr);

    wlmtk_root_destroy(root_ptr);
    wlmtk_desktop_destroy(desktop_ptr);
    wl_display_destroy(wl_display_ptr);
    wlr_scene_node_destroy(&wlr_scene_ptr->tree.node);
}

/* == End of root.c ======================================================== */
