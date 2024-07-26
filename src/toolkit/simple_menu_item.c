/* ========================================================================= */
/**
 * @file simple_menu_item.c
 * Copyright (c) 2024 by Philipp Kaeser <kaeser@gubbe.ch>
 */

#include "simple_menu_item.h"

#include "menu_item.h"

/* == Declarations ========================================================= */

/** State of a simple menu item. */
struct _wlmtk_simple_menu_item_t {
    /** Superclass: a menu item. */
    wlmtk_menu_item_t         super_menu_item;

    /** Original VMT of the superclass @ref wlmtk_element_t. */
    wlmtk_element_vmt_t       orig_element_vmt;
    /** Original VMT. */
    wlmtk_menu_item_vmt_t     orig_vmt;
};

static void _wlmtk_simple_menu_item_element_destroy(
    wlmtk_element_t *element_ptr);
static void _wlmtk_simple_menu_item_clicked(wlmtk_menu_item_t *menu_item_ptr);

/* == Data ================================================================= */

/** Virtual method table for the simple menu item. */
static const wlmtk_menu_item_vmt_t _wlmtk_simple_menu_item_vmt = {
    .clicked = _wlmtk_simple_menu_item_clicked
};
/** Virtual method table for the simple menu item's element superclass. */
static const wlmtk_element_vmt_t _wlmtk_simple_menu_item_element_vmt = {
    .destroy = _wlmtk_simple_menu_item_element_destroy
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_simple_menu_item_t *wlmtk_simple_menu_item_create(
    const char *text_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmtk_simple_menu_item_t *simple_menu_item_ptr = logged_calloc(
        1, sizeof(wlmtk_simple_menu_item_t));
    if (NULL == simple_menu_item_ptr) return NULL;

    if (!wlmtk_menu_item_init(
            &simple_menu_item_ptr->super_menu_item,
            env_ptr)) {
        wlmtk_simple_menu_item_destroy(simple_menu_item_ptr);
        return NULL;
    }
    simple_menu_item_ptr->orig_vmt = wlmtk_menu_item_extend(
        &simple_menu_item_ptr->super_menu_item, &_wlmtk_simple_menu_item_vmt);
    simple_menu_item_ptr->orig_element_vmt = wlmtk_element_extend(
        wlmtk_menu_item_element(&simple_menu_item_ptr->super_menu_item),
        &_wlmtk_simple_menu_item_element_vmt);

    if (!wlmtk_menu_item_set_text(
            &simple_menu_item_ptr->super_menu_item,
            text_ptr)) {
        wlmtk_simple_menu_item_destroy(simple_menu_item_ptr);
        return NULL;
    }

    return simple_menu_item_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_simple_menu_item_destroy(
    wlmtk_simple_menu_item_t *simple_menu_item_ptr)
{
    wlmtk_menu_item_fini(&simple_menu_item_ptr->super_menu_item);
    free(simple_menu_item_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_elemnt_vmt_t::destroy. Wraps to local dtor. */
void _wlmtk_simple_menu_item_element_destroy(
    wlmtk_element_t *element_ptr)
{
    wlmtk_simple_menu_item_t *simple_menu_item_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_simple_menu_item_t,
        super_menu_item.super_buffer.super_element);
    wlmtk_simple_menu_item_destroy(simple_menu_item_ptr);
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_menu_item_vmt_t::clicked for the simple menu item. */
void _wlmtk_simple_menu_item_clicked(wlmtk_menu_item_t *menu_item_ptr)
{
    bs_log(BS_WARNING, "FIXME: Clicked %p", menu_item_ptr);
}

/* == End of simple_menu_item.c ============================================ */
