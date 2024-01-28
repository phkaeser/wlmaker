/* ========================================================================= */
/**
 * @file menu.c
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

#include "menu.h"

#include "config.h"

#include <libbase/libbase.h>
#include <linux/input-event-codes.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xcursor_manager.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** State of the menu. */
typedef struct {
    /** The interactive (parent structure). */
    wlmaker_interactive_t     interactive;
    /** Back-link to the view. */
    wlmaker_view_t            *view_ptr;

    /** This menu's `wlmaker_menu_item_t` elements. */
    bs_dllist_t               menu_items;

    /** Holds the background of the menu items, with margins pre-drawn. */
    cairo_surface_t           *background_cairo_surface_ptr;
    /** Width of the menu, in pixels. */
    uint32_t                  width;
    /** Height of the menu, in pixels. */
    uint32_t                  height;

    /** The item currently under the pointer. May be NULL, if none. */
    wlmaker_menu_item_t       *focussed_item_ptr;
} wlmaker_menu_t;

static wlmaker_menu_t *menu_from_interactive(
    wlmaker_interactive_t *interactive_ptr);

static void _menu_enter(
    wlmaker_interactive_t *interactive_ptr);
static void _menu_leave(
    wlmaker_interactive_t *interactive_ptr);
static void _menu_motion(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y);
static void _menu_focus(
    wlmaker_interactive_t *interactive_ptr);
static void _menu_button(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y,
    struct wlr_pointer_button_event *wlr_pointer_button_event_ptr);
static void _menu_destroy(wlmaker_interactive_t *interactive_ptr);

static cairo_surface_t *create_background(wlmaker_menu_t *menu_ptr);
static bool items_init(
    wlmaker_menu_t *menu_ptr,
    const wlmaker_menu_item_descriptor_t *descriptor_ptr,
    void *callback_ud_ptr);
static struct wlr_buffer *create_drawn_buffer(wlmaker_menu_t *menu_ptr);
static void redraw_if_needed(wlmaker_menu_t *menu_ptr);
static void focus_item(
    wlmaker_menu_t *menu_ptr,
    wlmaker_menu_item_t *menu_item_ptr);
static void dlnode_draw(bs_dllist_node_t *node_ptr, void *ud_ptr);
static bool dlnode_contains(bs_dllist_node_t *node_ptr, void *ud_ptr);
static bool dlnode_needs_redraw(bs_dllist_node_t *dlnode_ptr, void *ud_ptr);

/* == Data ================================================================= */

