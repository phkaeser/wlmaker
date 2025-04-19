/* ========================================================================= */
/**
 * @file action_item.h
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
#ifndef __WLMAKER_ACTION_ITEM_H__
#define __WLMAKER_ACTION_ITEM_H__

#include <stddef.h>
#include <libbase/libbase.h>

/** Forward declaration: An action-triggering menu item. */
typedef struct _wlmaker_action_item_t wlmaker_action_item_t;

#include "action.h"
#include "server.h"
#include "toolkit/toolkit.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Descriptor for creating a menu item triggering an action. */
typedef struct {
    /** Text for the menu item. */
    const char                *text_ptr;
    /** The action to trigger. */
    wlmaker_action_t          action;
    /** Extra argument for @ref wlmaker_action_execute. */
    char                      *action_arg_ptr;
    /**
     * Where to store the @ref wlmaker_action_item_t, relative to the
     * `dest_ptr` argument of @ref wlmaker_action_item_create_from_desc.
     */
    size_t                    destination_ofs;
} wlmaker_action_item_desc_t;

/**
 * Creates a menu item that triggers a @ref wlmaker_action_t.
 *
 * @param text_ptr
 * @param style_ptr
 * @param action
 * @param action_arg_ptr      Extra argument. Will be duplicated.
 * @param server_ptr
 * @param env_ptr
 *
 * @return Poitner to the menu item's handle or NULL on error.
 */
wlmaker_action_item_t *wlmaker_action_item_create(
    const char *text_ptr,
    const wlmtk_menu_item_style_t *style_ptr,
    wlmaker_action_t action,
    const char *action_arg_ptr,
    wlmaker_server_t *server_ptr,
    wlmtk_env_t *env_ptr);

/**
 * Creates a menu item triggering an action item from a descriptor.
 *
 * @param desc_ptr
 * @param dest_ptr
 * @param style_ptr
 * @param server_ptr
 * @param env_ptr
 *
 * @return Pointer to the item's handle or NULL on error.
 */
wlmaker_action_item_t *wlmaker_action_item_create_from_desc(
    const wlmaker_action_item_desc_t *desc_ptr,
    void *dest_ptr,
    const wlmtk_menu_item_style_t *style_ptr,
    wlmaker_server_t *server_ptr,
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

/** Unit test cases. */
extern const bs_test_case_t   wlmaker_action_item_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMAKER_ACTION_ITEM_H__ */
/* == End of action_item.h ================================================= */
