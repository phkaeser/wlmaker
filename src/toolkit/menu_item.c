/* ========================================================================= */
/**
 * @file menu_item.c
 *
 * @copyright
 * Copyright 2024 Google LLC
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

#include "gfxbuf.h"
#include "primitives.h"

/* == Declarations ========================================================= */

static bool _wlmtk_menu_item_redraw(
    wlmtk_menu_item_t *menu_item_ptr);
static void _wlmtk_menu_item_apply_state(wlmtk_menu_item_t *menu_item_ptr);
static struct wlr_buffer *_wlmtk_menu_item_create_buffer(
    wlmtk_menu_item_t *menu_item_ptr,
    wlmtk_menu_item_state_t state);

static bool _wlmtk_menu_item_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);
static void _wlmtk_menu_item_element_pointer_enter(
    wlmtk_element_t *element_ptr);
static void _wlmtk_menu_item_element_pointer_leave(
    wlmtk_element_t *element_ptr);

/* == Data ================================================================= */

/** Virtual method table for the menu item's super class: Element. */
static const wlmtk_element_vmt_t _wlmtk_menu_item_element_vmt = {
    .pointer_button = _wlmtk_menu_item_element_pointer_button,
    .pointer_enter = _wlmtk_menu_item_element_pointer_enter,
    .pointer_leave = _wlmtk_menu_item_element_pointer_leave,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_menu_item_t *wlmtk_menu_item_create(wlmtk_env_t *env_ptr)
{
    wlmtk_menu_item_t *menu_item_ptr = logged_calloc(
        1, sizeof(wlmtk_menu_item_t));
    if (NULL == menu_item_ptr) return NULL;

    if (!wlmtk_menu_item_init(menu_item_ptr, env_ptr)) {
        wlmtk_menu_item_destroy(menu_item_ptr);
        return NULL;
    }
    return menu_item_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_menu_item_destroy(wlmtk_menu_item_t *menu_item_ptr)
{
    wlmtk_menu_item_fini(menu_item_ptr);
    free(menu_item_ptr);
}

/* -------------------------------------------------------------------------*/
bool wlmtk_menu_item_init(wlmtk_menu_item_t *menu_item_ptr,
                          wlmtk_env_t *env_ptr)
{
    memset(menu_item_ptr, 0, sizeof(wlmtk_menu_item_t));

    if (!wlmtk_buffer_init(&menu_item_ptr->super_buffer, env_ptr)) {
        wlmtk_menu_item_fini(menu_item_ptr);
        return false;
    }

    menu_item_ptr->orig_super_element_vmt = wlmtk_element_extend(
        &menu_item_ptr->super_buffer.super_element,
        &_wlmtk_menu_item_element_vmt);

    menu_item_ptr->enabled = true;
    menu_item_ptr->state = MENU_ITEM_ENABLED;
    return true;
}

/* -------------------------------------------------------------------------*/
wlmtk_menu_item_vmt_t wlmtk_menu_item_extend(
    wlmtk_menu_item_t *menu_item_ptr,
    const wlmtk_menu_item_vmt_t *menu_item_vmt_ptr)
{
    wlmtk_menu_item_vmt_t orig_vmt = menu_item_ptr->vmt;

    if (NULL != menu_item_vmt_ptr->clicked) {
        menu_item_ptr->vmt.clicked = menu_item_vmt_ptr->clicked;
    }

    return orig_vmt;
}

/* -------------------------------------------------------------------------*/
void wlmtk_menu_item_fini(wlmtk_menu_item_t *menu_item_ptr)
{
    if (NULL != menu_item_ptr->text_ptr) {
        free(menu_item_ptr->text_ptr);
        menu_item_ptr->text_ptr = NULL;
    }

    wlr_buffer_drop_nullify(&menu_item_ptr->enabled_wlr_buffer_ptr);
    wlr_buffer_drop_nullify(&menu_item_ptr->highlighted_wlr_buffer_ptr);
    wlr_buffer_drop_nullify(&menu_item_ptr->disabled_wlr_buffer_ptr);

    wlmtk_buffer_fini(&menu_item_ptr->super_buffer);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_menu_item_set_text(
    wlmtk_menu_item_t *menu_item_ptr,
    const char *text_ptr)
{
    char *new_text_ptr = logged_strdup(text_ptr);
    if (NULL == new_text_ptr) return false;

    if (NULL != menu_item_ptr->text_ptr) free(menu_item_ptr->text_ptr);
    menu_item_ptr->text_ptr = new_text_ptr;

    return _wlmtk_menu_item_redraw(menu_item_ptr);
}

/* -------------------------------------------------------------------------*/
void wlmtk_menu_item_set_enabled(
    wlmtk_menu_item_t *menu_item_ptr,
    bool enabled)
{
    if (menu_item_ptr->enabled == enabled) return;

    menu_item_ptr->enabled = enabled;

    if (menu_item_ptr->enabled) {
        if (menu_item_ptr->super_buffer.super_element.pointer_inside) {
            menu_item_ptr->state = MENU_ITEM_HIGHLIGHTED;
        } else {
            menu_item_ptr->state = MENU_ITEM_ENABLED;
        }
    } else {
        menu_item_ptr->state = MENU_ITEM_DISABLED;
    }

    _wlmtk_menu_item_apply_state(menu_item_ptr);
}

/* -------------------------------------------------------------------------*/
bs_dllist_node_t *wlmtk_dlnode_from_menu_item(
    wlmtk_menu_item_t *menu_item_ptr)
{
    return &menu_item_ptr->dlnode;
}

/* -------------------------------------------------------------------------*/
wlmtk_menu_item_t *wlmtk_menu_item_from_dlnode(bs_dllist_node_t *dlnode_ptr)
{
    return BS_CONTAINER_OF(dlnode_ptr, wlmtk_menu_item_t, dlnode);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_menu_item_element(wlmtk_menu_item_t *menu_item_ptr)
{
    return wlmtk_buffer_element(&menu_item_ptr->super_buffer);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Redraws the buffers for the menu item. Also updates the buffer state. */
bool _wlmtk_menu_item_redraw(wlmtk_menu_item_t *menu_item_ptr)
{
    struct wlr_buffer *e, *h, *d;

    e = _wlmtk_menu_item_create_buffer(menu_item_ptr, MENU_ITEM_ENABLED);
    h = _wlmtk_menu_item_create_buffer(menu_item_ptr, MENU_ITEM_HIGHLIGHTED);
    d = _wlmtk_menu_item_create_buffer(menu_item_ptr, MENU_ITEM_DISABLED);

    if (NULL == e || NULL == d || NULL == h) {
        wlr_buffer_drop_nullify(&e);
        wlr_buffer_drop_nullify(&h);
        wlr_buffer_drop_nullify(&d);
        return false;
    }

    wlr_buffer_drop_nullify(&menu_item_ptr->enabled_wlr_buffer_ptr);
    menu_item_ptr->enabled_wlr_buffer_ptr = e;
    wlr_buffer_drop_nullify(&menu_item_ptr->highlighted_wlr_buffer_ptr);
    menu_item_ptr->highlighted_wlr_buffer_ptr = h;
    wlr_buffer_drop_nullify(&menu_item_ptr->disabled_wlr_buffer_ptr);
    menu_item_ptr->disabled_wlr_buffer_ptr = d;

    _wlmtk_menu_item_apply_state(menu_item_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
/** Applies the state: Sets the parent buffer's content accordingly. */
void _wlmtk_menu_item_apply_state(wlmtk_menu_item_t *menu_item_ptr)
{
    switch (menu_item_ptr->state) {
    case MENU_ITEM_ENABLED:
        wlmtk_buffer_set(&menu_item_ptr->super_buffer,
                         menu_item_ptr->enabled_wlr_buffer_ptr);
        break;

    case MENU_ITEM_HIGHLIGHTED:
        wlmtk_buffer_set(&menu_item_ptr->super_buffer,
                         menu_item_ptr->highlighted_wlr_buffer_ptr);
        break;

    case MENU_ITEM_DISABLED:
        wlmtk_buffer_set(&menu_item_ptr->super_buffer,
                         menu_item_ptr->disabled_wlr_buffer_ptr);
        break;

    default:
        bs_log(BS_FATAL, "Unhandled state %d", menu_item_ptr->state);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a wlr_buffer with the menu item drawn for the given state.
 *
 * @param menu_item_ptr
 * @param state
 *
 * @return A wlr_buffer, or NULL on error.
 */
struct wlr_buffer *_wlmtk_menu_item_create_buffer(
    wlmtk_menu_item_t *menu_item_ptr,
    wlmtk_menu_item_state_t state)
{
    struct wlr_buffer *wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(
        menu_item_ptr->width, menu_item_ptr->style.height);
    if (NULL == wlr_buffer_ptr) {
        bs_log(BS_ERROR, "Failed bs_gfxbuf_create_wlr_buffer(%d, %"PRIu64")",
               menu_item_ptr->width, menu_item_ptr->style.height);
        return NULL;
    }

    cairo_t *cairo_ptr = cairo_create_from_wlr_buffer(wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        bs_log(BS_ERROR, "Failed cairo_create_from_wlr_buffer(%p)",
               wlr_buffer_ptr);
        wlr_buffer_drop(wlr_buffer_ptr);
        return NULL;
    }

    const char *text_ptr = "";
    if (NULL != menu_item_ptr->text_ptr) text_ptr = menu_item_ptr->text_ptr;

    wlmtk_style_fill_t *fill_ptr = &menu_item_ptr->style.fill;
    uint32_t color = menu_item_ptr->style.enabled_text_color;

    if (MENU_ITEM_HIGHLIGHTED == state) {
        fill_ptr = &menu_item_ptr->style.highlighted_fill;
        color = menu_item_ptr->style.highlighted_text_color;
    } else if (MENU_ITEM_DISABLED == state) {
        color = menu_item_ptr->style.disabled_text_color;
    }

    wlmaker_primitives_cairo_fill(cairo_ptr, fill_ptr);

    wlmaker_primitives_draw_text(
        cairo_ptr,
        4, 4 + menu_item_ptr->style.font.size,
        &menu_item_ptr->style.font,
        color,
        text_ptr);

    cairo_destroy(cairo_ptr);
    return wlr_buffer_ptr;
}

/* ------------------------------------------------------------------------- */
/** Checks if the button event is a click, and calls the handler. */
bool _wlmtk_menu_item_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmtk_menu_item_t *menu_item_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_menu_item_t, super_buffer.super_element);


    if (button_event_ptr->button != BTN_LEFT) return false;
    if (WLMTK_BUTTON_CLICK != button_event_ptr->type) return false;
    if (MENU_ITEM_HIGHLIGHTED != menu_item_ptr->state) return false;

    if (NULL != menu_item_ptr->vmt.clicked) {
        menu_item_ptr->vmt.clicked(menu_item_ptr);
    }
    // Note: We consider this accepted even if there's no 'clicked' handler.
    return true;
}

/* ------------------------------------------------------------------------- */
/** Handles when the pointer enters the element: Highlights, if enabled. */
void _wlmtk_menu_item_element_pointer_enter(
    wlmtk_element_t *element_ptr)
{
    wlmtk_menu_item_t *menu_item_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_menu_item_t, super_buffer.super_element);
    menu_item_ptr->orig_super_element_vmt.pointer_enter(element_ptr);

    if (menu_item_ptr->enabled) {
        menu_item_ptr->state = MENU_ITEM_HIGHLIGHTED;
    }
    _wlmtk_menu_item_apply_state(menu_item_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles when the pointer leaves the element: Ends highlight. */
void _wlmtk_menu_item_element_pointer_leave(
    wlmtk_element_t *element_ptr)
{
    wlmtk_menu_item_t *menu_item_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_menu_item_t, super_buffer.super_element);
    menu_item_ptr->orig_super_element_vmt.pointer_leave(element_ptr);

    if (menu_item_ptr->enabled) {
        menu_item_ptr->state = MENU_ITEM_ENABLED;
    }
    _wlmtk_menu_item_apply_state(menu_item_ptr);
}

/* == Unit tests =========================================================== */

static void test_ctor_dtor(bs_test_t *test_ptr);
static void test_buffers(bs_test_t *test_ptr);
static void test_pointer(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_menu_item_test_cases[] = {
    { 1, "ctor_dtor", test_ctor_dtor },
    { 1, "buffers", test_buffers },
    { 1, "pointer", test_pointer },
    { 0, NULL, NULL }
};

/** Style definition used for unit tests. */
static const wlmtk_menu_item_style_t _wlmtk_menu_item_test_style = {
    .fill = {
        .type = WLMTK_STYLE_COLOR_DGRADIENT,
        .param = { .dgradient = { .from = 0xff102040, .to = 0xff4080ff }}
    },
    .highlighted_fill = {
        .type = WLMTK_STYLE_COLOR_SOLID,
        .param = { .solid = { .color = 0xffc0d0e0 } }
    },
    .font = { .face = "Helvetica", .size = 14 },
    .height = 24,
    .enabled_text_color = 0xfff0f060,
    .highlighted_text_color = 0xff204080,
    .disabled_text_color = 0xff807060,
};

/* ------------------------------------------------------------------------- */
/** Exercises ctor and dtor and a few accessors. */
void test_ctor_dtor(bs_test_t *test_ptr)
{
    wlmtk_menu_item_t *menu_item_ptr = wlmtk_menu_item_create(NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, menu_item_ptr);

    bs_dllist_node_t *dlnode_ptr = wlmtk_dlnode_from_menu_item(menu_item_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, dlnode_ptr, &menu_item_ptr->dlnode);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        menu_item_ptr,
        wlmtk_menu_item_from_dlnode(dlnode_ptr));

    BS_TEST_VERIFY_EQ(
        test_ptr,
        &menu_item_ptr->super_buffer.super_element,
        wlmtk_menu_item_element(menu_item_ptr));

    BS_TEST_VERIFY_TRUE(
        test_ptr, wlmtk_menu_item_set_text(menu_item_ptr, "Text"));

    wlmtk_menu_item_destroy(menu_item_ptr);
}

/* ------------------------------------------------------------------------- */
/** Exercises drawing. */
void test_buffers(bs_test_t *test_ptr)
{
    wlmtk_menu_item_t *menu_item_ptr = wlmtk_menu_item_create(NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, menu_item_ptr);
    menu_item_ptr->style = _wlmtk_menu_item_test_style;
    menu_item_ptr->width = 80;
    wlmtk_menu_item_set_text(menu_item_ptr, "Menu item");

    bs_gfxbuf_t *g;

    g = bs_gfxbuf_from_wlr_buffer(menu_item_ptr->enabled_wlr_buffer_ptr);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, g, "toolkit/menu_item_enabled.png");

    g = bs_gfxbuf_from_wlr_buffer(menu_item_ptr->highlighted_wlr_buffer_ptr);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, g, "toolkit/menu_item_highlighted.png");

    g = bs_gfxbuf_from_wlr_buffer(menu_item_ptr->disabled_wlr_buffer_ptr);
    BS_TEST_VERIFY_GFXBUF_EQUALS_PNG(
        test_ptr, g, "toolkit/menu_item_disabled.png");

    wlmtk_menu_item_destroy(menu_item_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests pointer entering & leaving. */
void test_pointer(bs_test_t *test_ptr)
{
    wlmtk_menu_item_t *menu_item_ptr = wlmtk_menu_item_create(NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, menu_item_ptr);
    wlmtk_element_t *e = wlmtk_menu_item_element(menu_item_ptr);
    wlmtk_button_event_t lbtn_ev = {
        .button = BTN_LEFT, .type = WLMTK_BUTTON_CLICK };

    menu_item_ptr->style = _wlmtk_menu_item_test_style;
    menu_item_ptr->width = 80;
    wlmtk_menu_item_set_text(menu_item_ptr, "Menu item");

    // Initial state: enabled.
    BS_TEST_VERIFY_EQ(test_ptr, MENU_ITEM_ENABLED, menu_item_ptr->state);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        menu_item_ptr->super_buffer.wlr_buffer_ptr,
        menu_item_ptr->enabled_wlr_buffer_ptr);

    // Click event. Not passed, since not highlighted.
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_element_pointer_button(e, &lbtn_ev));

    // Disable it, verify texture and state.
    wlmtk_menu_item_set_enabled(menu_item_ptr, false);
    BS_TEST_VERIFY_EQ(test_ptr, MENU_ITEM_DISABLED, menu_item_ptr->state);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        menu_item_ptr->super_buffer.wlr_buffer_ptr,
        menu_item_ptr->disabled_wlr_buffer_ptr);

    // Pointer enters the item, but remains disabled.
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_pointer_motion(e, 20, 10, 1));
    BS_TEST_VERIFY_EQ(test_ptr, MENU_ITEM_DISABLED, menu_item_ptr->state);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        menu_item_ptr->super_buffer.wlr_buffer_ptr,
        menu_item_ptr->disabled_wlr_buffer_ptr);

    // When enabled, will be highlighted since pointer is inside.
    wlmtk_menu_item_set_enabled(menu_item_ptr, true);
    BS_TEST_VERIFY_EQ(test_ptr, MENU_ITEM_HIGHLIGHTED, menu_item_ptr->state);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        menu_item_ptr->super_buffer.wlr_buffer_ptr,
        menu_item_ptr->highlighted_wlr_buffer_ptr);

    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_element_pointer_button(e, &lbtn_ev));

    // Pointer moves outside: disabled.
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_element_pointer_motion(e, 90, 10, 2));
    BS_TEST_VERIFY_EQ(test_ptr, MENU_ITEM_ENABLED, menu_item_ptr->state);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        menu_item_ptr->super_buffer.wlr_buffer_ptr,
        menu_item_ptr->enabled_wlr_buffer_ptr);

    wlmtk_menu_item_destroy(menu_item_ptr);
}

/* == End of menu_item.c =================================================== */
