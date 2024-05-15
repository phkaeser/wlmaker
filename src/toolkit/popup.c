/* ========================================================================= */
/**
 * @file popup.c
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

#include "popup.h"

/* == Declarations ========================================================= */

static void _wlmtk_popup_handle_surface_map(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmtk_popup_handle_surface_unmap(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_popup_init(
    wlmtk_popup_t *popup_ptr,
    wlmtk_env_t *env_ptr,
    wlmtk_surface_t *surface_ptr)
{
    memset(popup_ptr, 0, sizeof(wlmtk_popup_t));
    if (!wlmtk_container_init(&popup_ptr->super_container, env_ptr)) {
        return false;
    }

    if (!wlmtk_container_init(&popup_ptr->popup_container, env_ptr)) {
        wlmtk_popup_fini(popup_ptr);
        return false;
    }
    wlmtk_container_add_element(
        &popup_ptr->super_container,
        &popup_ptr->popup_container.super_element);
    wlmtk_element_set_visible(&popup_ptr->popup_container.super_element, true);

    if (NULL != surface_ptr) {
        wlmtk_container_add_element(
            &popup_ptr->super_container,
            wlmtk_surface_element(surface_ptr));
        popup_ptr->surface_ptr = surface_ptr;

        wlmtk_surface_connect_map_listener_signal(
            surface_ptr,
            &popup_ptr->surface_map_listener,
            _wlmtk_popup_handle_surface_map);
        wlmtk_surface_connect_unmap_listener_signal(
            surface_ptr,
            &popup_ptr->surface_unmap_listener,
            _wlmtk_popup_handle_surface_unmap);
    }

    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_popup_fini(wlmtk_popup_t *popup_ptr)
{
    if (NULL != wlmtk_popup_element(popup_ptr)->parent_container_ptr) {
        wlmtk_container_remove_element(
            wlmtk_popup_element(popup_ptr)->parent_container_ptr,
            wlmtk_popup_element(popup_ptr));
    }

    if (NULL != popup_ptr->surface_ptr) {
        wlmtk_util_disconnect_listener(&popup_ptr->surface_unmap_listener);
        wlmtk_util_disconnect_listener(&popup_ptr->surface_map_listener);

        wlmtk_container_remove_element(
            &popup_ptr->super_container,
            wlmtk_surface_element(popup_ptr->surface_ptr));
        popup_ptr->surface_ptr = NULL;
    }

    if (popup_ptr->popup_container.super_element.parent_container_ptr) {
        wlmtk_container_remove_element(
            &popup_ptr->super_container,
            &popup_ptr->popup_container.super_element);
    }
    wlmtk_container_fini(&popup_ptr->popup_container);

    wlmtk_container_fini(&popup_ptr->super_container);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_popup_element(wlmtk_popup_t *popup_ptr)
{
    return &popup_ptr->super_container.super_element;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handles the `surface_map` signal of the `wlr_surface`: Makes the popup
 * visible.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmtk_popup_handle_surface_map(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_popup_t *popup_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_popup_t, surface_map_listener);

    wlmtk_element_set_visible(
        wlmtk_surface_element(popup_ptr->surface_ptr),
        true);
}

/* ------------------------------------------------------------------------- */
/**
 * Handles the `surface_unmap` signal of the `wlr_surface`: Makes the popup
 * invisible.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmtk_popup_handle_surface_unmap(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_popup_t *popup_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_popup_t, surface_unmap_listener);

    wlmtk_element_set_visible(
        wlmtk_surface_element(popup_ptr->surface_ptr),
        false);
}

/* == End of popup.c ======================================================= */
