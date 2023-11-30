/* ========================================================================= */
/**
 * @file window.c
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

#include "window.h"

#include "workspace.h"

/* == Declarations ========================================================= */

bool wlmtk_window_init(wlmtk_window_t *window_ptr,
                       const wlmtk_window_impl_t *impl_ptr,
                       wlmtk_env_t *env_ptr,
                       wlmtk_content_t *content_ptr);
void wlmtk_window_fini(wlmtk_window_t *window_ptr);

static bool _wlmtk_window_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);

    static void wlmtk_window_set_activated_impl(
    wlmtk_window_t *window_ptr,
    bool activated);
static void wlmtk_window_set_server_side_decorated_impl(
    wlmtk_window_t *window_ptr,
    bool decorated);
static void wlmtk_window_request_close_impl(wlmtk_window_t *window_ptr);
static void wlmtk_window_request_minimize_impl(wlmtk_window_t *window_ptr);
static void wlmtk_window_request_move_impl(wlmtk_window_t *window_ptr);
static void wlmtk_window_request_resize_impl(
    wlmtk_window_t *window_ptr,
    uint32_t edges);
static void wlmtk_window_request_size_impl(
    wlmtk_window_t *window_ptr,
    int width,
    int height);
static void wlmtk_window_request_position_and_size_impl(
    wlmtk_window_t *window_ptr,
    int x,
    int y,
    int width,
    int height);

static void fake_window_destroy(wlmtk_window_t *window_ptr);
static void fake_window_set_activated(
    wlmtk_window_t *window_ptr,
    bool activated);
static void fake_window_set_server_side_decorated(
    wlmtk_window_t *window_ptr,
    bool decorated);
static void fake_window_request_close(wlmtk_window_t *window_ptr);
static void fake_window_request_minimize(wlmtk_window_t *window_ptr);
static void fake_window_request_move(wlmtk_window_t *window_ptr);
static void fake_window_request_resize(
    wlmtk_window_t *window_ptr,
    uint32_t edges);
static void fake_window_request_size(
    wlmtk_window_t *window_ptr,
    int width,
    int height);
static void fake_window_request_position_and_size(
    wlmtk_window_t *window_ptr,
    int x,
    int y,
    int width,
    int height);

static wlmtk_pending_update_t *prepare_update(
    wlmtk_window_t *window_ptr);
static void release_update(
    wlmtk_window_t *window_ptr,
    wlmtk_pending_update_t *update_ptr);

static void _wlmtk_box_update_layout(wlmtk_container_t *container_ptr);

/* == Data ================================================================= */

/** Virtual method table for the window's element superclass. */
static const wlmtk_element_vmt_t window_element_vmt = {
    .pointer_button = _wlmtk_window_element_pointer_button,
};
/** Virtual method table for the window's container superclass. */
static const wlmtk_container_vmt_t window_container_vmt = {
    .update_layout = _wlmtk_box_update_layout,
};

/** Default methods of @ref wlmtk_window_t. To override for a mock. */
static const wlmtk_window_impl_t window_default_impl = {
    .destroy = wlmtk_window_destroy,
    .set_activated = wlmtk_window_set_activated_impl,
    .set_server_side_decorated = wlmtk_window_set_server_side_decorated_impl,
    .request_close = wlmtk_window_request_close_impl,
    .request_minimize = wlmtk_window_request_minimize_impl,
    .request_move = wlmtk_window_request_move_impl,
    .request_resize = wlmtk_window_request_resize_impl,
    .request_size = wlmtk_window_request_size_impl,
    .request_position_and_size = wlmtk_window_request_position_and_size_impl,
};

/** Default methods of @ref wlmtk_window_t. To override for a mock. */
static const wlmtk_window_impl_t fake_window_impl = {
    .destroy = fake_window_destroy,
    .set_activated = fake_window_set_activated,
    .set_server_side_decorated = fake_window_set_server_side_decorated,
    .request_close = fake_window_request_close,
    .request_minimize = fake_window_request_minimize,
    .request_move = fake_window_request_move,
    .request_resize = fake_window_request_resize,
    .request_size = fake_window_request_size,
    .request_position_and_size = fake_window_request_position_and_size,
};

