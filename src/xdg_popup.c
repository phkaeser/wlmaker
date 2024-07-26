/* ========================================================================= */
/**
 * @file xdg_popup.c
 *
 * @copyright
 * Copyright 2023 Google LLC
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

#include "xdg_popup.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

static void handle_reposition(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_new_popup(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _wlmaker_xdg_popup_element_destroy(
    wlmtk_element_t *element_ptr);

/* == Data ================================================================= */

/** Virtual method table of the parent's @ref wlmtk_element_t. */
static const wlmtk_element_vmt_t _wlmaker_xdg_popup_element_vmt = {
    .destroy = _wlmaker_xdg_popup_element_destroy
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_xdg_popup_t *wlmaker_xdg_popup_create(
    struct wlr_xdg_popup *wlr_xdg_popup_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr = logged_calloc(
        1, sizeof(wlmaker_xdg_popup_t));
    if (NULL == wlmaker_xdg_popup_ptr) return NULL;
    wlmaker_xdg_popup_ptr->wlr_xdg_popup_ptr = wlr_xdg_popup_ptr;

    wlmaker_xdg_popup_ptr->surface_ptr = wlmtk_surface_create(
        wlr_xdg_popup_ptr->base->surface, env_ptr);
    if (NULL == wlmaker_xdg_popup_ptr->surface_ptr) {
        wlmaker_xdg_popup_destroy(wlmaker_xdg_popup_ptr);
        return NULL;
    }

    if (!wlmtk_popup_init(
            &wlmaker_xdg_popup_ptr->super_popup,
            env_ptr,
            wlmtk_surface_element(wlmaker_xdg_popup_ptr->surface_ptr))) {
        wlmaker_xdg_popup_destroy(wlmaker_xdg_popup_ptr);
        return NULL;
    }
    wlmtk_element_extend(
        wlmtk_popup_element(&wlmaker_xdg_popup_ptr->super_popup),
        &_wlmaker_xdg_popup_element_vmt);
    wlmtk_element_set_position(
        wlmtk_popup_element(&wlmaker_xdg_popup_ptr->super_popup),
        wlr_xdg_popup_ptr->scheduled.geometry.x,
        wlr_xdg_popup_ptr->scheduled.geometry.y);

    wlmtk_util_connect_listener_signal(
        &wlr_xdg_popup_ptr->events.reposition,
        &wlmaker_xdg_popup_ptr->reposition_listener,
        handle_reposition);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_popup_ptr->base->events.destroy,
        &wlmaker_xdg_popup_ptr->destroy_listener,
        handle_destroy);
    wlmtk_util_connect_listener_signal(
        &wlr_xdg_popup_ptr->base->events.new_popup,
        &wlmaker_xdg_popup_ptr->new_popup_listener,
        handle_new_popup);

    return wlmaker_xdg_popup_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_xdg_popup_destroy(wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr)
{
    wlmtk_util_disconnect_listener(
        &wlmaker_xdg_popup_ptr->new_popup_listener);
    wlmtk_util_disconnect_listener(
        &wlmaker_xdg_popup_ptr->destroy_listener);
    wlmtk_util_disconnect_listener(
        &wlmaker_xdg_popup_ptr->reposition_listener);

    wlmtk_popup_fini(&wlmaker_xdg_popup_ptr->super_popup);

    if (NULL != wlmaker_xdg_popup_ptr->surface_ptr) {
        wlmtk_surface_destroy(wlmaker_xdg_popup_ptr->surface_ptr);
        wlmaker_xdg_popup_ptr->surface_ptr = NULL;
    }
    free(wlmaker_xdg_popup_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Handles repositioning. */
void handle_reposition(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xdg_popup_t, reposition_listener);

    wlmtk_element_set_position(
        wlmtk_popup_element(&wlmaker_xdg_popup_ptr->super_popup),
        wlmaker_xdg_popup_ptr->wlr_xdg_popup_ptr->scheduled.geometry.x,
        wlmaker_xdg_popup_ptr->wlr_xdg_popup_ptr->scheduled.geometry.y);
}

/* ------------------------------------------------------------------------- */
/** Handles popup destruction: Removes from parent, and destroy. */
void handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xdg_popup_t, destroy_listener);

    wlmaker_xdg_popup_destroy(wlmaker_xdg_popup_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles further popups. Creates them and adds them to parent's content. */
void handle_new_popup(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xdg_popup_t, new_popup_listener);
    struct wlr_xdg_popup *wlr_xdg_popup_ptr = data_ptr;

    wlmaker_xdg_popup_t *new_popup_ptr = wlmaker_xdg_popup_create(
        wlr_xdg_popup_ptr,
        wlmtk_popup_element(&wlmaker_xdg_popup_ptr->super_popup)->env_ptr);
    if (NULL == new_popup_ptr) {
        wl_resource_post_error(
            wlr_xdg_popup_ptr->resource,
            WL_DISPLAY_ERROR_NO_MEMORY,
            "Failed wlmtk_xdg_popup_create.");
        return;
    }

    wlmtk_element_set_visible(
        wlmtk_popup_element(&new_popup_ptr->super_popup), true);
    wlmtk_container_add_element(
        &wlmaker_xdg_popup_ptr->super_popup.popup_container,
        wlmtk_popup_element(&new_popup_ptr->super_popup));

    bs_log(BS_INFO, "XDG popup %p: New popup %p",
           wlmaker_xdg_popup_ptr, wlr_xdg_popup_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of @ref wlmtk_element_vmt_t::destroy. Virtual dtor.
 *
 * @param element_ptr
 */
void _wlmaker_xdg_popup_element_destroy(
    wlmtk_element_t *element_ptr)
{
    wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr = BS_CONTAINER_OF(
        element_ptr, wlmaker_xdg_popup_t,
        super_popup.super_container.super_element);

    wlmaker_xdg_popup_destroy(wlmaker_xdg_popup_ptr);
}

/* == End of xdg_popup.c ==================================================== */
