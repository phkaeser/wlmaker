/* ========================================================================= */
/**
 * @file element.c
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

#include "element.h"

#include <libbase/libbase.h>

#include "../button.h"
#include "../resizebar.h"
#include "../titlebar.h"

/* == Declarations ========================================================= */

/** Decorations element. */
struct _wlmaker_decorations_element_t {
    /** Scene graph node holding the element. */
    struct wlr_scene_buffer   *wlr_scene_buffer_ptr;

    /**
     * Interactive for the element. To be created by instantiated element, will
     * be destroyed in @ref wlmaker_decorations_element_fini.
     */
    wlmaker_interactive_t     *interactive_ptr;

    /**
     * Margin of the element, or NULL.
     *
     * TODO(kaeser@gubbe.ch): Consider moving this to the "container".
     */
    wlmaker_decorations_margin_t *margin_ptr;
};

/** State of a button. */
struct _wlmaker_decorations_button_t {
    /** The base element. */
    wlmaker_decorations_element_t element;
    /** Back-link. */
    wlmaker_view_t            *view_ptr;
};

/** State of a title. */
struct _wlmaker_decorations_title_t {
    /** The base element. */
    wlmaker_decorations_element_t element;
    /** Back-link. */
    wlmaker_view_t            *view_ptr;
};