/** Style of the title bar. */
// TODO(kaeser@gubbe.ch): Move to central config. */
static const wlmtk_titlebar_style_t titlebar_style = {
    .focussed_fill = {
        .type = WLMTK_STYLE_COLOR_HGRADIENT,
        .param = { .hgradient = { .from = 0xff505a5e,.to = 0xff202a2e }}
    },
    .blurred_fill = {
        .type = WLMTK_STYLE_COLOR_HGRADIENT,
        .param = { .hgradient = { .from = 0xffc2c0c5,.to = 0xff828085 }}
    },
    .focussed_text_color = 0xffffffff,
    .blurred_text_color = 0xff000000,
    .height = 22,
    .bezel_width = 1,
};

/** Style of the resize bar. */
// TODO(kaeser@gubbe.ch): Move to central config. */
static const wlmtk_resizebar_style_t resizebar_style = {
    .fill = {
        .type = WLMTK_STYLE_COLOR_SOLID,
        .param = { .solid = { .color = 0xffc2c0c5 }}
    },
    .height = 7,
    .corner_width = 29,
    .bezel_width = 1,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
/**
 * Initializes the window.
 *
 * @param window_ptr
 * @param impl_ptr
 * @param env_ptr
 * @param content_ptr         Will take ownership of it.
 *
 * @return true on success.
 */
bool wlmtk_window_init(wlmtk_window_t *window_ptr,
                       const wlmtk_window_impl_t *impl_ptr,
                       wlmtk_env_t *env_ptr,
                       wlmtk_content_t *content_ptr)
{
    BS_ASSERT(NULL != window_ptr);
    BS_ASSERT(NULL != impl_ptr);
    BS_ASSERT(NULL != impl_ptr->destroy);
    BS_ASSERT(NULL != impl_ptr->set_activated);
    BS_ASSERT(NULL != impl_ptr->set_server_side_decorated);
    BS_ASSERT(NULL != impl_ptr->request_close);
    BS_ASSERT(NULL != impl_ptr->request_minimize);
    BS_ASSERT(NULL != impl_ptr->request_move);
    BS_ASSERT(NULL != impl_ptr->request_resize);
    BS_ASSERT(NULL != impl_ptr->request_size);
    BS_ASSERT(NULL != impl_ptr->request_position_and_size);
    memcpy(&window_ptr->impl, impl_ptr, sizeof(wlmtk_window_impl_t));

    for (size_t i = 0; i < WLMTK_WINDOW_MAX_PENDING; ++i) {
        bs_dllist_push_back(&window_ptr->available_updates,
                            &window_ptr->pre_allocated_updates[i].dlnode);
    }

    if (!wlmtk_box_init(&window_ptr->super_box, env_ptr,
                        WLMTK_BOX_VERTICAL)) {
        wlmtk_window_fini(window_ptr);
        return false;
    }
    window_ptr->orig_super_element_vmt = wlmtk_element_extend(
        &window_ptr->super_box.super_container.super_element,
        &window_element_vmt);
    window_ptr->orig_super_container_vmt = wlmtk_container_extend(
        &window_ptr->super_box.super_container, &window_container_vmt);

    wlmtk_window_set_title(window_ptr, NULL);

    window_ptr->resizebar_ptr = wlmtk_resizebar_create(
        env_ptr, window_ptr, &resizebar_style);
    if (NULL == window_ptr->resizebar_ptr) {
        wlmtk_window_fini(window_ptr);
        return false;
    }
    wlmtk_container_add_element(
        &window_ptr->super_box.super_container,
        wlmtk_resizebar_element(window_ptr->resizebar_ptr));
    wlmtk_element_set_visible(
        wlmtk_resizebar_element(window_ptr->resizebar_ptr), true);

    wlmtk_container_add_element_atop(
        &window_ptr->super_box.super_container,
        wlmtk_resizebar_element(window_ptr->resizebar_ptr),
        wlmtk_content_element(content_ptr));
    window_ptr->content_ptr = content_ptr;
    wlmtk_content_set_window(content_ptr, window_ptr);
    wlmtk_element_set_visible(wlmtk_content_element(content_ptr), true);

    window_ptr->titlebar_ptr = wlmtk_titlebar_create(
        env_ptr, window_ptr, &titlebar_style);
    if (NULL == window_ptr->titlebar_ptr) {
        wlmtk_window_fini(window_ptr);
        return false;
    }
    wlmtk_container_add_element(
        &window_ptr->super_box.super_container,
        wlmtk_titlebar_element(window_ptr->titlebar_ptr));
    wlmtk_element_set_visible(
        wlmtk_titlebar_element(window_ptr->titlebar_ptr), true);

    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Uninitializes the winodw.
 *
 * @param window_ptr
 */
void wlmtk_window_fini(wlmtk_window_t *window_ptr)
{
    if (NULL != window_ptr->titlebar_ptr) {
        wlmtk_container_remove_element(
            &window_ptr->super_box.super_container,
            wlmtk_titlebar_element(window_ptr->titlebar_ptr));
        wlmtk_titlebar_destroy(window_ptr->titlebar_ptr);
        window_ptr->titlebar_ptr = NULL;
    }

    if (NULL != window_ptr->resizebar_ptr) {
        wlmtk_container_remove_element(
            &window_ptr->super_box.super_container,
            wlmtk_resizebar_element(window_ptr->resizebar_ptr));
        wlmtk_resizebar_destroy(window_ptr->resizebar_ptr);
        window_ptr->resizebar_ptr = NULL;
    }

    if (NULL != window_ptr->content_ptr) {
        wlmtk_container_remove_element(
            &window_ptr->super_box.super_container,
            wlmtk_content_element(window_ptr->content_ptr));
        wlmtk_element_set_visible(
            wlmtk_content_element(window_ptr->content_ptr), false);
        wlmtk_content_set_window(window_ptr->content_ptr, NULL);

        wlmtk_element_destroy(wlmtk_content_element(window_ptr->content_ptr));
        window_ptr->content_ptr = NULL;
    }

    if (NULL != window_ptr->title_ptr) {
        free(window_ptr->title_ptr);
        window_ptr->title_ptr = NULL;
    }

    wlmtk_box_fini(&window_ptr->super_box);
}

/* ------------------------------------------------------------------------- */
wlmtk_window_t *wlmtk_window_create(
    wlmtk_env_t *env_ptr,
    wlmtk_content_t *content_ptr)
{
    wlmtk_window_t *window_ptr = logged_calloc(1, sizeof(wlmtk_window_t));
    if (NULL == window_ptr) return NULL;

    if (!wlmtk_window_init(
            window_ptr, &window_default_impl, env_ptr, content_ptr)) {
        wlmtk_window_destroy(window_ptr);
        return NULL;
    }

    return window_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_destroy(wlmtk_window_t *window_ptr)
{
    wlmtk_window_fini(window_ptr);
    free(window_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_window_element(wlmtk_window_t *window_ptr)
{
    return &window_ptr->super_box.super_container.super_element;
}

/* ------------------------------------------------------------------------- */
wlmtk_window_t *wlmtk_window_from_element(wlmtk_element_t *element_ptr)
{
    // DEBT: FIXME - The assertion here is too lose.
    wlmtk_window_t *window_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_window_t, super_box.super_container.super_element);

    BS_ASSERT(_wlmtk_box_update_layout ==
              window_ptr->super_box.super_container.vmt.update_layout);
    return window_ptr;
}


/* ------------------------------------------------------------------------- */
void wlmtk_window_get_size(
    wlmtk_window_t *window_ptr,
    int *width_ptr,
    int *height_ptr)
{
    // TODO(kaeser@gubbe.ch): Add decoration, if server-side-decorated.
    wlmtk_content_get_size(window_ptr->content_ptr, width_ptr, height_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_serial(wlmtk_window_t *window_ptr, uint32_t serial)
{
    bs_dllist_node_t *dlnode_ptr;
    while (NULL != (dlnode_ptr = window_ptr->pending_updates.head_ptr)) {
        wlmtk_pending_update_t *pending_update_ptr = BS_CONTAINER_OF(
            dlnode_ptr, wlmtk_pending_update_t, dlnode);

        int32_t delta = pending_update_ptr->serial - serial;
        if (0 < delta) break;

        if (pending_update_ptr->serial == serial) {
            if (window_ptr->content_ptr->committed_width !=
                pending_update_ptr->width) {
                bs_log(BS_ERROR, "FIXME: width mismatch!");
            }
            if (window_ptr->content_ptr->committed_height !=
                pending_update_ptr->height) {
                bs_log(BS_ERROR, "FIXME: height mismatch!");
            }
        }

        wlmtk_element_set_position(
            wlmtk_window_element(window_ptr),
            pending_update_ptr->x,
            pending_update_ptr->y);
        release_update(window_ptr, pending_update_ptr);
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_set_activated(
    wlmtk_window_t *window_ptr,
    bool activated)
{
    window_ptr->impl.set_activated(window_ptr, activated);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_set_title(
    wlmtk_window_t *window_ptr,
    const char *title_ptr)
{
    char *new_title_ptr = NULL;
    if (NULL != title_ptr) {
        new_title_ptr = logged_strdup(title_ptr);
        BS_ASSERT(NULL != new_title_ptr);
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Unnamed window %p", window_ptr);
        new_title_ptr = logged_strdup(buf);
        BS_ASSERT(NULL != new_title_ptr);
    }

    if (NULL != window_ptr->title_ptr) {
        if (0 == strcmp(window_ptr->title_ptr, new_title_ptr)) {
            free(new_title_ptr);
            return;
        }
        free(window_ptr->title_ptr);
    }
    window_ptr->title_ptr = new_title_ptr;

    if (NULL != window_ptr->titlebar_ptr) {
        wlmtk_titlebar_set_title(window_ptr->titlebar_ptr,
                                 window_ptr->title_ptr);
    }
}

/* ------------------------------------------------------------------------- */
const char *wlmtk_window_get_title(wlmtk_window_t *window_ptr)
{
    BS_ASSERT(NULL != window_ptr->title_ptr);
    return window_ptr->title_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_set_server_side_decorated(
    wlmtk_window_t *window_ptr,
    bool decorated)
{
    window_ptr->impl.set_server_side_decorated(window_ptr, decorated);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_request_close(wlmtk_window_t *window_ptr)
{
    window_ptr->impl.request_close(window_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_request_minimize(wlmtk_window_t *window_ptr)
{
    window_ptr->impl.request_minimize(window_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_request_move(wlmtk_window_t *window_ptr)
{
    window_ptr->impl.request_move(window_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_request_resize(wlmtk_window_t *window_ptr,
                                 uint32_t edges)
{
    window_ptr->impl.request_resize(window_ptr, edges);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_request_size(
    wlmtk_window_t *window_ptr,
    int width,
    int height)
{
    window_ptr->impl.request_size(window_ptr, width, height);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_request_position_and_size(
    wlmtk_window_t *window_ptr,
    int x,
    int y,
    int width,
    int height)
{
    window_ptr->impl.request_position_and_size(
        window_ptr, x, y, width, height);
}

/* ------------------------------------------------------------------------- */
wlmtk_fake_window_t *wlmtk_fake_window_create(void)
{
    wlmtk_fake_window_t *fake_window_ptr = logged_calloc(
        1, sizeof(wlmtk_fake_window_t));
    if (NULL == fake_window_ptr) return NULL;

    fake_window_ptr->fake_content_ptr = wlmtk_fake_content_create();
    if (NULL == fake_window_ptr->fake_content_ptr) {
        wlmtk_fake_window_destroy(fake_window_ptr);
        return NULL;
    }

    if (!wlmtk_window_init(&fake_window_ptr->window,
                           &fake_window_impl,
                           NULL,
                           &fake_window_ptr->fake_content_ptr->content)) {
        wlmtk_fake_window_destroy(fake_window_ptr);
        return NULL;
    }

    return fake_window_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_fake_window_destroy(wlmtk_fake_window_t *fake_window_ptr)
{
    wlmtk_window_fini(&fake_window_ptr->window);
    free(fake_window_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Activates window on button press, and calls the parent's implementation. */
bool _wlmtk_window_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmtk_window_t *window_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_window_t, super_box.super_container.super_element);

    // We shouldn't receive buttons when not mapped.
    BS_ASSERT(
        NULL !=
        window_ptr->super_box.super_container.super_element.parent_container_ptr);
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_from_container(
        window_ptr->super_box.super_container.super_element.parent_container_ptr);
    wlmtk_workspace_activate_window(workspace_ptr, window_ptr);
    wlmtk_workspace_raise_window(workspace_ptr, window_ptr);

    return window_ptr->orig_super_element_vmt.pointer_button(
        element_ptr, button_event_ptr);
}

/* ------------------------------------------------------------------------- */
/** Default implementation of @ref wlmtk_window_set_activated. */
void wlmtk_window_set_activated_impl(
    wlmtk_window_t *window_ptr,
    bool activated)
{
    wlmtk_content_set_activated(window_ptr->content_ptr, activated);
    if (NULL != window_ptr->titlebar_ptr) {
        wlmtk_titlebar_set_activated(window_ptr->titlebar_ptr, activated);
    }
}

/* ------------------------------------------------------------------------- */
/** Default implementation of @ref wlmtk_window_set_server_side_decorated. */
void wlmtk_window_set_server_side_decorated_impl(
    wlmtk_window_t *window_ptr,
    bool decorated)
{
    // TODO(kaeser@gubbe.ch): Implement.
    bs_log(BS_INFO, "Set server side decoration for window %p: %d",
           window_ptr, decorated);
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_window_request_close. Requests content closure. */
void wlmtk_window_request_close_impl(wlmtk_window_t *window_ptr)
{
    wlmtk_content_request_close(window_ptr->content_ptr);
}

/* ------------------------------------------------------------------------- */
/** Default implementation of @ref wlmtk_window_request_minimize. */
void wlmtk_window_request_minimize_impl(wlmtk_window_t *window_ptr)
{
    bs_log(BS_INFO, "Requesting window %p to minimize.", window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Default implementation of @ref wlmtk_window_request_move. */
void wlmtk_window_request_move_impl(wlmtk_window_t *window_ptr)
{
    BS_ASSERT(
        NULL !=
        window_ptr->super_box.super_container.super_element.parent_container_ptr);
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_from_container(
        window_ptr->super_box.super_container.super_element.parent_container_ptr);
    wlmtk_workspace_begin_window_move(workspace_ptr, window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Default implementation of @ref wlmtk_window_request_resize. */
void wlmtk_window_request_resize_impl(wlmtk_window_t *window_ptr, uint32_t edges)
{
    BS_ASSERT(
        NULL !=
        window_ptr->super_box.super_container.super_element.parent_container_ptr);
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_from_container(
        window_ptr->super_box.super_container.super_element.parent_container_ptr);
    wlmtk_workspace_begin_window_resize(workspace_ptr, window_ptr, edges);
}

/* ------------------------------------------------------------------------- */
/** Default implementation of @ref wlmtk_window_request_size. */
void wlmtk_window_request_size_impl(
    wlmtk_window_t *window_ptr,
    int width,
    int height)
{
    // TODO(kaeser@gubbe.ch): Adjust for decoration size, if server-side.
    wlmtk_content_request_size(window_ptr->content_ptr, width, height);

    // TODO(kaeser@gubbe.ch): For client content (eg. a wlr_surface), setting
    // the size is an asynchronous operation and should be handled as such.
    // Meaning: In example of resizing at the top-left corner, we'll want to
    // request the content to adjust size, but wait with adjusting the
    // content position until the size adjustment is applied. This implies we
    // may need to combine the request_size and set_position methods for window.
}

/* ------------------------------------------------------------------------- */
/** Default implementation of @ref wlmtk_window_request_position_and_size. */
void wlmtk_window_request_position_and_size_impl(
    wlmtk_window_t *window_ptr,
    int x,
    int y,
    int width,
    int height)
{
    uint32_t serial = wlmtk_content_request_size(
        window_ptr->content_ptr, width, height);

    wlmtk_pending_update_t *pending_update_ptr = prepare_update(window_ptr);
    pending_update_ptr->serial = serial;
    pending_update_ptr->x = x;
    pending_update_ptr->y = y;
    pending_update_ptr->width = width;
    pending_update_ptr->height = height;

    // TODO(kaeser@gubbe.ch): Handle synchronous case: @ref wlmtk_window_serial
    // may have been called early, so we should check if serial had just been
    // called before (or is below the last @wlmt_window_serial). In that case,
    // the pending state should be applied right away.
}

/* ------------------------------------------------------------------------- */
/**
 * Prepares a positional update: Allocates an item and attach it to the end
 * of the list of pending updates.
 *
 * @param window_ptr
 *
 * @return A pointer to a @ref wlmtk_pending_update_t, already positioned at the
 *     back of @ref wlmtk_window_t::pending_updates.
 */
wlmtk_pending_update_t *prepare_update(
    wlmtk_window_t *window_ptr)
{
    bs_dllist_node_t *dlnode_ptr = bs_dllist_pop_front(
        &window_ptr->available_updates);
    if (NULL == dlnode_ptr) {
        dlnode_ptr = bs_dllist_pop_front(&window_ptr->pending_updates);
        bs_log(BS_WARNING, "Window %p: No updates available.", window_ptr);
        // TODO(kaeser@gubbe.ch): Hm, should we apply this (old) update?
    }
    wlmtk_pending_update_t *update_ptr = BS_CONTAINER_OF(
        dlnode_ptr, wlmtk_pending_update_t, dlnode);
    bs_dllist_push_back(&window_ptr->pending_updates, &update_ptr->dlnode);
    return update_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Releases a pending positional update. Moves it to the list of
 * @ref wlmtk_window_t::available_updates.
 *
 * @param window_ptr
 * @param update_ptr
 */
void release_update(
    wlmtk_window_t *window_ptr,
    wlmtk_pending_update_t *update_ptr)
{
    bs_dllist_remove(&window_ptr->pending_updates, &update_ptr->dlnode);
    bs_dllist_push_front(&window_ptr->available_updates, &update_ptr->dlnode);
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of @ref wlmtk_container_vmt_t::update_layout.
 *
 * Invoked when the window's contained elements triggered a layout update,
 * and will use this to trigger (potential) size updates to the window
 * decorations.
 *
 * @param container_ptr
 */
void _wlmtk_box_update_layout(wlmtk_container_t *container_ptr)
{
    wlmtk_window_t *window_ptr = BS_CONTAINER_OF(
        container_ptr, wlmtk_window_t, super_box.super_container);

    window_ptr->orig_super_container_vmt.update_layout(container_ptr);

    if (NULL != window_ptr->content_ptr) {
        int width;
        wlmtk_content_get_size(window_ptr->content_ptr, &width, NULL);
        if (NULL != window_ptr->titlebar_ptr) {
            wlmtk_titlebar_set_width(window_ptr->titlebar_ptr, width);
        }
        if (NULL != window_ptr->resizebar_ptr) {
            wlmtk_resizebar_set_width(window_ptr->resizebar_ptr, width);
        }
    }
}

/* == Virtual method implementation for the fake window ==================== */

/* ------------------------------------------------------------------------- */
/** Virtual dtor, wraps to @ref wlmtk_fake_window_destroy. */
void fake_window_destroy(wlmtk_window_t *window_ptr)
{
    wlmtk_fake_window_t *fake_window_ptr = BS_CONTAINER_OF(
        window_ptr, wlmtk_fake_window_t, window);
    wlmtk_fake_window_destroy(fake_window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Fake implementation of @ref wlmtk_window_set_activated. */
void fake_window_set_activated(
    wlmtk_window_t *window_ptr,
    bool activated)
{
    wlmtk_fake_window_t *fake_window_ptr = BS_CONTAINER_OF(
        window_ptr, wlmtk_fake_window_t, window);
    fake_window_ptr->activated = activated;
}

/* ------------------------------------------------------------------------- */
/** Fake implementation of @ref wlmtk_window_set_server_side_decorated. */
void fake_window_set_server_side_decorated(
    wlmtk_window_t *window_ptr,
    bool decorated)
{
    wlmtk_fake_window_t *fake_window_ptr = BS_CONTAINER_OF(
        window_ptr, wlmtk_fake_window_t, window);
    fake_window_ptr->decorated = decorated;
}

/* ------------------------------------------------------------------------- */
/** Fake implementation of @ref wlmtk_window_request_close. */
void fake_window_request_close(wlmtk_window_t *window_ptr)
{
    wlmtk_fake_window_t *fake_window_ptr = BS_CONTAINER_OF(
        window_ptr, wlmtk_fake_window_t, window);
    fake_window_ptr->request_close_called = true;
}

/* ------------------------------------------------------------------------- */
/** Fake implementation of @ref wlmtk_window_request_minimize. */
void fake_window_request_minimize(wlmtk_window_t *window_ptr)
{
    wlmtk_fake_window_t *fake_window_ptr = BS_CONTAINER_OF(
        window_ptr, wlmtk_fake_window_t, window);
    fake_window_ptr->request_minimize_called = true;
}

/* ------------------------------------------------------------------------- */
/** Fake implementation of @ref wlmtk_window_request_move. */
void fake_window_request_move(wlmtk_window_t *window_ptr)
{
    wlmtk_fake_window_t *fake_window_ptr = BS_CONTAINER_OF(
        window_ptr, wlmtk_fake_window_t, window);
    fake_window_ptr->request_move_called = true;
}

/* ------------------------------------------------------------------------- */
/** Fake implementation of @ref wlmtk_window_request_resize. */
void fake_window_request_resize(
    wlmtk_window_t *window_ptr,
    uint32_t edges)
{
    wlmtk_fake_window_t *fake_window_ptr = BS_CONTAINER_OF(
        window_ptr, wlmtk_fake_window_t, window);
    fake_window_ptr->request_resize_called = true;
    fake_window_ptr->request_resize_edges = edges;
}

/* ------------------------------------------------------------------------- */
/** Fake implementation of @ref wlmtk_window_request_size. */
void fake_window_request_size(
    wlmtk_window_t *window_ptr,
    int width,
    int height)
{
    wlmtk_fake_window_t *fake_window_ptr = BS_CONTAINER_OF(
        window_ptr, wlmtk_fake_window_t, window);
    fake_window_ptr->request_size_called = true;
    fake_window_ptr->width = width;
    fake_window_ptr->height = height;
}

/* ------------------------------------------------------------------------- */
/** Fake implementation of @ref wlmtk_window_request_position_and_size. */
void fake_window_request_position_and_size(
    wlmtk_window_t *window_ptr,
    int x,
    int y,
    int width,
    int height)
{
    wlmtk_fake_window_t *fake_window_ptr = BS_CONTAINER_OF(
        window_ptr, wlmtk_fake_window_t, window);
    fake_window_ptr->request_position_and_size_called = true;
    fake_window_ptr->x = x;
    fake_window_ptr->y = y;
    fake_window_ptr->width = width;
    fake_window_ptr->height = height;
}


/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);
static void test_set_title(bs_test_t *test_ptr);
static void test_request_close(bs_test_t *test_ptr);
static void test_set_activated(bs_test_t *test_ptr);
static void test_fake(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_window_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "set_title", test_set_title },
    { 1, "request_close", test_request_close },
    { 1, "set_activated", test_set_activated },
    { 1, "fake", test_fake },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests setup and teardown. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_fake_content_t *fake_content_ptr = wlmtk_fake_content_create();
    wlmtk_window_t *window_ptr = wlmtk_window_create(
        NULL, &fake_content_ptr->content);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, window_ptr,
                      fake_content_ptr->content.window_ptr);

    wlmtk_window_destroy(window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests title. */
void test_set_title(bs_test_t *test_ptr)
{
    wlmtk_fake_content_t *fake_content_ptr = wlmtk_fake_content_create();
    wlmtk_window_t *window_ptr = wlmtk_window_create(
        NULL, &fake_content_ptr->content);

    wlmtk_window_set_title(window_ptr, "Title");
    BS_TEST_VERIFY_STREQ(
        test_ptr,
        "Title",
        wlmtk_window_get_title(window_ptr));

    wlmtk_window_set_title(window_ptr, NULL);
    BS_TEST_VERIFY_STRMATCH(
        test_ptr,
        wlmtk_window_get_title(window_ptr),
        "Unnamed window .*");

    wlmtk_window_destroy(window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests activation. */
void test_request_close(bs_test_t *test_ptr)
{
    wlmtk_fake_content_t *fake_content_ptr = wlmtk_fake_content_create();
    wlmtk_window_t *window_ptr = wlmtk_window_create(
        NULL, &fake_content_ptr->content);

    wlmtk_window_request_close(window_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, fake_content_ptr->request_close_called);

    wlmtk_window_destroy(window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests activation. */
void test_set_activated(bs_test_t *test_ptr)
{
    wlmtk_fake_content_t *fake_content_ptr = wlmtk_fake_content_create();
    wlmtk_window_t *window_ptr = wlmtk_window_create(
        NULL, &fake_content_ptr->content);

    wlmtk_window_set_activated(window_ptr, true);
    BS_TEST_VERIFY_TRUE(test_ptr, fake_content_ptr->activated);

    wlmtk_window_set_activated(window_ptr, false);
    BS_TEST_VERIFY_FALSE(test_ptr, fake_content_ptr->activated);

    wlmtk_window_destroy(window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests fake window ctor and dtor. */
void test_fake(bs_test_t *test_ptr)
{
    wlmtk_fake_window_t *fake_window_ptr = wlmtk_fake_window_create();
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fake_window_ptr);
    wlmtk_fake_window_destroy(fake_window_ptr);
}

/* == End of window.c ====================================================== */
