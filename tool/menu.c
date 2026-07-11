/* ========================================================================= */
/**
 * @file menu.c
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser (kaeser@gubbe.ch)
 * Copyright (c) 2026 Google LLC and Philipp Kaeser
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

#include "menu.h"

#include <desktop-parser/desktop-parser.h>
#include <basedir.h>
#include <fts.h>
#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "item.h"

/* == Declarations ========================================================= */

struct wlmtool_menu;

static bool _wlmtool_menu_add_themes_path(
    struct wlmtool_menu *menu_ptr,
    const char *path_ptr);
static bool _wlmtool_menu_add_themes_file(
    const char *resolved_path_ptr,
    const FTSENT *ftsent_ptr,
    void *ud_ptr);

static bool _wlmtool_menu_add_desktop_path(
    struct wlmtool_menu *menu_ptr,
    const char *path_ptr,
    const char *locale_ptr);
static bool _wlmtool_menu_add_desktop_file(
    const char *resolved_path_ptr,
    const FTSENT *ftsent_ptr,
    void *ud_ptr);

static char *_wlmtool_menu_themes_name_from_plist_file(const char *path_ptr);
static const char *_wlmtool_menu_category_from_entry(
    const struct desktop_entry *desktop_entry_ptr);

/** Lookup table for category names */
struct _category_translation {
    /** Category that is used in a `.desktop` file. */
    const char *desktop_category_ptr;
    /** Category name to use in the menu. */
    const char *menu_category_ptr;
};

/** Arguments to @ref _wlmtool_menu_add_desktop_file */
struct _wlmtool_menu_add_desktop_file_arg {
    /** Parser for the `.desktop` files. */
    struct desktop_parser     *desktop_parser_ptr;
    /** Root menu. */
    struct wlmtool_menu       *root_menu_ptr;
    /** Chosen locale. */
    const char                *locale_ptr;
};

/* == Data ================================================================= */

/** Command to load a theme file. */
static const char* _wlmtool_action_load_theme_from_file = "ThemeLoadFromFile";

/** Recognized categories. */
// TODO(kaeser@gubbe.ch): Internationalize this.
static const struct _category_translation _wlmtool_menu_category_table[] = {
    { "AudioVideo", "Audio & Video" },
    { "Video", "Video" },
    { "Development", "Development" },
    { "Education", "Education" },
    { "Game", "Game" },
    { "Graphics", "Graphics" },
    { "Network", "Network" },
    { "Office", "Office" },
    { "Science", "Science" },
    { "Settings", "Settings" },
    { "System", "System" },
    { "Utility", "Utility" },

    // Reserved categories.
    { "Screensaver", "Screensaver" },
    { "TrayIcon", "Tray Icon" },
    { "Applet", "Applet" },
    { "Shell", "Shell" },
    { NULL, NULL },
};

/** Category to use for any entry that is not categorized. */
// TODO(kaeser@gubbe.ch): Internationalize.
static const char *_wlmtool_menu_category_other_ptr = "Other";

