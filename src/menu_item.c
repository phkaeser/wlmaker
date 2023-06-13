/* ========================================================================= */
/**
 * @file menu_item.c
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

#include "menu_item.h"

#include <libbase/libbase.h>

#include "config.h"
#include "toolkit/toolkit.h"

/* == Declarations ========================================================= */

/** State of a menu item. */
typedef enum {
    /** Undefined: May not have been drawn or is initializing. */
    WLMAKER_MENU_ITEM_STATE_UNDEFINED = 0,
    /** Disabled: Cannot be clicked or selected. */
    WLMAKER_MENU_ITEM_STATE_DISABLED,
    /** Enabled: Can be clicked or selected, but is currently not selected. */
    WLMAKER_MENU_ITEM_STATE_ENABLED,
    /** Selected: Currently under the pointer. */
    WLMAKER_MENU_ITEM_STATE_SELECTED
} wlmaker_menu_item_state_t;

/** State of a menu item. */
struct _wlmaker_menu_item_t {
    /** Element of a double-linked list: `wlmaker_menu_t.menu_items`. */
    bs_dllist_node_t          dlnode;

    /** Points to this item's descriptor. */
    const wlmaker_menu_item_descriptor_t *descriptor_ptr;

    /** Width of the menu item. Will be drawn to this size, clip if needed. */
    uint32_t                  width;
    /** Height of the menu item. Will be drawn to this size, clip if needed. */
    uint32_t                  height;
    /** Horizontal position of the menu item, within the menu's buffer. */
    uint32_t                  x;
    /** Vertical position of the menu item, within the menu's buffer. */
    uint32_t                  y;

