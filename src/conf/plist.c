/* ========================================================================= */
/**
 * @file plist.c
 *
 * @copyright
 * Copyright 2024 Google LLC
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

#include "plist.h"

#include "grammar.h"
#include "analyzer.h"

/* == Declarations ========================================================= */

/* == Exported methods ===================================================== */

#if 0
    yyscan_t scanner;
    yylex_init(&scanner);

    YY_BUFFER_STATE buf_state;
    buf_state = yy_scan_string("(){}", scanner);
    yyparse(scanner);
    yy_delete_buffer(buf_state, scanner);

    yylex_destroy(scanner);
    return EXIT_SUCCESS;
#endif

/* == Local (static) methods =============================================== */

/* == Unit tests =========================================================== */

const bs_test_case_t wlmcfg_plist_test_cases[] = {
    { 0, NULL, NULL }
};

/* == End of plist.c ======================================================= */
