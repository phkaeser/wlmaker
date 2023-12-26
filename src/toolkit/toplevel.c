/* ========================================================================= */
/**
 * @file toplevel.c
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

#include "toplevel.h"

#include "rectangle.h"
#include "workspace.h"

#include "wlr/util/box.h"

/* == Declarations ========================================================= */

/** Maximum number of pending state updates. */
#define WLMTK_TOPLEVEL_MAX_PENDING 64

/** Virtual method table for the toplevel. */
struct  _wlmtk_toplevel_vmt_t {
    /** Destructor. */
    void (*destroy)(wlmtk_toplevel_t *toplevel_ptr);
    /** Virtual method for @ref wlmtk_toplevel_set_activated. */
    void (*set_activated)(wlmtk_toplevel_t *toplevel_ptr,
                          bool activated);
    /** Virtual method for @ref wlmtk_toplevel_request_close. */
    void (*request_close)(wlmtk_toplevel_t *toplevel_ptr);
    /** Virtual method for @ref wlmtk_toplevel_request_minimize. */
    void (*request_minimize)(wlmtk_toplevel_t *toplevel_ptr);
    /** Virtual method for @ref wlmtk_toplevel_request_move. */
    void (*request_move)(wlmtk_toplevel_t *toplevel_ptr);
    /** Virtual method for @ref wlmtk_toplevel_request_resize. */
    void (*request_resize)(wlmtk_toplevel_t *toplevel_ptr,
                           uint32_t edges);
    /** Virtual method for @ref wlmtk_toplevel_request_position_and_size. */
    void (*request_position_and_size)(wlmtk_toplevel_t *toplevel_ptr,
                                      int x, int y, int width, int height);
};

/** Pending positional updates for @ref wlmtk_toplevel_t::content_ptr. */
typedef struct {
    /** Node within @ref wlmtk_toplevel_t::pending_updates. */
    bs_dllist_node_t          dlnode;
    /** Serial of the update. */
    uint32_t                  serial;
    /** Pending X position of the content. */
    int                       x;
    /** Pending Y position of the content. */
    int                       y;
    /** Content's width that is to be committed at serial. */
    unsigned                  width;
    /** Content's hehight that is to be committed at serial. */
    unsigned                  height;
} wlmtk_pending_update_t;

/** State of the toplevel. */
struct _wlmtk_toplevel_t {
    /** Superclass: Bordered. */
    wlmtk_bordered_t          super_bordered;
    /** Original virtual method table of the toplevel's element superclass. */
    wlmtk_element_vmt_t       orig_super_element_vmt;
    /** Original virtual method table of the toplevel' container superclass. */
    wlmtk_container_vmt_t     orig_super_container_vmt;

    /** Virtual method table. */
    wlmtk_toplevel_vmt_t        vmt;

    /** Box: In `super_bordered`, holds content, title bar and resizebar. */
    wlmtk_box_t               box;

    /** Content of this toplevel. */
    wlmtk_content_t           *content_ptr;
    /** Titlebar. */
    wlmtk_titlebar_t          *titlebar_ptr;
    /** Resizebar. */
    wlmtk_resizebar_t         *resizebar_ptr;

    /** Toplevel title. Set through @ref wlmtk_toplevel_set_title. */
    char                      *title_ptr;

    /** Pending updates. */
    bs_dllist_t               pending_updates;
    /** List of udpates currently available. */
    bs_dllist_t               available_updates;
    /** Pre-alloocated updates. */
    wlmtk_pending_update_t     pre_allocated_updates[WLMTK_TOPLEVEL_MAX_PENDING];

    /** Organic size of the toplevel, ie. when not maximized. */
    struct wlr_box             organic_size;
    /** Whether the toplevel has been requested as maximized. */
    bool                       maximized;

    /**
     * Stores whether the toplevel is server-side decorated.
     *
     * This is equivalent to (titlebar_ptr != NULL && resizebar_ptr != NULL).
     */
    bool                       server_side_decorated;
};

/** State of a fake toplevel: Includes the public record and the toplevel. */
typedef struct {
    /** Toplevel state. */
    wlmtk_toplevel_t            toplevel;
    /** Fake toplevel - public state. */
    wlmtk_fake_toplevel_t       fake_toplevel;
} wlmtk_fake_toplevel_state_t;

static bool _wlmtk_toplevel_init(
    wlmtk_toplevel_t *toplevel_ptr,
    wlmtk_env_t *env_ptr,
    wlmtk_content_t *content_ptr);
static void _wlmtk_toplevel_fini(wlmtk_toplevel_t *toplevel_ptr);
static wlmtk_toplevel_vmt_t _wlmtk_toplevel_extend(
    wlmtk_toplevel_t *toplevel_ptr,
    const wlmtk_toplevel_vmt_t *toplevel_vmt_ptr);

static bool _wlmtk_toplevel_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);
static void _wlmtk_toplevel_container_update_layout(
    wlmtk_container_t *container_ptr);

static void _wlmtk_toplevel_set_activated(
    wlmtk_toplevel_t *toplevel_ptr,
    bool activated);
static void _wlmtk_toplevel_request_close(wlmtk_toplevel_t *toplevel_ptr);
static void _wlmtk_toplevel_request_minimize(wlmtk_toplevel_t *toplevel_ptr);
static void _wlmtk_toplevel_request_move(wlmtk_toplevel_t *toplevel_ptr);
static void _wlmtk_toplevel_request_resize(
    wlmtk_toplevel_t *toplevel_ptr,
    uint32_t edges);
static void _wlmtk_toplevel_request_position_and_size(
    wlmtk_toplevel_t *toplevel_ptr,
    int x,
    int y,
    int width,
    int height);

