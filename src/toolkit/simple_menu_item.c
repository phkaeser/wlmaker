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

/* == End of simple_menu_item.c ============================================ */
