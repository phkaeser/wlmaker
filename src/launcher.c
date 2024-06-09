/* ========================================================================= */
/**
 * @file launcher.c
 * Copyright (c) 2024 by Philipp Kaeser <kaeser@gubbe.ch>
 */

#include "launcher.h"

#include <limits.h>
#include <libbase/libbase.h>
#include <toolkit/toolkit.h>

#include "conf/decode.h"
#include "conf/plist.h"

/* == Declarations ========================================================= */

/** State of a launcher. */
struct _wlmaker_launcher_t {
    /** The launcher is derived from a @ref wlmtk_tile_t. */
    wlmtk_tile_t              super_tile;
    /** Original virtual method table fo the element. */
    wlmtk_element_vmt_t       orig_element_vmt;

    /** Image element. One element of @ref wlmaker_launcher_t::super_tile. */
    wlmtk_image_t             *image_ptr;

    /** Back-link to the server. */
    wlmaker_server_t          *server_ptr;

    /** Commandline to launch the associated application. */
    char                      *cmdline_ptr;
    /** Path to the icon. */
    char                      *icon_path_ptr;

    /** Windows that are running from subprocesses of this App (launcher). */
    bs_ptr_set_t              *created_windows_ptr;
    /** Windows that are mapped from subprocesses of this App (launcher). */
    bs_ptr_set_t              *mapped_windows_ptr;
    /** Subprocesses that were created by this launcher. */
    bs_ptr_set_t              *subprocesses_ptr;
};

/** Plist descroptor for a launcher. */
static const wlmcfg_desc_t _wlmaker_launcher_plist_desc[] = {
    WLMCFG_DESC_STRING(
        "CommandLine", true, wlmaker_launcher_t, cmdline_ptr, ""),
    WLMCFG_DESC_STRING(
        "Icon", true, wlmaker_launcher_t, icon_path_ptr, ""),
    WLMCFG_DESC_SENTINEL(),
};

/** Lookup paths for icons. */
static const char *lookup_paths[] = {
    "/usr/share/icons/wlmaker",
    "/usr/local/share/icons/wlmaker",
#if defined(WLMAKER_SOURCE_DIR)
    WLMAKER_SOURCE_DIR "/icons",
#endif  // WLMAKER_SOURCE_DIR
#if defined(WLMAKER_ICON_DATA_DIR)
    WLMAKER_ICON_DATA_DIR,
#endif  // WLMAKER_ICON_DATA_DIR
    NULL
};

static bool _wlmaker_launcher_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);

static void _wlmaker_launcher_start(wlmaker_launcher_t *launcher_ptr);

static void _wlmaker_launcher_handle_terminated(
    void *userdata_ptr,
    wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    int state,
    int code);
static void _wlmaker_launcher_handle_window_created(
    void *userdata_ptr,
    wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    wlmtk_window_t *window_ptr);
static void _wlmaker_launcher_handle_window_mapped(
    void *userdata_ptr,
    wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    wlmtk_window_t *window_ptr);
static void _wlmaker_launcher_handle_window_unmapped(
    void *userdata_ptr,
    wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    wlmtk_window_t *window_ptr);
static void _wlmaker_launcher_handle_window_destroyed(
    void *userdata_ptr,
    wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    wlmtk_window_t *window_ptr);

/* == Data ================================================================= */

