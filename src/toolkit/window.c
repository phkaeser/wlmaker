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

#include "box.h"
#include "titlebar.h"
#include "workspace.h"

/* == Declarations ========================================================= */

/** Maximum number of pending state updates. */
#define WLMTK_WINDOW_MAX_PENDING 64

/** State of the window. */
struct _wlmtk_window_t {
    /** Superclass: Box. */
    wlmtk_box_t               super_box;

    /** Content of this window. */
    wlmtk_content_t           *content_ptr;
    /** Titlebar. */
    wlmtk_titlebar_t          *titlebar_ptr;

    /** Pending updates. */
    bs_dllist_t               pending_updates;
    /** Set of pre-allocated updates. */
    bs_dllist_t               updates_available;
};

/** Pending positional updates. */
typedef struct {
    /** Node within @ref wlmtk_window_t::pending_updates. */
    bs_dllist_node_t          dlnode;
    /** Serial of the update. */
    uint32_t                  serial;
    /** Pending X position. */
    int                       x;
    /** Pending Y position. */
    int                       y;
    /** Width that is to be committed at serial. */
    unsigned                  width;
    /** Height that is to be committed at serial. */
    unsigned                  height;
} wlmtk_pending_update_t;

static void box_update_layout(wlmtk_box_t *box_ptr);
static void window_box_destroy(wlmtk_box_t *box_ptr);

/* == Data ================================================================= */

