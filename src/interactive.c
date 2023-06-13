/* ========================================================================= */
/**
 * @file interactive.c
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

#include "interactive.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
void wlmaker_interactive_init(
    wlmaker_interactive_t *interactive_ptr,
    const wlmaker_interactive_impl_t *impl_ptr,
    struct wlr_scene_buffer *wlr_scene_buffer_ptr,
    wlmaker_cursor_t *cursor_ptr,
    struct wlr_buffer *initial_wlr_buffer_ptr)
{
    interactive_ptr->impl = impl_ptr;
    interactive_ptr->wlr_scene_buffer_ptr = wlr_scene_buffer_ptr;
    interactive_ptr->cursor_ptr = cursor_ptr;

    wlmaker_interactive_set_texture(interactive_ptr, initial_wlr_buffer_ptr);
    wlr_scene_node_set_enabled(
        &interactive_ptr->wlr_scene_buffer_ptr->node,
        true);
}

/* ------------------------------------------------------------------------- */
void wlmaker_interactive_set_texture(
    wlmaker_interactive_t *interactive_ptr,
    struct wlr_buffer *wlr_buffer_ptr)
{
    interactive_ptr->width = wlr_buffer_ptr->width;
    interactive_ptr->height = wlr_buffer_ptr->height;
    wlr_scene_buffer_set_buffer(
        interactive_ptr->wlr_scene_buffer_ptr,
        wlr_buffer_ptr);
    wlr_scene_buffer_set_dest_size(
        interactive_ptr->wlr_scene_buffer_ptr,
        interactive_ptr->width,
        interactive_ptr->height);
}

/* ------------------------------------------------------------------------- */
int wlmaker_interactive_node_cmp(const bs_avltree_node_t *node_ptr,
                                 const void *key_ptr)
{
    wlmaker_interactive_t *interactive_ptr = wlmaker_interactive_from_avlnode(
        (bs_avltree_node_t*)node_ptr);

    void *node_key_ptr = &interactive_ptr->wlr_scene_buffer_ptr->node;
    if (node_key_ptr < key_ptr) {
        return -1;
    } else if (node_key_ptr > key_ptr) {
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
void wlmaker_interactive_node_destroy(bs_avltree_node_t *node_ptr)
{
    wlmaker_interactive_t* i_ptr = wlmaker_interactive_from_avlnode(node_ptr);
    i_ptr->impl->destroy(i_ptr);
}

/* ------------------------------------------------------------------------- */
wlmaker_interactive_t *wlmaker_interactive_from_avlnode(
    bs_avltree_node_t *node_ptr)
{
    return BS_CONTAINER_OF(node_ptr, wlmaker_interactive_t, avlnode);
}

/* == End of interactive.c ================================================= */
