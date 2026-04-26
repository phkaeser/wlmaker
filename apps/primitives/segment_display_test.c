/* ========================================================================= */
/**
 * @file cairo_segment_display_test.c
 *
 * @copyright
 * Copyright 2023 Google LLC
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
#include <stddef.h>

#include "segment_display.h"

/** Unit tests. */
const bs_test_set_t *test_sets[] = {
    &wlm_cairo_segment_display_test_set,
    NULL,
};

#if !defined(TEST_DATA_DIR)
/** Directory root for looking up test data. See `bs_test_resolve_path`. */
#define TEST_DATA_DIR "./"
#endif  // TEST_DATA_DIR

/** Main program, runs the unit tests. */
int main(int argc, const char **argv)
{
    const bs_test_param_t params = {
        .test_data_dir_ptr   = TEST_DATA_DIR
    };
    return bs_test_sets(test_sets, argc, argv, &params);
}

/* == End of segment_display_test.c ======================================== */
