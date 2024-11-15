/* ========================================================================= */
/**
 * @file root_menu.c
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

#include "root_menu.h"

#include <libbase/libbase.h>

/* == Declarations ========================================================= */

/** State of the root menu. */
struct _wlmaker_root_menu_t {
    /** Window. */
    wlmtk_window_t            *window_ptr;

    /** The root menu's window content base instance. */
    wlmtk_content_t           content;
    /** The root menu base instance. */
    wlmtk_menu_t              menu;
    /** For now: One entry in the root menu, for 'Exit'. */
    wlmtk_simple_menu_item_t  *exit_item_ptr;
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_root_menu_t *wlmaker_root_menu_create(
    const wlmtk_window_style_t *window_style_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmaker_root_menu_t *root_menu_ptr = logged_calloc(
        1, sizeof(wlmaker_root_menu_t));
    if (NULL == root_menu_ptr) return NULL;

    if (!wlmtk_menu_init(&root_menu_ptr->menu,
                         menu_style_ptr,
                         env_ptr)) {
        wlmaker_root_menu_destroy(root_menu_ptr);
        return NULL;
    }

    root_menu_ptr->exit_item_ptr = wlmtk_simple_menu_item_create(
        "Exit", &menu_style_ptr->item, env_ptr);
    if (NULL == root_menu_ptr->exit_item_ptr) {
        wlmaker_root_menu_destroy(root_menu_ptr);
        return NULL;
    }
    wlmtk_menu_add_item(
        &root_menu_ptr->menu,
        wlmtk_simple_menu_item_menu_item(root_menu_ptr->exit_item_ptr));

    if (!wlmtk_content_init(
            &root_menu_ptr->content,
            wlmtk_menu_element(&root_menu_ptr->menu),
            env_ptr)) {
        wlmaker_root_menu_destroy(root_menu_ptr);
        return NULL;
    }
    struct wlr_box box = wlmtk_element_get_dimensions_box(
        wlmtk_menu_element(&root_menu_ptr->menu));
    // TODO(kaeser@gubbe.ch): Should not be required. Also, the sequence
    // of set_server_side_decorated and set_attributes is brittle.
    wlmtk_content_commit(
        &root_menu_ptr->content,
        box.width,
        box.height,
        0);

    root_menu_ptr->window_ptr = wlmtk_window_create(
        &root_menu_ptr->content,
        window_style_ptr,
        env_ptr);
    if (NULL == root_menu_ptr->window_ptr) {
        wlmaker_root_menu_destroy(root_menu_ptr);
        return NULL;
    }
    wlmtk_window_set_title(root_menu_ptr->window_ptr, "Root Menu");
    wlmtk_window_set_server_side_decorated(root_menu_ptr->window_ptr, true);
    wlmtk_window_set_properties(root_menu_ptr->window_ptr, 0);

    return root_menu_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_root_menu_destroy(wlmaker_root_menu_t *root_menu_ptr)
{
    if (NULL != root_menu_ptr->window_ptr) {
        // Unmap, in case it's not unmapped yet.
        wlmtk_workspace_t *workspace_ptr = wlmtk_window_get_workspace(
            root_menu_ptr->window_ptr);
        if (NULL != workspace_ptr) {
            wlmtk_workspace_unmap_window(workspace_ptr,
                                         root_menu_ptr->window_ptr);
        }

        wlmtk_window_destroy(root_menu_ptr->window_ptr);
        root_menu_ptr->window_ptr = NULL;
    }

    wlmtk_content_fini(&root_menu_ptr->content);

    if (NULL != root_menu_ptr->exit_item_ptr) {
        wlmtk_menu_remove_item(
            &root_menu_ptr->menu,
            wlmtk_simple_menu_item_menu_item(root_menu_ptr->exit_item_ptr));
        wlmtk_simple_menu_item_destroy(root_menu_ptr->exit_item_ptr);
        root_menu_ptr->exit_item_ptr = NULL;
    }
    wlmtk_menu_fini(&root_menu_ptr->menu);

    free(root_menu_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_window_t *wlmaker_root_menu_window(wlmaker_root_menu_t *root_menu_ptr)
{
    return root_menu_ptr->window_ptr;
}

/* == Local (static) methods =============================================== */

/* == End of root_menu.c =================================================== */
