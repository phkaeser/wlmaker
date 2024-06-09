/* ========================================================================= */
/**
 * @file launcher.c
 * Copyright (c) 2024 by Philipp Kaeser <kaeser@gubbe.ch>
 */

#include "launcher.h"

#include <libbase/libbase.h>
#include <toolkit/toolkit.h>

#include "conf/decode.h"
#include "conf/plist.h"

/* == Declarations ========================================================= */

/** State of a launcher. */
struct _wlmaker_launcher_t {
    /** The launcher is derived from a @ref wlmtk_tile_t. */
    wlmtk_tile_t              super_tile;

    /** Commandline to launch the associated application. */
    char                      *cmdline_ptr;
    /** Path to the icon. */
    char                      *icon_path_ptr;
};

/** Plist descroptor for a launcher. */
static const wlmcfg_desc_t _wlmaker_launcher_plist_desc[] = {
    WLMCFG_DESC_STRING(
        "CommandLine", true, wlmaker_launcher_t, cmdline_ptr, ""),
    WLMCFG_DESC_STRING(
        "Icon", true, wlmaker_launcher_t, icon_path_ptr, ""),
    WLMCFG_DESC_SENTINEL(),
};

/* == Data ================================================================= */

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_launcher_t *wlmaker_launcher_create(
    wlmaker_server_t *server_ptr,
    const wlmtk_tile_style_t *style_ptr)
{
    wlmaker_launcher_t *launcher_ptr = logged_calloc(
        1, sizeof(wlmaker_launcher_t));
    if (NULL == launcher_ptr) return NULL;

    if (!wlmtk_tile_init(&launcher_ptr->super_tile,
                         style_ptr,
                         server_ptr->env_ptr)) {

        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_tile_element(&launcher_ptr->super_tile), true);

    return launcher_ptr;
}

/* ------------------------------------------------------------------------- */
wlmaker_launcher_t *wlmaker_launcher_create_from_plist(
    wlmaker_server_t *server_ptr,
    const wlmtk_tile_style_t *style_ptr,
    wlmcfg_dict_t *dict_ptr)
{
    wlmaker_launcher_t *launcher_ptr = logged_calloc(
        1, sizeof(wlmaker_launcher_t));
    if (NULL == launcher_ptr) return NULL;

    if (!wlmtk_tile_init(&launcher_ptr->super_tile,
                         style_ptr,
                         server_ptr->env_ptr)) {

        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_tile_element(&launcher_ptr->super_tile), true);

    // FIXME: Create icon buffer.

#if 0
    // FIXME == Resolution should be done by caller.
    // Resolve to a full path, and verify the file exists.
    char full_path[PATH_MAX];
    char *path_ptr = bs_file_resolve_and_lookup_from_paths(
        image_path_ptr, lookup_paths_ptr, 0, full_path);
    if (NULL == path_ptr) {
        bs_log(BS_ERROR | BS_ERRNO,
               "Failed bs_file_resolve_and_lookup_from_paths(%s, ...).",
               icon_path_ptr);
        wlmtk_image_destroy(image_ptr);
        return NULL;
    }
#endif


    if (!wlmcfg_decode_dict(
            dict_ptr, _wlmaker_launcher_plist_desc, launcher_ptr)) {
        bs_log(BS_ERROR, "Failed to create launcher from plist dict.");
        wlmaker_launcher_destroy(launcher_ptr);
        return NULL;
    }

    return launcher_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_launcher_destroy(wlmaker_launcher_t *launcher_ptr)
{
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

/* == Unit tests =========================================================== */

static void test_create_destroy(bs_test_t *test_ptr);
static void test_create_from_plist(bs_test_t *test_ptr);

const bs_test_case_t wlmaker_launcher_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 1, "create_from_plist", test_create_from_plist },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises ctor and dtor of @ref wlmaker_launcher_t. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_tile_style_t style = { .size = 96 };
    wlmaker_server_t server = {};
    wlmaker_launcher_t *launcher_ptr = wlmaker_launcher_create(
        &server, &style);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, launcher_ptr);

    BS_TEST_VERIFY_EQ(
        test_ptr,
        &launcher_ptr->super_tile,
        wlmaker_launcher_tile(launcher_ptr));

    wlmaker_launcher_destroy(launcher_ptr);
}

/* ------------------------------------------------------------------------- */
/** Exercises plist parser. */
void test_create_from_plist(bs_test_t *test_ptr)
{
    static const wlmtk_tile_style_t style = { .size = 96 };
    static const char *plist_ptr = "{CommandLine = \"a\"; Icon = \"b\";}";
    wlmaker_server_t server = {};

    wlmcfg_dict_t *dict_ptr = wlmcfg_dict_from_object(
        wlmcfg_create_object_from_plist_string(plist_ptr));
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, dict_ptr);
    wlmaker_launcher_t *launcher_ptr = wlmaker_launcher_create_from_plist(
        &server, &style, dict_ptr);
    wlmcfg_dict_unref(dict_ptr);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, launcher_ptr);

    BS_TEST_VERIFY_STREQ(test_ptr, "a", launcher_ptr->cmdline_ptr);
    BS_TEST_VERIFY_STREQ(test_ptr, "b", launcher_ptr->icon_path_ptr);

    wlmaker_launcher_destroy(launcher_ptr);
}

/* == End of launcher.c ==================================================== */
