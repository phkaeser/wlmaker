/* ========================================================================= */
/**
 * @file launcher.c
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
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

#include "launcher.h"

#include <cairo.h>
#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <linux/input-event-codes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>

#include "toolkit/toolkit.h"

struct wlm_util_subprocess;

/* == Declarations ========================================================= */

/** State of a launcher. */
struct _wlmdock_launcher_t {
    /** The launcher is derived from a @ref wlmtk_tile_t. */
    wlmtk_tile_t              super_tile;
    /** Original virtual method table fo the element. */
    wlmtk_element_vmt_t       orig_element_vmt;

    /** Image element. One element of @ref wlmdock_launcher_t::super_tile. */
    wlmtk_image_t             *image_ptr;
    /** Overlay element. Atop on the tile. */
    wlmtk_buffer_t            overlay_buffer;

    /** Subprocess monitor to register launched processes to. */
    wlm_util_subprocess_monitor_t *monitor_ptr;

    /** Commandline to launch the associated application. */
    char                      *cmdline_ptr;
    /** Path to the icon. */
    char                      *icon_path_ptr;

    /** Resolved icon path. */
    char                      *resolved_icon_path_ptr;

    /** Subprocesses that were created by this launcher. */
    bs_ptr_set_t              *subprocesses_ptr;
};

/** Plist descroptor for a launcher. */
static const bspl_desc_t _wlmdock_launcher_plist_desc[] = {
    BSPL_DESC_STRING(
        "CommandLine", true, wlmdock_launcher_t, cmdline_ptr, cmdline_ptr, ""),
    BSPL_DESC_STRING(
        "Icon", true, wlmdock_launcher_t, icon_path_ptr, icon_path_ptr, ""),
    BSPL_DESC_SENTINEL(),
};

static void _wlmdock_launcher_update_overlay(wlmdock_launcher_t *launcher_ptr);
static struct wlr_buffer *_wlmdock_launcher_create_overlay_buffer(
    wlmdock_launcher_t *launcher_ptr);

static void _wlmdock_launcher_element_destroy(
    wlmtk_element_t *element_ptr);
static bool _wlmdock_launcher_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);

static void _wlmdock_launcher_start(wlmdock_launcher_t *launcher_ptr);

static void _wlmdock_launcher_handle_terminated(
    void *userdata_ptr,
    struct wlm_util_subprocess *subprocess_handle_ptr,
    int state,
    int code);

static bool _wlmdock_launcher_set_content_size(
    wlmtk_tile_t *tile_ptr,
    uint64_t content_size);

/* == Data ================================================================= */

/** The launcher's extension to @ref wlmtk_element_t virtual method table. */
static const wlmtk_element_vmt_t _wlmdock_launcher_element_vmt = {
    .destroy = _wlmdock_launcher_element_destroy,
    .pointer_button = _wlmdock_launcher_pointer_button,
};