/** Command to use for terminal. */
// TODO(kaeser@gubbe.ch): This should be configurable.
static const char *_wlmtool_menu_terminal_command = "/usr/bin/foot";

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bspl_array_t *wlmtool_menu_generate_appearance(const char *path_ptr)
{
    struct wlmtool_menu *root_menu_ptr = wlmtool_menu_create("Appearance");
    if (NULL == root_menu_ptr) return NULL;

    struct wlmtool_menu *menu_ptr = NULL;
    menu_ptr = wlmtool_menu_get_or_create_submenu(
        root_menu_ptr, "System Themes");
    if (NULL == menu_ptr) {
        wlmtool_item_destroy(wlmtool_item_from_menu(root_menu_ptr));
        return NULL;
    }

    if (NULL != path_ptr) {
        if (!_wlmtool_menu_add_themes_path(root_menu_ptr, path_ptr)) {
            wlmtool_item_destroy(wlmtool_item_from_menu(root_menu_ptr));
            return NULL;
        }
    } else {
        xdgHandle xdg_handle;
        if (NULL == xdgInitHandle(&xdg_handle)) {
            wlmtool_item_destroy(wlmtool_item_from_menu(root_menu_ptr));
            return NULL;
        }

        bs_ptr_set_t *dir_set_ptr = bs_ptr_set_create();
        const char* const *dirs_ptr = xdgDataDirectories(&xdg_handle);
        for (; NULL != dirs_ptr && NULL != *dirs_ptr; ++dirs_ptr) {
            bs_ptr_set_insert(dir_set_ptr, (void*)*dirs_ptr);
            char *p = bs_strdupf("%s/%s", *dirs_ptr, "wlmaker/Themes");
            if (NULL == p) {
                wlmtool_item_destroy(wlmtool_item_from_menu(root_menu_ptr));
                return NULL;
            }
            bool rv = _wlmtool_menu_add_themes_path(menu_ptr, p);
            free(p);
            if (!rv) {
                wlmtool_item_destroy(wlmtool_item_from_menu(root_menu_ptr));
                return NULL;
            }
        }

        menu_ptr = wlmtool_menu_get_or_create_submenu(
            root_menu_ptr, "User Themes");
        char *p = bs_strdupf("%s/%s", xdgDataHome(&xdg_handle), "wlmaker/Themes");
        if (NULL == p) {
            wlmtool_item_destroy(wlmtool_item_from_menu(root_menu_ptr));
            return NULL;
        }
        bool rv = _wlmtool_menu_add_themes_path(menu_ptr, p);
        free(p);
        p = NULL;
        if (!rv) {
            wlmtool_item_destroy(wlmtool_item_from_menu(root_menu_ptr));
            return NULL;
        }

        menu_ptr = wlmtool_menu_get_or_create_submenu(
            root_menu_ptr, "User Themes");
        p = bs_strdupf("%s/%s", xdgConfigHome(&xdg_handle), "wlmaker/Themes");
        if (NULL == p) {
            wlmtool_item_destroy(wlmtool_item_from_menu(root_menu_ptr));
            return NULL;
        }
        rv = _wlmtool_menu_add_themes_path(menu_ptr, p);
        free(p);
        if (!rv) {
            wlmtool_item_destroy(wlmtool_item_from_menu(root_menu_ptr));
            return NULL;
        }

        bs_ptr_set_destroy(dir_set_ptr);
        xdgWipeHandle(&xdg_handle);
    }

    bspl_array_t *array_ptr = wlmtool_item_create_array(
        wlmtool_item_from_menu(root_menu_ptr));
    wlmtool_item_destroy(wlmtool_item_from_menu(root_menu_ptr));
    return array_ptr;
}

