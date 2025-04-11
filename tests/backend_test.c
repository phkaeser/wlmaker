/* ========================================================================= */
/**
 * @file backend_test.c
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

#include <backend/backend.h>

/** Backend unit tests. */
const bs_test_set_t backend_tests[] = {
    { 1, "backend", wlmbe_backend_test_cases },
    { 0, NULL, NULL }
};

/** Main program, runs the unit tests. */
int main(int argc, const char **argv)
{
    const bs_test_param_t params = {};
    return bs_test(backend_tests, argc, argv, &params);
}

/* == End of backend_test.c ================================================ */
