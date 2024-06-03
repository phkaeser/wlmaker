/* ========================================================================= */
/**
 * @file launcher.c
 * Copyright (c) 2024 by Philipp Kaeser <kaeser@gubbe.ch>
 */

#include "launcher.h"

#include <libbase/libbase.h>
#include <toolkit/toolkit.h>

/* == Declarations ========================================================= */

/** State of a launcher. */
struct _wlmaker_launcher_t {
    /** The launcher is derived from a @ref wlmtk_tile_t. */
    wlmtk_tile_t              super_tile;
};

/* == Data ================================================================= */

/** Temporary: Hard-coded style of launcher. To be derived from config. */
static const wlmtk_tile_style_t launcher_style = {
    .fill = {
        .type = WLMTK_STYLE_COLOR_DGRADIENT,
        .param = { .hgradient = { .from = 0xffa6a6b6,.to = 0xff515561 }}
    },
    .size = 96,
    .bezel_width = 3
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_launcher_t *wlmaker_launcher_create(wlmaker_server_t *server_ptr)
{
    wlmaker_launcher_t *launcher_ptr = logged_calloc(
        1, sizeof(wlmaker_launcher_t));
    if (NULL == launcher_ptr) return NULL;

    if (!wlmtk_tile_init(&launcher_ptr->super_tile,
                         &launcher_style,
                         server_ptr->env_ptr)) {

        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_tile_element(&launcher_ptr->super_tile), true);

    return launcher_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_launcher_destroy(wlmaker_launcher_t *launcher_ptr)
{
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

const bs_test_case_t wlmaker_launcher_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises ctor and dtor of @ref wlmaker_launcher_t. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmaker_server_t server = {};
    wlmaker_launcher_t *launcher_ptr = wlmaker_launcher_create(&server);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, launcher_ptr);

    BS_TEST_VERIFY_EQ(
        test_ptr,
        &launcher_ptr->super_tile,
        wlmaker_launcher_tile(launcher_ptr));

    wlmaker_launcher_destroy(launcher_ptr);
}

/* == End of launcher.c ==================================================== */