/** Method table for the box's virtual methods. */
static const wlmtk_box_impl_t window_box_impl = {
    .destroy = window_box_destroy,
    .update_layout = box_update_layout,
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
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_window_t *wlmtk_window_create(wlmtk_content_t *content_ptr)
{
    wlmtk_window_t *window_ptr = logged_calloc(1, sizeof(wlmtk_window_t));
    if (NULL == window_ptr) return NULL;

    if (!wlmtk_box_init(&window_ptr->super_box,
                        &window_box_impl,
                        WLMTK_BOX_VERTICAL)) {
        wlmtk_window_destroy(window_ptr);
        return NULL;
    }

    wlmtk_container_add_element(
        &window_ptr->super_box.super_container,
        wlmtk_content_element(content_ptr));
    window_ptr->content_ptr = content_ptr;
    wlmtk_content_set_window(content_ptr, window_ptr);
    wlmtk_element_set_visible(wlmtk_content_element(content_ptr), true);

    int width;
    wlmtk_content_get_size(content_ptr, &width, NULL);
    window_ptr->titlebar_ptr = wlmtk_titlebar_create(
        width, &titlebar_style);
    if (NULL == window_ptr->titlebar_ptr) {
        wlmtk_window_destroy(window_ptr);
        return NULL;
    }
    wlmtk_container_add_element(
        &window_ptr->super_box.super_container,
        wlmtk_titlebar_element(window_ptr->titlebar_ptr));
    wlmtk_element_set_visible(
        wlmtk_titlebar_element(window_ptr->titlebar_ptr), true);

    return window_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_destroy(wlmtk_window_t *window_ptr)
{
    if (NULL != window_ptr->titlebar_ptr) {
        wlmtk_container_remove_element(
            &window_ptr->super_box.super_container,
            wlmtk_titlebar_element(window_ptr->titlebar_ptr));
        wlmtk_titlebar_destroy(window_ptr->titlebar_ptr);
        window_ptr->titlebar_ptr = NULL;
    }

    wlmtk_container_remove_element(
        &window_ptr->super_box.super_container,
        wlmtk_content_element(window_ptr->content_ptr));
    wlmtk_element_set_visible(
        wlmtk_content_element(window_ptr->content_ptr), false);
    wlmtk_content_set_window(window_ptr->content_ptr, NULL);

    if (NULL != window_ptr->content_ptr) {
        wlmtk_content_destroy(window_ptr->content_ptr);
        window_ptr->content_ptr = NULL;
    }

    wlmtk_box_fini(&window_ptr->super_box);
    free(window_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_window_element(wlmtk_window_t *window_ptr)
{
    return &window_ptr->super_box.super_container.super_element;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_set_activated(
    wlmtk_window_t *window_ptr,
    bool activated)
{
    wlmtk_content_set_activated(window_ptr->content_ptr, activated);
    if (NULL != window_ptr->titlebar_ptr) {
        wlmtk_titlebar_set_activated(window_ptr->titlebar_ptr, activated);
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_set_server_side_decorated(
    wlmtk_window_t *window_ptr,
    bool decorated)
{
    // TODO(kaeser@gubbe.ch): Implement.
    bs_log(BS_INFO, "Set server side decoration for window %p: %d",
           window_ptr, decorated);
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
void wlmtk_window_request_size(
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
void wlmtk_window_request_position_and_size(
    wlmtk_window_t *window_ptr,
    int x,
    int y,
    int width,
    int height)
{
    uint32_t serial = wlmtk_content_request_size(
        window_ptr->content_ptr, width, height);

    // TODO(kaeser@gubbe.ch): Use a pre-allocated array, and apply pending
    // states when in risk of overflow.
    wlmtk_pending_update_t *pending_update_ptr = logged_calloc(
        1, sizeof(wlmtk_pending_update_t));
    BS_ASSERT(NULL != pending_update_ptr);
    pending_update_ptr->serial = serial;
    pending_update_ptr->x = x;
    pending_update_ptr->y = y;
    pending_update_ptr->width = width;
    pending_update_ptr->height = height;
    bs_dllist_push_back(&window_ptr->pending_updates,
                        &pending_update_ptr->dlnode);

    // TODO(kaeser@gubbe.ch): Handle synchronous case: @ref wlmtk_window_serial
    // may have been called early, so we should check if serial had just been
    // called before (or is below the last @wlmt_window_serial). In that case,
    // the pending state should be applied right away.
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_serial(wlmtk_window_t *window_ptr, uint32_t serial)
{
    while (!bs_dllist_empty(&window_ptr->pending_updates)) {
        bs_dllist_node_t *dlnode_ptr = window_ptr->pending_updates.head_ptr;
        wlmtk_pending_update_t *pending_update_ptr = BS_CONTAINER_OF(
            dlnode_ptr, wlmtk_pending_update_t, dlnode);

        if (pending_update_ptr->serial > serial) {
            break;
        }

        BS_ASSERT(dlnode_ptr == bs_dllist_pop_front(
                      &window_ptr->pending_updates));
        if (pending_update_ptr->serial < serial) {
            free(pending_update_ptr);
            continue;
        }

        BS_ASSERT(pending_update_ptr->serial == serial);
        wlmtk_element_set_position(
            wlmtk_window_element(window_ptr),
            pending_update_ptr->x,
            pending_update_ptr->y);
        free(pending_update_ptr);
    }
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_request_move(wlmtk_window_t *window_ptr)
{
    BS_ASSERT(
        NULL !=
        window_ptr->super_box.super_container.super_element.parent_container_ptr);
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_from_container(
        window_ptr->super_box.super_container.super_element.parent_container_ptr);
    wlmtk_workspace_begin_window_move(workspace_ptr, window_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window_request_resize(wlmtk_window_t *window_ptr, uint32_t edges)
{
    BS_ASSERT(
        NULL !=
        window_ptr->super_box.super_container.super_element.parent_container_ptr);
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_from_container(
        window_ptr->super_box.super_container.super_element.parent_container_ptr);
    wlmtk_workspace_begin_window_resize(workspace_ptr, window_ptr, edges);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Implementation of @ref wlmtk_box_impl_t::update_layout.
 *
 * Invoked when the window's contained elements triggered a layout update,
 * and will use this to trigger (potential) size updates to the window
 * decorations.
 *
 * @param box_ptr
 */
void box_update_layout(wlmtk_box_t *box_ptr)
{
    wlmtk_window_t *window_ptr = BS_CONTAINER_OF(
        box_ptr, wlmtk_window_t, super_box);

    if (NULL != window_ptr->content_ptr) {
        int width;
        wlmtk_content_get_size(window_ptr->content_ptr, &width, NULL);
        if (NULL != window_ptr->titlebar_ptr) {
            wlmtk_titlebar_set_width(window_ptr->titlebar_ptr, width);
        }
    }
}

/* ------------------------------------------------------------------------- */
/** Virtual destructor, in case called from box. Wraps to our dtor. */
void window_box_destroy(wlmtk_box_t *box_ptr)
{
    wlmtk_window_t *window_ptr = BS_CONTAINER_OF(
        box_ptr, wlmtk_window_t, super_box);
    wlmtk_window_destroy(window_ptr);
}

/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);
static void test_set_activated(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_window_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "set_activated", test_set_activated },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests setup and teardown. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_fake_content_t *fake_content_ptr = wlmtk_fake_content_create();
    wlmtk_window_t *window_ptr = wlmtk_window_create(
        &fake_content_ptr->content);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, window_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, window_ptr,
                      fake_content_ptr->content.window_ptr);

    wlmtk_window_destroy(window_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests activation. */
void test_set_activated(bs_test_t *test_ptr)
{
    wlmtk_fake_content_t *fake_content_ptr = wlmtk_fake_content_create();
    wlmtk_window_t *window_ptr = wlmtk_window_create(
        &fake_content_ptr->content);

    wlmtk_window_set_activated(window_ptr, true);
    BS_TEST_VERIFY_TRUE(test_ptr, fake_content_ptr->activated);

    wlmtk_window_set_activated(window_ptr, false);
    BS_TEST_VERIFY_FALSE(test_ptr, fake_content_ptr->activated);

    wlmtk_window_destroy(window_ptr);
}

/* == End of window.c ====================================================== */
