/* ========================================================================= */
/**
 * @file element.h
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
#ifndef __ELEMENT_H__
#define __ELEMENT_H__

#include "../view.h"

#include "margin.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

/** Forward declaration: Abstract base "class", the element. */
typedef struct _wlmaker_decorations_element_t wlmaker_decorations_element_t;
/** Forward declaration: Button. */
typedef struct _wlmaker_decorations_button_t wlmaker_decorations_button_t;
/** Forward declaration: Title. */
typedef struct _wlmaker_decorations_title_t wlmaker_decorations_title_t;
/** Forward declaration: An element of the resize bar. */
typedef struct _wlmaker_decorations_resize_t wlmaker_decorations_resize_t;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Initializes the element.
 *
 * An element is a wrapper around an "interactive", and adds the scene graph
 * node and optionally margins. On the long run, this should be unified with
 * the interactive.
 *
 * @param element_ptr
 * @param wlr_scene_tree_ptr  The container's scene graph tree.
 * @param data_ptr            Data to set in the scene node's `data` field.
 * @param width               Of the element, used for adding margins.
 * @param height              Of the element, used for adding margins.
 * @param edges               Which edges to add as margins, or 0 for none.
 */
bool wlmaker_decorations_element_init(
    wlmaker_decorations_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    void *data_ptr,
    unsigned width,
    unsigned height,
    uint32_t edges);

/**
 * Releases all resources for the element.
 *
 * @param element_ptr
 */
void wlmaker_decorations_element_fini(
    wlmaker_decorations_element_t *element_ptr);

/**
 * Sets position of the element, relative to the parent's scene graph tree.
 *
 * @param element_ptr
 * @param x
 * @param y
 */
void wlmaker_decorations_element_set_position(
    wlmaker_decorations_element_t *element_ptr,
    int x,
    int y);

/**
 * Creates a button element, wrapping an "element" on the button interactive.
 *
 * @param wlr_scene_tree_ptr
 * @param cursor_ptr
 * @param callback
 * @param view_ptr
 * @param button_released_ptr
 * @param button_pressed_ptr
 * @param button_blurred_ptr
 * @param edges
 *
 * @return A pointer to the button, or NULL on error. Must be free-d via
 *     @ref wlmaker_decorations_button_destroy.
 */
wlmaker_decorations_button_t *wlmaker_decorations_button_create(
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_interactive_callback_t callback,
    wlmaker_view_t *view_ptr,
    struct wlr_buffer *button_released_ptr,
    struct wlr_buffer *button_pressed_ptr,
    struct wlr_buffer *button_blurred_ptr,
    uint32_t edges);

/** Destroys the button element. */
void wlmaker_decorations_button_destroy(
    wlmaker_decorations_button_t *button_ptr);

/** Returns the base "element" for the button. */
wlmaker_decorations_element_t *wlmaker_decorations_element_from_button(
    wlmaker_decorations_button_t *button_ptr);

/**
 * Updates the textures used for the button.
 *
 * @param button_ptr
 * @param button_released_ptr
 * @param button_pressed_ptr
 * @param button_blurred_ptr
 */
void wlmaker_decorations_button_set_textures(
    wlmaker_decorations_button_t *button_ptr,
    struct wlr_buffer *button_released_ptr,
    struct wlr_buffer *button_pressed_ptr,
    struct wlr_buffer *button_blurred_ptr);

/**
 * Creates a title element, wrapping an "element" on the titlebar interactive.
 *
 * @param wlr_scene_tree_ptr
 * @param cursor_ptr
 * @param view_ptr
 * @param title_buffer_ptr
 * @param title_blurred_buffer_ptr
 *
 * @return A pointer to the title, must be free-d via
 *     @ref wlmaker_decorations_title_destroy.
 */
wlmaker_decorations_title_t *wlmaker_decorations_title_create(
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_view_t *view_ptr,
    struct wlr_buffer *title_buffer_ptr,
    struct wlr_buffer *title_blurred_buffer_ptr);

/** Destroys the title element. */
void wlmaker_decorations_title_destroy(
    wlmaker_decorations_title_t *title_ptr);

/** Returns the base "element" for the title. */
wlmaker_decorations_element_t *wlmaker_decorations_element_from_title(
    wlmaker_decorations_title_t *title_ptr);

/**
 * Updates the textures used for the title.
 *
 * @param title_ptr
 * @param title_buffer_ptr
 * @param title_blurred_buffer_ptr
 */
void wlmaker_decorations_title_set_textures(
    wlmaker_decorations_title_t *title_ptr,
    struct wlr_buffer *title_buffer_ptr,
    struct wlr_buffer *title_blurred_buffer_ptr);

/**
 * Creates a resizebar, wrapping an "element" on the resizebar interactive.
 *
 * @param wlr_scene_tree_ptr
 * @param cursor_ptr
 * @param view_ptr
 * @param resize_buffer_ptr
 * @param resize_pressed_buffer_ptr
 * @param edges               Edges controlled by this resizebar. Also used
 *                            to deduce edges for the margins.
 *
 * @return A pointer to the resizebar, must be free-d via
 *     @ref wlmaker_decorations_resize_destroy.
 */
wlmaker_decorations_resize_t *wlmaker_decorations_resize_create(
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_view_t *view_ptr,
    struct wlr_buffer *resize_buffer_ptr,
    struct wlr_buffer *resize_pressed_buffer_ptr,
    uint32_t edges);

/** Destroys the resize element. */
void wlmaker_decorations_resize_destroy(
    wlmaker_decorations_resize_t *resize_ptr);

/** Returns the base "element" for the resize. */
wlmaker_decorations_element_t *wlmaker_decorations_element_from_resize(
    wlmaker_decorations_resize_t *resize_ptr);

/**
 * Updates the textures used for the resize.
 *
 * @param resize_ptr
 * @param resize_buffer_ptr
 * @param resize_pressed_buffer_ptr
 */
void wlmaker_decorations_resize_set_textures(
    wlmaker_decorations_resize_t *resize_ptr,
    struct wlr_buffer *resize_buffer_ptr,
    struct wlr_buffer *resize_pressed_buffer_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __ELEMENT_H__ */
/* == End of element.h ===================================================== */
