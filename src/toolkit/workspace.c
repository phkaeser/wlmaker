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

#include "toolkit.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the workspace. */
struct _wlmtk_workspace_t {
    /** Superclass: Container. */
    wlmtk_container_t         super_container;

    /**
     * The workspace's element map() method will expect a parent from where to
     * retrieve the wlroots scene graph tree from. As a toplevel construct,
     * there is not really a parent, so we use this fake class instead.
     *
     * TODO(kaeser@gubbe.ch): This should live in wlmaker_server_t; ultimately
     * that is the "container" that holds all workspaces.
     */
    wlmtk_container_t         fake_parent;
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

    if (!wlmtk_container_init(&workspace_ptr->super_container,
                              &workspace_container_impl)) {
        wlmtk_workspace_destroy(workspace_ptr);
        return NULL;
    }

    workspace_ptr->fake_parent.wlr_scene_tree_ptr = wlr_scene_tree_ptr;
    wlmtk_element_set_parent_container(
        &workspace_ptr->super_container.super_element,
        &workspace_ptr->fake_parent);
    wlmtk_element_set_visible(
        &workspace_ptr->super_container.super_element, true);

    return workspace_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_workspace_destroy(wlmtk_workspace_t *workspace_ptr)
{
    wlmtk_element_set_parent_container(
        &workspace_ptr->super_container.super_element,
        NULL);

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

const bs_test_case_t wlmtk_workspace_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "map_unmap", test_map_unmap },
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

/** dtor for the content under test. */
static void test_content_destroy(
    wlmtk_content_t *content_ptr)
{
    wlmtk_content_fini(content_ptr);
}
/** scene node creation for the node under test. */
static struct wlr_scene_node *test_content_create_scene_node(
    __UNUSED__ wlmtk_content_t *content_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr)
{
    struct wlr_scene_buffer *wlr_scene_buffer_ptr = wlr_scene_buffer_create(
        wlr_scene_tree_ptr, NULL);
    return &wlr_scene_buffer_ptr->node;
}
/** Method table for the node under test. */
static const wlmtk_content_impl_t test_content_impl = {
    .destroy = test_content_destroy,
    .create_scene_node = test_content_create_scene_node
};

/* ------------------------------------------------------------------------- */
/** Verifies that mapping and unmapping windows works. */
void test_map_unmap(bs_test_t *test_ptr)
{
    wlmtk_container_t *fake_parent_ptr = wlmtk_container_create_fake_parent();
    BS_ASSERT(NULL != fake_parent_ptr);

    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
        fake_parent_ptr->wlr_scene_tree_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, workspace_ptr);

    wlmtk_content_t content;
    wlmtk_content_init(&content, &test_content_impl);
    wlmtk_window_t *window_ptr = wlmtk_window_create(&content);
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
        content.super_element.wlr_scene_node_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window_element(window_ptr)->visible);

    wlmtk_workspace_unmap_window(workspace_ptr, window_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        wlmtk_window_element(window_ptr)->wlr_scene_node_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        content.super_element.wlr_scene_node_ptr);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window_element(window_ptr)->visible);

    wlmtk_window_destroy(window_ptr);
    wlmtk_workspace_destroy(workspace_ptr);
    wlmtk_container_destroy(fake_parent_ptr);
}

/* == End of workspace.c =================================================== */
