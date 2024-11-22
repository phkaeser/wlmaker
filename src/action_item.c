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

    /** Action to trigger when clicked. */
    wlmaker_action_t          action;
    /** Back-link to @ref wlmaker_server_t, for executing the action. */
    wlmaker_server_t          *server_ptr;
};

static void _wlmaker_action_item_element_destroy(
    wlmtk_element_t *element_ptr);
static void _wlmaker_action_item_clicked(
    wlmtk_menu_item_t *menu_item_ptr);

/* == Data ================================================================= */

/** Virtual method table for the action-triggering menu item. */
static const wlmtk_menu_item_vmt_t _wlmaker_action_item_vmt = {
    .clicked = _wlmaker_action_item_clicked
};
/** Virtual method table for the menu item's element superclass. */
static const wlmtk_element_vmt_t _wlmaker_action_item_element_vmt = {
    .destroy = _wlmaker_action_item_element_destroy
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_action_item_t *wlmaker_action_item_create(
    const char *text_ptr,
    const wlmtk_menu_item_style_t *style_ptr,
    wlmaker_action_t action,
    wlmaker_server_t *server_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmaker_action_item_t *action_item_ptr = logged_calloc(
        1, sizeof(wlmaker_action_item_t));
    if (NULL == action_item_ptr) return NULL;
    action_item_ptr->action = action;
    action_item_ptr->server_ptr = server_ptr;

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
    wlmtk_element_extend(
        wlmtk_menu_item_element(&action_item_ptr->super_menu_item),
        &_wlmaker_action_item_element_vmt);
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
/** Implements @ref wlmtk_element_vmt_t::destroy. Routes to instance's dtor. */
void _wlmaker_action_item_element_destroy(
    wlmtk_element_t *element_ptr)
{
    wlmaker_action_item_t *action_item_ptr = BS_CONTAINER_OF(
        element_ptr, wlmaker_action_item_t,
        super_menu_item.super_buffer.super_element);
    wlmaker_action_item_destroy(action_item_ptr);
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_menu_item_vmt_t::clicked. Triggers the action. */
void _wlmaker_action_item_clicked(wlmtk_menu_item_t *menu_item_ptr)
{
    wlmaker_action_item_t *action_item_ptr = BS_CONTAINER_OF(
        menu_item_ptr, wlmaker_action_item_t, super_menu_item);

    wlmaker_action_execute(
        action_item_ptr->server_ptr,
        action_item_ptr->action);

    if (NULL != action_item_ptr->server_ptr->root_menu_ptr) {
        wlmaker_root_menu_destroy(action_item_ptr->server_ptr->root_menu_ptr);
    }
}

/* == End of action_item.c ================================================== */
