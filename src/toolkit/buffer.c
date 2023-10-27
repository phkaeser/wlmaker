/* ========================================================================= */
/**
 * @file buffer.c
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

#include "buffer.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

static void element_destroy(wlmtk_element_t *element_ptr);
static struct wlr_scene_node *element_create_scene_node(
    wlmtk_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr);
static void element_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr);

/* == Data ================================================================= */

/** Method table for the buffer's virtual methods. */
static const wlmtk_element_impl_t super_element_impl = {
    .destroy = element_destroy,
    .create_scene_node = element_create_scene_node,
    .get_dimensions = element_get_dimensions,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_buffer_init(
    wlmtk_buffer_t *buffer_ptr,
    const wlmtk_buffer_impl_t *buffer_impl_ptr,
    struct wlr_buffer *wlr_buffer_ptr)
{
    BS_ASSERT(NULL != buffer_ptr);
    memset(buffer_ptr, 0, sizeof(wlmtk_buffer_t));
    BS_ASSERT(NULL != buffer_impl_ptr);
    BS_ASSERT(NULL != buffer_impl_ptr->destroy);
    memcpy(&buffer_ptr->impl, buffer_impl_ptr, sizeof(wlmtk_buffer_impl_t));

    if (!wlmtk_element_init(
            &buffer_ptr->super_element, &super_element_impl)) {
        return false;
    }

    wlmtk_buffer_set(buffer_ptr, wlr_buffer_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_buffer_fini(wlmtk_buffer_t *buffer_ptr)
{
    if (NULL != buffer_ptr->wlr_buffer_ptr) {
        wlr_buffer_unlock(buffer_ptr->wlr_buffer_ptr);
        buffer_ptr->wlr_buffer_ptr = NULL;
    }

    if (NULL != buffer_ptr->super_element.wlr_scene_node_ptr) {
        // TODO: Wire up a destry listener, and clear the local pointer
        // if (NULL != buffer_ptr->wlr_scene_buffer_ptr) {
        wlr_scene_node_destroy(&buffer_ptr->wlr_scene_buffer_ptr->node);
        buffer_ptr->wlr_scene_buffer_ptr = NULL;
    }

    wlmtk_element_fini(&buffer_ptr->super_element);
}

/* ------------------------------------------------------------------------- */
void wlmtk_buffer_set(
    wlmtk_buffer_t *buffer_ptr,
    struct wlr_buffer *wlr_buffer_ptr)
{
    if (NULL != buffer_ptr->wlr_buffer_ptr) {
        wlr_buffer_unlock(buffer_ptr->wlr_buffer_ptr);
    }
    buffer_ptr->wlr_buffer_ptr = wlr_buffer_lock(wlr_buffer_ptr);

    if (NULL != buffer_ptr->wlr_scene_buffer_ptr) {
        wlr_scene_buffer_set_buffer(
            buffer_ptr->wlr_scene_buffer_ptr,
            buffer_ptr->wlr_buffer_ptr);
    }
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the superclass wlmtk_element_t::destroy method.
 *
 * Forwards the call to the wlmtk_buffer_t::destroy method.
 *
 * @param element_ptr
 */
void element_destroy(wlmtk_element_t *element_ptr)
{
    wlmtk_buffer_t *buffer_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_buffer_t, super_element);
    buffer_ptr->impl.destroy(buffer_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the superclass wlmtk_element_t::create_scene_node method.
 *
 * Creates a `struct wlr_scene_buffer` attached to `wlr_scene_tree_ptr`.
 *
 * @param element_ptr
 * @param wlr_scene_tree_ptr
 */
struct wlr_scene_node *element_create_scene_node(
    wlmtk_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr)
{
    wlmtk_buffer_t *buffer_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_buffer_t, super_element);

    BS_ASSERT(NULL == buffer_ptr->wlr_scene_buffer_ptr);
    buffer_ptr->wlr_scene_buffer_ptr = wlr_scene_buffer_create(
        wlr_scene_tree_ptr,
        buffer_ptr->wlr_buffer_ptr);
    BS_ASSERT(NULL != buffer_ptr->wlr_scene_buffer_ptr);

    return &buffer_ptr->wlr_scene_buffer_ptr->node;
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the element's get_dimensions method: Return dimensions.
 *
 * @param element_ptr
 * @param left_ptr            Leftmost position. May be NULL.
 * @param top_ptr             Topmost position. May be NULL.
 * @param right_ptr           Rightmost position. Ma be NULL.
 * @param bottom_ptr          Bottommost position. May be NULL.
 */
void element_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr)
{
    wlmtk_buffer_t *buffer_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_buffer_t, super_element);

    if (NULL != left_ptr) *left_ptr = 0;
    if (NULL != top_ptr) *top_ptr = 0;
    if (NULL != right_ptr) *right_ptr = buffer_ptr->wlr_buffer_ptr->width;
    if (NULL != bottom_ptr) *bottom_ptr = buffer_ptr->wlr_buffer_ptr->height;
}

/* == End of buffer.c ====================================================== */