/** The launcher's extension to @ref wlmtk_element_t virtual method table. */
static const wlmtk_element_vmt_t _wlmaker_launcher_element_vmt = {
    .pointer_button = _wlmaker_launcher_pointer_button,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_launcher_t *wlmaker_launcher_create_from_plist(
    wlmaker_server_t *server_ptr,
    const wlmtk_tile_style_t *style_ptr,
    wlmcfg_dict_t *dict_ptr)
{
    wlmaker_launcher_t *launcher_ptr = logged_calloc(
        1, sizeof(wlmaker_launcher_t));
    if (NULL == launcher_ptr) return NULL;
    launcher_ptr->server_ptr = server_ptr;

    if (!wlmtk_tile_init(&launcher_ptr->super_tile,
                         style_ptr,
                         server_ptr->env_ptr)) {

        return NULL;
    }
    launcher_ptr->orig_element_vmt = wlmtk_element_extend(
        wlmtk_tile_element(&launcher_ptr->super_tile),
        &_wlmaker_launcher_element_vmt);
    wlmtk_element_set_visible(
        wlmtk_tile_element(&launcher_ptr->super_tile), true);

    if (!wlmcfg_decode_dict(
            dict_ptr, _wlmaker_launcher_plist_desc, launcher_ptr)) {
        bs_log(BS_ERROR, "Failed to create launcher from plist dict.");
        wlmaker_launcher_destroy(launcher_ptr);
        return NULL;
    }

    launcher_ptr->created_windows_ptr = bs_ptr_set_create();
    if (NULL == launcher_ptr->created_windows_ptr) {
        wlmaker_launcher_destroy(launcher_ptr);
        return NULL;
    }
    launcher_ptr->mapped_windows_ptr = bs_ptr_set_create();
    if (NULL == launcher_ptr->mapped_windows_ptr) {
        wlmaker_launcher_destroy(launcher_ptr);
        return NULL;
    }
    launcher_ptr->subprocesses_ptr = bs_ptr_set_create();
    if (NULL == launcher_ptr->subprocesses_ptr) {
        wlmaker_launcher_destroy(launcher_ptr);
        return NULL;
    }

    // Resolves to a full path, and verifies the icon file exists.
    char full_path[PATH_MAX];
    char *path_ptr = bs_file_resolve_and_lookup_from_paths(
        BS_ASSERT_NOTNULL(launcher_ptr->icon_path_ptr),
        lookup_paths, 0, full_path);
    if (NULL == path_ptr) {
        bs_log(BS_ERROR | BS_ERRNO,
               "Failed bs_file_resolve_and_lookup_from_paths(\"%s\" ...)",
               launcher_ptr->icon_path_ptr);
        wlmaker_launcher_destroy(launcher_ptr);
        return NULL;
    }
    launcher_ptr->image_ptr = wlmtk_image_create(
        path_ptr, server_ptr->env_ptr);
    if (NULL == launcher_ptr->image_ptr) {
        wlmaker_launcher_destroy(launcher_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_image_element(launcher_ptr->image_ptr), true);
    wlmtk_tile_set_content(
        &launcher_ptr->super_tile,
        wlmtk_image_element(launcher_ptr->image_ptr));

    return launcher_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_launcher_destroy(wlmaker_launcher_t *launcher_ptr)
{
    if (NULL != launcher_ptr->image_ptr) {
        wlmtk_tile_set_content(&launcher_ptr->super_tile, NULL);
        wlmtk_image_destroy(launcher_ptr->image_ptr);
        launcher_ptr->image_ptr = NULL;
    }

    if (NULL != launcher_ptr->subprocesses_ptr) {
        wlmaker_subprocess_handle_t *subprocess_handle_ptr;
        while (NULL != (subprocess_handle_ptr = bs_ptr_set_any(
                            launcher_ptr->subprocesses_ptr))) {
            wlmaker_subprocess_monitor_cede(
                launcher_ptr->server_ptr->monitor_ptr,
                subprocess_handle_ptr);
            bs_ptr_set_erase(launcher_ptr->subprocesses_ptr,
                             subprocess_handle_ptr);
        }
        bs_ptr_set_destroy(launcher_ptr->subprocesses_ptr);
        launcher_ptr->subprocesses_ptr = NULL;
    }
    if (NULL != launcher_ptr->mapped_windows_ptr) {
        bs_ptr_set_destroy(launcher_ptr->mapped_windows_ptr);
        launcher_ptr->mapped_windows_ptr = NULL;
    }
    if (NULL != launcher_ptr->created_windows_ptr) {
        bs_ptr_set_destroy(launcher_ptr->created_windows_ptr);
        launcher_ptr->created_windows_ptr = NULL;
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
wlmtk_tile_t *wlmaker_launcher_tile(wlmaker_launcher_t *launcher_ptr)
{
    return &launcher_ptr->super_tile;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Implements @ref wlmtk_element_vmt_t::pointer_button.
 *
 * @param element_ptr
 * @param button_event_ptr
 *
 * @return true always.
 */
bool _wlmaker_launcher_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmaker_launcher_t *launcher_ptr = BS_CONTAINER_OF(
        element_ptr, wlmaker_launcher_t,
        super_tile.super_container.super_element);

    if (BTN_LEFT != button_event_ptr->button) return true;
    if (WLMTK_BUTTON_CLICK != button_event_ptr->type) return true;

    _wlmaker_launcher_start(launcher_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Starts the application, called when the launcher is clicked.
 *
 * @param launcher_ptr
 */
void _wlmaker_launcher_start(wlmaker_launcher_t *launcher_ptr)
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

    wlmaker_subprocess_handle_t *subprocess_handle_ptr;
    subprocess_handle_ptr = wlmaker_subprocess_monitor_entrust(
        launcher_ptr->server_ptr->monitor_ptr,
        subprocess_ptr,
        _wlmaker_launcher_handle_terminated,
        launcher_ptr,
        _wlmaker_launcher_handle_window_created,
        _wlmaker_launcher_handle_window_mapped,
        _wlmaker_launcher_handle_window_unmapped,
        _wlmaker_launcher_handle_window_destroyed);

    if (!bs_ptr_set_insert(launcher_ptr->subprocesses_ptr,
                           subprocess_handle_ptr)) {
        bs_log(BS_WARNING, "Launcher %p: Failed bs_ptr_set_insert(%p, %p). "
               "Will not show status of subprocess in App.",
               launcher_ptr, launcher_ptr->subprocesses_ptr,
               subprocess_handle_ptr);
        wlmaker_subprocess_monitor_cede(
            launcher_ptr->server_ptr->monitor_ptr,
            subprocess_handle_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Callback handler for when the registered subprocess terminates.
 *
 * @param userdata_ptr        Points to @ref wlmaker_launcher_t.
 * @param subprocess_handle_ptr
 * @param exit_status
 * @param signal_number
 */
void _wlmaker_launcher_handle_terminated(
    void *userdata_ptr,
    wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    int exit_status,
    int signal_number)
{
    wlmaker_launcher_t *launcher_ptr = userdata_ptr;
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
    wlmaker_subprocess_monitor_cede(
        launcher_ptr->server_ptr->monitor_ptr,
        subprocess_handle_ptr);
    bs_ptr_set_erase(launcher_ptr->subprocesses_ptr,
                     subprocess_handle_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Callback for then a window from the launched subprocess is created.
 *
 * Registers the windows as "created", and will then redraw the launcher tile
 * to reflect potential status changes.
 *
 * @param userdata_ptr        Points to the @ref wlmaker_launcher_t.
 * @param subprocess_handle_ptr
 * @param window_ptr
 */
void _wlmaker_launcher_handle_window_created(
    void *userdata_ptr,
    __UNUSED__ wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    wlmtk_window_t *window_ptr)
{
    wlmaker_launcher_t *launcher_ptr = userdata_ptr;

    bool rv = bs_ptr_set_insert(launcher_ptr->created_windows_ptr, window_ptr);
    if (!rv) bs_log(BS_ERROR, "Failed bs_ptr_set_insert(%p)", window_ptr);

    // FIXME : redraw_tile(dock_app_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Callback for then a window from the launched subprocess is mapped.
 *
 * Registers the window as "mapped", and will then redraw the launcher tile
 * to reflect potential status changes.
 *
 * @param userdata_ptr        Points to the @ref wlmaker_launcher_t.
 * @param subprocess_handle_ptr
 * @param window_ptr
 */
void _wlmaker_launcher_handle_window_mapped(
    void *userdata_ptr,
    __UNUSED__ wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    wlmtk_window_t *window_ptr)
{
    wlmaker_launcher_t *launcher_ptr = userdata_ptr;

    // TODO(kaeser@gubbe.ch): Appears we do encounter this scenario. File this
    // as a bug and fix it.
    // BS_ASSERT(bs_ptr_set_contains(launcher_ptr->created_windows_ptr, window_ptr));

    bool rv = bs_ptr_set_insert(launcher_ptr->mapped_windows_ptr, window_ptr);
    if (!rv) bs_log(BS_ERROR, "Failed bs_ptr_set_insert(%p)", window_ptr);

    // FIXME: redraw_tile(dock_app_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Callback for then a window from the launched subprocess is unmapped.
 *
 * Removes the window from the set of "mapped" windows, and will then redraw
 * the launcher tile to reflect potential status changes.
 *
 * @param userdata_ptr        Points to the @ref wlmaker_launcher_t.
 * @param subprocess_handle_ptr
 * @param window_ptr
 */
void _wlmaker_launcher_handle_window_unmapped(
    void *userdata_ptr,
    __UNUSED__ wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    wlmtk_window_t *window_ptr)
{
    wlmaker_launcher_t *launcher_ptr = userdata_ptr;

    bs_ptr_set_erase(launcher_ptr->mapped_windows_ptr, window_ptr);

    // FIXME: redraw_tile(dock_app_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Callback for then a window from the launched subprocess is destroyed.
 *
 * Removes the window from the set of "created" windows, and will then redraw
 * the launcher tile to reflect potential status changes.
 *
 * @param userdata_ptr        Points to the @ref wlmaker_launcher_t.
 * @param subprocess_handle_ptr
 * @param window_ptr
 */
void _wlmaker_launcher_handle_window_destroyed(
    void *userdata_ptr,
    __UNUSED__ wlmaker_subprocess_handle_t *subprocess_handle_ptr,
    wlmtk_window_t *window_ptr)
{
    wlmaker_launcher_t *launcher_ptr = userdata_ptr;

    bs_ptr_set_erase(launcher_ptr->created_windows_ptr, window_ptr);

    // FIXME: redraw_tile(dock_app_ptr);
}

/* == Unit tests =========================================================== */

static void test_create_from_plist(bs_test_t *test_ptr);

const bs_test_case_t wlmaker_launcher_test_cases[] = {
    { 1, "create_from_plist", test_create_from_plist },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises plist parser. */
void test_create_from_plist(bs_test_t *test_ptr)
{
    static const wlmtk_tile_style_t style = { .size = 96 };
    static const char *plist_ptr =
        "{CommandLine = \"a\"; Icon = \"chrome-48x48.png\";}";
    wlmaker_server_t server = {};

    wlmcfg_dict_t *dict_ptr = wlmcfg_dict_from_object(
        wlmcfg_create_object_from_plist_string(plist_ptr));
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dict_ptr);
    wlmaker_launcher_t *launcher_ptr = wlmaker_launcher_create_from_plist(
        &server, &style, dict_ptr);
    wlmcfg_dict_unref(dict_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, launcher_ptr);

    BS_TEST_VERIFY_STREQ(test_ptr, "a", launcher_ptr->cmdline_ptr);
    BS_TEST_VERIFY_STREQ(
        test_ptr, "chrome-48x48.png", launcher_ptr->icon_path_ptr);

    wlmaker_launcher_destroy(launcher_ptr);
}

/* == End of launcher.c ==================================================== */
