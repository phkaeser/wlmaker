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

#include <inttypes.h>
#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-server-core.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

#include "../etc/root_menu.h"
#include "action.h"
#include "action_item.h"
#include "config.h"
#include "subprocess_monitor.h"

/* == Declarations ========================================================= */

/** State of the root menu. */
struct _wlmaker_root_menu_t {
    /** Window. */
    wlmtk_window_t            *window_ptr;

    /** The root menu's window content base instance. */
    wlmtk_content_t           content;
    /** The root menu base instance. */
    wlmtk_menu_t              *menu_ptr;
    /** Listener for @ref wlmtk_menu_events_t::open_changed. */
    struct wl_listener        menu_open_changed_listener;
    /** Listener for @ref wlmtk_menu_events_t::request_close. */
    struct wl_listener        menu_request_close_listener;

    /** Back-link to the server. */
    wlmaker_server_t          *server_ptr;
};

/** State of a menu generator, while waiting for subprocess to complete. */
typedef struct {
    /** Subprocess handle. */
    wlmaker_subprocess_handle_t *subprocess_handle_ptr;
    /** Back-link to the server. */
    wlmaker_server_t          *server_ptr;
    /** The menu this generator is going to populate. */
    wlmtk_menu_t              *menu_ptr;
    /** Menu style. */
    wlmtk_menu_style_t        menu_style;
    /** Dynamic buffer to hold stdout while the process is running. */
    bs_dynbuf_t               *stdout_dynbuf_ptr;
    /** Listener for @ref wlmtk_menu_events_t::destroy. */
    struct wl_listener        menu_destroy_listener;
} wlmaker_root_menu_generator_t;

static void _wlmaker_root_menu_content_request_close(
    wlmtk_content_t *content_ptr);
static void _wlmaker_root_menu_content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated);

