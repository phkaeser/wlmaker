/* ========================================================================= */
/**
 * @file window2.c
 *
 * @copyright
 * Copyright 2025 Google LLC
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

#include "window2.h"

#include "bordered.h"
#include "container.h"
#include "titlebar.h"
#include "resizebar.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output_layout.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** Window handle. */
struct _wlmtk_window2_t {
    /** Bordered, wraps around the box. */
    wlmtk_bordered_t          bordered;
    /** Virtual method table of @ref wlmtk_window2_t::bordered. */
    wlmtk_container_vmt_t     orig_super_container_vmt;

    /** Composed of a box: Holds decoration, popup container and content. */
    wlmtk_box_t               box;

    /** Events for this window. */
    wlmtk_window2_events_t    events;

    /** Element in @ref wlmtk_workspace_t::window2s, when mapped. */
    bs_dllist_node_t          dlnode;

    /** The content. */
    wlmtk_element_t           *content_element_ptr;

    /** The workspace, when mapped to a workspace. NULL otherwise. */
    wlmtk_workspace_t         *workspace_ptr;

    /** Window style. */
    wlmtk_window_style_t      style;
    /** Menu style. */
    wlmtk_menu_style_t        menu_style;

    /** The titlebar, when server-side decorated. */
    wlmtk_titlebar_t          *titlebar_ptr;
    /** The resize-bar, when server-side decorated. */
    wlmtk_resizebar_t         *resizebar_ptr;

    /** The window's title. */
    char                      *title_ptr;

    /** Properties of the window. See @ref wlmtk_window_property_t. */
    uint32_t                  properties;

    /** Whether this window has server-side decorations. */
    bool                      server_side_decorated;
    /** Whether this windows is currently in fullscreen mode. */
    bool                      fullscreen;
    /** Whether this window is currently activated (has keyboard focus). */
    bool                      activated;
};

static bool _wlmtk_window2_container_update_layout(
    wlmtk_container_t *container_ptr);

static void _wlmtk_window2_apply_decoration(wlmtk_window2_t *window_ptr);
static void _wlmtk_window2_create_titlebar(wlmtk_window2_t *window_ptr);
static void _wlmtk_window2_create_resizebar(wlmtk_window2_t *window_ptr);
static void _wlmtk_window2_destroy_titlebar(wlmtk_window2_t *window_ptr);
static void _wlmtk_window2_destroy_resizebar(wlmtk_window2_t *window_ptr);

/* == Data ================================================================= */

