/* ========================================================================= */
/**
 * @file files.c
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

#include "files.h"

#include <stdbool.h>
#include <stdlib.h>
#include <basedir.h>
#include <libbase/libbase.h>

/* == Declarations ========================================================= */

/** State of the files module. */
struct _wlmaker_files_t {
    /** Handle for libxdg-basedir. */
    xdgHandle                 xdg_handle;

    /** Directory name to use beneath the XDG_<any>_HOME paths. */
    char                      *dirname_ptr;
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_files_t *wlmaker_files_create(const char *dirname_ptr)
{
    wlmaker_files_t *files_ptr = logged_calloc(1, sizeof(wlmaker_files_t));
    if (NULL == files_ptr) return NULL;

    files_ptr->dirname_ptr = logged_strdup(dirname_ptr);
    if (NULL == files_ptr->dirname_ptr) goto error;

    if (NULL == xdgInitHandle(&files_ptr->xdg_handle)) goto error;

    return files_ptr;

error:
    wlmaker_files_destroy(files_ptr);
    return NULL;

}

/* ------------------------------------------------------------------------- */
void wlmaker_files_destroy(wlmaker_files_t *files_ptr)
{
    if (NULL != files_ptr->dirname_ptr) {
        free(files_ptr->dirname_ptr);
        files_ptr->dirname_ptr = NULL;
    }

    xdgWipeHandle(&files_ptr->xdg_handle);
    free(files_ptr);
}

/* ------------------------------------------------------------------------- */
char *wlmaker_files_xdg_config_fname(
    wlmaker_files_t *files_ptr,
    const char *fname_ptr)
{
    const char *config_home_ptr = xdgConfigHome(&files_ptr->xdg_handle);
    if (NULL == config_home_ptr) return NULL;

    return bs_strdupf(
        "%s/%s/%s",
        config_home_ptr,
        files_ptr->dirname_ptr,
        fname_ptr);
}

/* == Unit Tests =========================================================== */

static void _wlmaker_files_test_builders(bs_test_t *test_ptr);

/** Unit test cases. */
static const bs_test_case_t wlmaker_files_test_cases[] = {
    { true, "builders", _wlmaker_files_test_builders },
    BS_TEST_CASE_SENTINEL()
};

const bs_test_set_t wlmaker_files_test_set = BS_TEST_SET(
    true, "files", wlmaker_files_test_cases);

/* ------------------------------------------------------------------------- */
/** Tests building filenames relative to XDG base directories. */
void _wlmaker_files_test_builders(bs_test_t *test_ptr)
{
    wlmaker_files_t *files_ptr = wlmaker_files_create("wlmaker");
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, files_ptr);

    char *f = wlmaker_files_xdg_config_fname(files_ptr, "state.plist");
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, f);
    BS_TEST_VERIFY_STRMATCH(test_ptr, f, "/wlmaker/state.plist$");
    free(f);

    wlmaker_files_destroy(files_ptr);
}

/* == End of files.c ======================================================= */