static wlmtk_pending_update_t *_wlmtk_toplevel_prepare_update(
    wlmtk_toplevel_t *toplevel_ptr);
static void _wlmtk_toplevel_release_update(
    wlmtk_toplevel_t *toplevel_ptr,
    wlmtk_pending_update_t *update_ptr);
static wlmtk_workspace_t *_wlmtk_toplevel_workspace(wlmtk_toplevel_t *toplevel_ptr);

/* == Data ================================================================= */

/** Virtual method table for the toplevel's element superclass. */
static const wlmtk_element_vmt_t toplevel_element_vmt = {
    .pointer_button = _wlmtk_toplevel_element_pointer_button,
};
/** Virtual method table for the toplevel's container superclass. */
static const wlmtk_container_vmt_t toplevel_container_vmt = {
    .update_layout = _wlmtk_toplevel_container_update_layout,
};
/** Virtual method table for the toplevel itself. */
static const wlmtk_toplevel_vmt_t _wlmtk_toplevel_vmt = {
    .set_activated = _wlmtk_toplevel_set_activated,
    .request_close = _wlmtk_toplevel_request_close,
    .request_minimize = _wlmtk_toplevel_request_minimize,
    .request_move = _wlmtk_toplevel_request_move,
    .request_resize = _wlmtk_toplevel_request_resize,
    .request_position_and_size = _wlmtk_toplevel_request_position_and_size,
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
    .margin_style = { .width = 1, .color = 0xff000000 },
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
    .margin_style = { .width = 0, .color = 0xff000000 },
};

/** Style of the margin between title, content and resizebar. */
static const wlmtk_margin_style_t margin_style = {
    .width = 1,
    .color = 0xff000000,
};