/* ------------------------------------------------------------------------- */
bspl_array_t *wlmtool_menu_generate_applications(
    const char *path_ptr,
    const char *locale_ptr)
{
    struct wlmtool_menu *menu_ptr = wlmtool_menu_create("Applications");
    if (NULL == menu_ptr) return NULL;

    if (NULL != path_ptr) {
        if (!_wlmtool_menu_add_desktop_path(menu_ptr, path_ptr, locale_ptr)) {
            wlmtool_item_destroy(wlmtool_item_from_menu(menu_ptr));
            return NULL;
        }
    } else {
        // No path provided. Iterate over XDG data directories.
        xdgHandle xdg_handle;
        if (NULL == xdgInitHandle(&xdg_handle)) return NULL;
        const char* const *dirs_ptr = xdgSearchableDataDirectories(&xdg_handle);
        for (; NULL != dirs_ptr && NULL != *dirs_ptr; ++dirs_ptr) {
            char *p = bs_strdupf("%s/applications", *dirs_ptr);
            if (NULL == p) return NULL;
            bool rv = _wlmtool_menu_add_desktop_path(menu_ptr, p, locale_ptr);
            if (!rv) {
                bs_log(BS_ERROR, "Failed to add desktop files from %s", p);
            }
            free(p);
            if (!rv) return NULL;
        }
        xdgWipeHandle(&xdg_handle);
    }

    bspl_array_t *array_ptr = wlmtool_item_create_array(
        wlmtool_item_from_menu(menu_ptr));
    wlmtool_item_destroy(wlmtool_item_from_menu(menu_ptr));
    return array_ptr;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Convenience wrapper to add themes file from `path_ptr` to `menu_ptr`. */
bool _wlmtool_menu_add_themes_path(
    struct wlmtool_menu *menu_ptr,
    const char *path_ptr)
{
    if (bs_file_walk_tree(
            path_ptr, "*.plist", S_IFREG, false,
            _wlmtool_menu_add_themes_file, menu_ptr)) return true;
    bs_log(BS_ERROR, "Failed to add Themes files from %s", path_ptr);
    return false;
}

/* ------------------------------------------------------------------------- */
/** Adds a menu entry for the plist Themes file at `resolved_path_ptr`. */
bool _wlmtool_menu_add_themes_file(
    const char *resolved_path_ptr,
    __UNUSED__ const FTSENT *ftsent_ptr,
    void *ud_ptr)
{
    struct wlmtool_menu *menu_ptr = ud_ptr;

    // Creates a "Theme" menu item from the plist file.
    char *n = _wlmtool_menu_themes_name_from_plist_file(resolved_path_ptr);
    if (NULL == n) return false;
    struct wlmtool_item *item_ptr = wlmtool_file_action_create(
        n, "ThemeLoadFromFile", resolved_path_ptr);
    free(n);
    if (NULL == item_ptr) return false;

    if (wlmtool_menu_add_item(menu_ptr, item_ptr)) return true;

    bs_log(BS_INFO, "Skipping already added file \"%s\"", resolved_path_ptr);
    wlmtool_item_destroy(item_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
/** Convenience wrapper to add desktop files from `path_ptr` to `menu_ptr`. */
bool _wlmtool_menu_add_desktop_path(
    struct wlmtool_menu *menu_ptr,
    const char *path_ptr,
    const char *locale_ptr)
{
    struct _wlmtool_menu_add_desktop_file_arg arg = {
        .desktop_parser_ptr = desktop_parser_create(locale_ptr),
        .root_menu_ptr = menu_ptr,
        .locale_ptr = locale_ptr
    };
    if (NULL == arg.desktop_parser_ptr) return false;

    bool rv = bs_file_walk_tree(
        path_ptr, "*.desktop", S_IFREG, false,
        _wlmtool_menu_add_desktop_file, &arg);
    if (!rv) bs_log(BS_ERROR, "Failed to add desktop files from %s", path_ptr);
    desktop_parser_destroy(arg.desktop_parser_ptr);
    return rv;
}

/* ------------------------------------------------------------------------- */
/** Adds a menu entry for a desktop file. */
bool _wlmtool_menu_add_desktop_file(
    const char *resolved_path_ptr,
    __UNUSED__ const FTSENT *ftsent_ptr,
    void *ud_ptr)
{
    struct _wlmtool_menu_add_desktop_file_arg *arg_ptr = ud_ptr;
    struct desktop_entry de = {};
    char *cmd_ptr = NULL;

    int error_line = desktop_parser_file_to_entry(
        arg_ptr->desktop_parser_ptr,
        resolved_path_ptr,
        &de);
    if (0 != error_line) {
        bs_log(BS_WARNING,
               "Failed desktop_parser_file_to_entry(%p, %s, %p) at line %d",
               arg_ptr->desktop_parser_ptr,
               resolved_path_ptr,
               &de, error_line);
        return false;
    }

    if (DESKTOP_ENTRY_TYPE_APPLICATION != de.type ||
        de.no_display ||
        de.hidden) {
        desktop_parser_entry_release(&de);
        return true;
    }

    const char *c = _wlmtool_menu_category_from_entry(&de);
    if (NULL == c) {
        bs_log(BS_WARNING, "Failed to lookup category for %s",
               resolved_path_ptr);
        goto error;
    }

    struct wlmtool_menu *cat_menu_ptr = wlmtool_menu_get_or_create_submenu(
        arg_ptr->root_menu_ptr, c);
    if (NULL == cat_menu_ptr) goto error;

    if (NULL == de.try_exec_ptr && NULL == de.exec_ptr) {
        bs_log(BS_WARNING, "Missing 'Exec' and 'TryExec' in desktop file %s",
               resolved_path_ptr);
        goto error;
    }
    if (de.terminal) {
        cmd_ptr = bs_strdupf(
            "%s %s",
            _wlmtool_menu_terminal_command,
            de.try_exec_ptr ? de.try_exec_ptr : de.exec_ptr);
    } else {
        cmd_ptr = logged_strdup(
            de.try_exec_ptr ? de.try_exec_ptr : de.exec_ptr);
    }
    if (NULL == cmd_ptr) goto error;

    if (NULL == de.name_ptr) {
        bs_log(BS_WARNING, "Missing 'Name' in desktop file %s",
               resolved_path_ptr);
        goto error;
    }

    const char *a[] = { de.name_ptr, "ShellExecute", cmd_ptr, NULL };
    struct wlmtool_item *item_ptr = wlmtool_entry_create(resolved_path_ptr, a);
    if (NULL == item_ptr) goto error;
    if (!wlmtool_menu_add_item(cat_menu_ptr, item_ptr)) {
        bs_log(BS_WARNING, "Duplicate? Failed to add entry for %s",
               resolved_path_ptr);
        goto error;
    }

    free(cmd_ptr);
    cmd_ptr = NULL;
    desktop_parser_entry_release(&de);
    return true;

error:
    if (NULL != cmd_ptr) free(cmd_ptr);
    desktop_parser_entry_release(&de);
    return false;
}

/* ------------------------------------------------------------------------- */
/**
 * Returns a string holding name to show in the menu for the file action.
 *
 * This will attempt to load the file as a PList, and extract a string value
 * keyed "Name" from the toplevel dict. If `path_ptr` is not a plist file, or
 * does not hold a Plist dict as toplevel object, it returns NULL.
 * If there is no "Name" entry in the toplevel dict, the function returns the
 * basename of `path_ptr`.
 *
 * @param path_ptr
 *
 * @return An allocated string, or NULL on error. The allocated string must be
 *     released by calling free().
 */
char *_wlmtool_menu_themes_name_from_plist_file(const char *path_ptr)
{
    // Load the plist, and lookup the value of "Name" in the toplevel dict.
    bspl_object_t *object_ptr = bspl_create_object_from_plist_file(path_ptr);
    if (NULL == object_ptr) return NULL;

    bspl_dict_t *dict_ptr = bspl_dict_from_object(object_ptr);
    if (NULL == dict_ptr) {
        bs_log(BS_WARNING, "Not a Plist dict in %s", path_ptr);
        bspl_object_unref(object_ptr);
        return NULL;
    }

    char *name_ptr = NULL;
    const char *plist_name_ptr = bspl_dict_get_string_value(dict_ptr, "Name");
    if (NULL != plist_name_ptr) name_ptr = logged_strdup(plist_name_ptr);
    bspl_object_unref(object_ptr);
    if (NULL != name_ptr) return name_ptr;

    // No such value in the plist file. Use the file's basename.
    char *p = logged_strdup(path_ptr);
    if (NULL == p) return false;
    char *basename_ptr = basename(p);
    name_ptr = logged_strdup(basename_ptr);
    free(p);
    return name_ptr;
}

/* ------------------------------------------------------------------------- */
/** Looks up the category name for a desktop's file menu entry. */
const char *_wlmtool_menu_category_from_entry(
    const struct desktop_entry *desktop_entry_ptr)
{
    if (NULL != desktop_entry_ptr->category_ptrs) {
        for (char **c = desktop_entry_ptr->category_ptrs; *c; ++c) {
            for (const struct _category_translation *ct =
                     &_wlmtool_menu_category_table[0];
                 ct->desktop_category_ptr != NULL;
                 ++ct) {
                if (0 == strcmp(*c, ct->desktop_category_ptr)) {
                    return ct->menu_category_ptr;
                }
            }
        }
    }
    return _wlmtool_menu_category_other_ptr;
}

/* == Unit Tests =========================================================== */

static void _wlmtool_menu_test_generate_themes(bs_test_t *test_ptr);
static void _wlmtool_menu_test_generate_applications(bs_test_t *test_ptr);

/** Test cases for the menu generator. */
static const bs_test_case_t   _wlmtool_menu_test_cases[] = {
    { true, "generate_themes", _wlmtool_menu_test_generate_themes },
    { true, "applications", _wlmtool_menu_test_generate_applications },
    BS_TEST_CASE_SENTINEL(),
};

const bs_test_set_t           wlmtool_menu_test_set = BS_TEST_SET(
    true, "menu", _wlmtool_menu_test_cases);

/* ------------------------------------------------------------------------- */
/** Verifies themes menu generation from the test files. */
void _wlmtool_menu_test_generate_themes(bs_test_t *test_ptr)
{
    struct wlmtool_menu *m = wlmtool_menu_create("Appearance");
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, m);

    const char *p = bs_test_data_path(test_ptr, "Themes");
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmtool_menu_add_themes_path(m, p));
    bspl_array_t *a = wlmtool_item_create_array(wlmtool_item_from_menu(m));
    wlmtool_item_destroy(wlmtool_item_from_menu(m));

    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, a);

    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, a);
    BS_TEST_VERIFY_EQ(test_ptr, 3, bspl_array_size(a));
    BS_TEST_VERIFY_STREQ(test_ptr, "Appearance", bspl_array_string_value_at(a, 0));

    const char *load = _wlmtool_action_load_theme_from_file;
    bspl_array_t *aa = bspl_array_from_object(bspl_array_at(a, 1));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, aa);
    BS_TEST_VERIFY_EQ_OR_RETURN(test_ptr, 3, bspl_array_size(aa));
    BS_TEST_VERIFY_STREQ(test_ptr, "ThemeA", bspl_array_string_value_at(aa, 0));
    BS_TEST_VERIFY_STREQ(test_ptr, load, bspl_array_string_value_at(aa, 1));

    aa = bspl_array_from_object(bspl_array_at(a, 2));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, aa);
    BS_TEST_VERIFY_EQ_OR_RETURN(test_ptr, 3, bspl_array_size(aa));
    BS_TEST_VERIFY_STREQ(test_ptr, "ThemeB", bspl_array_string_value_at(aa, 0));
    BS_TEST_VERIFY_STREQ(test_ptr, load, bspl_array_string_value_at(aa, 1));

    bspl_array_unref(a);
}

