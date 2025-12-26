/* ========================================================================= */
/**
 * @file gen_menu.c
 *
 * @copyright
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

#include "gen_menu.h"

#include <basedir.h>
#include <desktop-parser/desktop-parser.h>
#include <ftw.h>
#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <locale.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

struct desktop_parser;

/* == Declarations ========================================================= */

/** Category to use for any entry that is not categorized. */
// TODO(kaeser@gubbe.ch): Internationalize.
static const char *category_other_ptr = "Other";

/** The category menu. */
struct category_menu {
    /** Node within the overall menu tree. */
    bs_avltree_node_t         avlnode;
    /** Name of the category. */
    char                      *category_ptr;
    /** Tree for holding the menu items. A tree for uniqueness + sorting. */
    bs_avltree_t              *entry_tree_ptr;
};

/** A menu entry. */
struct menu_entry {
    /** Node within @ref category_menu::entry_tree_ptr. */
    bs_avltree_node_t         avlnode;
    /** The parsed .desktop entry information. */
    struct desktop_entry      entry;
    /** Already encoded as plist. */
    bspl_array_t              *array_ptr;
};

/** Lookup table for category names */
struct category_translation {
    /** Category that is used in a `.desktop` file. */
    const char *desktop_category_ptr;
    /** Category name to use in the menu. */
    const char *menu_category_ptr;
};

static bool add_entry_to_menu_tree(
    bs_avltree_t *menu_tree_ptr,
    struct menu_entry *menu_entry_ptr);

static struct category_menu *menu_create(const char *category_ptr);
static int menu_cmp(const bs_avltree_node_t *node_ptr,
                    const void *key_ptr);
static void menu_destroy(bs_avltree_node_t *node_ptr);

static struct menu_entry *entry_create(struct desktop_parser *parser,
                                       const char *path_ptr);
static int entry_cmp(const bs_avltree_node_t *node_ptr,
                     const void *key_ptr);
static void entry_destroy(bs_avltree_node_t *node_ptr);

static const char *category_from_entry(const struct menu_entry *menu_entry_ptr);
static bspl_array_t *array_from_entry(const struct desktop_entry *entry_ptr);
static bspl_array_t *array_from_tree(bs_avltree_t *menu_tree_ptr);

/* == Data ================================================================= */

