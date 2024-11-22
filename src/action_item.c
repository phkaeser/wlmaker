/* ========================================================================= */
/**
 * @file action_item.c
 * Copyright (c) 2024 by Philipp Kaeser <kaeser@gubbe.ch>
 */

#include "action_item.h"

/* == Declarations ========================================================= */

/** State of an action item that triggers a @ref wlmaker_action_t. */
struct _wlmaker_action_item_t {
    /** Superclass: a menu item. */
    wlmtk_menu_item_t         super_menu_item;
};

static void _wlmaker_action_item_clicked(
    wlmtk_menu_item_t *menu_item_ptr);

/* == Data ================================================================= */

/** Virtual method table for the simple menu item. */
static const wlmtk_menu_item_vmt_t _wlmaker_action_item_vmt = {
    .clicked = _wlmaker_action_item_clicked
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_action_item_t *wlmaker_action_item_create(
    const char *text_ptr,
    const wlmtk_menu_item_style_t *style_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmaker_action_item_t *action_item_ptr = logged_calloc(
        1, sizeof(wlmaker_action_item_t));
    if (NULL == action_item_ptr) return NULL;

    if (!wlmtk_menu_item_init(
            &action_item_ptr->super_menu_item,
            style_ptr,
            env_ptr)) {
        wlmaker_action_item_destroy(action_item_ptr);
        return NULL;
    }
    wlmtk_menu_item_extend(
        &action_item_ptr->super_menu_item,
        &_wlmaker_action_item_vmt);
    // TODO(kaeser@gubbe.ch): Should not be required!
    action_item_ptr->super_menu_item.width = style_ptr->width;

    if (!wlmtk_menu_item_set_text(
            &action_item_ptr->super_menu_item,
            text_ptr)) {
        wlmaker_action_item_destroy(action_item_ptr);
        return NULL;
    }

    return action_item_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_action_item_destroy(wlmaker_action_item_t *action_item_ptr)
{
    wlmtk_menu_item_fini(&action_item_ptr->super_menu_item);
    free(action_item_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_menu_item_t *wlmaker_action_item_menu_item(
    wlmaker_action_item_t *action_item_ptr)
{
    return &action_item_ptr->super_menu_item;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_menu_item_vmt_t::clicked. Triggers the action. */
void _wlmaker_action_item_clicked(wlmtk_menu_item_t *menu_item_ptr)
{
    wlmaker_action_item_t *action_item_ptr = BS_CONTAINER_OF(
        menu_item_ptr, wlmaker_action_item_t, super_menu_item);
    bs_log(BS_WARNING, "Unimplemented: Action item '%s' clicked (%p)",
           action_item_ptr->super_menu_item.text_ptr,
           action_item_ptr);
}

/* == End of action_item.c ================================================== */