/** Launcher's extension to @ref wlmtk_tile_t virtual method table. */
static const wlmtk_tile_vmt_t _wlmdock_launcher_tile_vmt = {
    .set_content_size = _wlmdock_launcher_set_content_size,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmdock_launcher_t *wlmdock_launcher_create_from_plist(
    const struct wlmtk_tile_style *style_ptr,
    bspl_dict_t *dict_ptr,
    wlm_util_subprocess_monitor_t *monitor_ptr,
    wlm_util_files_t *files_ptr)
{
    wlmdock_launcher_t *launcher_ptr = logged_calloc(
        1, sizeof(wlmdock_launcher_t));
    if (NULL == launcher_ptr) return NULL;
    launcher_ptr->monitor_ptr = monitor_ptr;

     if (!wlmtk_tile_init(&launcher_ptr->super_tile, style_ptr)) {
         return NULL;
    }
    launcher_ptr->orig_element_vmt = wlmtk_element_extend(
        wlmtk_tile_element(&launcher_ptr->super_tile),
        &_wlmdock_launcher_element_vmt);
    wlmtk_tile_extend(&launcher_ptr->super_tile, &_wlmdock_launcher_tile_vmt);
    wlmtk_element_set_visible(
        wlmtk_tile_element(&launcher_ptr->super_tile), true);

    launcher_ptr->subprocesses_ptr = bs_ptr_set_create();
    if (NULL == launcher_ptr->subprocesses_ptr) {
        wlmdock_launcher_destroy(launcher_ptr);
        return NULL;
    }

    if (!wlmtk_buffer_init(&launcher_ptr->overlay_buffer)) {
        wlmdock_launcher_destroy(launcher_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_buffer_element(&launcher_ptr->overlay_buffer), true);
    _wlmdock_launcher_update_overlay(launcher_ptr);
    wlmtk_tile_set_overlay(
        &launcher_ptr->super_tile,
        wlmtk_buffer_element(&launcher_ptr->overlay_buffer));

    if (!bspl_decode_dict(
            dict_ptr, _wlmdock_launcher_plist_desc, launcher_ptr)) {
        bs_log(BS_ERROR, "Failed to create launcher from plist dict.");
        wlmdock_launcher_destroy(launcher_ptr);
        return NULL;
    }

    // Resolves to a full path, and verifies the icon file exists.
    char *p = bs_strdupf("icons/%s", launcher_ptr->icon_path_ptr);
    if (NULL == p) {
        bs_log(BS_ERROR | BS_ERRNO, "Failed bs_strdupf(\"icons/%s\")",
               launcher_ptr->icon_path_ptr);
        wlmdock_launcher_destroy(launcher_ptr);
        return NULL;
    }
    launcher_ptr->resolved_icon_path_ptr = wlm_util_files_xdg_data_find(
        files_ptr, p, S_IFREG);
    free(p);
    if (NULL == launcher_ptr->resolved_icon_path_ptr) {
        bs_log(BS_ERROR,
               "Failed to locate \"icons/%s\" in ${XDG_DATA_DIRS}/wlmaker",
               launcher_ptr->icon_path_ptr);
#ifndef WLMAKER_SOURCE_DIR
        wlmdock_launcher_destroy(launcher_ptr);
        return NULL;
#else
        launcher_ptr->resolved_icon_path_ptr = bs_strdupf(
            WLMAKER_SOURCE_DIR "/share/wlmaker/icons/%s",
            launcher_ptr->icon_path_ptr);
        if (NULL == launcher_ptr->resolved_icon_path_ptr) {
            wlmdock_launcher_destroy(launcher_ptr);
            return NULL;
        }
#endif
    }
    launcher_ptr->image_ptr = wlmtk_image_create_scaled(
        launcher_ptr->resolved_icon_path_ptr,
        launcher_ptr->super_tile.style.content_size,
        launcher_ptr->super_tile.style.content_size);
    if (NULL == launcher_ptr->image_ptr) {
        wlmdock_launcher_destroy(launcher_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_image_element(launcher_ptr->image_ptr), true);
    wlmtk_tile_set_content(
        &launcher_ptr->super_tile,
        wlmtk_image_element(launcher_ptr->image_ptr));

    wlmtk_element_set_visible(
        wlmtk_tile_element(&launcher_ptr->super_tile), true);


    return launcher_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmdock_launcher_destroy(wlmdock_launcher_t *launcher_ptr)
{
    if (NULL != launcher_ptr->image_ptr) {
        wlmtk_tile_set_content(&launcher_ptr->super_tile, NULL);
        wlmtk_image_destroy(launcher_ptr->image_ptr);
        launcher_ptr->image_ptr = NULL;
    }

    wlmtk_tile_set_overlay(&launcher_ptr->super_tile, NULL);
    wlmtk_buffer_fini(&launcher_ptr->overlay_buffer);

    if (NULL != launcher_ptr->subprocesses_ptr) {
        struct wlm_util_subprocess *subprocess_handle_ptr;
        while (NULL != (subprocess_handle_ptr = bs_ptr_set_any(
                            launcher_ptr->subprocesses_ptr))) {
            wlm_util_subprocess_monitor_cede(
                launcher_ptr->monitor_ptr,
                subprocess_handle_ptr);
            bs_ptr_set_erase(launcher_ptr->subprocesses_ptr,
                             subprocess_handle_ptr);
        }
        bs_ptr_set_destroy(launcher_ptr->subprocesses_ptr);
        launcher_ptr->subprocesses_ptr = NULL;
    }

    if (NULL != launcher_ptr->resolved_icon_path_ptr) {
        free(launcher_ptr->resolved_icon_path_ptr);
        launcher_ptr->resolved_icon_path_ptr = NULL;
    }

    if (NULL != launcher_ptr->cmdline_ptr) {
        free(launcher_ptr->cmdline_ptr);
        launcher_ptr->cmdline_ptr = NULL;
    }

    if (NULL != launcher_ptr->icon_path_ptr) {
        free(launcher_ptr->icon_path_ptr);
        launcher_ptr->icon_path_ptr = NULL;
    }

    wlmtk_tile_fini(&launcher_ptr->super_tile);
    free(launcher_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_tile_t *wlmdock_launcher_tile(wlmdock_launcher_t *launcher_ptr)
{
    return &launcher_ptr->super_tile;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Redraws the overlay element. */
void _wlmdock_launcher_update_overlay(wlmdock_launcher_t *launcher_ptr)
{
    struct wlr_buffer *wlr_buffer_ptr = _wlmdock_launcher_create_overlay_buffer(
        launcher_ptr);
    if (NULL == wlr_buffer_ptr) return;
    wlmtk_buffer_set(&launcher_ptr->overlay_buffer, wlr_buffer_ptr);
    wlr_buffer_drop(wlr_buffer_ptr);
}

/* ------------------------------------------------------------------------- */
/** Creates an overlay wlr_buffer. */
struct wlr_buffer *_wlmdock_launcher_create_overlay_buffer(
    wlmdock_launcher_t *launcher_ptr)
{
    int s = launcher_ptr->super_tile.style.size;
    struct wlr_buffer *wlr_buffer_ptr = bs_gfxbuf_create_wlr_buffer(s, s);
    if (NULL == wlr_buffer_ptr) return NULL;

    if (bs_ptr_set_empty(launcher_ptr->subprocesses_ptr)) {
        return wlr_buffer_ptr;
    }

    cairo_t *cairo_ptr = cairo_create_from_wlr_buffer(wlr_buffer_ptr);
    if (NULL == cairo_ptr) {
        wlr_buffer_drop(wlr_buffer_ptr);
        return NULL;
    }

    float r, g, b, alpha;
    bs_gfxbuf_argb8888_to_floats(0xff12905a, &r, &g, &b, &alpha);
    cairo_pattern_t *cairo_pattern_ptr = cairo_pattern_create_rgba(
        r, g, b, alpha);
    cairo_set_source(cairo_ptr, cairo_pattern_ptr);
    cairo_pattern_destroy(cairo_pattern_ptr);
    cairo_rectangle(cairo_ptr, 0, s - 12 * s / 64, s, 12 * s / 64);
    cairo_fill(cairo_ptr);
    cairo_stroke(cairo_ptr);

    cairo_select_font_face(cairo_ptr, "Helvetica",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cairo_ptr, 10.0 * s / 64.0);
    cairo_set_source_argb8888(cairo_ptr, 0xffffffff);
    cairo_move_to(cairo_ptr, 4 * s / 64, s - 2 * s / 64);
    cairo_show_text(cairo_ptr, "Running");

    cairo_destroy(cairo_ptr);
    return wlr_buffer_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Implements @ref wlmtk_element_vmt_t::destroy. Calls
 * @ref wlmdock_launcher_destroy.
 *
 * @param element_ptr
 */
void _wlmdock_launcher_element_destroy(
    wlmtk_element_t *element_ptr)
{
    wlmdock_launcher_t *launcher_ptr = BS_CONTAINER_OF(
        element_ptr, wlmdock_launcher_t,
        super_tile.super_container.super_element);
    wlmdock_launcher_destroy(launcher_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Implements @ref wlmtk_element_vmt_t::pointer_button.
 *
 * @param element_ptr
 * @param button_event_ptr
 *
 * @return true always.
 */
bool _wlmdock_launcher_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmdock_launcher_t *launcher_ptr = BS_CONTAINER_OF(
        element_ptr, wlmdock_launcher_t,
        super_tile.super_container.super_element);

    if (BTN_LEFT != button_event_ptr->button) return true;
    if (WLMTK_BUTTON_CLICK != button_event_ptr->type) return true;

    _wlmdock_launcher_start(launcher_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Starts the application, called when the launcher is clicked.
 *
 * @param launcher_ptr
 */
void _wlmdock_launcher_start(wlmdock_launcher_t *launcher_ptr)
{
    bs_subprocess_t *subprocess_ptr = bs_subprocess_create_cmdline(
        launcher_ptr->cmdline_ptr);
    if (NULL == subprocess_ptr) {
        bs_log(BS_ERROR, "Failed bs_subprocess_create_cmdline(%s)",
               launcher_ptr->cmdline_ptr);
        return;
    }

    if (!bs_subprocess_start(subprocess_ptr)) {
        bs_log(BS_ERROR, "Failed bs_subprocess_start for %s",
               launcher_ptr->cmdline_ptr);
        bs_subprocess_destroy(subprocess_ptr);
        return;
    }

    struct wlm_util_subprocess *subprocess_handle_ptr;
    subprocess_handle_ptr = wlm_util_subprocess_monitor_entrust(
        launcher_ptr->monitor_ptr,
        subprocess_ptr,
        _wlmdock_launcher_handle_terminated,
        launcher_ptr,
        NULL);

    bs_log(BS_ERROR, "FIXME: launched.");

    if (!bs_ptr_set_insert(launcher_ptr->subprocesses_ptr,
                           subprocess_handle_ptr)) {
        bs_log(BS_WARNING, "Launcher %p: Failed bs_ptr_set_insert(%p, %p). "
               "Will not show status of subprocess in App.",
               launcher_ptr, launcher_ptr->subprocesses_ptr,
               subprocess_handle_ptr);
        wlm_util_subprocess_monitor_cede(
            launcher_ptr->monitor_ptr,
            subprocess_handle_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Callback handler for when the registered subprocess terminates.
 *
 * @param userdata_ptr        Points to @ref wlmdock_launcher_t.
 * @param subprocess_handle_ptr
 * @param exit_status
 * @param signal_number
 */
void _wlmdock_launcher_handle_terminated(
    void *userdata_ptr,
    __UNUSED__ struct wlm_util_subprocess *subprocess_handle_ptr,
    int exit_status,
    int signal_number)
{
    wlmdock_launcher_t *launcher_ptr = userdata_ptr;
    const char *format_ptr;
    int code;

    if (0 == signal_number) {
        format_ptr = "App '%s' (%p) terminated, status code %d.";
        code = exit_status;
    } else {
        format_ptr = "App '%s' (%p) killed by signal %d.";
        code = signal_number;
    }

    bs_log(BS_INFO, format_ptr,
           launcher_ptr->cmdline_ptr,
           launcher_ptr,
           code);
    // TODO(kaeser@gubbe.ch): Keep exit status and latest output available
    // for visualization.
    wlm_util_subprocess_monitor_cede(
        launcher_ptr->monitor_ptr,
        subprocess_handle_ptr);
    bs_ptr_set_erase(launcher_ptr->subprocesses_ptr,
                     subprocess_handle_ptr);
}

/* ------------------------------------------------------------------------- */
/** Implements @ref wlmtk_tile_vmt_t::set_content_size. Set the image size. */
bool _wlmdock_launcher_set_content_size(
    wlmtk_tile_t *tile_ptr,
    uint64_t content_size)
{
    wlmdock_launcher_t *launcher_ptr = BS_CONTAINER_OF(
        tile_ptr, wlmdock_launcher_t, super_tile);

    wlmtk_image_t *image_ptr = wlmtk_image_create_scaled(
        launcher_ptr->resolved_icon_path_ptr, content_size, content_size);
    if (NULL == image_ptr) return false;

    wlmtk_element_set_visible(wlmtk_image_element(image_ptr), true);
    wlmtk_tile_set_content(tile_ptr, wlmtk_image_element(image_ptr));
    wlmtk_image_destroy(launcher_ptr->image_ptr);
    launcher_ptr->image_ptr = image_ptr;
    return true;
}

/* == Unit tests =========================================================== */

static void test_create_from_plist(bs_test_t *test_ptr);

/** Unit test cases. */
static const bs_test_case_t wlmdock_launcher_test_cases[] = {
    { true, "create_from_plist", test_create_from_plist },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmdock_launcher_test_set = BS_TEST_SET(
    true, "launcher", wlmdock_launcher_test_cases);

/* ------------------------------------------------------------------------- */
/** Exercises plist parser. */
void test_create_from_plist(bs_test_t *test_ptr)
{
    static const struct wlmtk_tile_style style = { .size = 96 };
    static const char *plist_ptr =
        "{CommandLine = \"a\"; Icon = \"chrome-56x56.png\";}";

    bs_test_setenv(test_ptr, "XDG_DATA_DIRS", WLMAKER_SOURCE_DIR "/share");

    bspl_dict_t *dict_ptr = bspl_dict_from_object(
        bspl_create_object_from_plist_string(plist_ptr));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, dict_ptr);

    wlm_util_files_t *files_ptr = wlm_util_files_create("wlmaker");
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, files_ptr);

    wlmdock_launcher_t *launcher_ptr = wlmdock_launcher_create_from_plist(
        &style, dict_ptr, NULL, files_ptr);
    bspl_dict_unref(dict_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, launcher_ptr);

    BS_TEST_VERIFY_STREQ(test_ptr, "a", launcher_ptr->cmdline_ptr);
    BS_TEST_VERIFY_STREQ(
        test_ptr, "chrome-56x56.png", launcher_ptr->icon_path_ptr);

    wlmdock_launcher_destroy(launcher_ptr);
    wlm_util_files_destroy(files_ptr);
}

/* == End of launcher.c ==================================================== */