/** State of a resize. */
struct _wlmaker_decorations_resize_t {
    /** The base element. */
    wlmaker_decorations_element_t element;
    /** Back-link. */
    wlmaker_view_t            *view_ptr;
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmaker_decorations_element_init(
    wlmaker_decorations_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    void *data_ptr,
    unsigned width,
    unsigned height,
    uint32_t edges)
{
    BS_ASSERT(NULL == element_ptr->wlr_scene_buffer_ptr);

    element_ptr->wlr_scene_buffer_ptr = wlr_scene_buffer_create(
        wlr_scene_tree_ptr, NULL);
    if (NULL == element_ptr->wlr_scene_buffer_ptr) {
        wlmaker_decorations_element_fini(element_ptr);
        return false;
    }
    element_ptr->wlr_scene_buffer_ptr->node.data = data_ptr;

    if (0 != edges) {
        element_ptr->margin_ptr = wlmaker_decorations_margin_create(
            wlr_scene_tree_ptr,
            0, 0, width, height,  // element_ptr->width, element_ptr->height,
            edges);
    }

    return true;
}

/* ------------------------------------------------------------------------- */
void wlmaker_decorations_element_fini(
    wlmaker_decorations_element_t *element_ptr)
{
    if (NULL != element_ptr->margin_ptr) {
        wlmaker_decorations_margin_destroy(element_ptr->margin_ptr);
        element_ptr->margin_ptr = NULL;
    }

    if (NULL != element_ptr->interactive_ptr) {
        wlmaker_interactive_node_destroy(
            &element_ptr->interactive_ptr->avlnode);
        element_ptr->interactive_ptr = NULL;
    }

    if (NULL != element_ptr->wlr_scene_buffer_ptr) {
        wlr_scene_node_destroy(&element_ptr->wlr_scene_buffer_ptr->node);
        element_ptr->wlr_scene_buffer_ptr = NULL;
    }
}

/* ------------------------------------------------------------------------- */
void wlmaker_decorations_element_set_position(
    wlmaker_decorations_element_t *element_ptr,
    int x,
    int y)
{
    wlr_scene_node_set_position(
        &element_ptr->wlr_scene_buffer_ptr->node, x, y);
    if (NULL != element_ptr->margin_ptr) {
        wlmaker_decorations_margin_set_position(
            element_ptr->margin_ptr, x, y);
    }
}

/* ------------------------------------------------------------------------- */
wlmaker_decorations_button_t *wlmaker_decorations_button_create(
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_interactive_callback_t callback,
    wlmaker_view_t *view_ptr,
    struct wlr_buffer *button_released_ptr,
    struct wlr_buffer *button_pressed_ptr,
    struct wlr_buffer *button_blurred_ptr,
    uint32_t edges)
{
    BS_ASSERT(button_released_ptr->width == button_pressed_ptr->width);
    BS_ASSERT(button_released_ptr->width == button_blurred_ptr->width);
    BS_ASSERT(button_released_ptr->height == button_pressed_ptr->height);
    BS_ASSERT(button_released_ptr->height == button_blurred_ptr->height);

    wlmaker_decorations_button_t *button_ptr = logged_calloc(
        1, sizeof(wlmaker_decorations_button_t));
    if (NULL == button_ptr) return NULL;
    if (!wlmaker_decorations_element_init(
            &button_ptr->element,
            wlr_scene_tree_ptr,
            view_ptr,
            button_released_ptr->width,
            button_released_ptr->height,
            edges)) {
        wlmaker_decorations_button_destroy(button_ptr);
        return NULL;
    }

    button_ptr->element.interactive_ptr = wlmaker_button_create(
        button_ptr->element.wlr_scene_buffer_ptr,
        cursor_ptr,
        callback,
        view_ptr,
        button_released_ptr,
        button_pressed_ptr,
        button_blurred_ptr);
    if (NULL == button_ptr->element.interactive_ptr) {
        wlmaker_decorations_button_destroy(button_ptr);
        return NULL;
    }

    wlmaker_interactive_focus(
        button_ptr->element.interactive_ptr,
        view_ptr->active);

    if (!bs_avltree_insert(
            view_ptr->interactive_tree_ptr,
            &button_ptr->element.interactive_ptr->wlr_scene_buffer_ptr->node,
            &button_ptr->element.interactive_ptr->avlnode,
            false)) {
        bs_log(BS_ERROR, "Unexpected: Fail to insert into tree.");
        wlmaker_decorations_button_destroy(button_ptr);
        return NULL;
    }
    button_ptr->view_ptr = view_ptr;

    return button_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_decorations_button_destroy(
    wlmaker_decorations_button_t *button_ptr)
{
    if (NULL != button_ptr->view_ptr) {
        bs_avltree_delete(
            button_ptr->view_ptr->interactive_tree_ptr,
            &button_ptr->element.interactive_ptr->wlr_scene_buffer_ptr->node);
        button_ptr->view_ptr = NULL;
    }

    // The interactive is deleted in element_fini().

    wlmaker_decorations_element_fini(&button_ptr->element);
    free(button_ptr);
}

/* ------------------------------------------------------------------------- */
wlmaker_decorations_element_t *wlmaker_decorations_element_from_button(
    wlmaker_decorations_button_t *button_ptr)
{
    return &button_ptr->element;
}

/* ------------------------------------------------------------------------- */
void wlmaker_decorations_button_set_textures(
    wlmaker_decorations_button_t *button_ptr,
    struct wlr_buffer *button_released_ptr,
    struct wlr_buffer *button_pressed_ptr,
    struct wlr_buffer *button_blurred_ptr)
{
    BS_ASSERT(button_released_ptr->width == button_pressed_ptr->width);
    BS_ASSERT(button_released_ptr->width == button_blurred_ptr->width);
    BS_ASSERT(button_released_ptr->height == button_pressed_ptr->height);
    BS_ASSERT(button_released_ptr->height == button_blurred_ptr->height);

    wlmaker_button_set_textures(
        button_ptr->element.interactive_ptr,
        button_released_ptr,
        button_pressed_ptr,
        button_blurred_ptr);

    if (NULL != button_ptr->element.margin_ptr) {
        wlmaker_decorations_margin_set_size(
            button_ptr->element.margin_ptr,
            button_released_ptr->width,
            button_released_ptr->height);
    }
}

/* ------------------------------------------------------------------------- */
wlmaker_decorations_title_t *wlmaker_decorations_title_create(
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_view_t *view_ptr,
    struct wlr_buffer *title_buffer_ptr,
    struct wlr_buffer *title_blurred_buffer_ptr)
{
    BS_ASSERT(title_buffer_ptr->width == title_blurred_buffer_ptr->width);
    BS_ASSERT(title_buffer_ptr->height == title_blurred_buffer_ptr->height);

    wlmaker_decorations_title_t *title_ptr = logged_calloc(
        1, sizeof(wlmaker_decorations_title_t));
    if (NULL == title_ptr) return NULL;
    if (!wlmaker_decorations_element_init(
            &title_ptr->element,
            wlr_scene_tree_ptr,
            view_ptr,
            title_buffer_ptr->width,
            title_buffer_ptr->height,
            WLR_EDGE_LEFT | WLR_EDGE_TOP | WLR_EDGE_RIGHT)) {
        wlmaker_decorations_title_destroy(title_ptr);
        return NULL;
    }

    title_ptr->element.interactive_ptr = wlmaker_titlebar_create(
        title_ptr->element.wlr_scene_buffer_ptr,
        cursor_ptr,
        view_ptr,
        title_buffer_ptr,
        title_blurred_buffer_ptr);
    if (NULL == title_ptr->element.interactive_ptr) {
        wlmaker_decorations_title_destroy(title_ptr);
        return NULL;
    }

    wlmaker_interactive_focus(
        title_ptr->element.interactive_ptr,
        view_ptr->active);

    if (!bs_avltree_insert(
            view_ptr->interactive_tree_ptr,
            &title_ptr->element.interactive_ptr->wlr_scene_buffer_ptr->node,
            &title_ptr->element.interactive_ptr->avlnode,
            false)) {
        bs_log(BS_ERROR, "Unexpected: Fail to insert into tree.");
        wlmaker_decorations_title_destroy(title_ptr);
        return NULL;
    }
    title_ptr->view_ptr = view_ptr;

    return title_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_decorations_title_destroy(
    wlmaker_decorations_title_t *title_ptr)
{
    if (NULL != title_ptr->view_ptr) {
        bs_avltree_delete(
            title_ptr->view_ptr->interactive_tree_ptr,
            &title_ptr->element.interactive_ptr->wlr_scene_buffer_ptr->node);
        title_ptr->view_ptr = NULL;
    }

    // The interactive is deleted in element_fini().

    wlmaker_decorations_element_fini(&title_ptr->element);
    free(title_ptr);
}

/* ------------------------------------------------------------------------- */
wlmaker_decorations_element_t *wlmaker_decorations_element_from_title(
    wlmaker_decorations_title_t *title_ptr)
{
    return &title_ptr->element;
}

/* ------------------------------------------------------------------------- */
void wlmaker_decorations_title_set_textures(
    wlmaker_decorations_title_t *title_ptr,
    struct wlr_buffer *title_buffer_ptr,
    struct wlr_buffer *title_blurred_buffer_ptr)
{
    BS_ASSERT(title_buffer_ptr->width == title_blurred_buffer_ptr->width);
    BS_ASSERT(title_buffer_ptr->height == title_blurred_buffer_ptr->height);

    wlmaker_title_set_texture(
        title_ptr->element.interactive_ptr,
        title_buffer_ptr,
        title_blurred_buffer_ptr);

    if (NULL != title_ptr->element.margin_ptr) {
        wlmaker_decorations_margin_set_size(
            title_ptr->element.margin_ptr,
            title_buffer_ptr->width,
            title_buffer_ptr->height);
    }
}

/* ------------------------------------------------------------------------- */
wlmaker_decorations_resize_t *wlmaker_decorations_resize_create(
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_view_t *view_ptr,
    struct wlr_buffer *resize_buffer_ptr,
    struct wlr_buffer *resize_pressed_buffer_ptr,
    uint32_t edges)
{
    BS_ASSERT(resize_buffer_ptr->width == resize_pressed_buffer_ptr->width);
    BS_ASSERT(resize_buffer_ptr->height == resize_pressed_buffer_ptr->height);

    wlmaker_decorations_resize_t *resize_ptr = logged_calloc(
        1, sizeof(wlmaker_decorations_resize_t));
    if (NULL == resize_ptr) return NULL;
    if (!wlmaker_decorations_element_init(
            &resize_ptr->element,
            wlr_scene_tree_ptr,
            view_ptr,
            resize_buffer_ptr->width,
            resize_buffer_ptr->height,
            edges)) {
        wlmaker_decorations_resize_destroy(resize_ptr);
        return NULL;
    }

    resize_ptr->element.interactive_ptr = wlmaker_resizebar_create(
        resize_ptr->element.wlr_scene_buffer_ptr,
        cursor_ptr,
        view_ptr,
        resize_buffer_ptr,
        resize_pressed_buffer_ptr,
        edges);
    if (NULL == resize_ptr->element.interactive_ptr) {
        wlmaker_decorations_resize_destroy(resize_ptr);
        return NULL;
    }

    if (!bs_avltree_insert(
            view_ptr->interactive_tree_ptr,
            &resize_ptr->element.interactive_ptr->wlr_scene_buffer_ptr->node,
            &resize_ptr->element.interactive_ptr->avlnode,
            false)) {
        bs_log(BS_ERROR, "Unexpected: Fail to insert into tree.");
        wlmaker_decorations_resize_destroy(resize_ptr);
        return NULL;
    }
    resize_ptr->view_ptr = view_ptr;

    return resize_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_decorations_resize_destroy(
    wlmaker_decorations_resize_t *resize_ptr)
{
    if (NULL != resize_ptr->view_ptr) {
        bs_avltree_delete(
            resize_ptr->view_ptr->interactive_tree_ptr,
            &resize_ptr->element.interactive_ptr->wlr_scene_buffer_ptr->node);
        resize_ptr->view_ptr = NULL;
    }

    // The interactive is deleted in element_fini().

    wlmaker_decorations_element_fini(&resize_ptr->element);
    free(resize_ptr);
}

/* ------------------------------------------------------------------------- */
wlmaker_decorations_element_t *wlmaker_decorations_element_from_resize(
    wlmaker_decorations_resize_t *resize_ptr)
{
    return &resize_ptr->element;
}

/* ------------------------------------------------------------------------- */
void wlmaker_decorations_resize_set_textures(
    wlmaker_decorations_resize_t *resize_ptr,
    struct wlr_buffer *resize_buffer_ptr,
    struct wlr_buffer *resize_pressed_buffer_ptr)
{
    BS_ASSERT(resize_buffer_ptr->width == resize_pressed_buffer_ptr->width);
    BS_ASSERT(resize_buffer_ptr->height == resize_pressed_buffer_ptr->height);

    wlmaker_resizebar_set_textures(
        resize_ptr->element.interactive_ptr,
        resize_buffer_ptr,
        resize_pressed_buffer_ptr);

    if (NULL != resize_ptr->element.margin_ptr) {
        wlmaker_decorations_margin_set_size(
            resize_ptr->element.margin_ptr,
            resize_buffer_ptr->width,
            resize_buffer_ptr->height);
    }
}

/* == End of element.c ===================================================== */