/** Virtual method table for the window's container superclass. */
static const wlmtk_container_vmt_t _wlmtk_window2_container_vmt = {
    .update_layout = _wlmtk_window2_container_update_layout,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_window2_t *wlmtk_window2_create(
    wlmtk_element_t *content_element_ptr,
    const wlmtk_window_style_t *style_ptr,
    const wlmtk_menu_style_t *menu_style_ptr)
{
    wlmtk_window2_t *window_ptr = logged_calloc(1, sizeof(wlmtk_window2_t));
    if (NULL == window_ptr) return NULL;
    window_ptr->style = *style_ptr;
    window_ptr->menu_style = *menu_style_ptr;
    wl_signal_init(&window_ptr->events.state_changed);
    wl_signal_init(&window_ptr->events.request_close);
    wl_signal_init(&window_ptr->events.set_activated);

    if (!wlmtk_window2_set_title(window_ptr, NULL)) goto error;

    if (!wlmtk_box_init(
            &window_ptr->box,
            WLMTK_BOX_VERTICAL,
            &window_ptr->style.margin)) {
        goto error;
    }
    wlmtk_element_set_visible(wlmtk_box_element(&window_ptr->box), true);

    if (NULL != content_element_ptr) {
        wlmtk_box_add_element_front(&window_ptr->box, content_element_ptr);
        window_ptr->content_element_ptr = content_element_ptr;
    }

    if (!wlmtk_bordered_init(
            &window_ptr->bordered,
            wlmtk_box_element(&window_ptr->box),
            &window_ptr->style.margin)) {
        goto error;
    }
    window_ptr->orig_super_container_vmt = wlmtk_container_extend(
        &window_ptr->bordered.super_container,
        &_wlmtk_window2_container_vmt);

    return window_ptr;

error:
    wlmtk_window2_destroy(window_ptr);
    return NULL;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_destroy(wlmtk_window2_t *window_ptr)
{
    wlmtk_window2_set_server_side_decorated(window_ptr, false);

    wlmtk_bordered_fini(&window_ptr->bordered);

    if (NULL != window_ptr->content_element_ptr) {
        wlmtk_box_remove_element(
            &window_ptr->box,
            window_ptr->content_element_ptr);
        window_ptr->content_element_ptr = NULL;
    }
    wlmtk_box_fini(&window_ptr->box);

    if (NULL != window_ptr->title_ptr) {
        free(window_ptr->title_ptr);
        window_ptr->title_ptr = NULL;
    }
    free(window_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_window2_events_t *wlmtk_window2_events(wlmtk_window2_t *window_ptr)
{
    return &window_ptr->events;
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_window2_element(wlmtk_window2_t *window_ptr)
{
    return &window_ptr->bordered.super_container.super_element;
}

/* ------------------------------------------------------------------------- */
wlmtk_window2_t *wlmtk_window2_from_element(wlmtk_element_t *element_ptr)
{
    return BS_CONTAINER_OF(
        element_ptr, wlmtk_window2_t, bordered.super_container.super_element);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_set_properties(
    wlmtk_window2_t *window_ptr,
    uint32_t properties)
{
    if (window_ptr->properties == properties) return;

    window_ptr->properties = properties;
    _wlmtk_window2_apply_decoration(window_ptr);
}

/* ------------------------------------------------------------------------- */
const wlmtk_util_client_t *wlmtk_window2_get_client_ptr(
    __UNUSED__ wlmtk_window2_t *window_ptr)
{
    // TODO(kaeser@gubbe.ch): Wire this up.
    static wlmtk_util_client_t client = {};
    return &client;
}

/* ------------------------------------------------------------------------- */
bool wlmtk_window2_set_title(
    wlmtk_window2_t *window_ptr,
    const char *title_ptr)
{
    char *new_title_ptr = NULL;
    if (NULL != title_ptr) {
        new_title_ptr = logged_strdup(title_ptr);
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "Unnamed window %p", window_ptr);
        new_title_ptr = logged_strdup(buf);
    }
    if (NULL == new_title_ptr) return false;

    if (NULL != window_ptr->title_ptr) {
        if (0 == strcmp(window_ptr->title_ptr, new_title_ptr)) {
            free(new_title_ptr);
            return true;
        }
        free(window_ptr->title_ptr);
    }
    window_ptr->title_ptr = new_title_ptr;

    if (NULL != window_ptr->titlebar_ptr) {
        wlmtk_titlebar_set_title(window_ptr->titlebar_ptr,
                                 window_ptr->title_ptr);
    }
    return true;
}

/* ------------------------------------------------------------------------- */
const char *wlmtk_window2_get_title(wlmtk_window2_t *window_ptr)
{
    BS_ASSERT(NULL != window_ptr->title_ptr);
    return window_ptr->title_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_set_activated(
    wlmtk_window2_t *window_ptr,
    bool activated)
{
    if (window_ptr->activated == activated) return;

    window_ptr->activated = activated;
    wl_signal_emit(&window_ptr->events.set_activated, NULL);

    if (NULL != window_ptr->titlebar_ptr) {
        wlmtk_titlebar_set_activated(window_ptr->titlebar_ptr, activated);
    }

    if (!activated) {
        wlmtk_window2_menu_set_enabled(window_ptr, false);

        // TODO(kaeser@gubbe.ch): Should test this behaviour.
        if (NULL != window_ptr->workspace_ptr) {
            wlmtk_workspace_activate_window(window_ptr->workspace_ptr, NULL);
        }
    }
}

/* ------------------------------------------------------------------------- */
bool wlmtk_window2_is_activated(wlmtk_window2_t *window_ptr)
{
    return window_ptr->activated;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_request_close(wlmtk_window2_t *window_ptr)
{
    if (!(window_ptr->properties & WLMTK_WINDOW_PROPERTY_CLOSABLE)) return;
    wl_signal_emit(&window_ptr->events.request_close, NULL);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_request_minimize(wlmtk_window2_t *window_ptr)
{
    bs_log(BS_ERROR, "TODO: Request minimize for window %p", window_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_request_resize(wlmtk_window2_t *window_ptr,
                                  uint32_t edges)
{
    bs_log(BS_ERROR, "TODO: Request resize for window %p, edges %"PRIx32,
           window_ptr, edges);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_request_move(wlmtk_window2_t *window_ptr)
{
    bs_log(BS_ERROR, "TODO: Request move for window %p", window_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_request_fullscreen(
    wlmtk_window2_t *window_ptr,
    bool fullscreen)
{
    bs_log(BS_ERROR, "TODO: Request fullscreen for window %p: %d",
           window_ptr, fullscreen);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_window2_is_fullscreen(wlmtk_window2_t *window_ptr)
{
    bs_log(BS_ERROR, "TODO: is_fullscreen for window %p", window_ptr);
    return false;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_request_maximized(
    wlmtk_window2_t *window_ptr,
    bool maximized)
{
    bs_log(BS_ERROR, "TODO: Request maximized for window %p: %d",
           window_ptr, maximized);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_window2_is_maximized(wlmtk_window2_t *window_ptr)
{
    bs_log(BS_ERROR, "TODO: is_maximized for window %p", window_ptr);
    return false;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_request_shaded(wlmtk_window2_t *window_ptr, bool shaded)
{
    bs_log(BS_ERROR, "TODO: Request shaded for window %p: %d",
           window_ptr, shaded);
}

/* ------------------------------------------------------------------------- */
bool wlmtk_window2_is_shaded(wlmtk_window2_t *window_ptr)
{
    bs_log(BS_ERROR, "TODO: is_shaded for window %p", window_ptr);
    return false;
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_menu_set_enabled(wlmtk_window2_t *window_ptr, bool enabled)
{
    bs_log(BS_ERROR, "TODO: Set menu for window %p %d", window_ptr, enabled);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_set_server_side_decorated(
    wlmtk_window2_t *window_ptr,
    bool decorated)
{
    if (window_ptr->server_side_decorated == decorated) return;

    window_ptr->server_side_decorated = decorated;
    _wlmtk_window2_apply_decoration(window_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_window2_set_workspace(
    wlmtk_window2_t *window_ptr,
    wlmtk_workspace_t *workspace_ptr)
{
    if (window_ptr->workspace_ptr == workspace_ptr) return;
    window_ptr->workspace_ptr = workspace_ptr;

    wl_signal_emit(&window_ptr->events.state_changed, window_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_workspace_t *wlmtk_window2_get_workspace(wlmtk_window2_t *window_ptr)
{
    return window_ptr->workspace_ptr;
}

/* ------------------------------------------------------------------------- */
bs_dllist_node_t *wlmtk_dlnode_from_window2(wlmtk_window2_t *window_ptr)
{
    return &window_ptr->dlnode;
}

/* ------------------------------------------------------------------------- */
wlmtk_window2_t *wlmtk_window2_from_dlnode(bs_dllist_node_t *dlnode_ptr)
{
    return BS_CONTAINER_OF(dlnode_ptr, wlmtk_window2_t, dlnode);
}

/* == Local (static) methods =============================================== */

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
bool _wlmtk_window2_container_update_layout(wlmtk_container_t *container_ptr)
{
    wlmtk_window2_t *window_ptr = BS_CONTAINER_OF(
        container_ptr, wlmtk_window2_t, bordered.super_container);

    // Must update the layout of wlmtk_bordered_t.
    window_ptr->orig_super_container_vmt.update_layout(container_ptr);

    if (NULL != window_ptr->content_element_ptr) {
        struct wlr_box box = wlmtk_element_get_dimensions_box(
            window_ptr->content_element_ptr);
        if (NULL != window_ptr->titlebar_ptr) {
            wlmtk_titlebar_set_width(window_ptr->titlebar_ptr, box.width);
        }
        if (NULL != window_ptr->resizebar_ptr) {
            wlmtk_resizebar_set_width(window_ptr->resizebar_ptr, box.width);
        }
    }
    return true;
}

/* ------------------------------------------------------------------------- */
/** Applies window decoration depending on current state. */
void _wlmtk_window2_apply_decoration(wlmtk_window2_t *window_ptr)
{
    wlmtk_margin_style_t bstyle = window_ptr->style.border;

    if (window_ptr->server_side_decorated && !window_ptr->fullscreen) {
        _wlmtk_window2_create_titlebar(window_ptr);
    } else {
        bstyle.width = 0;
        _wlmtk_window2_destroy_titlebar(window_ptr);
    }

    if (NULL != window_ptr->titlebar_ptr) {
        uint32_t properties = 0;
        if (window_ptr->properties & WLMTK_WINDOW_PROPERTY_ICONIFIABLE) {
            properties |= WLMTK_TITLEBAR_PROPERTY_ICONIFY;
        }
        if (window_ptr->properties & WLMTK_WINDOW_PROPERTY_CLOSABLE) {
            properties |= WLMTK_TITLEBAR_PROPERTY_CLOSE;
        }
        wlmtk_titlebar_set_properties(window_ptr->titlebar_ptr, properties);
        wlmtk_titlebar_set_activated(
            window_ptr->titlebar_ptr, window_ptr->activated);
    }

    if (window_ptr->server_side_decorated &&
        !window_ptr->fullscreen &&
        (window_ptr->properties & WLMTK_WINDOW_PROPERTY_RESIZABLE)) {
        _wlmtk_window2_create_resizebar(window_ptr);
    } else {
        _wlmtk_window2_destroy_resizebar(window_ptr);
    }

    wlmtk_bordered_set_style(&window_ptr->bordered, &bstyle);
}

/* ------------------------------------------------------------------------- */
void _wlmtk_window2_create_titlebar(wlmtk_window2_t *window_ptr)
{
    BS_ASSERT(window_ptr->server_side_decorated && !window_ptr->fullscreen);

    // Guard clause: Don't add decoration.
    if (NULL != window_ptr->titlebar_ptr) return;

    // Create decoration.
    window_ptr->titlebar_ptr = wlmtk_titlebar2_create(
        window_ptr, &window_ptr->style.titlebar);
    BS_ASSERT(NULL != window_ptr->titlebar_ptr);

    wlmtk_element_set_visible(
        wlmtk_titlebar_element(window_ptr->titlebar_ptr), true);
    wlmtk_titlebar_set_activated(
        window_ptr->titlebar_ptr,
        window_ptr->activated);

    // Hm, if the content has a popup that extends over the titlebar area,
    // it'll be partially obscured. That will look odd... Well, let's
    // address that problem once there's a situation.
    wlmtk_box_add_element_front(
        &window_ptr->box,
        wlmtk_titlebar_element(window_ptr->titlebar_ptr));
}

/* ------------------------------------------------------------------------- */
void _wlmtk_window2_destroy_titlebar(wlmtk_window2_t *window_ptr)
{
    BS_ASSERT(!window_ptr->server_side_decorated || window_ptr->fullscreen);

    if (NULL == window_ptr->titlebar_ptr) return;

    wlmtk_box_remove_element(
        &window_ptr->box,
        wlmtk_titlebar_element(window_ptr->titlebar_ptr));
    wlmtk_titlebar_destroy(window_ptr->titlebar_ptr);
    window_ptr->titlebar_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
void _wlmtk_window2_create_resizebar(wlmtk_window2_t *window_ptr)
{
    BS_ASSERT(window_ptr->server_side_decorated && !window_ptr->fullscreen);

    // Guard clause: Don't add decoration.
    if (NULL != window_ptr->resizebar_ptr) return;

    window_ptr->resizebar_ptr = wlmtk_resizebar2_create(
        window_ptr, &window_ptr->style.resizebar);
    BS_ASSERT(NULL != window_ptr->resizebar_ptr);
    wlmtk_element_set_visible(
        wlmtk_resizebar_element(window_ptr->resizebar_ptr), true);
    wlmtk_box_add_element_back(
        &window_ptr->box,
        wlmtk_resizebar_element(window_ptr->resizebar_ptr));
}

/* ------------------------------------------------------------------------- */
void _wlmtk_window2_destroy_resizebar(wlmtk_window2_t *window_ptr)
{
    BS_ASSERT(!window_ptr->server_side_decorated ||
              window_ptr->fullscreen ||
              !(window_ptr->properties & WLMTK_WINDOW_PROPERTY_RESIZABLE));

    if (NULL == window_ptr->resizebar_ptr) return;

    wlmtk_box_remove_element(
        &window_ptr->box,
        wlmtk_resizebar_element(window_ptr->resizebar_ptr));
    wlmtk_resizebar_destroy(window_ptr->resizebar_ptr);
    window_ptr->resizebar_ptr = NULL;
}

/* == Unit Tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);
static void test_decoration(bs_test_t *test_ptr);
static void test_events(bs_test_t *test_ptr);
static void test_set_activated(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_window2_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "decoration", test_decoration },
    { 1, "events", test_events },
    { 1, "set_activated", test_set_activated },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises ctor and dtor and some accessors. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fe = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe);

    wlmtk_window_style_t s = {};
    wlmtk_menu_style_t ms = {};
    wlmtk_window2_t *w = wlmtk_window2_create(&fe->element, &s, &ms);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w);

    // Title. Must be set when unnamed, and override otherwise.
    BS_TEST_VERIFY_STRMATCH(
        test_ptr,
        wlmtk_window2_get_title(w),
        "Unnamed window .*");
    wlmtk_window2_set_title(w, "Title");
    BS_TEST_VERIFY_STREQ(test_ptr, "Title", wlmtk_window2_get_title(w));

    // Transform to element and dlnode and back.
    wlmtk_element_t *e = wlmtk_window2_element(w);
    BS_TEST_VERIFY_EQ(test_ptr, w, wlmtk_window2_from_element(e));
    bs_dllist_node_t *dlnode_ptr = wlmtk_dlnode_from_window2(w);
    BS_TEST_VERIFY_EQ(test_ptr, w, wlmtk_window2_from_dlnode(dlnode_ptr));

    // set_workspace and get_workspace.
    struct wl_display *display_ptr = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, display_ptr);
    struct wlr_output_layout *wlr_output_layout_ptr =
        wlr_output_layout_create(display_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, wlr_output_layout_ptr);
    wlmtk_tile_style_t ts = {};
    wlmtk_workspace_t *workspace_ptr = wlmtk_workspace_create(
        wlr_output_layout_ptr, "t", &ts);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, workspace_ptr);

    wlmtk_util_test_listener_t l;
    wlmtk_util_connect_test_listener(
        &wlmtk_window2_events(w)->state_changed, &l);

    wlmtk_window2_set_workspace(w, workspace_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        workspace_ptr,
        wlmtk_window2_get_workspace(w));

    wlmtk_util_clear_test_listener(&l);
    wlmtk_window2_set_workspace(w, workspace_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, l.calls);

    wlmtk_workspace_destroy(workspace_ptr);
    wl_display_destroy(display_ptr);

    wlmtk_window2_destroy(w);
    wlmtk_element_destroy(&fe->element);
}

/* ------------------------------------------------------------------------- */
/** Tests server-side decoration. */
void test_decoration(bs_test_t *test_ptr)
{
    wlmtk_fake_element_t *fe = wlmtk_fake_element_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, fe);
    fe->dimensions = (struct wlr_box){ .width = 42, .height = 20 };

    wlmtk_window_style_t s = {};
    wlmtk_menu_style_t ms = {};
    wlmtk_window2_t *w = wlmtk_window2_create(&fe->element, &s, &ms);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w);

    // Initially: no decoration.
    BS_TEST_VERIFY_EQ(test_ptr, NULL, w->titlebar_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, w->resizebar_ptr);

    // Enable: Default property is not resizable, so we only get titlebar.
    wlmtk_window2_set_server_side_decorated(w, true);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, w->titlebar_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, w->resizebar_ptr);

    // Set property. Now we must have resize bar.
    wlmtk_window2_set_properties(w, WLMTK_WINDOW_PROPERTY_RESIZABLE);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, w->resizebar_ptr);

    // Disable. Decoration vanishes.
    wlmtk_window2_set_server_side_decorated(w, false);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, w->titlebar_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, w->resizebar_ptr);

    // Re-enable. Properties are sticky, so we want all.
    wlmtk_window2_set_server_side_decorated(w, true);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, w->titlebar_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, w->resizebar_ptr);

    // Verify width: Must match the fake element's dimensions.
    struct wlr_box b = wlmtk_element_get_dimensions_box(
        wlmtk_titlebar_element(w->titlebar_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, 42, b.width);
    b = wlmtk_element_get_dimensions_box(
        wlmtk_resizebar_element(w->resizebar_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, 42, b.width);

    // An update in the element would call the paren's update_layout.
    fe->dimensions = (struct wlr_box){ .width = 52, .height = 20 };
    wlmtk_container_update_layout_and_pointer_focus(
        fe->element.parent_container_ptr);
    b = wlmtk_element_get_dimensions_box(
        wlmtk_titlebar_element(w->titlebar_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, 52, b.width);
    b = wlmtk_element_get_dimensions_box(
        wlmtk_resizebar_element(w->resizebar_ptr));
    BS_TEST_VERIFY_EQ(test_ptr, 52, b.width);

    wlmtk_window2_destroy(w);
    wlmtk_element_destroy(&fe->element);
}

/* ------------------------------------------------------------------------- */
void test_events(bs_test_t *test_ptr)
{
    wlmtk_window_style_t s = {};
    wlmtk_menu_style_t ms = {};
    wlmtk_window2_t *w = wlmtk_window2_create(NULL, &s, &ms);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w);

    wlmtk_util_test_listener_t l;

    wlmtk_util_connect_test_listener(
        &wlmtk_window2_events(w)->request_close, &l);
    wlmtk_window2_request_close(w);
    BS_TEST_VERIFY_EQ(test_ptr, 0, l.calls);

    wlmtk_window2_set_properties(w, WLMTK_WINDOW_PROPERTY_CLOSABLE);
    wlmtk_window2_request_close(w);
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);

    wlmtk_window2_destroy(w);
}

/* ------------------------------------------------------------------------- */
/** Tests that activation updates decoration and callback. */
void test_set_activated(bs_test_t *test_ptr)
{
    wlmtk_window_style_t s = {};
    wlmtk_menu_style_t ms = {};
    wlmtk_window2_t *w = wlmtk_window2_create(NULL, &s, &ms);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, w);

    wlmtk_util_test_listener_t l;
    wlmtk_util_connect_test_listener(
        &wlmtk_window2_events(w)->set_activated, &l);

    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window2_is_activated(w));

    wlmtk_window2_set_activated(w, true);
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_window2_is_activated(w));
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);

    wlmtk_window2_set_server_side_decorated(w, true);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_titlebar_is_activated(w->titlebar_ptr));

    wlmtk_util_clear_test_listener(&l);
    wlmtk_window2_set_activated(w, false);
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_window2_is_activated(w));
    BS_TEST_VERIFY_EQ(test_ptr, 1, l.calls);
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_titlebar_is_activated(w->titlebar_ptr));

    wlmtk_window2_destroy(w);
}

/* == End of window2.c ===================================================== */