    /** Current status, according to mouse position and clickedness. */
    wlmaker_menu_item_state_t state;
    /** Status that is drawn. */
    wlmaker_menu_item_state_t drawn_state;
    /** Argument to provide to the item's callback. May be NULL. */
    void                      *callback_ud_ptr;
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_menu_item_t *wlmaker_menu_item_create(
    const wlmaker_menu_item_descriptor_t *desc_ptr,
    void *callback_ud_ptr)
{
    wlmaker_menu_item_t *menu_item_ptr = logged_calloc(
        1, sizeof(wlmaker_menu_item_t));
    if (NULL == menu_item_ptr) return NULL;
    menu_item_ptr->descriptor_ptr = desc_ptr;
    menu_item_ptr->callback_ud_ptr = callback_ud_ptr;

    menu_item_ptr->state = WLMAKER_MENU_ITEM_STATE_ENABLED;
    return menu_item_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_menu_item_destroy(wlmaker_menu_item_t *menu_item_ptr)
{
    free(menu_item_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmaker_menu_item_get_desired_size(
    const wlmaker_menu_item_t *menu_item_ptr,
    uint32_t *width_ptr, uint32_t *height_ptr)
{
    menu_item_ptr = menu_item_ptr;  // currently unused.
    if (NULL != width_ptr) *width_ptr = 256;
    if (NULL != height_ptr) *height_ptr = 22;
}

/* ------------------------------------------------------------------------- */
void wlmaker_menu_item_set_size(
    wlmaker_menu_item_t *menu_item_ptr,
    uint32_t width,
    uint32_t height)
{
    menu_item_ptr->width = width;
    menu_item_ptr->height = height;
}

/* ------------------------------------------------------------------------- */
void wlmaker_menu_item_set_position(
    wlmaker_menu_item_t *menu_item_ptr,
    uint32_t x,
    uint32_t y)
{
    menu_item_ptr->x = x;
    menu_item_ptr->y = y;
}

/* ------------------------------------------------------------------------- */
void wlmaker_menu_item_draw(
    wlmaker_menu_item_t *menu_item_ptr,
    cairo_t *cairo_ptr)
{
    cairo_save(cairo_ptr);

    wlmaker_primitives_draw_bezel_at(
        cairo_ptr, menu_item_ptr->x, menu_item_ptr->y,
        menu_item_ptr->width, menu_item_ptr->height, 1.0, true);

    const wlmaker_style_fill_t *fill_ptr = NULL;
    uint32_t text_color = 0;
    switch (menu_item_ptr->state) {
    case WLMAKER_MENU_ITEM_STATE_ENABLED:
        fill_ptr = &wlmaker_config_theme.menu_item_enabled_fill;
        text_color = wlmaker_config_theme.menu_item_enabled_text_color;
        break;
    case WLMAKER_MENU_ITEM_STATE_SELECTED:
        fill_ptr = &wlmaker_config_theme.menu_item_selected_fill;
        text_color = wlmaker_config_theme.menu_item_selected_text_color;
        break;
    default:
        bs_log(BS_FATAL, "Unhandled item state: %d", menu_item_ptr->state);
        BS_ABORT();
    }
    BS_ASSERT(NULL != fill_ptr);

    wlmaker_primitives_cairo_fill_at(
        cairo_ptr,
        menu_item_ptr->x + 1, menu_item_ptr->y + 1,
        menu_item_ptr->width - 2, menu_item_ptr->height - 2,
        fill_ptr);

    cairo_select_font_face(
        cairo_ptr, "Helvetica",
        CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cairo_ptr, 15.0);
    cairo_set_source_argb8888(cairo_ptr, text_color);
    cairo_move_to(
        cairo_ptr, menu_item_ptr->x + 6, menu_item_ptr->y + 16);
    cairo_show_text(cairo_ptr,
                    menu_item_ptr->descriptor_ptr->param.entry.label_ptr);

    cairo_restore(cairo_ptr);

    menu_item_ptr->drawn_state = menu_item_ptr->state;
}

/* ------------------------------------------------------------------------- */
bs_dllist_node_t *wlmaker_dlnode_from_menu_item(
    wlmaker_menu_item_t *item_ptr)
{
    return &item_ptr->dlnode;
}

/* ------------------------------------------------------------------------- */
wlmaker_menu_item_t *wlmaker_menu_item_from_dlnode(
    bs_dllist_node_t *dlnode_ptr)
{
    if (NULL == dlnode_ptr) return NULL;
    return BS_CONTAINER_OF(dlnode_ptr, wlmaker_menu_item_t, dlnode);
}

/* ------------------------------------------------------------------------- */
void wlmaker_menu_item_set_focus(
    wlmaker_menu_item_t *menu_item_ptr,
    bool focussed)
{
    if (focussed) {
        menu_item_ptr->state = WLMAKER_MENU_ITEM_STATE_SELECTED;
    } else {
        menu_item_ptr->state = WLMAKER_MENU_ITEM_STATE_ENABLED;
    }
}

/* ------------------------------------------------------------------------- */
bool wlmaker_menu_item_contains(
    const wlmaker_menu_item_t *menu_item_ptr,
    double x,
    double y)
{
    return (menu_item_ptr->x <= x &&
            x < menu_item_ptr->x + menu_item_ptr->width &&
            menu_item_ptr->y <= y &&
            y <  menu_item_ptr->y + menu_item_ptr->height);
}

/* ------------------------------------------------------------------------- */
bool wlmaker_menu_item_redraw_needed(
    const wlmaker_menu_item_t *menu_item_ptr)
{
    return (menu_item_ptr->state != menu_item_ptr->drawn_state);
}

/* ------------------------------------------------------------------------- */
void wlmaker_menu_item_execute(
    const wlmaker_menu_item_t *menu_item_ptr)
{
    if (NULL != menu_item_ptr->descriptor_ptr->param.entry.callback) {
        menu_item_ptr->descriptor_ptr->param.entry.callback(
            menu_item_ptr->callback_ud_ptr);
    }
}

/* == Local (static) methods =============================================== */

/* == Unit tests =========================================================== */

static void test_draw(bs_test_t *test_ptr);
static void test_contains(bs_test_t *test_ptr);

/** Unit tests. */
const bs_test_case_t wlmaker_menu_item_test_cases[] = {
    { 1, "draw", test_draw },
    { 1, "contains", test_contains },
    { 0, NULL, NULL }
};

/** Descriptor of the menu item used in the unit test. */
static const wlmaker_menu_item_descriptor_t test_descriptor = {
    .type = WLMAKER_MENU_ITEM_ENTRY,
    .param.entry = { .label_ptr = "Label", .callback = NULL }
};

/** Properties of the fill, used for the unit test. */
static const wlmaker_style_fill_t test_fill = {
    .type = WLMAKER_STYLE_COLOR_DGRADIENT,
    .param = { .hgradient = { .from = 0xffa6a6b6,.to = 0xff515561 }}
};

/** Verifies the menu item is drawn as desired. */
void test_draw(bs_test_t *test_ptr)
{
    wlmaker_menu_item_t *item_ptr = wlmaker_menu_item_create(
        &test_descriptor, NULL);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, item_ptr);

    bs_gfxbuf_t *gfxbuf_ptr = bs_gfxbuf_create(256, 22);
    if (NULL == gfxbuf_ptr) {
        BS_TEST_FAIL(test_ptr, "Failed bs_gfxbuf_create(256, 22)");
        return;
    }
    cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, cairo_ptr);
    wlmaker_primitives_cairo_fill(cairo_ptr, &test_fill);
    wlmaker_menu_item_set_size(item_ptr, 256, 22);
    wlmaker_menu_item_draw(item_ptr, cairo_ptr);
    cairo_destroy(cairo_ptr);

    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, gfxbuf_ptr, "menu_item.png");
    BS_TEST_VERIFY_EQ(
        test_ptr, item_ptr->drawn_state, WLMAKER_MENU_ITEM_STATE_ENABLED);

    cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, cairo_ptr);
    wlmaker_primitives_cairo_fill(cairo_ptr, &test_fill);
    item_ptr->state = WLMAKER_MENU_ITEM_STATE_SELECTED;
    wlmaker_menu_item_draw(item_ptr, cairo_ptr);
    cairo_destroy(cairo_ptr);

    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, gfxbuf_ptr, "menu_item_selected.png");
    BS_TEST_VERIFY_EQ(
        test_ptr, item_ptr->drawn_state, WLMAKER_MENU_ITEM_STATE_SELECTED);

    wlmaker_menu_item_destroy(item_ptr);
}

/** Verifies the contains function. */
void test_contains(bs_test_t *test_ptr)
{
    wlmaker_menu_item_t *i = wlmaker_menu_item_create(
        &test_descriptor, NULL);
    wlmaker_menu_item_set_position(i, 10, 20);
    wlmaker_menu_item_set_size(i, 100, 30);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmaker_menu_item_contains(i, 9, 19));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmaker_menu_item_contains(i, 10, 20));
    BS_TEST_VERIFY_TRUE(test_ptr, wlmaker_menu_item_contains(i, 109, 49));
    BS_TEST_VERIFY_FALSE(test_ptr, wlmaker_menu_item_contains(i, 110, 50));
    wlmaker_menu_item_destroy(i);
}

/* == End of menu_item.c =================================================== */