/** Implementation: callbacks for the interactive. */
static const wlmaker_interactive_impl_t wlmaker_interactive_menu_impl = {
    .enter = _menu_enter,
    .leave = _menu_leave,
    .motion = _menu_motion,
    .focus = _menu_focus,
    .button = _menu_button,
    .destroy = _menu_destroy
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_interactive_t *wlmaker_menu_create(
    struct wlr_scene_buffer *wlr_scene_buffer_ptr,
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_view_t *view_ptr,
    const wlmaker_menu_item_descriptor_t *descriptor_ptr,
    void *callback_ud_ptr)
{
    wlmaker_menu_t *menu_ptr = logged_calloc(1, sizeof(wlmaker_menu_t));
    if (NULL == menu_ptr) return NULL;
    menu_ptr->view_ptr = view_ptr;

    if (!items_init(menu_ptr, descriptor_ptr, callback_ud_ptr)) {
        _menu_destroy(&menu_ptr->interactive);
        return NULL;
    }

    menu_ptr->background_cairo_surface_ptr = create_background(menu_ptr);
    if (NULL == menu_ptr->background_cairo_surface_ptr) {
        _menu_destroy(&menu_ptr->interactive);
        return NULL;
    }

    struct wlr_buffer *wlr_buffer_ptr = create_drawn_buffer(menu_ptr);
    if (NULL == wlr_buffer_ptr) {
        _menu_destroy(&menu_ptr->interactive);
        return NULL;
    }

    wlmaker_interactive_init(
        &menu_ptr->interactive,
        &wlmaker_interactive_menu_impl,
        wlr_scene_buffer_ptr,
        cursor_ptr,
        wlr_buffer_ptr);
    bs_log(BS_INFO, "Created menu %p", menu_ptr);
    return &menu_ptr->interactive;
}

/* ------------------------------------------------------------------------- */
void wlmaker_menu_get_size(
    wlmaker_interactive_t *interactive_ptr,
    uint32_t *width_ptr, uint32_t *height_ptr)
{
    const wlmaker_menu_t *menu_ptr = menu_from_interactive(interactive_ptr);
    if (NULL != width_ptr) *width_ptr = menu_ptr->width;
    if (NULL != height_ptr) *height_ptr = menu_ptr->height;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Cast (with assertion) the |interactive_ptr| to the `wlmaker_menu_t`.
 *
 * @param interactive_ptr
 *
 * @return Pointer to the corresponding `wlmaker_menu_t`
 */
wlmaker_menu_t *menu_from_interactive(
    wlmaker_interactive_t *interactive_ptr)
{
    if (NULL != interactive_ptr &&
        interactive_ptr->impl != &wlmaker_interactive_menu_impl) {
        bs_log(BS_FATAL, "Not a menu: %p", interactive_ptr);
    }
    return BS_CONTAINER_OF(interactive_ptr, wlmaker_menu_t, interactive);
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Cursor enters the menu area.
 *
 * Will adjust the cursor image to a |left_ptr|. Actual highlighting is done
 * by the _menu_motion call.
 *
 * @param interactive_ptr
 */
void _menu_enter(
    wlmaker_interactive_t *interactive_ptr)
{
    wlr_cursor_set_xcursor(
        interactive_ptr->cursor_ptr->wlr_cursor_ptr,
        interactive_ptr->cursor_ptr->wlr_xcursor_manager_ptr,
        "left_ptr");
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Cursor leaves the button area.
 *
 * Will blur (de-select) any currently focussed menu item.
 *
 * @param interactive_ptr
 */
void _menu_leave(
    wlmaker_interactive_t *interactive_ptr)
{
    wlmaker_menu_t *menu_ptr = menu_from_interactive(interactive_ptr);
    focus_item(menu_ptr, NULL);
    redraw_if_needed(menu_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Handle cursor motion.
 *
 * @param interactive_ptr
 * @param x                   New cursor x pos, relative to the interactive.
 * @param y                   New cursor y pos, relative to the interactive.
 */
void _menu_motion(
    wlmaker_interactive_t *interactive_ptr,
    double x, double y)
{
    wlmaker_menu_t *menu_ptr = menu_from_interactive(interactive_ptr);
    bs_vector_2f_t pos = BS_VECTOR_2F(x, y);
    wlmaker_menu_item_t *menu_item_ptr = wlmaker_menu_item_from_dlnode(
        bs_dllist_find(&menu_ptr->menu_items, dlnode_contains, &pos));
    focus_item(menu_ptr, menu_item_ptr);
    redraw_if_needed(menu_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Focus state changes.
 *
 * TODO: Disable the menu when the focus is lost.
 *
 * @param interactive_ptr
 */
void _menu_focus(    __UNUSED__ wlmaker_interactive_t *interactive_ptr)
{
}

/* ------------------------------------------------------------------------- */
/**
 * Interactive callback: Handle cursor button, ie. button press or release.
 *
 * TODO: Identify menu item that was activated, forward the call.
 *
 * @param interactive_ptr
 * @param x
 * @param y
 * @param wlr_pointer_button_event_ptr
 */
void _menu_button(
    wlmaker_interactive_t *interactive_ptr,
    __UNUSED__ double x,
    __UNUSED__ double y,
    struct wlr_pointer_button_event *wlr_pointer_button_event_ptr)
{
    wlmaker_menu_t *menu_ptr = menu_from_interactive(interactive_ptr);

    if (wlr_pointer_button_event_ptr->button != BTN_RIGHT) return;
    switch (wlr_pointer_button_event_ptr->state) {
    case WLR_BUTTON_RELEASED:
        if (NULL != menu_ptr->focussed_item_ptr) {
            wlmaker_menu_item_execute(menu_ptr->focussed_item_ptr);
        }
        wlmaker_view_window_menu_hide(menu_ptr->view_ptr);
        break;

    case WLR_BUTTON_PRESSED:
    default:
        break;
    }

}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the menu interactive.
 *
 * @param interactive_ptr
 */
void _menu_destroy(wlmaker_interactive_t *interactive_ptr)
{
    wlmaker_menu_t *menu_ptr = menu_from_interactive(interactive_ptr);

    if (NULL != menu_ptr->background_cairo_surface_ptr) {
        cairo_surface_destroy(menu_ptr->background_cairo_surface_ptr);
        menu_ptr->background_cairo_surface_ptr = NULL;
    }

    bs_dllist_node_t *dlnode_ptr;
    while (NULL != (dlnode_ptr = bs_dllist_pop_front(&menu_ptr->menu_items))) {
        wlmaker_menu_item_destroy(wlmaker_menu_item_from_dlnode(dlnode_ptr));
    }

    free(menu_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Creates the menu's background. Expects `menu_items` to be populated.
 *
 * @param menu_ptr
 *
 * @return A pointer to the `cairo_surface_t` holding the background. Must be
 *     destroyed via cairo_surface_destroy().
 */
cairo_surface_t *create_background(wlmaker_menu_t *menu_ptr)
{
    uint32_t w = menu_ptr->width;
    uint32_t h = menu_ptr->height;

    cairo_surface_t *surface_ptr = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, w, h);
    if (NULL == surface_ptr) {
        bs_log(BS_ERROR, "Failed cairo_image_surface_create("
               "CAIRO_FORMAT_ARGB32, %u, %u)", w, h);
        return NULL;
    }
    cairo_t *cairo_ptr = cairo_create(surface_ptr);
    if (NULL == cairo_ptr) {
        bs_log(BS_ERROR, "Failed cairo_create(%p)", cairo_ptr);
        cairo_surface_destroy(surface_ptr);
        return NULL;
    }

    // Draw the background.
    uint32_t margin = wlmaker_config_theme.menu_margin_width;
    wlmaker_primitives_cairo_fill_at(
        cairo_ptr, margin, margin, w - margin, h - margin,
        &wlmaker_config_theme.menu_fill);

    // Draw the side margins.
    float r, g, b, a;
    bs_gfxbuf_argb8888_to_floats(
        wlmaker_config_theme.menu_margin_color, &r, &g, &b, &a);
    cairo_pattern_t *cairo_pattern_ptr = cairo_pattern_create_rgba(r, g, b, a);
    if (NULL == cairo_pattern_ptr) {
        cairo_destroy(cairo_ptr);
        cairo_surface_destroy(surface_ptr);
        return NULL;
    }
    cairo_set_source(cairo_ptr, cairo_pattern_ptr);
    cairo_pattern_destroy(cairo_pattern_ptr);

    cairo_rectangle(cairo_ptr, 0, 0, w, margin);
    cairo_rectangle(cairo_ptr, 0, margin, margin, h - margin);
    cairo_rectangle(cairo_ptr, w - margin, margin, w, h - margin);
    cairo_rectangle(cairo_ptr, 0, h - margin, w, h);
    cairo_fill(cairo_ptr);

    // Draw the padding between each item.
    uint32_t pos_y = margin;
    for (bs_dllist_node_t *dlnode_ptr = menu_ptr->menu_items.head_ptr;
         dlnode_ptr != NULL && dlnode_ptr != menu_ptr->menu_items.tail_ptr;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        uint32_t desired_height;
        wlmaker_menu_item_get_desired_size(
            wlmaker_menu_item_from_dlnode(dlnode_ptr), NULL, &desired_height);
        pos_y += desired_height;
        cairo_rectangle(cairo_ptr, margin, pos_y, w - 2 * margin,
                        wlmaker_config_theme.menu_padding_width);
        cairo_fill(cairo_ptr);
        pos_y += wlmaker_config_theme.menu_padding_width;
    }

    cairo_destroy(cairo_ptr);
    return surface_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Initializes the menu items, width and height from the given descriptor.
 *
 * @param menu_ptr
 * @param desc_ptr
 * @param callback_ud_ptr     Argument to provide to item's callbacks.
 *
 * @return true on success.
 */
bool items_init(
    wlmaker_menu_t *menu_ptr,
    const wlmaker_menu_item_descriptor_t *desc_ptr,
    void *callback_ud_ptr)
{
    // First: Get width and total height of the menu.
    menu_ptr->width = 0;
    menu_ptr->height = 2 * wlmaker_config_theme.menu_margin_width;
    for ( ; desc_ptr->type != WLMAKER_MENU_ITEM_SENTINEL; desc_ptr++) {
        wlmaker_menu_item_t *item_ptr = wlmaker_menu_item_create(
            desc_ptr, callback_ud_ptr);
        if (NULL == item_ptr) return false;
        bs_dllist_push_back(&menu_ptr->menu_items,
                            wlmaker_dlnode_from_menu_item(item_ptr));

        uint32_t desired_width, desired_height;
        wlmaker_menu_item_get_desired_size(
            item_ptr, &desired_width, &desired_height);
        menu_ptr->width = BS_MAX(desired_width, menu_ptr->width);
        menu_ptr->height += desired_height;
        if ((desc_ptr + 1)->type != WLMAKER_MENU_ITEM_SENTINEL) {
            menu_ptr->height += wlmaker_config_theme.menu_padding_width;
        }
    }
    menu_ptr->width += 2 * wlmaker_config_theme.menu_margin_width;

    // Then, set the position and dimensions of each menu item.
    int pos_y = wlmaker_config_theme.menu_margin_width;
    for (bs_dllist_node_t *dlnode_ptr = menu_ptr->menu_items.head_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        uint32_t height;
        wlmaker_menu_item_get_desired_size(
            wlmaker_menu_item_from_dlnode(dlnode_ptr), NULL, &height);
        wlmaker_menu_item_set_size(
            wlmaker_menu_item_from_dlnode(dlnode_ptr),
            menu_ptr->width - 2 * wlmaker_config_theme.menu_margin_width,
            height);
        wlmaker_menu_item_set_position(
            wlmaker_menu_item_from_dlnode(dlnode_ptr),
            wlmaker_config_theme.menu_margin_width, pos_y);
        pos_y += height + wlmaker_config_theme.menu_padding_width;
    }

    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a `struct wlr_buffer` of suitable size and draws the menu into it.
 *
 * @param menu_ptr
 *
 * @return A pointer to a `struct wlr_buffer`.
 */
struct wlr_buffer *create_drawn_buffer(wlmaker_menu_t *menu_ptr)
{
    struct wlr_buffer *wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(
        menu_ptr->width, menu_ptr->height);
    if (NULL == wlr_buffer_ptr) return NULL;

    cairo_t *cairo_ptr = cairo_create_from_wlr_buffer(wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        wlr_buffer_drop(wlr_buffer_ptr);
        return NULL;
    }
    cairo_set_source_surface(
        cairo_ptr, menu_ptr->background_cairo_surface_ptr, 0, 0);
    cairo_rectangle(cairo_ptr, 0, 0, menu_ptr->width, menu_ptr->height);
    cairo_fill(cairo_ptr);
    bs_dllist_for_each(&menu_ptr->menu_items, dlnode_draw, cairo_ptr);
    cairo_destroy(cairo_ptr);

    return wlr_buffer_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Determines whether any menu item needs a redraw, then redraws if needed.
 *
 * @param menu_ptr
 */
void redraw_if_needed(wlmaker_menu_t *menu_ptr)
{
    if (bs_dllist_find(&menu_ptr->menu_items, dlnode_needs_redraw, NULL)) {
        struct wlr_buffer *wlr_buffer_ptr = create_drawn_buffer(menu_ptr);
        wlmaker_interactive_set_texture(&menu_ptr->interactive, wlr_buffer_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Sets |menu_item_ptr| as the focussed (selected) item, and de-selects any
 * previously selected item.
 *
 * @param menu_ptr
 * @param menu_item_ptr       May be NULL.
 */
void focus_item(
    wlmaker_menu_t *menu_ptr,
    wlmaker_menu_item_t *menu_item_ptr)
{
    // Nothing to do?
    if (menu_ptr->focussed_item_ptr == menu_item_ptr) return;

    if (NULL != menu_ptr->focussed_item_ptr) {
        wlmaker_menu_item_set_focus(menu_ptr->focussed_item_ptr, false);
    }
    menu_ptr->focussed_item_ptr = menu_item_ptr;
    if (NULL != menu_ptr->focussed_item_ptr) {
        wlmaker_menu_item_set_focus(menu_ptr->focussed_item_ptr, true);
    }
}

/* ------------------------------------------------------------------------- */
/** Draws |node_ptr| into the `cairo_t` at |ud_ptr|. */
void dlnode_draw(bs_dllist_node_t *node_ptr, void *ud_ptr)
{
    wlmaker_menu_item_draw(
        wlmaker_menu_item_from_dlnode(node_ptr), (cairo_t*)ud_ptr);
}

/* ------------------------------------------------------------------------- */
/** Whether the item at |node_ptr| contains the coordinates at |ud_ptr|. */
bool dlnode_contains(bs_dllist_node_t *node_ptr, void *ud_ptr)
{
    bs_vector_2f_t *pos = ud_ptr;
    return wlmaker_menu_item_contains(
        wlmaker_menu_item_from_dlnode(node_ptr), pos->x, pos->y);
}

/* ------------------------------------------------------------------------- */
/** Whether the item at |node_ptr| needs to be redrawn. */
bool dlnode_needs_redraw(bs_dllist_node_t *dlnode_ptr, __UNUSED__ void *ud_ptr)
{
    return (wlmaker_menu_item_redraw_needed(
                wlmaker_menu_item_from_dlnode(dlnode_ptr)));
}

/* == Unit tests =========================================================== */

static void test_create(bs_test_t *test_ptr);
static void test_select(bs_test_t *test_ptr);

/** Unit tests. */
const bs_test_case_t wlmaker_menu_test_cases[] = {
    { 1, "create", test_create },
    { 1, "select", test_select },
    { 0, NULL, NULL }
};

/** Menu descriptor for unit tests. */
static const wlmaker_menu_item_descriptor_t test_descriptors[] = {
    WLMAKER_MENU_ITEM_DESCRIPTOR_ENTRY("entry1", NULL),
    WLMAKER_MENU_ITEM_DESCRIPTOR_ENTRY("entry2", NULL),
    WLMAKER_MENU_ITEM_DESCRIPTOR_ENTRY("entry3", NULL),
    WLMAKER_MENU_ITEM_DESCRIPTOR_SENTINEL(),
};

/** Tests create and destroy methods of the menu, useful for leak checks. */
void test_create(bs_test_t *test_ptr)
{
    wlmaker_server_t server;
    memset(&server, 0, sizeof(wlmaker_server_t));
    server.wlr_scene_ptr = wlr_scene_create();
    struct wlr_scene_buffer *wlr_scene_buffer_ptr = wlr_scene_buffer_create(
        &server.wlr_scene_ptr->tree, NULL);

    wlmaker_interactive_t *i_ptr = wlmaker_menu_create(
        wlr_scene_buffer_ptr,
        NULL,  // wlmaker_cursor_t.
        NULL,  // wlmaker_view_t.
        test_descriptors,
        NULL);  // callback_ud_ptr.
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, i_ptr);

    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(wlr_scene_buffer_ptr->buffer),
        "menu.png");

    _menu_destroy(i_ptr);
}

/** Tests that the items are selected as desired. */
void test_select(bs_test_t *test_ptr)
{
    wlmaker_server_t server;
    memset(&server, 0, sizeof(wlmaker_server_t));
    server.wlr_scene_ptr = wlr_scene_create();
    struct wlr_scene_buffer *wlr_scene_buffer_ptr = wlr_scene_buffer_create(
        &server.wlr_scene_ptr->tree, NULL);

    wlmaker_interactive_t *i_ptr = wlmaker_menu_create(
        wlr_scene_buffer_ptr,
        NULL,  // wlmaker_cursor_t.
        NULL,  // wlmaker_view_t.
        test_descriptors,
        NULL);  // callback_ud_ptr.
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, i_ptr);

    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(wlr_scene_buffer_ptr->buffer),
        "menu.png");

    _menu_motion(i_ptr, 10, 10);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(wlr_scene_buffer_ptr->buffer),
        "menu_1.png");

    _menu_motion(i_ptr, 10, 30);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(wlr_scene_buffer_ptr->buffer),
        "menu_2.png");

    _menu_motion(i_ptr, 10, 50);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(wlr_scene_buffer_ptr->buffer),
        "menu_3.png");

    _menu_motion(i_ptr, 10, 100);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr,
        bs_gfxbuf_from_wlr_buffer(wlr_scene_buffer_ptr->buffer),
        "menu.png");

    _menu_destroy(i_ptr);
}

/* == End of menu.c ======================================================== */
