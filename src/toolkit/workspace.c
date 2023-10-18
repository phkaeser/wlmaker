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

/** State of the workspace. */
struct _wlmtk_workspace_t {
    /** Superclass: Container. */
    wlmtk_container_t         super_container;
};

static void workspace_container_destroy(wlmtk_container_t *container_ptr);

/** Method table for the container's virtual methods. */
const wlmtk_container_impl_t  workspace_container_impl = {
    .destroy = workspace_container_destroy
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
    BS_ASSERT(container_ptr->impl_ptr == &workspace_container_impl);
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
// FIXME : Add tests for button.
void wlmtk_workspace_button(
    wlmtk_workspace_t *workspace_ptr,
    const struct wlr_pointer_button_event *event_ptr)
{
    wlmtk_button_event_t event;
    wlmtk_element_t *focused_element_ptr;

    // Guard clause: nothing to pass on if no element has the focus.
    focused_element_ptr =
        workspace_ptr->super_container.pointer_focus_element_ptr;
    if (NULL == focused_element_ptr) return;

    event.button = event_ptr->button;
    if (WLR_BUTTON_PRESSED == event_ptr->state) {
        event.type = WLMTK_BUTTON_DOWN;
        wlmtk_element_pointer_button(focused_element_ptr, &event);

    } else if (WLR_BUTTON_RELEASED == event_ptr->state) {
        event.type = WLMTK_BUTTON_UP;
        wlmtk_element_pointer_button(focused_element_ptr, &event);
        event.type = WLMTK_BUTTON_CLICK;
        wlmtk_element_pointer_button(focused_element_ptr, &event);

    } else {
        bs_log(BS_WARNING,
               "Workspace %p: Unhandled state 0x%x for button 0x%x",
               workspace_ptr, event_ptr->state, event_ptr->button);
    }

    event = event;
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

    fake_element_ptr->pointer_motion_return_value = true;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_workspace_motion(workspace_ptr, 0, 0, 1234));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        fake_element_ptr->pointer_motion_called);

    wlmtk_container_remove_element(
        &workspace_ptr->super_container, &fake_element_ptr->element);

    wlmtk_element_destroy(&fake_element_ptr->element);
    wlmtk_workspace_destroy(workspace_ptr);
    wlmtk_container_destroy(fake_parent_ptr);
}

/* == End of workspace.c =================================================== */
