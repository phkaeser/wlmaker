/* ========================================================================= */
/**
 * @file menu_item.h
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
#ifndef __MENU_ITEM_H__
#define __MENU_ITEM_H__

#include <libbase/libbase.h>
#include <cairo.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Types of menu items. */
typedef enum {
    WLMAKER_MENU_ITEM_SENTINEL = 0,
    WLMAKER_MENU_ITEM_ENTRY,
    WLMAKER_MENU_ITEM_SEPARATOR
} wlmaker_menu_item_type_t;

/** Forward definition of the menu item handler. */
typedef struct _wlmaker_menu_item_t wlmaker_menu_item_t;

/** Defines the parameters of one menu item. */
typedef struct {
    /** Type of the menu item. */
    wlmaker_menu_item_type_t   type;
    /** Parameters of the menu item. */
    union {
        /** Parameters for a menu entry. */
        struct {
            /** Label. */
            char              *label_ptr;
            /** Callback. */
            void (*callback)(void *ud_ptr);
        } entry;
    } param;
} wlmaker_menu_item_descriptor_t;

/** Defines a menu entry descriptor. */
#define WLMAKER_MENU_ITEM_DESCRIPTOR_ENTRY(_label, _callback) { \
    .type = WLMAKER_MENU_ITEM_ENTRY,                            \
        .param = { .entry = {                                   \
        .label_ptr = _label,                                    \
        .callback = _callback                                   \
    } }                                                         \
}

/** Defines a sentinel descriptor. */
#define WLMAKER_MENU_ITEM_DESCRIPTOR_SENTINEL() {          \
    .type = WLMAKER_MENU_ITEM_SENTINEL                     \
}


/**
 * Creates a menu item from the given descriptor.
 *
 * @param desc_ptr
 * @param callback_ud_ptr     Argument to provide to item's callbacks.
 *
 * @return A pointer to a `wlmaker_menu_item_t` or NULL on error. Must be
 *     destroyed by calling wlmaker_menu_item_destroy().
 */
wlmaker_menu_item_t *wlmaker_menu_item_create(
    const wlmaker_menu_item_descriptor_t *desc_ptr,
    void *callback_ud_ptr);

/**
 * Destroys a menu item previously created by wlmaker_menu_item_create().
 *
 * @param menu_item_ptr
 */
void wlmaker_menu_item_destroy(wlmaker_menu_item_t *menu_item_ptr);

/**
 * Retrieves the desired size by the menu item.
 *
 * This provides the size sufficient to show the full menu item information. If
 * the menu opts to draw the item with a smaller size, some information may be
 * omitted, eg. the label might get clipped.
 *
 * @param menu_item_ptr
 * @param width_ptr           May be NULL.
 * @param height_ptr          May be NULL.
 */
void wlmaker_menu_item_get_desired_size(
    const wlmaker_menu_item_t *menu_item_ptr,
    uint32_t *width_ptr, uint32_t *height_ptr);

/**
 * Sets the size of the menu item. Will be used throughout subsequent draw
 * operations.
 *
 * @param menu_item_ptr
 * @param width
 * @param height
 */
void wlmaker_menu_item_set_size(
    wlmaker_menu_item_t *menu_item_ptr,
    uint32_t width,
    uint32_t height);

/**
 * Sets the size of this menu item, relative to the `cairo_t` it will draw in.
 *
 * @param menu_item_ptr
 * @param x
 * @param y
 */
void wlmaker_menu_item_set_position(
    wlmaker_menu_item_t *menu_item_ptr,
    uint32_t x,
    uint32_t y);

/**
 * Draws the menu item. Uses the position and size set previously.
 *
 * @param menu_item_ptr
 * @param cairo_ptr
 */
void wlmaker_menu_item_draw(
    wlmaker_menu_item_t *menu_item_ptr,
    cairo_t *cairo_ptr);

/**
 * Cast: Returns the a pointer to the `bs_dllist_node_t` of |item_ptr|.
 *
 * @param item_ptr
 *
 * @return The pointer.
 */
bs_dllist_node_t *wlmaker_dlnode_from_menu_item(
    wlmaker_menu_item_t *item_ptr);

/**
 * Cast: Returns the `wlmaker_menu_item_t` holding the |dlnode_ptr|.
 *
 * @param dlnode_ptr
 *
 * @return The pointer.
 */
wlmaker_menu_item_t *wlmaker_menu_item_from_dlnode(
    bs_dllist_node_t *dlnode_ptr);

/**
 * Sets the pointer focus state of the menu item (show as selected).
 *
 * @param menu_item_ptr
 * @param focussed
 */
void wlmaker_menu_item_set_focus(
    wlmaker_menu_item_t *menu_item_ptr,
    bool focussed);

/**
 * Returns whether the menu item contains |x|, |y|.
 *
 * This is satisfied if |x| is in [item.x, item.x + width) and
 * |y| is in [item.y, item.y + height).
 *
 * @param menu_item_ptr
 * @param x
 * @param y
 */
bool wlmaker_menu_item_contains(
    const wlmaker_menu_item_t *menu_item_ptr,
    double x,
    double y);

/**
 * Returns whether the menu item should be redrawn.
 *
 * This is the case when the state becomes different from the drawn state.
 *
 * @param menu_item_ptr
 */
bool wlmaker_menu_item_redraw_needed(
    const wlmaker_menu_item_t *menu_item_ptr);

/**
 * Executes the action associated with the menu item, ie. call the callback.
 *
 * @param menu_item_ptr
 */
void wlmaker_menu_item_execute(
    const wlmaker_menu_item_t *menu_item_ptr);

/** Unit tests. */
extern const bs_test_case_t wlmaker_menu_item_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __MENU_ITEM_H__ */
/* == End of menu_item.h =================================================== */
