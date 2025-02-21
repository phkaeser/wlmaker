/* ========================================================================= */
/**
 * @file submenu.c
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

#include "submenu.h"

#include "popup_menu.h"

/* == Declarations ========================================================= */

struct _wlmtk_submenu_t {
    wlmtk_menu_item_t         super_menu_item;

    wlmtk_popup_menu_t        *popup_menu_ptr;


    wlmtk_menu_item_t         item1;
    wlmtk_menu_item_t         item2;
};

static void _wlmtk_submenu_element_destroy(wlmtk_element_t *element_ptr);
static void _wlmtk_submenu_menu_item_clicked(wlmtk_menu_item_t *menu_item_ptr);

/* == Data ================================================================= */

static const wlmtk_element_vmt_t _wlmtk_submenu_element_vmt = {
    .destroy = _wlmtk_submenu_element_destroy
};

static const wlmtk_menu_item_vmt_t _wlmtk_submenu_menu_item_vmt = {
    .clicked = _wlmtk_submenu_menu_item_clicked
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_submenu_t *wlmtk_submenu_create(
    const wlmtk_menu_style_t *style_ptr,
    wlmtk_env_t *env_ptr,
    wlmtk_popup_menu_t *parent_pum_ptr)
{
    wlmtk_submenu_t *submenu_ptr = logged_calloc(1, sizeof(wlmtk_submenu_t));
    if (NULL == submenu_ptr) return NULL;

    if (!wlmtk_menu_item_init(
            &submenu_ptr->super_menu_item,
            &style_ptr->item,
            env_ptr)) {
        wlmtk_submenu_destroy(submenu_ptr);
        return NULL;
    }
    wlmtk_element_extend(
        wlmtk_menu_item_element(&submenu_ptr->super_menu_item),
        &_wlmtk_submenu_element_vmt);
    wlmtk_menu_item_extend(
        &submenu_ptr->super_menu_item,
        &_wlmtk_submenu_menu_item_vmt);
    // TODO(kaeser@gubbe.ch): Should not be required!
    submenu_ptr->super_menu_item.width = style_ptr->item.width;

    submenu_ptr->popup_menu_ptr = wlmtk_popup_menu_create(style_ptr, env_ptr);
    if (NULL == submenu_ptr->popup_menu_ptr) {
        wlmtk_submenu_destroy(submenu_ptr);
        return NULL;
    }


    wlmtk_menu_item_set_text(&submenu_ptr->super_menu_item, "FIXME");
    wlmtk_menu_item_init(&submenu_ptr->item1, &style_ptr->item, env_ptr);
    wlmtk_menu_item_init(&submenu_ptr->item2, &style_ptr->item, env_ptr);
    submenu_ptr->item1.width = style_ptr->item.width;
    submenu_ptr->item2.width = style_ptr->item.width;
    wlmtk_menu_item_set_text(&submenu_ptr->item1, "FIXME: Item 1");
    wlmtk_menu_item_set_text(&submenu_ptr->item2, "FIXME: Item 2");

    wlmtk_menu_add_item(
        wlmtk_popup_menu_menu(submenu_ptr->popup_menu_ptr),
        &submenu_ptr->item1);
    wlmtk_menu_add_item(
        wlmtk_popup_menu_menu(submenu_ptr->popup_menu_ptr),
        &submenu_ptr->item2);

    // FIXME: add new popup to parent wlmtk_popup_menu_popup(parent_pum_ptr);

    return submenu_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_submenu_destroy(wlmtk_submenu_t *submenu_ptr)
{

    if (NULL != submenu_ptr->popup_menu_ptr) {

        wlmtk_menu_remove_item(
            wlmtk_popup_menu_menu(submenu_ptr->popup_menu_ptr),
            &submenu_ptr->item1);
        wlmtk_menu_remove_item(
            wlmtk_popup_menu_menu(submenu_ptr->popup_menu_ptr),
            &submenu_ptr->item2);

        wlmtk_popup_menu_destroy(submenu_ptr->popup_menu_ptr);
        submenu_ptr->popup_menu_ptr = NULL;
    }

    wlmtk_menu_item_fini(&submenu_ptr->item2);
    wlmtk_menu_item_fini(&submenu_ptr->item1);

    wlmtk_menu_item_fini(&submenu_ptr->super_menu_item);
    free(submenu_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_menu_item_t *wlmtk_submenu_menu_item(wlmtk_submenu_t *submenu_ptr)
{
    return &submenu_ptr->super_menu_item;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
void _wlmtk_submenu_element_destroy(wlmtk_element_t *element_ptr)
{
    wlmtk_submenu_t *submenu_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_submenu_t,
        super_menu_item.super_buffer.super_element);
    wlmtk_submenu_destroy(submenu_ptr);
}

/* ------------------------------------------------------------------------- */
void _wlmtk_submenu_menu_item_clicked(wlmtk_menu_item_t *menu_item_ptr)
{
    wlmtk_submenu_t *submenu_ptr = BS_CONTAINER_OF(
        menu_item_ptr, wlmtk_submenu_t, super_menu_item);

    bs_log(BS_ERROR, "FIXME: Clicked: %p", submenu_ptr);
}

/* == End of submenu.c ===================================================== */