/** Style of the border around the toplevel. */
static const wlmtk_margin_style_t border_style = {
    .width = 1,
    .color = 0xff000000,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_toplevel_t *wlmtk_toplevel_create(
    wlmtk_env_t *env_ptr,
    wlmtk_content_t *content_ptr)
{
    wlmtk_toplevel_t *toplevel_ptr = logged_calloc(1, sizeof(wlmtk_toplevel_t));
    if (NULL == toplevel_ptr) return NULL;

    if (!_wlmtk_toplevel_init(toplevel_ptr, env_ptr, content_ptr)) {
        wlmtk_toplevel_destroy(toplevel_ptr);
        return NULL;
    }

    return toplevel_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_toplevel_destroy(wlmtk_toplevel_t *toplevel_ptr)
{
    _wlmtk_toplevel_fini(toplevel_ptr);
    free(toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_toplevel_element(wlmtk_toplevel_t *toplevel_ptr)
{
    return &toplevel_ptr->super_bordered.super_container.super_element;
}

/* ------------------------------------------------------------------------- */
wlmtk_toplevel_t *wlmtk_toplevel_from_element(wlmtk_element_t *element_ptr)
{
    wlmtk_toplevel_t *toplevel_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_toplevel_t, super_bordered.super_container.super_element);
    BS_ASSERT(_wlmtk_toplevel_container_update_layout ==
              toplevel_ptr->super_bordered.super_container.vmt.update_layout);
    return toplevel_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_toplevel_set_activated(
    wlmtk_toplevel_t *toplevel_ptr,
    bool activated)
{
    toplevel_ptr->vmt.set_activated(toplevel_ptr, activated);
}

/* ------------------------------------------------------------------------- */
void wlmtk_toplevel_set_server_side_decorated(
    wlmtk_toplevel_t *toplevel_ptr,
    bool decorated)
{
    // TODO(kaeser@gubbe.ch): Implement.
    bs_log(BS_INFO, "Set server side decoration for toplevel %p: %d",
           toplevel_ptr, decorated);

    if (toplevel_ptr->server_side_decorated == decorated) return;

    if (decorated) {
        // Create decoration.
        toplevel_ptr->titlebar_ptr = wlmtk_titlebar_create(
            toplevel_ptr->super_bordered.super_container.super_element.env_ptr,
            toplevel_ptr, &titlebar_style);
        BS_ASSERT(NULL != toplevel_ptr->titlebar_ptr);
        wlmtk_element_set_visible(
            wlmtk_titlebar_element(toplevel_ptr->titlebar_ptr), true);
        wlmtk_box_add_element_front(
            &toplevel_ptr->box,
            wlmtk_titlebar_element(toplevel_ptr->titlebar_ptr));

        toplevel_ptr->resizebar_ptr = wlmtk_resizebar_create(
            toplevel_ptr->super_bordered.super_container.super_element.env_ptr,
            toplevel_ptr, &resizebar_style);
        BS_ASSERT(NULL != toplevel_ptr->resizebar_ptr);
        wlmtk_element_set_visible(
            wlmtk_resizebar_element(toplevel_ptr->resizebar_ptr), true);
        wlmtk_box_add_element_back(
            &toplevel_ptr->box,
            wlmtk_resizebar_element(toplevel_ptr->resizebar_ptr));
    } else {
        // Remove & destroy the decoration.
        wlmtk_box_remove_element(
            &toplevel_ptr->box,
            wlmtk_titlebar_element(toplevel_ptr->titlebar_ptr));
        wlmtk_titlebar_destroy(toplevel_ptr->titlebar_ptr);
        toplevel_ptr->titlebar_ptr = NULL;

        wlmtk_box_remove_element(
            &toplevel_ptr->box,
            wlmtk_resizebar_element(toplevel_ptr->resizebar_ptr));
        wlmtk_resizebar_destroy(toplevel_ptr->resizebar_ptr);
        toplevel_ptr->resizebar_ptr = NULL;
    }

    toplevel_ptr->server_side_decorated = decorated;
}

/* ------------------------------------------------------------------------- */
void wlmtk_toplevel_set_title(
    wlmtk_toplevel_t *toplevel_ptr,
    const char *title_ptr)
{
    char *new_title_ptr = NULL;
    if (NULL != title_ptr) {
        new_title_ptr = logged_strdup(title_ptr);
        BS_ASSERT(NULL != new_title_ptr);
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Unnamed toplevel %p", toplevel_ptr);
        new_title_ptr = logged_strdup(buf);
        BS_ASSERT(NULL != new_title_ptr);
    }

    if (NULL != toplevel_ptr->title_ptr) {
        if (0 == strcmp(toplevel_ptr->title_ptr, new_title_ptr)) {
            free(new_title_ptr);
            return;
        }
        free(toplevel_ptr->title_ptr);
    }
    toplevel_ptr->title_ptr = new_title_ptr;

    if (NULL != toplevel_ptr->titlebar_ptr) {
        wlmtk_titlebar_set_title(toplevel_ptr->titlebar_ptr,
                                 toplevel_ptr->title_ptr);
    }
}

/* ------------------------------------------------------------------------- */
const char *wlmtk_toplevel_get_title(wlmtk_toplevel_t *toplevel_ptr)
{
    BS_ASSERT(NULL != toplevel_ptr->title_ptr);
    return toplevel_ptr->title_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_toplevel_request_close(wlmtk_toplevel_t *toplevel_ptr)
{
    toplevel_ptr->vmt.request_close(toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_toplevel_request_minimize(wlmtk_toplevel_t *toplevel_ptr)
{
    toplevel_ptr->vmt.request_minimize(toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_toplevel_request_maximize(
    wlmtk_toplevel_t *toplevel_ptr,
    bool maximized)
{
    if (toplevel_ptr->maximized == maximized) return;

    toplevel_ptr->maximized = maximized;

    struct wlr_box box;
    if (toplevel_ptr->maximized) {
        box = wlmtk_workspace_get_maximize_extents(
            _wlmtk_toplevel_workspace(toplevel_ptr));
    } else {
        box = toplevel_ptr->organic_size;
    }

    _wlmtk_toplevel_request_position_and_size(
        toplevel_ptr, box.x, box.y, box.width, box.height);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_toplevel_maximized(wlmtk_toplevel_t *toplevel_ptr)
{
    return toplevel_ptr->maximized;
}

/* ------------------------------------------------------------------------- */
void wlmtk_toplevel_request_move(wlmtk_toplevel_t *toplevel_ptr)
{
    toplevel_ptr->vmt.request_move(toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_toplevel_request_resize(wlmtk_toplevel_t *toplevel_ptr,
                                 uint32_t edges)
{
    toplevel_ptr->vmt.request_resize(toplevel_ptr, edges);
}

/* ------------------------------------------------------------------------- */
void wlmtk_toplevel_set_position(wlmtk_toplevel_t *toplevel_ptr, int x, int y)
{
    toplevel_ptr->organic_size.x = x;
    toplevel_ptr->organic_size.y = y;
    wlmtk_element_set_position(wlmtk_toplevel_element(toplevel_ptr), x, y);
}

/* ------------------------------------------------------------------------- */
void wlmtk_toplevel_get_size(
    wlmtk_toplevel_t *toplevel_ptr,
    int *width_ptr,
    int *height_ptr)
{
    // TODO(kaeser@gubbe.ch): Add decoration, if server-side-decorated.
    wlmtk_content_get_size(toplevel_ptr->content_ptr, width_ptr, height_ptr);

    if (NULL != toplevel_ptr->titlebar_ptr) {
        *height_ptr += titlebar_style.height + margin_style.width;
    }
    if (NULL != toplevel_ptr->resizebar_ptr) {
        *height_ptr += resizebar_style.height + margin_style.width;
    }
    *height_ptr += 2 * border_style.width;

    *width_ptr += 2 * border_style.width;
}

/* ------------------------------------------------------------------------- */
void wlmtk_toplevel_request_size(
    wlmtk_toplevel_t *toplevel_ptr,
    int width,
    int height)
{
    // TODO(kaeser@gubbe.ch): Adjust for decoration size, if server-side.
    wlmtk_content_request_size(toplevel_ptr->content_ptr, width, height);

    // TODO(kaeser@gubbe.ch): For client content (eg. a wlr_surface), setting
    // the size is an asynchronous operation and should be handled as such.
    // Meaning: In example of resizing at the top-left corner, we'll want to
    // request the content to adjust size, but wait with adjusting the
    // content position until the size adjustment is applied. This implies we
    // may need to combine the request_size and set_position methods for toplevel.
}

/* ------------------------------------------------------------------------- */
struct wlr_box wlmtk_toplevel_get_position_and_size(
    wlmtk_toplevel_t *toplevel_ptr)
{
    struct wlr_box box;

    wlmtk_element_get_position(
        wlmtk_toplevel_element(toplevel_ptr), &box.x, &box.y);
    wlmtk_toplevel_get_size(toplevel_ptr, &box.width, &box.height);
    return box;
}

/* ------------------------------------------------------------------------- */
void wlmtk_toplevel_request_position_and_size(
    wlmtk_toplevel_t *toplevel_ptr,
    int x,
    int y,
    int width,
    int height)
{
    toplevel_ptr->vmt.request_position_and_size(
        toplevel_ptr, x, y, width, height);

    toplevel_ptr->organic_size.x = x;
    toplevel_ptr->organic_size.y = y;
    toplevel_ptr->organic_size.width = width;
    toplevel_ptr->organic_size.height = height;
}

/* ------------------------------------------------------------------------- */
void wlmtk_toplevel_serial(wlmtk_toplevel_t *toplevel_ptr, uint32_t serial)
{
    bs_dllist_node_t *dlnode_ptr;

    if (!toplevel_ptr->maximized &&
        NULL == toplevel_ptr->pending_updates.head_ptr) {
        wlmtk_toplevel_get_size(toplevel_ptr,
                              &toplevel_ptr->organic_size.width,
                              &toplevel_ptr->organic_size.height);
        return;
    }

    while (NULL != (dlnode_ptr = toplevel_ptr->pending_updates.head_ptr)) {
        wlmtk_pending_update_t *pending_update_ptr = BS_CONTAINER_OF(
            dlnode_ptr, wlmtk_pending_update_t, dlnode);

        int32_t delta = pending_update_ptr->serial - serial;
        if (0 < delta) break;

        if (pending_update_ptr->serial == serial) {
            if (toplevel_ptr->content_ptr->committed_width !=
                pending_update_ptr->width) {
                bs_log(BS_ERROR, "FIXME: width mismatch!");
            }
            if (toplevel_ptr->content_ptr->committed_height !=
                pending_update_ptr->height) {
                bs_log(BS_ERROR, "FIXME: height mismatch!");
            }
        }

        wlmtk_element_set_position(
            wlmtk_toplevel_element(toplevel_ptr),
            pending_update_ptr->x,
            pending_update_ptr->y);
        _wlmtk_toplevel_release_update(toplevel_ptr, pending_update_ptr);
    }
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Initializes an (allocated) toplevel.
 *
 * @param toplevel_ptr
 * @param env_ptr
 * @param content_ptr
 *
 * @return true on success.
 */
bool _wlmtk_toplevel_init(
    wlmtk_toplevel_t *toplevel_ptr,
    wlmtk_env_t *env_ptr,
    wlmtk_content_t *content_ptr)
{
    BS_ASSERT(NULL != toplevel_ptr);
    memcpy(&toplevel_ptr->vmt, &_wlmtk_toplevel_vmt, sizeof(wlmtk_toplevel_vmt_t));

    for (size_t i = 0; i < WLMTK_TOPLEVEL_MAX_PENDING; ++i) {
        bs_dllist_push_back(&toplevel_ptr->available_updates,
                            &toplevel_ptr->pre_allocated_updates[i].dlnode);
    }

    if (!wlmtk_box_init(&toplevel_ptr->box, env_ptr,
                        WLMTK_BOX_VERTICAL,
                        &margin_style)) {
        _wlmtk_toplevel_fini(toplevel_ptr);
        return false;
    }
    wlmtk_element_set_visible(
        &toplevel_ptr->box.super_container.super_element, true);

    if (!wlmtk_bordered_init(&toplevel_ptr->super_bordered,
                             env_ptr,
                             &toplevel_ptr->box.super_container.super_element,
                             &border_style)) {
        _wlmtk_toplevel_fini(toplevel_ptr);
        return false;
    }

    toplevel_ptr->orig_super_element_vmt = wlmtk_element_extend(
        &toplevel_ptr->super_bordered.super_container.super_element,
        &toplevel_element_vmt);
    toplevel_ptr->orig_super_container_vmt = wlmtk_container_extend(
        &toplevel_ptr->super_bordered.super_container, &toplevel_container_vmt);

    wlmtk_toplevel_set_title(toplevel_ptr, NULL);

    wlmtk_box_add_element_front(
        &toplevel_ptr->box,
        wlmtk_content_element(content_ptr));
    toplevel_ptr->content_ptr = content_ptr;
    wlmtk_content_set_toplevel(content_ptr, toplevel_ptr);
    wlmtk_element_set_visible(wlmtk_content_element(content_ptr), true);

    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Uninitializes the winodw.
 *
 * @param toplevel_ptr
 */
void _wlmtk_toplevel_fini(wlmtk_toplevel_t *toplevel_ptr)
{
    wlmtk_toplevel_set_server_side_decorated(toplevel_ptr, false);

    if (NULL != toplevel_ptr->content_ptr) {
        wlmtk_box_remove_element(
            &toplevel_ptr->box,
            wlmtk_content_element(toplevel_ptr->content_ptr));
        wlmtk_element_set_visible(
            wlmtk_content_element(toplevel_ptr->content_ptr), false);
        wlmtk_content_set_toplevel(toplevel_ptr->content_ptr, NULL);

        wlmtk_element_destroy(wlmtk_content_element(toplevel_ptr->content_ptr));
        toplevel_ptr->content_ptr = NULL;
    }

    if (NULL != toplevel_ptr->title_ptr) {
        free(toplevel_ptr->title_ptr);
        toplevel_ptr->title_ptr = NULL;
    }

    wlmtk_bordered_fini(&toplevel_ptr->super_bordered);
    wlmtk_box_fini(&toplevel_ptr->box);
}

/* ------------------------------------------------------------------------- */
/**
 * Extends the toplevel's virtual methods.
 *
 * @param toplevel_ptr
 * @param toplevel_vmt_ptr
 *
 * @return The previous virtual method table.
 */
wlmtk_toplevel_vmt_t _wlmtk_toplevel_extend(
    wlmtk_toplevel_t *toplevel_ptr,
    const wlmtk_toplevel_vmt_t *toplevel_vmt_ptr)
{
    wlmtk_toplevel_vmt_t orig_vmt = toplevel_ptr->vmt;

    if (NULL != toplevel_vmt_ptr->set_activated) {
        toplevel_ptr->vmt.set_activated = toplevel_vmt_ptr->set_activated;
    }
    if (NULL != toplevel_vmt_ptr->request_close) {
        toplevel_ptr->vmt.request_close = toplevel_vmt_ptr->request_close;
    }
    if (NULL != toplevel_vmt_ptr->request_minimize) {
        toplevel_ptr->vmt.request_minimize = toplevel_vmt_ptr->request_minimize;
    }
    if (NULL != toplevel_vmt_ptr->request_move) {
        toplevel_ptr->vmt.request_move = toplevel_vmt_ptr->request_move;
    }
    if (NULL != toplevel_vmt_ptr->request_resize) {
        toplevel_ptr->vmt.request_resize = toplevel_vmt_ptr->request_resize;
    }
    if (NULL != toplevel_vmt_ptr->request_position_and_size) {
        toplevel_ptr->vmt.request_position_and_size =
            toplevel_vmt_ptr->request_position_and_size;
    }

    return orig_vmt;
}

/* ------------------------------------------------------------------------- */
/** Activates toplevel on button press, and calls the parent's implementation. */
bool _wlmtk_toplevel_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmtk_toplevel_t *toplevel_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_toplevel_t, super_bordered.super_container.super_element);

    // We shouldn't receive buttons when not mapped.
    wlmtk_workspace_t *workspace_ptr = _wlmtk_toplevel_workspace(toplevel_ptr);
    wlmtk_workspace_activate_toplevel(workspace_ptr, toplevel_ptr);
    wlmtk_workspace_raise_toplevel(workspace_ptr, toplevel_ptr);

    return toplevel_ptr->orig_super_element_vmt.pointer_button(
        element_ptr, button_event_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of @ref wlmtk_container_vmt_t::update_layout.
 *
 * Invoked when the toplevel's contained elements triggered a layout update,
 * and will use this to trigger (potential) size updates to the toplevel
 * decorations.
 *
 * @param container_ptr
 */
void _wlmtk_toplevel_container_update_layout(wlmtk_container_t *container_ptr)
{
    wlmtk_toplevel_t *toplevel_ptr = BS_CONTAINER_OF(
        container_ptr, wlmtk_toplevel_t, super_bordered.super_container);

    toplevel_ptr->orig_super_container_vmt.update_layout(container_ptr);

    if (NULL != toplevel_ptr->content_ptr) {
        int width;
        wlmtk_content_get_size(toplevel_ptr->content_ptr, &width, NULL);
        if (NULL != toplevel_ptr->titlebar_ptr) {
            wlmtk_titlebar_set_width(toplevel_ptr->titlebar_ptr, width);
        }
        if (NULL != toplevel_ptr->resizebar_ptr) {
            wlmtk_resizebar_set_width(toplevel_ptr->resizebar_ptr, width);
        }
    }
}

/* ------------------------------------------------------------------------- */
/** Default implementation of @ref wlmtk_toplevel_set_activated. */
void _wlmtk_toplevel_set_activated(
    wlmtk_toplevel_t *toplevel_ptr,
    bool activated)
{
    wlmtk_content_set_activated(toplevel_ptr->content_ptr, activated);
    if (NULL != toplevel_ptr->titlebar_ptr) {
        wlmtk_titlebar_set_activated(toplevel_ptr->titlebar_ptr, activated);
    }
}

/* ------------------------------------------------------------------------- */
/** Default implementation of @ref wlmtk_toplevel_request_close. */
void _wlmtk_toplevel_request_close(wlmtk_toplevel_t *toplevel_ptr)
{
    wlmtk_content_request_close(toplevel_ptr->content_ptr);
}

/* ------------------------------------------------------------------------- */
/** Default implementation of @ref wlmtk_toplevel_request_minimize. */
void _wlmtk_toplevel_request_minimize(wlmtk_toplevel_t *toplevel_ptr)
{
    bs_log(BS_INFO, "Requesting toplevel %p to minimize.", toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
/** Default implementation of @ref wlmtk_toplevel_request_move. */
void _wlmtk_toplevel_request_move(wlmtk_toplevel_t *toplevel_ptr)
{
    wlmtk_workspace_begin_toplevel_move(
        _wlmtk_toplevel_workspace(toplevel_ptr), toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
/** Default implementation of @ref wlmtk_toplevel_request_resize. */
void _wlmtk_toplevel_request_resize(wlmtk_toplevel_t *toplevel_ptr, uint32_t edges)
{
    wlmtk_workspace_begin_toplevel_resize(
        _wlmtk_toplevel_workspace(toplevel_ptr), toplevel_ptr, edges);
}

/* ------------------------------------------------------------------------- */
/** Default implementation of @ref wlmtk_toplevel_request_position_and_size. */
void _wlmtk_toplevel_request_position_and_size(
    wlmtk_toplevel_t *toplevel_ptr,
    int x,
    int y,
    int width,
    int height)
{
    // Correct for borders, margin and decoration.
    if (NULL != toplevel_ptr->titlebar_ptr) {
        height -= titlebar_style.height + margin_style.width;
    }
    if (NULL != toplevel_ptr->resizebar_ptr) {
        height -= resizebar_style.height + margin_style.width;
    }
    height -= 2 * border_style.width;
    width -= 2 * border_style.width;
    height = BS_MAX(0, height);
    width = BS_MAX(0, width);

    uint32_t serial = wlmtk_content_request_size(
        toplevel_ptr->content_ptr, width, height);

    wlmtk_pending_update_t *pending_update_ptr =
        _wlmtk_toplevel_prepare_update(toplevel_ptr);
    pending_update_ptr->serial = serial;
    pending_update_ptr->x = x;
    pending_update_ptr->y = y;
    pending_update_ptr->width = width;
    pending_update_ptr->height = height;

    // TODO(kaeser@gubbe.ch): Handle synchronous case: @ref wlmtk_toplevel_serial
    // may have been called early, so we should check if serial had just been
    // called before (or is below the last @wlmt_toplevel_serial). In that case,
    // the pending state should be applied right away.
}

/* ------------------------------------------------------------------------- */
/**
 * Prepares a positional update: Allocates an item and attach it to the end
 * of the list of pending updates.
 *
 * @param toplevel_ptr
 *
 * @return A pointer to a @ref wlmtk_pending_update_t, already positioned at the
 *     back of @ref wlmtk_toplevel_t::pending_updates.
 */
wlmtk_pending_update_t *_wlmtk_toplevel_prepare_update(
    wlmtk_toplevel_t *toplevel_ptr)
{
    bs_dllist_node_t *dlnode_ptr = bs_dllist_pop_front(
        &toplevel_ptr->available_updates);
    if (NULL == dlnode_ptr) {
        dlnode_ptr = bs_dllist_pop_front(&toplevel_ptr->pending_updates);
        bs_log(BS_WARNING, "Toplevel %p: No updates available.", toplevel_ptr);
        // TODO(kaeser@gubbe.ch): Hm, should we apply this (old) update?
    }
    wlmtk_pending_update_t *update_ptr = BS_CONTAINER_OF(
        dlnode_ptr, wlmtk_pending_update_t, dlnode);
    bs_dllist_push_back(&toplevel_ptr->pending_updates, &update_ptr->dlnode);
    return update_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Releases a pending positional update. Moves it to the list of
 * @ref wlmtk_toplevel_t::available_updates.
 *
 * @param toplevel_ptr
 * @param update_ptr
 */
void _wlmtk_toplevel_release_update(
    wlmtk_toplevel_t *toplevel_ptr,
    wlmtk_pending_update_t *update_ptr)
{
    bs_dllist_remove(&toplevel_ptr->pending_updates, &update_ptr->dlnode);
    bs_dllist_push_front(&toplevel_ptr->available_updates, &update_ptr->dlnode);
}

/* ------------------------------------------------------------------------- */
/** Returns the workspace of the (mapped) toplevel. */
wlmtk_workspace_t *_wlmtk_toplevel_workspace(wlmtk_toplevel_t *toplevel_ptr)
{
    BS_ASSERT(NULL != wlmtk_toplevel_element(toplevel_ptr)->parent_container_ptr);
    return wlmtk_workspace_from_container(
        wlmtk_toplevel_element(toplevel_ptr)->parent_container_ptr);
}

/* == Implementation of the fake toplevel ==================================== */

static void _wlmtk_fake_toplevel_set_activated(
    wlmtk_toplevel_t *toplevel_ptr,
    bool activated);
static void _wlmtk_fake_toplevel_request_close(wlmtk_toplevel_t *toplevel_ptr);
static void _wlmtk_fake_toplevel_request_minimize(wlmtk_toplevel_t *toplevel_ptr);
static void _wlmtk_fake_toplevel_request_move(wlmtk_toplevel_t *toplevel_ptr);
static void _wlmtk_fake_toplevel_request_resize(
    wlmtk_toplevel_t *toplevel_ptr,
    uint32_t edges);
static void _wlmtk_fake_toplevel_request_position_and_size(
    wlmtk_toplevel_t *toplevel_ptr,
    int x,
    int y,
    int width,
    int height);

/** Virtual method table for the fake toplevel itself. */
static const wlmtk_toplevel_vmt_t _wlmtk_fake_toplevel_vmt = {
    .set_activated = _wlmtk_fake_toplevel_set_activated,
    .request_close = _wlmtk_fake_toplevel_request_close,
    .request_minimize = _wlmtk_fake_toplevel_request_minimize,
    .request_move = _wlmtk_fake_toplevel_request_move,
    .request_resize = _wlmtk_fake_toplevel_request_resize,
    .request_position_and_size = _wlmtk_fake_toplevel_request_position_and_size,

};

/* ------------------------------------------------------------------------- */
wlmtk_fake_toplevel_t *wlmtk_fake_toplevel_create(void)
{
    wlmtk_fake_toplevel_state_t *fake_toplevel_state_ptr = logged_calloc(
        1, sizeof(wlmtk_fake_toplevel_state_t));
    if (NULL == fake_toplevel_state_ptr) return NULL;

    fake_toplevel_state_ptr->fake_toplevel.fake_content_ptr =
        wlmtk_fake_content_create();
    if (NULL == fake_toplevel_state_ptr->fake_toplevel.fake_content_ptr) {
        wlmtk_fake_toplevel_destroy(&fake_toplevel_state_ptr->fake_toplevel);
        return NULL;
    }

    if (!_wlmtk_toplevel_init(
            &fake_toplevel_state_ptr->toplevel,
            NULL,
            &fake_toplevel_state_ptr->fake_toplevel.fake_content_ptr->content)) {
        wlmtk_fake_toplevel_destroy(&fake_toplevel_state_ptr->fake_toplevel);
        return NULL;
    }
    fake_toplevel_state_ptr->fake_toplevel.toplevel_ptr =
        &fake_toplevel_state_ptr->toplevel;

    // Extend. We don't save the VMT, since it's for fake only.
    _wlmtk_toplevel_extend(&fake_toplevel_state_ptr->toplevel,
                         &_wlmtk_fake_toplevel_vmt);
    return &fake_toplevel_state_ptr->fake_toplevel;
}

/* ------------------------------------------------------------------------- */
void wlmtk_fake_toplevel_destroy(wlmtk_fake_toplevel_t *fake_toplevel_ptr)
{
    wlmtk_fake_toplevel_state_t *fake_toplevel_state_ptr = BS_CONTAINER_OF(
        fake_toplevel_ptr, wlmtk_fake_toplevel_state_t, fake_toplevel);

    _wlmtk_toplevel_fini(&fake_toplevel_state_ptr->toplevel);
    free(fake_toplevel_state_ptr);
}

/* ------------------------------------------------------------------------- */
/** Fake implementation of @ref wlmtk_toplevel_set_activated. Records call. */
void _wlmtk_fake_toplevel_set_activated(
    wlmtk_toplevel_t *toplevel_ptr,
    bool activated)
{
    wlmtk_fake_toplevel_state_t *fake_toplevel_state_ptr = BS_CONTAINER_OF(
        toplevel_ptr, wlmtk_fake_toplevel_state_t, toplevel);
    fake_toplevel_state_ptr->fake_toplevel.activated = activated;
}

/* ------------------------------------------------------------------------- */
/** Fake implementation of @ref wlmtk_toplevel_request_close. Records call. */
void _wlmtk_fake_toplevel_request_close(wlmtk_toplevel_t *toplevel_ptr)
{
    wlmtk_fake_toplevel_state_t *fake_toplevel_state_ptr = BS_CONTAINER_OF(
        toplevel_ptr, wlmtk_fake_toplevel_state_t, toplevel);
    fake_toplevel_state_ptr->fake_toplevel.request_close_called = true;
}

/* ------------------------------------------------------------------------- */
/** Fake implementation of @ref wlmtk_toplevel_request_minimize. Records call. */
void _wlmtk_fake_toplevel_request_minimize(wlmtk_toplevel_t *toplevel_ptr)
{
    wlmtk_fake_toplevel_state_t *fake_toplevel_state_ptr = BS_CONTAINER_OF(
        toplevel_ptr, wlmtk_fake_toplevel_state_t, toplevel);
    fake_toplevel_state_ptr->fake_toplevel.request_minimize_called = true;
}

/* ------------------------------------------------------------------------- */
/** Fake implementation of @ref wlmtk_toplevel_request_move. Records call */
void _wlmtk_fake_toplevel_request_move(wlmtk_toplevel_t *toplevel_ptr)
{
    wlmtk_fake_toplevel_state_t *fake_toplevel_state_ptr = BS_CONTAINER_OF(
        toplevel_ptr, wlmtk_fake_toplevel_state_t, toplevel);
    fake_toplevel_state_ptr->fake_toplevel.request_move_called = true;
}

/* ------------------------------------------------------------------------- */
/** Fake implementation of @ref wlmtk_toplevel_request_resize. Records call. */
void _wlmtk_fake_toplevel_request_resize(
    wlmtk_toplevel_t *toplevel_ptr,
    uint32_t edges)
{
    wlmtk_fake_toplevel_state_t *fake_toplevel_state_ptr = BS_CONTAINER_OF(
        toplevel_ptr, wlmtk_fake_toplevel_state_t, toplevel);
    fake_toplevel_state_ptr->fake_toplevel.request_resize_called = true;
    fake_toplevel_state_ptr->fake_toplevel.request_resize_edges = edges;
}

/* ------------------------------------------------------------------------- */
/** Fake implementation of @ref wlmtk_toplevel_request_position_and_size. */
void _wlmtk_fake_toplevel_request_position_and_size(
    wlmtk_toplevel_t *toplevel_ptr,
    int x,
    int y,
    int width,
    int height)
{
    wlmtk_fake_toplevel_state_t *fake_toplevel_state_ptr = BS_CONTAINER_OF(
        toplevel_ptr, wlmtk_fake_toplevel_state_t, toplevel);
    fake_toplevel_state_ptr->fake_toplevel.request_position_and_size_called = true;
    fake_toplevel_state_ptr->fake_toplevel.x = x;
    fake_toplevel_state_ptr->fake_toplevel.y = y;
    fake_toplevel_state_ptr->fake_toplevel.width = width;
    fake_toplevel_state_ptr->fake_toplevel.height = height;
}

/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);
static void test_set_title(bs_test_t *test_ptr);
static void test_request_close(bs_test_t *test_ptr);
static void test_set_activated(bs_test_t *test_ptr);
static void test_server_side_decorated(bs_test_t *test_ptr);
static void test_maximize(bs_test_t *test_ptr);
static void test_fake(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_toplevel_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "set_title", test_set_title },
    { 1, "request_close", test_request_close },
    { 1, "set_activated", test_set_activated },
    { 1, "set_server_side_decorated", test_server_side_decorated },
    { 1, "maximize", test_maximize },
    { 1, "fake", test_fake },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests setup and teardown. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_fake_content_t *fake_content_ptr = wlmtk_fake_content_create();
    wlmtk_toplevel_t *toplevel_ptr = wlmtk_toplevel_create(
        NULL, &fake_content_ptr->content);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, toplevel_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, toplevel_ptr,
                      fake_content_ptr->content.toplevel_ptr);

    wlmtk_toplevel_destroy(toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests title. */
void test_set_title(bs_test_t *test_ptr)
{
    wlmtk_fake_content_t *fake_content_ptr = wlmtk_fake_content_create();
    wlmtk_toplevel_t *toplevel_ptr = wlmtk_toplevel_create(
        NULL, &fake_content_ptr->content);

    wlmtk_toplevel_set_title(toplevel_ptr, "Title");
    BS_TEST_VERIFY_STREQ(
        test_ptr,
        "Title",
        wlmtk_toplevel_get_title(toplevel_ptr));

    wlmtk_toplevel_set_title(toplevel_ptr, NULL);
    BS_TEST_VERIFY_STRMATCH(
        test_ptr,
        wlmtk_toplevel_get_title(toplevel_ptr),
        "Unnamed toplevel .*");

    wlmtk_toplevel_destroy(toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests activation. */
void test_request_close(bs_test_t *test_ptr)
{
    wlmtk_fake_content_t *fake_content_ptr = wlmtk_fake_content_create();
    wlmtk_toplevel_t *toplevel_ptr = wlmtk_toplevel_create(
        NULL, &fake_content_ptr->content);

    wlmtk_toplevel_request_close(toplevel_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, fake_content_ptr->request_close_called);

    wlmtk_toplevel_destroy(toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests activation. */
void test_set_activated(bs_test_t *test_ptr)
{
    wlmtk_fake_content_t *fake_content_ptr = wlmtk_fake_content_create();
    wlmtk_toplevel_t *toplevel_ptr = wlmtk_toplevel_create(
        NULL, &fake_content_ptr->content);

    wlmtk_toplevel_set_activated(toplevel_ptr, true);
    BS_TEST_VERIFY_TRUE(test_ptr, fake_content_ptr->activated);

    wlmtk_toplevel_set_activated(toplevel_ptr, false);
    BS_TEST_VERIFY_FALSE(test_ptr, fake_content_ptr->activated);

    wlmtk_toplevel_destroy(toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests enabling and disabling server-side decoration. */
void test_server_side_decorated(bs_test_t *test_ptr)
{
    wlmtk_fake_content_t *fake_content_ptr = wlmtk_fake_content_create();
    wlmtk_toplevel_t *toplevel_ptr = wlmtk_toplevel_create(
        NULL, &fake_content_ptr->content);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, toplevel_ptr->titlebar_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, toplevel_ptr->resizebar_ptr);

    wlmtk_toplevel_set_server_side_decorated(toplevel_ptr, true);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, toplevel_ptr->titlebar_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, toplevel_ptr->resizebar_ptr);

    wlmtk_toplevel_set_server_side_decorated(toplevel_ptr, false);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, toplevel_ptr->titlebar_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, toplevel_ptr->resizebar_ptr);

    wlmtk_toplevel_destroy(toplevel_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests maximizing and un-maximizing a toplevel. */
void test_maximize(bs_test_t *test_ptr)
{
    wlmtk_container_t *fake_parent_ptr = wlmtk_container_create_fake_parent();
    BS_ASSERT(NULL != fake_parent_ptr);
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
        NULL, fake_parent_ptr->wlr_scene_tree_ptr);
    struct wlr_box extents = { .width = 1024, .height = 768 }, box;
    wlmtk_workspace_set_extents(workspace_ptr, &extents);
    BS_ASSERT(NULL != workspace_ptr);

    wlmtk_fake_content_t *fake_content_ptr = wlmtk_fake_content_create();
    wlmtk_toplevel_t *toplevel_ptr = wlmtk_toplevel_create(
        NULL, &fake_content_ptr->content);
    BS_ASSERT(NULL != toplevel_ptr);
    // Toplevel must be mapped to get maximized: Need workspace dimensions.
    wlmtk_workspace_map_toplevel(workspace_ptr, toplevel_ptr);

    // Set up initial organic size, and verify.
    wlmtk_toplevel_request_position_and_size(toplevel_ptr, 20, 10, 200, 100);
    wlmtk_fake_content_commit(fake_content_ptr);
    box = wlmtk_toplevel_get_position_and_size(toplevel_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 20, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, 10, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 200, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 100, box.height);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_toplevel_maximized(toplevel_ptr));

    // Re-position the toplevel.
    wlmtk_toplevel_set_position(toplevel_ptr, 50, 30);
    box = wlmtk_toplevel_get_position_and_size(toplevel_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 50, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, 30, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 200, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 100, box.height);

    // Trigger another serial update. Should not change position nor size.
    wlmtk_toplevel_serial(toplevel_ptr, 1234);
    box = wlmtk_toplevel_get_position_and_size(toplevel_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 50, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, 30, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 200, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 100, box.height);

    // Maximize.
    wlmtk_toplevel_request_maximize(toplevel_ptr, true);
    wlmtk_fake_content_commit(fake_content_ptr);
    box = wlmtk_toplevel_get_position_and_size(toplevel_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 960, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 704, box.height);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_toplevel_maximized(toplevel_ptr));

    // A second commit: should not overwrite the organic dimension.
    wlmtk_fake_content_commit(fake_content_ptr);

    // Unmaximize. Restore earlier organic size and position.
    wlmtk_toplevel_request_maximize(toplevel_ptr, false);
    wlmtk_fake_content_commit(fake_content_ptr);
    box = wlmtk_toplevel_get_position_and_size(toplevel_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 50, box.x);
    BS_TEST_VERIFY_EQ(test_ptr, 30, box.y);
    BS_TEST_VERIFY_EQ(test_ptr, 200, box.width);
    BS_TEST_VERIFY_EQ(test_ptr, 100, box.height);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_toplevel_maximized(toplevel_ptr));

    // TODO(kaeser@gubbe.ch): Define what should happen when a maximized
    // toplevel is moved. Should it lose maximization? Should it not move?
    // Or just move on?
    // Toplevel Maker keeps maximization, but it's ... odd.

    wlmtk_workspace_unmap_toplevel(workspace_ptr, toplevel_ptr);
    wlmtk_toplevel_destroy(toplevel_ptr);
    wlmtk_workspace_destroy(workspace_ptr);
    wlmtk_container_destroy_fake_parent(fake_parent_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests fake toplevel ctor and dtor. */
void test_fake(bs_test_t *test_ptr)
{
    wlmtk_fake_toplevel_t *fake_toplevel_ptr = wlmtk_fake_toplevel_create();
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fake_toplevel_ptr);
    wlmtk_fake_toplevel_destroy(fake_toplevel_ptr);
}

/* == End of toplevel.c ==================================================== */
