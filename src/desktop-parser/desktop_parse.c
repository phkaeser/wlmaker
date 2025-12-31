/* ========================================================================= */
/**
 * @file desktop_parse.c
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

#include <libbase/libbase.h>
#include <ini.h>

#include "parse.h"

/* == Declarations ========================================================= */

/* == Data ================================================================= */

/* == Exported methods ===================================================== */

/* == Local (static) methods =============================================== */

static int handler(
    __UNUSED__ void *ud_ptr,
    const char *section_ptr,
    const char *name_ptr,
    const char *value_ptr)
{
    bs_log(BS_ERROR, "FIXME: section %s - %s = %s",
           section_ptr, name_ptr, value_ptr);
    return 1;
}

/* == Unit Tests =========================================================== */

int main(__UNUSED__ int argc, __UNUSED__ char **argv)
{
    if (ini_parse("/etc/default/keyboard", handler, NULL)) {
        bs_log(BS_ERROR, "Failed");
        return EXIT_FAILURE;
    }

    bs_log(BS_ERROR, "Success.");
    return 0;
}

/* == End of desktop_parse.c =============================================== */
