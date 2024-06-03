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


/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_launcher_t *wlmaker_launcher_create()
{
    wlmaker_launcher_t *launcher_ptr = logged_calloc(
        1, sizeof(wlmaker_launcher_t));
    if (NULL == launcher_ptr) return NULL;

    return launcher_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_launcher_destroy(wlmaker_launcher_t *launcher_ptr)
{
    free(launcher_ptr);
}

/* == Local (static) methods =============================================== */

/* == End of launcher.c ================================================== */
