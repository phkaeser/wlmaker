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
static void handle_destroy2(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_new_popup(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_surface_map(
    struct wl_listener *listener_ptr,
    void *data_ptr);

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

    if (!wlmtk_content_init(
            &wlmaker_xdg_popup_ptr->super_content,
            wlmaker_xdg_popup_ptr->surface_ptr,
            env_ptr)) {
        wlmaker_xdg_popup_destroy(wlmaker_xdg_popup_ptr);
        return NULL;
    }

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

    wlmtk_surface_connect_map_listener_signal(
        wlmaker_xdg_popup_ptr->surface_ptr,
        &wlmaker_xdg_popup_ptr->surface_map_listener,
        handle_surface_map);

    return wlmaker_xdg_popup_ptr;
}

/* ------------------------------------------------------------------------- */
wlmaker_xdg_popup_t *wlmaker_xdg_popup2_create(
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
            wlmaker_xdg_popup_ptr->surface_ptr)) {
        wlmaker_xdg_popup_destroy(wlmaker_xdg_popup_ptr);
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &wlr_xdg_popup_ptr->base->events.destroy,
        &wlmaker_xdg_popup_ptr->destroy_listener,
        handle_destroy2);

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

    wlmtk_content_fini(&wlmaker_xdg_popup_ptr->super_content);

    wlmtk_popup_fini(&wlmaker_xdg_popup_ptr->super_popup);

    if (NULL != wlmaker_xdg_popup_ptr->surface_ptr) {
        wlmtk_surface_destroy(wlmaker_xdg_popup_ptr->surface_ptr);
        wlmaker_xdg_popup_ptr->surface_ptr = NULL;
    }
    free(wlmaker_xdg_popup_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Handles repositioning. Yet unimplemented. */
void handle_reposition(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xdg_popup_t, reposition_listener);

    bs_log(BS_WARNING, "Unhandled: reposition on XDG popup %p",
           wlmaker_xdg_popup_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles popup destruction: Removes from parent content, and destroy. */
void handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xdg_popup_t, destroy_listener);

    wlmtk_content_t *parent_content_ptr = wlmtk_content_get_parent_content(
        &wlmaker_xdg_popup_ptr->super_content);
    if (NULL != parent_content_ptr) {
        wlmtk_content_remove_popup(
            parent_content_ptr,
            &wlmaker_xdg_popup_ptr->super_content);
    }

    wlmaker_xdg_popup_destroy(wlmaker_xdg_popup_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles popup destruction: Removes from parent content, and destroy. */
void handle_destroy2(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xdg_popup_t, destroy_listener);

    // FIXME: This is ugly.
    wlmtk_container_t *parent_container_ptr = wlmtk_popup_element(
        &wlmaker_xdg_popup_ptr->super_popup)->parent_container_ptr;
    if (NULL != parent_container_ptr) {
        wlmtk_container_remove_element(
            parent_container_ptr,
            wlmtk_popup_element(&wlmaker_xdg_popup_ptr->super_popup));
    }

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

    wlmaker_xdg_popup_t *new_xdg_popup_ptr = wlmaker_xdg_popup_create(
        wlr_xdg_popup_ptr,
        wlmtk_content_element(&wlmaker_xdg_popup_ptr->super_content)->env_ptr);
    if (NULL == new_xdg_popup_ptr) {
        bs_log(BS_ERROR, "Failed xdg_popup_create(%p, %p)",
               wlr_xdg_popup_ptr,
               wlmtk_content_element(
                   &wlmaker_xdg_popup_ptr->super_content)->env_ptr);
        return;
    }

    wlmtk_element_set_visible(
        wlmtk_content_element(&new_xdg_popup_ptr->super_content), true);
    wlmtk_content_add_popup(
        &wlmaker_xdg_popup_ptr->super_content,
        &new_xdg_popup_ptr->super_content);

    bs_log(BS_INFO, "XDG popup %p: New popup %p",
           wlmaker_xdg_popup_ptr, new_xdg_popup_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `map` signal of the @ref wlmtk_surface_t.
 *
 * The only aspect to handle here is the positioning of the surface. Note:
 * It might be recommendable to move this under a `configure` handler (?).
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_surface_map(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_xdg_popup_t *wlmaker_xdg_popup_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_xdg_popup_t, surface_map_listener);

    wlmtk_element_set_position(
        wlmtk_content_element(&wlmaker_xdg_popup_ptr->super_content),
        wlmaker_xdg_popup_ptr->wlr_xdg_popup_ptr->current.geometry.x,
        wlmaker_xdg_popup_ptr->wlr_xdg_popup_ptr->current.geometry.y);
}

/* == End of xdg_popup.c ==================================================== */