/** Recognized categories. */
// TODO(kaeser@gubbe.ch): Internationalize this.
static const struct category_translation category_table[] = {
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

/* == Exported methods ===================================================== */

/** Global: Parser. Used in @ref ftw_callback. */
struct desktop_parser *global_parser;
/** Global: The menu tree. Used in @ref ftw_callback. */
bs_avltree_t *global_menu_tree_ptr;

/* ------------------------------------------------------------------------- */
/** Attempts to parse & add each entry from the tree. */
// TODO(kaeser@gubbe.ch): Replace this with a tree-walker that takes a
// userdata_ptr, and can apply a fnmatch().
int ftw_callback(
    const char *path_ptr,
    __UNUSED__ const struct stat *statbuf_ptr,
    __UNUSED__ int typeflag)
{
    if (typeflag != FTW_F) return 0;

    struct menu_entry *entry_ptr = entry_create(global_parser, path_ptr);
    if (NULL != entry_ptr) {
        add_entry_to_menu_tree(global_menu_tree_ptr, entry_ptr);
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
bspl_array_t *wlmaker_menu_generate(
    const char *locale_ptr,
    const char *path_ptr)
{
    struct desktop_parser *parser = desktop_parser_create(locale_ptr);
    if (NULL == parser) {
        bs_log(BS_ERROR, "Failed desktop_parser_create(\"%s\")",
               setlocale(LC_MESSAGES, NULL));
        return NULL;
    }

    bs_avltree_t *menu_tree_ptr = bs_avltree_create(menu_cmp, menu_destroy);
    if (NULL == menu_tree_ptr) {
        desktop_parser_destroy(parser);
        return NULL;
    }

    if (NULL != path_ptr) {
        struct stat statbuf;
        if (0 != stat(path_ptr, &statbuf)) {
            bs_log(BS_ERROR | BS_ERRNO, "Failed stat(\"%s\", %p)",
                   path_ptr, &statbuf);
            return NULL;
        }
        if (S_ISREG(statbuf.st_mode)) {
            struct menu_entry *entry_ptr = entry_create(parser, path_ptr);
            add_entry_to_menu_tree(menu_tree_ptr, entry_ptr);
        } else if (S_ISDIR(statbuf.st_mode)) {
            global_parser = parser;
            global_menu_tree_ptr = menu_tree_ptr;
            ftw(path_ptr, ftw_callback, 1024);
        } else {
            bs_log(BS_ERROR, "Not a file nor directory: \"%s\"", path_ptr);
        }
    } else {
        // No path provided. Iterate over XDG data directories.
        xdgHandle xdghandle;
        if (NULL == xdgInitHandle(&xdghandle)) return NULL;
        const char* const *dirs_ptr = xdgSearchableDataDirectories(&xdghandle);
        for (; NULL != dirs_ptr && NULL != *dirs_ptr; ++dirs_ptr) {
            char*path_ptr = bs_strdupf("%s/applications", *dirs_ptr);
            if (NULL != path_ptr) {
                global_parser = parser;
                global_menu_tree_ptr = menu_tree_ptr;
                ftw(path_ptr, ftw_callback, 1024);
            }
        }

        xdgWipeHandle(&xdghandle);
    }

    bspl_array_t *array_ptr = array_from_tree(menu_tree_ptr);
    bs_avltree_destroy(menu_tree_ptr);
    desktop_parser_destroy(parser);
    return array_ptr;
}


/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Adds the entry to the category menu. Creates a category menu, if needed. */
bool add_entry_to_menu_tree(
    bs_avltree_t *menu_tree_ptr,
    struct menu_entry *menu_entry_ptr)
{
    const char *c = category_from_entry(menu_entry_ptr);
    if (NULL == c) return false;

    struct category_menu *menu_ptr = NULL;
    bs_avltree_node_t *category_node_ptr = bs_avltree_lookup(menu_tree_ptr, c);
    if (NULL == category_node_ptr) {
        menu_ptr = menu_create(c);
        if (NULL == menu_ptr) return false;
        bs_avltree_insert(
            menu_tree_ptr,
            menu_ptr->category_ptr,
            &menu_ptr->avlnode,
            true);
    } else {
        menu_ptr = BS_CONTAINER_OF(
            category_node_ptr, struct category_menu, avlnode);
    }

    bool rv = bs_avltree_insert(
        menu_ptr->entry_tree_ptr,
        menu_entry_ptr->entry.name_ptr,
        &menu_entry_ptr->avlnode,
        false);
    if (!rv) {
        bs_log(BS_WARNING,
               "Failed to add entry for \"%s\" (in \"%s\"). Duplicate?",
               menu_entry_ptr->entry.name_ptr,
               c);
        // We won't remove the menu. Worst case, there's an empty menu...
    }

    return rv;
}

/* ------------------------------------------------------------------------- */
/** ctor for the menu. */
struct category_menu *menu_create(const char *category_ptr)
{
    struct category_menu *menu_ptr = logged_calloc(
        1, sizeof(struct category_menu));
    if (NULL == menu_ptr) return NULL;

    menu_ptr->category_ptr = logged_strdup(category_ptr);
    if (NULL == menu_ptr->category_ptr) {
        menu_destroy(&menu_ptr->avlnode);
        return NULL;
    }

    menu_ptr->entry_tree_ptr = bs_avltree_create(entry_cmp, entry_destroy);
    if (NULL == menu_ptr->entry_tree_ptr) {
        menu_destroy(&menu_ptr->avlnode);
        return NULL;
    }

    return menu_ptr;
}

/* ------------------------------------------------------------------------- */
/** comparator for the menu. */
int menu_cmp(const bs_avltree_node_t *node_ptr,
             const void *key_ptr)
{
    const struct category_menu *menu_ptr = BS_CONTAINER_OF(
        node_ptr, const struct category_menu, avlnode);
    return strcmp(menu_ptr->category_ptr, (const char *)key_ptr);
}

/* ------------------------------------------------------------------------- */
/** Dtor for the menu. */
void menu_destroy(bs_avltree_node_t *node_ptr)
{
    struct category_menu *menu_ptr = BS_CONTAINER_OF(
        node_ptr, struct category_menu, avlnode);

    if (NULL != menu_ptr->entry_tree_ptr) {
        bs_avltree_destroy(menu_ptr->entry_tree_ptr);
        menu_ptr->entry_tree_ptr = NULL;
    }

    if (NULL != menu_ptr->category_ptr) {
        free(menu_ptr->category_ptr);
        menu_ptr->category_ptr = NULL;
    }


    free(menu_ptr);
}

/* ------------------------------------------------------------------------- */
/** Ctor for the entry. */
struct menu_entry *entry_create(struct desktop_parser *parser,
                                const char *path_ptr)
{
    struct menu_entry *entry_ptr = logged_calloc(1, sizeof(struct menu_entry));
    if (NULL == entry_ptr) return NULL;

    int rv = desktop_parser_file_to_entry(parser, path_ptr, &entry_ptr->entry);
    if (0 != rv) {
        bs_log(BS_ERROR, "Failed desktop_parser_file_to_entry"
               "(%p, \"%s\", %p) at line %d",
               parser, path_ptr, &entry_ptr->entry, rv);
        entry_destroy(&entry_ptr->avlnode);
        return NULL;
    }

    if (DESKTOP_ENTRY_TYPE_APPLICATION != entry_ptr->entry.type ||
        NULL == entry_ptr->entry.name_ptr ||
        entry_ptr->entry.no_display ||
        entry_ptr->entry.hidden) {
        entry_destroy(&entry_ptr->avlnode);
        return NULL;
    }


    entry_ptr->array_ptr = array_from_entry(&entry_ptr->entry);
    if (NULL == entry_ptr->array_ptr) {
        entry_destroy(&entry_ptr->avlnode);
        return NULL;
    }

    return entry_ptr;
}

/* ------------------------------------------------------------------------- */
/** Comparator for the entry. */
int entry_cmp(const bs_avltree_node_t *node_ptr,
              const void *key_ptr)
{
    const struct menu_entry *entry_ptr = BS_CONTAINER_OF(
        node_ptr, const struct menu_entry, avlnode);
    return strcmp(entry_ptr->entry.name_ptr, (const char*)key_ptr);
}

/* ------------------------------------------------------------------------- */
/** Dtor for the entry. */
void entry_destroy(bs_avltree_node_t *node_ptr)
{
    struct menu_entry *entry_ptr = BS_CONTAINER_OF(
        node_ptr, struct menu_entry, avlnode);
    desktop_parser_entry_release(&entry_ptr->entry);
    free(entry_ptr);
}

/* ------------------------------------------------------------------------- */
/** Looks up the category name for the menu entry. */
const char *category_from_entry(const struct menu_entry *menu_entry_ptr)
{
    if (NULL != menu_entry_ptr->entry.category_ptrs) {
        for (char **c = menu_entry_ptr->entry.category_ptrs; *c; ++c) {
            for (const struct category_translation *ct = &category_table[0];
                 ct->desktop_category_ptr != NULL;
                 ++ct) {
                if (0 == strcmp(*c, ct->desktop_category_ptr)) {
                    return ct->menu_category_ptr;
                }
            }
        }
    }
    return category_other_ptr;
}

/* ------------------------------------------------------------------------- */
/** Returns the plist array for the menu entry. */
bspl_array_t *array_from_entry(const struct desktop_entry *entry_ptr)
{
    if (NULL == entry_ptr->try_exec_ptr && NULL == entry_ptr->exec_ptr) {
        return NULL;
    }

    bspl_array_t *array_ptr = bspl_array_create();
    if (NULL == array_ptr) return NULL;

    bspl_string_t *n = bspl_string_create(entry_ptr->name_ptr);
    bspl_string_t *a = bspl_string_create("ShellExecute");
    bspl_string_t *e = bspl_string_create(
        entry_ptr->try_exec_ptr ?
        entry_ptr->try_exec_ptr :
        entry_ptr->exec_ptr);

    if (!bspl_array_push_back(array_ptr, bspl_object_from_string(n)) ||
        !bspl_array_push_back(array_ptr, bspl_object_from_string(a)) ||
        !bspl_array_push_back(array_ptr, bspl_object_from_string(e)) ||
        3 != bspl_array_size(array_ptr)) {
        // Failed to build array. Zap it, return error.
        bspl_array_unref(array_ptr);
        array_ptr = NULL;
    }

    bspl_string_unref(e);
    bspl_string_unref(a);
    bspl_string_unref(n);
    return array_ptr;
}

/* ------------------------------------------------------------------------- */
/** Creates a plist array describing the full menu. */
bspl_array_t *array_from_tree(bs_avltree_t *menu_tree_ptr)
{
    bspl_array_t *array_ptr = BS_ASSERT_NOTNULL(bspl_array_create());

    bspl_array_push_back(
        array_ptr,
        // TODO(kaeser@gubbe.ch): Internationalize.
        bspl_object_from_string(bspl_string_create("Applications")));

    for (bs_avltree_node_t *cnode_ptr = bs_avltree_min(menu_tree_ptr);
         NULL != cnode_ptr;
         cnode_ptr = bs_avltree_node_next(menu_tree_ptr, cnode_ptr)) {
        struct category_menu *menu_ptr = BS_CONTAINER_OF(
            cnode_ptr, struct category_menu, avlnode);
        // Skip empty categories.
        if (0 >= bs_avltree_size(menu_ptr->entry_tree_ptr)) continue;

        bspl_array_t *menu_array_ptr = BS_ASSERT_NOTNULL(bspl_array_create());
        bspl_array_push_back(
            menu_array_ptr,
            bspl_object_from_string(bspl_string_create(
                                        menu_ptr->category_ptr)));
        for (bs_avltree_node_t *enode_ptr = bs_avltree_min(
                 menu_ptr->entry_tree_ptr);
             enode_ptr != NULL;
             enode_ptr = bs_avltree_node_next(
                 menu_ptr->entry_tree_ptr, enode_ptr)) {
            struct menu_entry *entry_ptr = BS_CONTAINER_OF(
                enode_ptr, struct menu_entry, avlnode);
            bspl_array_push_back(
                menu_array_ptr,
                bspl_object_from_array(entry_ptr->array_ptr));
        }
        bspl_array_push_back(
            array_ptr,
            bspl_object_from_array(menu_array_ptr));
    }

    return array_ptr;
}

/* == End of gen_menu.c ==================================================== */
