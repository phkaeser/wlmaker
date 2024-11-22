/* ========================================================================= */
/**
 * @file action_item.h
 * Copyright (c) 2024 by Philipp Kaeser <kaeser@gubbe.ch>
 */
#ifndef __WLMAKER_ACTION_ITEM_H__
#define __WLMAKER_ACTION_ITEM_H__

/** Forward declaration: An action-triggering menu item. */
typedef struct _wlmaker_action_item_t wlmaker_action_item_t;

#include "toolkit/toolkit.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates a menu item that triggers a @ref wlmaker_action_t.
 *
 * @param text_ptr
 * @param style_ptr
 * @param env_ptr
 *
 * @return Poitner to the menu item's handle or NULL on error.
 */
wlmaker_action_item_t *wlmaker_action_item_create(
    const char *text_ptr,
    const wlmtk_menu_item_style_t *style_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Destroys the action-triggering menu item.
 *
 * @param action_item_ptr
 */
void wlmaker_action_item_destroy(wlmaker_action_item_t *action_item_ptr);

/** @returns pointer to the superclass @ref wlmtk_menu_item_t. */
wlmtk_menu_item_t *wlmaker_action_item_menu_item(
    wlmaker_action_item_t *action_item_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_ACTION_ITEM_H__ */
/* == End of action_item.h ================================================= */