static void _wlmaker_root_menu_handle_menu_open_changed(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_root_menu_handle_request_close(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static bool _wlmaker_root_menu_init_menu_from_array(
    wlmtk_menu_t *menu_ptr,
    bspl_array_t *array_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmaker_server_t *server_ptr);
static bool _wlmaker_root_menu_populate_menu_items_from_array(
    wlmtk_menu_t *menu_ptr,
    bspl_array_t *array_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmaker_server_t *server_ptr);
static bool _wlmaker_root_menu_populate_menu_items_from_file(
    wlmtk_menu_t *menu_ptr,
    const char *filename_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmaker_server_t *server_ptr);
static bool _wlmaker_root_menu_populate_menu_items_from_generator(
    wlmtk_menu_t *menu_ptr,
    const char *command_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmaker_server_t *server_ptr);

static void _wlmaker_root_menu_generator_destroy(
    wlmaker_root_menu_generator_t *gen_menu_ptr);
static void _wlmaker_root_menu_generator_handle_menu_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmaker_root_menu_generator_handle_terminated(
    void *userdata_ptr,
    wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    int state,
    int code);

static wlmtk_menu_item_t *_wlmaker_root_menu_create_item_from_array(
    bspl_array_t *item_array_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmaker_server_t *server_ptr);
static wlmtk_menu_item_t *_wlmaker_root_menu_create_disabled_item(
    const wlmtk_menu_item_style_t *style_ptr,
    const char *fmt_ptr,
    ...) __ARG_PRINTF__(2, 3);

/* == Data ================================================================= */

/** Virtual method of the root menu's window content. */
static const wlmtk_content_vmt_t _wlmaker_root_menu_content_vmt = {
    .request_close = _wlmaker_root_menu_content_request_close,
    .set_activated = _wlmaker_root_menu_content_set_activated,
};

/** Lookup paths for the root menu config file. */
static const char *_wlmaker_root_menu_fname_ptrs[] = {
    "~/.wlmaker-root-menu.plist",
    "/usr/share/wlmaker/root-menu.plist",
    NULL  // Sentinel.
};

/** Indicates to load the file specified in following argument. */
static const char *_wlmaker_root_menu_statement_include = "IncludePlistMenu";

/**
 * Indicates to generate the menu using a shell command specified in the
 * following argument.
 */
static const char *_wlmaker_root_menu_statement_generate = "GeneratePlistMenu";

/**
 * Unit test injector: struct wl_display that will be terminated when
 * subprocess terminates. Must be NULL when not for in unit tests.
 */
struct wl_display             *_wlmaker_root_menu_test_wl_display_ptr = NULL;

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_root_menu_t *wlmaker_root_menu_create(
    wlmaker_server_t *server_ptr,
    const char *arg_root_menu_file_ptr,
    const wlmtk_window_style_t *window_style_ptr,
    const wlmtk_menu_style_t *menu_style_ptr)
{
    bspl_array_t *root_menu_array_ptr = bspl_array_from_object(
        wlmaker_plist_load(
            "root menu",
            arg_root_menu_file_ptr,
            _wlmaker_root_menu_fname_ptrs,
            embedded_binary_root_menu_data,
            embedded_binary_root_menu_size));
    if (NULL == root_menu_array_ptr) return NULL;

    if (bspl_array_size(root_menu_array_ptr) <= 1) {
        bs_log(BS_ERROR, "Needs > 1 array element for menu definition.");
        return NULL;
    }
    if (BSPL_STRING != bspl_object_type(
            bspl_array_at(root_menu_array_ptr, 0))) {
        bs_log(BS_ERROR, "Array element [0] must be a string.");
        return NULL;
    }

    wlmaker_root_menu_t *root_menu_ptr = logged_calloc(
        1, sizeof(wlmaker_root_menu_t));
    if (NULL == root_menu_ptr) return NULL;
    root_menu_ptr->server_ptr = server_ptr;
    root_menu_ptr->server_ptr->root_menu_ptr = root_menu_ptr;

    root_menu_ptr->menu_ptr = wlmtk_menu_create(menu_style_ptr);
    if (NULL == root_menu_ptr->menu_ptr) {
        wlmaker_root_menu_destroy(root_menu_ptr);
        bspl_array_unref(root_menu_array_ptr);
        return NULL;
    }
    if (!_wlmaker_root_menu_init_menu_from_array(
            root_menu_ptr->menu_ptr,
            root_menu_array_ptr,
            menu_style_ptr,
            server_ptr)) {
        bspl_array_unref(root_menu_array_ptr);
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &wlmtk_menu_events(root_menu_ptr->menu_ptr)->open_changed,
        &root_menu_ptr->menu_open_changed_listener,
        _wlmaker_root_menu_handle_menu_open_changed);
    wlmtk_util_connect_listener_signal(
        &wlmtk_menu_events(root_menu_ptr->menu_ptr)->request_close,
        &root_menu_ptr->menu_request_close_listener,
        _wlmaker_root_menu_handle_request_close);

    // FIXME - really terrible hack.
    wlmtk_pane_t *pane_ptr = wlmtk_menu_pane(root_menu_ptr->menu_ptr);
    struct wlr_box box = wlmtk_element_get_dimensions_box(
        wlmtk_menu_element(root_menu_ptr->menu_ptr));
    wlmtk_container_remove_element(
        &pane_ptr->super_container,
        pane_ptr->element_ptr);
    if (!wlmtk_content_init(
            &root_menu_ptr->content,
            pane_ptr->element_ptr)) {
        wlmaker_root_menu_destroy(root_menu_ptr);
        bspl_array_unref(root_menu_array_ptr);
        return NULL;
    }
    wlmtk_container_remove_element(
        &pane_ptr->super_container,
        &pane_ptr->popup_container.super_element);
    wlmtk_container_add_element(
        &root_menu_ptr->content.popup_container,
        &pane_ptr->popup_container.super_element);

    wlmtk_content_extend(
        &root_menu_ptr->content,
        &_wlmaker_root_menu_content_vmt);
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
        menu_style_ptr,
        server_ptr->wlr_seat_ptr);
    if (NULL == root_menu_ptr->window_ptr) {
        wlmaker_root_menu_destroy(root_menu_ptr);
        bspl_array_unref(root_menu_array_ptr);
        return NULL;
    }
    wlmtk_window_set_title(
        root_menu_ptr->window_ptr,
        bspl_array_string_value_at(root_menu_array_ptr, 0));
    wlmtk_window_set_server_side_decorated(root_menu_ptr->window_ptr, true);

    bspl_array_unref(root_menu_array_ptr);
    return root_menu_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_root_menu_destroy(wlmaker_root_menu_t *root_menu_ptr)
{
    if (NULL != root_menu_ptr->server_ptr) {
        BS_ASSERT(root_menu_ptr->server_ptr->root_menu_ptr == root_menu_ptr);
        root_menu_ptr->server_ptr->root_menu_ptr = NULL;
        root_menu_ptr->server_ptr = NULL;;
    }

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

    if (NULL != root_menu_ptr->menu_ptr) {
        wlmtk_content_set_element(&root_menu_ptr->content, NULL);
        wlmtk_pane_t *pane_ptr = wlmtk_menu_pane(root_menu_ptr->menu_ptr);
        wlmtk_container_add_element(
            &pane_ptr->super_container,
            pane_ptr->element_ptr);

        wlmtk_container_remove_element(
            &root_menu_ptr->content.popup_container,
            &pane_ptr->popup_container.super_element);
        wlmtk_container_add_element(
            &pane_ptr->super_container,
            &pane_ptr->popup_container.super_element);
    }

    wlmtk_content_fini(&root_menu_ptr->content);
    if (NULL != root_menu_ptr->menu_ptr) {
        wlmtk_util_disconnect_listener(
            &root_menu_ptr->menu_request_close_listener);
        wlmtk_util_disconnect_listener(
            &root_menu_ptr->menu_open_changed_listener);
        wlmtk_menu_destroy(root_menu_ptr->menu_ptr);
        root_menu_ptr->menu_ptr = NULL;
    }
    free(root_menu_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_window_t *wlmaker_root_menu_window(wlmaker_root_menu_t *root_menu_ptr)
{
    return root_menu_ptr->window_ptr;
}

/* ------------------------------------------------------------------------- */
wlmtk_menu_t *wlmaker_root_menu_menu(wlmaker_root_menu_t *root_menu_ptr)
{
    return root_menu_ptr->menu_ptr;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_content_vmt_t::request_close. Closes root menu. */
void _wlmaker_root_menu_content_request_close(
    wlmtk_content_t *content_ptr)
{
    wlmaker_root_menu_t *root_menu_ptr = BS_CONTAINER_OF(
        content_ptr, wlmaker_root_menu_t, content);

    wlmtk_menu_set_open(root_menu_ptr->menu_ptr, false);
}

/* ------------------------------------------------------------------------- */
/** Imlements @ref wlmtk_content_vmt_t::set_activated. Gets keyboard focus. */
void _wlmaker_root_menu_content_set_activated(
    wlmtk_content_t *content_ptr,
    bool activated)
{
    wlmaker_root_menu_t *root_menu_ptr = BS_CONTAINER_OF(
        content_ptr, wlmaker_root_menu_t, content);

    wlmtk_element_t *e = wlmtk_menu_pane(root_menu_ptr->menu_ptr)->element_ptr;
    if (NULL != e->parent_container_ptr) {
        wlmtk_container_set_keyboard_focus_element(
            e->parent_container_ptr, e, activated);
    }
}

/* ------------------------------------------------------------------------- */
/** Handles @ref wlmtk_menu_events_t::open_changed. Unmaps window on close. */
void _wlmaker_root_menu_handle_menu_open_changed(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_root_menu_t *root_menu_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_root_menu_t, menu_open_changed_listener);
    if (!wlmtk_menu_is_open(root_menu_ptr->menu_ptr) &&
        NULL != wlmtk_window_get_workspace(root_menu_ptr->window_ptr)) {
        wlmtk_workspace_unmap_window(
            wlmtk_window_get_workspace(root_menu_ptr->window_ptr),
            root_menu_ptr->window_ptr);
    } else {

        uint32_t properties = 0;
        if (WLMTK_MENU_MODE_RIGHTCLICK ==
            wlmtk_menu_get_mode(root_menu_ptr->menu_ptr)) {
            properties |= WLMTK_WINDOW_PROPERTY_RIGHTCLICK;

            // TODO(kaeser@gubbe.ch): Also undo, with that really terrible
            // hack of hacking the pane into the content.
            wlmtk_element_t *content_element_ptr =
                wlmtk_content_element(&root_menu_ptr->content);
            wlmtk_container_pointer_grab(
                content_element_ptr->parent_container_ptr,
                content_element_ptr);
        } else {
            properties |= WLMTK_WINDOW_PROPERTY_CLOSABLE;
        }
        wlmtk_window_set_properties(root_menu_ptr->window_ptr, properties);

    }
}

/* ------------------------------------------------------------------------- */
/** Listens to @ref wlmtk_menu_events_t::request_close. Closes the menu. */
void _wlmaker_root_menu_handle_request_close(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_root_menu_t *root_menu_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_root_menu_t, menu_request_close_listener);

    wlmtk_menu_set_open(root_menu_ptr->menu_ptr, false);
}

/* ------------------------------------------------------------------------- */
/**
 * Initializes the menu from the menu configuration array.
 *
 * The menu configuration is a Plist array. The first item is the menu's title,
 * while the second item defines the nature of the menu configuration:
 *
 * It can define a set of menu items, in form of Plist arrays:
 * @verbinclude tests/data/menu.plist
 *
 * Or, it is a definition to include a Plist menu:
 * @verbinclude tests/data/menu-include.plist
 *
 * Or, it is a definition to generate a Plist menu:
 * @verbinclude tests/data/menu-generate.plist
 *
 * @param menu_ptr
 * @param array_ptr
 * @param menu_style_ptr
 * @param server_ptr
 *
 * @return true on success.
 */
bool _wlmaker_root_menu_init_menu_from_array(
    wlmtk_menu_t *menu_ptr,
    bspl_array_t *array_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmaker_server_t *server_ptr)
{
    // (1) object must be array, and have >= 2 elements: title and content.
    if (2 > bspl_array_size(array_ptr)) {
        bs_log(BS_ERROR, "Plist menu definition array size must be >= 2.");
        return false;
    }

    bspl_object_t *content_object_ptr = bspl_array_at(array_ptr, 1);
    switch (bspl_object_type(content_object_ptr)) {
    case BSPL_ARRAY:

        // Indicates the first element is an item with a submenu, and there
        // are optionally further elements. Populate the parent menu from
        // that.
        return _wlmaker_root_menu_populate_menu_items_from_array(
            menu_ptr,
            array_ptr,
            menu_style_ptr,
            server_ptr);

    case BSPL_STRING:

        if (3 > bspl_array_size(array_ptr)) {
            bs_log(BS_ERROR, "Must have 3 objects on \"%s\"",
                   bspl_array_string_value_at(array_ptr, 1));
            return false;
        }

        if (0 == strcmp(
                bspl_string_value_from_object(content_object_ptr),
                _wlmaker_root_menu_statement_include)) {

            return _wlmaker_root_menu_populate_menu_items_from_file(
                menu_ptr,
                bspl_array_string_value_at(array_ptr, 2),
                menu_style_ptr,
                server_ptr);

        } else if (0 == strcmp(
                       bspl_string_value_from_object(content_object_ptr),                               _wlmaker_root_menu_statement_generate)) {

            return _wlmaker_root_menu_populate_menu_items_from_generator(
                menu_ptr,
                bspl_array_string_value_at(array_ptr, 2),
                menu_style_ptr,
                server_ptr);

        }
        bs_log(BS_ERROR, "Unknown menu definition \"%s\"",
               bspl_string_value_from_object(content_object_ptr));
        return false;

    default:
        break;
    }

    bs_log(BS_ERROR, "Unhandled object type to populate menu.");
    return false;
}

/* ------------------------------------------------------------------------- */
/**
 * Populates the menu's items from the Plist array. This handles the case of
 * a menu configuration that specifies the menu items as a Plist array.
 *
 * The first item of `array_ptr` is the menu's title, and each further item
 * is expected to be another Plist array, defining a menu item.
 *
 * @param menu_ptr
 * @param array_ptr
 * @param menu_style_ptr
 * @param server_ptr
 *
 * @return true on success
 */
bool _wlmaker_root_menu_populate_menu_items_from_array(
    wlmtk_menu_t *menu_ptr,
    bspl_array_t *array_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmaker_server_t *server_ptr)
{
    if (bspl_array_size(array_ptr) <= 1) {
        bs_log(BS_ERROR, "Needs > 1 array element for menu definition.");
        return false;
    }
    const char *name_ptr = bspl_array_string_value_at(array_ptr, 0);
    if (NULL == name_ptr) {
        bs_log(BS_ERROR, "Array element [0] must be a string.");
        return false;
    }

    for (size_t i = 1; i < bspl_array_size(array_ptr); ++i) {
        bspl_array_t *item_array_ptr = bspl_array_from_object(
            bspl_array_at(array_ptr, i));
        if (NULL == item_array_ptr) {
            bs_log(BS_ERROR, "Menu %s: Element [%zu] must be an array",
                   name_ptr, i);
            return false;
        }

        if (NULL == bspl_array_string_value_at(item_array_ptr, 0)) {
            bs_log(BS_ERROR,
                   "Menu %s: First element of item [%zu] must be a string",
                   name_ptr, i);
            return false;
        }

        wlmtk_menu_item_t *menu_item_ptr =
            _wlmaker_root_menu_create_item_from_array(
                item_array_ptr,
                menu_style_ptr,
                server_ptr);
        if (NULL == menu_item_ptr) return false;
        wlmtk_menu_add_item(menu_ptr, menu_item_ptr);
    }

    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Loads a Plist array from file and populates the menu's items from it.
 *
 * @param menu_ptr
 * @param filename_ptr
 * @param menu_style_ptr
 * @param server_ptr
 *
 * @return true on success
 */
bool _wlmaker_root_menu_populate_menu_items_from_file(
    wlmtk_menu_t *menu_ptr,
    const char *filename_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmaker_server_t *server_ptr)
{
    char *path_ptr = bs_file_resolve_path(filename_ptr, NULL);
    if (NULL == path_ptr) {
        bs_log(BS_ERROR, "Failed bs_file_resolve_path(\"%s\", NULL)",
               filename_ptr);
        return false;
    }

    bspl_object_t *object_ptr = bspl_create_object_from_plist_file(path_ptr);
    free(path_ptr);
    if (NULL == object_ptr || BSPL_ARRAY != bspl_object_type(object_ptr)) {
        bs_log(BS_ERROR, "Failed to load Plist ARRAY from \"%s\"",
               filename_ptr);
        if (NULL != object_ptr) bspl_object_unref(object_ptr);
        return false;
    }

    bool rv = _wlmaker_root_menu_populate_menu_items_from_array(
            menu_ptr,
            bspl_array_from_object(object_ptr),
            menu_style_ptr,
            server_ptr);
    if (!rv) {
        bs_log(BS_ERROR, "Failed to generate menu from Plist file \"%s\"",
               filename_ptr);
    }
    bspl_object_unref(object_ptr);
    return rv;
}

/* ------------------------------------------------------------------------- */
/**
 * Launches a subprocess, to populate the menu's items.
 *
 * Uses a @ref wlmaker_root_menu_generator_t to track state of the subprocess
 * and to tie it with the menu's lifecycle.
 *
 * @param menu_ptr
 * @param command_ptr
 * @param menu_style_ptr
 * @param server_ptr
 *
 * @return true on success
 */
bool _wlmaker_root_menu_populate_menu_items_from_generator(
    wlmtk_menu_t *menu_ptr,
    const char *command_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmaker_server_t *server_ptr)
{
    bs_subprocess_t *subprocess_ptr = NULL;
    wlmaker_root_menu_generator_t *generator_ptr = logged_calloc(
        1, sizeof(wlmaker_root_menu_generator_t));
    if (NULL == generator_ptr) return false;
    generator_ptr->server_ptr = server_ptr;
    generator_ptr->menu_ptr = menu_ptr;
    generator_ptr->menu_style = *menu_style_ptr;

    generator_ptr->stdout_dynbuf_ptr = bs_dynbuf_create(1024, INT32_MAX);
    if (NULL == generator_ptr->stdout_dynbuf_ptr) goto error;

    wlmtk_util_connect_listener_signal(
        &wlmtk_menu_events(menu_ptr)->destroy,
        &generator_ptr->menu_destroy_listener,
        _wlmaker_root_menu_generator_handle_menu_destroy);

    const char *args[] = { "/bin/sh", "-c", command_ptr, NULL };
    subprocess_ptr =  bs_subprocess_create(args[0], args, NULL);
    if (NULL == subprocess_ptr) goto error;

    if (!bs_subprocess_start(subprocess_ptr)) goto error;
    bs_log(BS_INFO,
           "Created subprocess %p [%"PRIdMAX"] for \"/bin/sh\" \"-c\" \"%s\"",
           subprocess_ptr,
           (intmax_t)bs_subprocess_pid(subprocess_ptr),
           command_ptr);

    generator_ptr->subprocess_handle_ptr = wlmaker_subprocess_monitor_entrust(
        server_ptr->monitor_ptr,
        subprocess_ptr,
        _wlmaker_root_menu_generator_handle_terminated,
        generator_ptr,
        NULL,
        NULL,
        NULL,
        NULL,
        generator_ptr->stdout_dynbuf_ptr);
    if (NULL == generator_ptr->subprocess_handle_ptr) goto error;

    return true;

error:
    if (NULL != subprocess_ptr) {
        bs_subprocess_destroy(subprocess_ptr);
    }
    _wlmaker_root_menu_generator_destroy(generator_ptr);
    return false;
}

/* ------------------------------------------------------------------------- */
/** Dtor for the menu generator. */
void _wlmaker_root_menu_generator_destroy(
    wlmaker_root_menu_generator_t *generator_ptr)
{
    if (NULL != generator_ptr->subprocess_handle_ptr) {
        wlmaker_subprocess_monitor_cede(
            generator_ptr->server_ptr->monitor_ptr,
            generator_ptr->subprocess_handle_ptr);
        generator_ptr->subprocess_handle_ptr = NULL;
    }

    if (NULL != generator_ptr->stdout_dynbuf_ptr) {
        bs_dynbuf_destroy(generator_ptr->stdout_dynbuf_ptr);
        generator_ptr->stdout_dynbuf_ptr = NULL;
    }

    wlmtk_util_disconnect_listener(&generator_ptr->menu_destroy_listener);
    free(generator_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handles @ref wlmtk_menu_events_t::destroy. Calls dtor. */
void _wlmaker_root_menu_generator_handle_menu_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_root_menu_generator_t *generator_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_root_menu_generator_t, menu_destroy_listener);

    generator_ptr->menu_ptr = NULL;
    _wlmaker_root_menu_generator_destroy(generator_ptr);
}

/* ------------------------------------------------------------------------- */
/** Handler for when the subprocess is terminated. */
void _wlmaker_root_menu_generator_handle_terminated(
    void *userdata_ptr,
    __UNUSED__ wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    int state,
    int code)
{
    wlmaker_root_menu_generator_t *generator_ptr = userdata_ptr;
    wlmtk_menu_item_t *menu_item_ptr = NULL;

    if (0 != state) {
        if (INT_MIN != state) {
            menu_item_ptr = _wlmaker_root_menu_create_disabled_item(
                &generator_ptr->menu_style.item,
                "Failed, exit code %d", state);
            bs_log(BS_ERROR, "Subprocess %p failed, exit code %d",
                   subprocess_handle_ptr, state);
        } else {
            menu_item_ptr = _wlmaker_root_menu_create_disabled_item(
                &generator_ptr->menu_style.item,
                "Failed, signal %d", code);
            bs_log(BS_ERROR, "Subprocess %p failed, signal %d",
                   subprocess_handle_ptr, code);
        }
    } else {

        bs_log(BS_INFO, "Subprocess %p terminated", subprocess_handle_ptr);

        bspl_object_t *object_ptr = bspl_create_object_from_dynbuf(
            generator_ptr->stdout_dynbuf_ptr);
        if (NULL == object_ptr ||
            BSPL_ARRAY != bspl_object_type(object_ptr)) {
            menu_item_ptr = _wlmaker_root_menu_create_disabled_item(
                &generator_ptr->menu_style.item,
                "Failed to parse Plist ARRAY from \"%.*s\"",
                (int)(generator_ptr->stdout_dynbuf_ptr->length),
                (char*)(generator_ptr->stdout_dynbuf_ptr->data_ptr));
            bs_log(BS_ERROR,
                   "Failed to parse Plist ARRAY from \"%.*s\"",
                   (int)(generator_ptr->stdout_dynbuf_ptr->length),
                   (char*)(generator_ptr->stdout_dynbuf_ptr->data_ptr));
        } else {

            if (!_wlmaker_root_menu_populate_menu_items_from_array(
                    generator_ptr->menu_ptr,
                    bspl_array_from_object(object_ptr),
                    &generator_ptr->menu_style,
                    generator_ptr->server_ptr)) {
                menu_item_ptr = _wlmaker_root_menu_create_disabled_item(
                    &generator_ptr->menu_style.item,
                    "Failed to populate menu from Plist ARRAY \"%.*s\"",
                    (int)(generator_ptr->stdout_dynbuf_ptr->length),
                    (char*)(generator_ptr->stdout_dynbuf_ptr->data_ptr));
                bs_log(BS_ERROR,
                       "Failed to populate menu from Plist ARRAY \"%.*s\"",
                       (int)(generator_ptr->stdout_dynbuf_ptr->length),
                       (char*)(generator_ptr->stdout_dynbuf_ptr->data_ptr));
            }
        }
        if (NULL != object_ptr) {
            bspl_object_unref(object_ptr);
            object_ptr = NULL;
        }
    }

    if (NULL!= menu_item_ptr) {
        wlmtk_menu_add_item(generator_ptr->menu_ptr, menu_item_ptr);
    }

    generator_ptr->subprocess_handle_ptr = NULL;

    if (NULL != _wlmaker_root_menu_test_wl_display_ptr) {
        wl_display_terminate(
            _wlmaker_root_menu_test_wl_display_ptr);
        _wlmaker_root_menu_test_wl_display_ptr = NULL;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a menu item from the Plist array.
 *
 * The Plist array either defines a menu action item, where the array elements
 * are `(Title, ActionName, OptionalActionArg)`. Or, it defines a submenu, as
 * specified in @ref _wlmaker_root_menu_init_menu_from_array.
 *
 * For the list of permitted `ActionName` values, see @ref wlmaker_action_desc.

 * @param item_array_ptr
 * @param menu_style_ptr
 * @param server_ptr
 *
 * @return The menu item, or NULL on error.
 */
wlmtk_menu_item_t *_wlmaker_root_menu_create_item_from_array(
    bspl_array_t *item_array_ptr,
    const wlmtk_menu_style_t *menu_style_ptr,
    wlmaker_server_t *server_ptr)
{
    wlmtk_menu_item_t *menu_item_ptr = wlmtk_menu_item_create(
        &menu_style_ptr->item);
    if (NULL == menu_item_ptr) return NULL;

    if (!wlmtk_menu_item_set_text(
            menu_item_ptr,
            bspl_array_string_value_at(item_array_ptr, 0))) goto error;

    // If second element is a string that translates to an action: Bind it.
    int action;
    if (bspl_enum_name_to_value(
            wlmaker_action_desc,
                bspl_array_string_value_at(item_array_ptr, 1),
            &action)) {
        wlmaker_menu_item_bind_action(
            menu_item_ptr,
            action,
            bspl_array_string_value_at(item_array_ptr, 2),
            server_ptr);
        return menu_item_ptr;
    }

    wlmtk_menu_t *submenu_ptr = wlmtk_menu_create(menu_style_ptr);
    if (NULL == submenu_ptr) goto error;
    wlmtk_menu_item_set_submenu(menu_item_ptr, submenu_ptr);

    if (!_wlmaker_root_menu_init_menu_from_array(
            submenu_ptr,
            item_array_ptr,
            menu_style_ptr,
            server_ptr)) {
        goto error;
    }
    return menu_item_ptr;

error:
wlmtk_menu_item_destroy(menu_item_ptr);
return NULL;
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a disabled menu item as a means to display generator state.
 *
 * @param style_ptr
 * @param fmt_ptr
 *
 * @return the disabled menu item, or NULL on error.
 */
wlmtk_menu_item_t *_wlmaker_root_menu_create_disabled_item(
    const wlmtk_menu_item_style_t *style_ptr,
    const char *fmt_ptr,
    ...)
{
    va_list ap;
    char buf[1024];

    va_start(ap, fmt_ptr);
    vsnprintf(buf, sizeof(buf), fmt_ptr, ap);
    va_end(ap);

    wlmtk_menu_item_t *menu_item_ptr = wlmtk_menu_item_create(style_ptr);
    if (NULL == menu_item_ptr) {
        bs_log(BS_ERROR, "Failed wlmtk_menu_item_create(%p) for \"%s\"",
               style_ptr, buf);
        return NULL;
    }

    wlmtk_menu_item_set_text(menu_item_ptr, buf);
    wlmtk_menu_item_set_enabled(menu_item_ptr, false);
    return menu_item_ptr;
}

/* == Unit tests =========================================================== */

static void test_default_menu(bs_test_t *test_ptr);
static void test_generated_menu(bs_test_t *test_ptr);

const bs_test_case_t wlmaker_root_menu_test_cases[] = {
    { 1, "default_menu", test_default_menu },
    { 1, "generated_menu", test_generated_menu },
    { 0, NULL, NULL }
};


/* ------------------------------------------------------------------------- */
/** Verifies that the compiled-in configuration translates into a menu. */
void test_default_menu(bs_test_t *test_ptr)
{
    wlmtk_window_style_t window_style = {};
    wlmtk_menu_style_t menu_style = {};
    wlmaker_server_t server = {};

    wlmaker_root_menu_t *root_menu_ptr = wlmaker_root_menu_create(
        &server, NULL, &window_style, &menu_style);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, root_menu_ptr);
    wlmaker_root_menu_destroy(root_menu_ptr);
}

/* ------------------------------------------------------------------------- */
/** Verifies that an example menu with generator is translated. */
void test_generated_menu(bs_test_t *test_ptr)
{
    wlmtk_window_style_t window_style = {};
    wlmtk_menu_style_t menu_style = {};

    wlmaker_server_t server = {
        .wl_display_ptr = wl_display_create(),
        .wlr_scene_ptr = wlr_scene_create()
    };
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, server.wlr_scene_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, server.wl_display_ptr);
    wl_signal_init(&server.window_created_event);
    wl_signal_init(&server.window_destroyed_event);
    server.wlr_output_layout_ptr = wlr_output_layout_create(
        server.wl_display_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, server.wlr_output_layout_ptr);
    server.root_ptr = wlmtk_root_create(
        server.wlr_scene_ptr,
        server.wlr_output_layout_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, server.root_ptr);
    server.monitor_ptr = wlmaker_subprocess_monitor_create(
        &server);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, server.monitor_ptr);

#ifndef WLMAKER_SOURCE_DIR
#error "Missing definition of WLMAKER_SOURCE_DIR!"
#endif
    BS_TEST_VERIFY_EQ_OR_RETURN(test_ptr, 0, chdir(WLMAKER_SOURCE_DIR));
    wlmaker_root_menu_t *root_menu_ptr;

    // Exercise & verify including a submenu from a file.
    root_menu_ptr = wlmaker_root_menu_create(
        &server,
        bs_test_resolve_path("menu-include.plist"),
        &window_style, &menu_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, root_menu_ptr);
    wlmtk_menu_t *menu_ptr = wlmaker_root_menu_menu(root_menu_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, 0, wlmtk_menu_items_size(menu_ptr));
    wlmaker_root_menu_destroy(root_menu_ptr);

    // Exercise & verify generating a submenu from a shell command.
    root_menu_ptr = wlmaker_root_menu_create(
        &server,
        bs_test_resolve_path("menu-generate.plist"),
        &window_style, &menu_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, root_menu_ptr);
    menu_ptr = wlmaker_root_menu_menu(root_menu_ptr);
    _wlmaker_root_menu_test_wl_display_ptr = server.wl_display_ptr;
    wl_display_run(server.wl_display_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, 0, wlmtk_menu_items_size(menu_ptr));
    wlmaker_root_menu_destroy(root_menu_ptr);

    // Exercise & verify that a menu can be generated from output of
    // Window Maker's `wmmenugen` command. This is conditional of that binary
    // being present on the host.
#ifndef WLMAKER_BINARY_DIR
#error "Missing definition of WLMAKER_BINARY_DIR!"
#endif
#if defined(WLMAKER_WMMENUGEN_PATH)
    root_menu_ptr = wlmaker_root_menu_create(
        &server,
        WLMAKER_BINARY_DIR "/etc/root-menu-debian.plist",
        &window_style, &menu_style);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, root_menu_ptr);
    menu_ptr = wlmaker_root_menu_menu(root_menu_ptr);
    _wlmaker_root_menu_test_wl_display_ptr = server.wl_display_ptr;
    wl_display_run(server.wl_display_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, 0, wlmtk_menu_items_size(menu_ptr));

    wlmtk_menu_item_t *item_ptr = wlmtk_menu_item_at(menu_ptr, 0);
    menu_ptr = wlmtk_menu_item_get_submenu(item_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, menu_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, 0, wlmtk_menu_items_size(menu_ptr));
    item_ptr = wlmtk_menu_item_at(menu_ptr, 0);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        WLMTK_MENU_ITEM_ENABLED,
        wlmtk_menu_item_get_state(item_ptr));

    wlmaker_root_menu_destroy(root_menu_ptr);
#endif

    wlmaker_subprocess_monitor_destroy(server.monitor_ptr);
    wlmtk_root_destroy(server.root_ptr);
    wl_display_destroy(server.wl_display_ptr);
    wlr_scene_node_destroy(&server.wlr_scene_ptr->tree.node);
}

/* == End of root_menu.c =================================================== */