/* ------------------------------------------------------------------------- */
/** Tests generation of the 'Applications' menu. */
void _wlmtool_menu_test_generate_applications(bs_test_t *test_ptr)
{
    const char *p = bs_test_data_path(test_ptr, "%s", "");
    bspl_array_t *a = wlmtool_menu_generate_applications(p, NULL);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, a);

    BS_TEST_VERIFY_EQ(test_ptr, 2, bspl_array_size(a));
    BS_TEST_VERIFY_STREQ(
        test_ptr, "Applications", bspl_array_string_value_at(a, 0));

    bspl_array_t *aa = bspl_array_from_object(bspl_array_at(a, 1));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, aa);
    BS_TEST_VERIFY_EQ_OR_RETURN(test_ptr, 2, bspl_array_size(aa));
    BS_TEST_VERIFY_STREQ(
        test_ptr, "System", bspl_array_string_value_at(aa, 0));

    bspl_array_t *aaa = bspl_array_from_object(bspl_array_at(aa, 1));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, aaa);
    BS_TEST_VERIFY_EQ_OR_RETURN(test_ptr, 3, bspl_array_size(aaa));
    BS_TEST_VERIFY_STREQ(
        test_ptr, "WaylandMaker", bspl_array_string_value_at(aaa, 0));
    BS_TEST_VERIFY_STREQ(
        test_ptr, "ShellExecute", bspl_array_string_value_at(aaa, 1));
    BS_TEST_VERIFY_STREQ(
        test_ptr, "/usr/bin/foot ./wlmaker",
        bspl_array_string_value_at(aaa, 2));

    bspl_array_unref(a);
}

/* == End of menu.c ======================================================== */
