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

#include "container.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output_layout.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** Window handle. */
struct _wlmtk_window2_t {
    /** Composed of : Holds decoration, popup container and content. */
    wlmtk_container_t         container;

    /** Events for this window. */
    wlmtk_window2_events_t    events;

    /** The content. */
    wlmtk_element_t           *content_element_ptr;

    /** The workspace, when mapped to a workspace. NULL otherwise. */
    wlmtk_workspace_t         *workspace_ptr;

    /** Window style. */
    wlmtk_window_style_t      style;
    /** Menu style. */
    wlmtk_menu_style_t        menu_style;
};

/* == Data ================================================================= */

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

    if (!wlmtk_container_init(&window_ptr->container)) {
        wlmtk_window2_destroy(window_ptr);
        return NULL;
    }

    wlmtk_container_add_element(&window_ptr->container, content_element_ptr);
    window_ptr->content_element_ptr = content_element_ptr;

    return window_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_destroy(wlmtk_window2_t *window_ptr)
{
    if (NULL != window_ptr->content_element_ptr) {
        wlmtk_container_remove_element(
            &window_ptr->container,
            window_ptr->content_element_ptr);
        window_ptr->content_element_ptr = NULL;
    }
    wlmtk_container_fini(&window_ptr->container);

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
    return &window_ptr->container.super_element;
}

/* ------------------------------------------------------------------------- */
wlmtk_window2_t *wlmtk_window2_from_element(wlmtk_element_t *element_ptr)
{
    return BS_CONTAINER_OF(
        element_ptr, wlmtk_window2_t, container.super_element);
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

/* == Local (static) methods =============================================== */

/* == Unit Tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_window2_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises ctor and dtor. */
static void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fe = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe);

    wlmtk_window_style_t s = {};
    wlmtk_menu_style_t ms = {};
    wlmtk_window2_t *window_ptr = wlmtk_window2_create(&fe->element, &s, &ms);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, window_ptr);

    wlmtk_element_t *e = wlmtk_window2_element(window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, window_ptr, wlmtk_window2_from_element(e));

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
        &wlmtk_window2_events(window_ptr)->state_changed, &l);

    wlmtk_window2_set_workspace(window_ptr, workspace_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        workspace_ptr,
        wlmtk_window2_get_workspace(window_ptr));

    wlmtk_util_clear_test_listener(&l);
    wlmtk_window2_set_workspace(window_ptr, workspace_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, l.calls);

    wlmtk_workspace_destroy(workspace_ptr);
    wl_display_destroy(display_ptr);

    wlmtk_window2_destroy(window_ptr);
    wlmtk_element_destroy(&fe->element);
}

/* == End of window2.c ===================================================== */
