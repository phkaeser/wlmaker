/* ========================================================================= */
/**
 * @file wlr_log.c
 *
 * @copyright
 * Copyright (c) 2026 Philipp Kaeser <kaeser@gubbe.ch>
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

#include "wlr_log.h"

#include <libbase/libbase.h>
#include <limits.h>
#include <regex.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <wlr/util/log.h>

/* == Declarations ========================================================= */

static void _wlm_util_wlr_log_to_bs(
    enum wlr_log_importance importance,
    const char *fmt,
    va_list args);

/* == Data ================================================================= */

/** Compiled regular expression for extracting file & line no. from wlr_log. */
static regex_t                _wlm_util_wlr_log_regex;

/** Regular expression string for extracting file & line no. from wlr_log. */
static const char             *_wlm_util_wlr_log_regex_string =
    "^\\[([^\\:]+)\\:([0-9]+)\\]\\ ";

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlm_util_wlr_log_init(enum wlr_log_importance verbosity)
{
    int rv = regcomp(
        &_wlm_util_wlr_log_regex,
        _wlm_util_wlr_log_regex_string,
        REG_EXTENDED);
    if (0 != rv) {
        char err_buf[512];
        regerror(rv, &_wlm_util_wlr_log_regex, err_buf, sizeof(err_buf));
        bs_log(BS_ERROR, "Failed compiling regular expression: %s", err_buf);
        return false;
    }

    wlr_log_init(verbosity, _wlm_util_wlr_log_to_bs);
    return true;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Wraps the wlr_log calls on bs_log.
 *
 * @param importance
 * @param fmt
 * @param args
 */
static void _wlm_util_wlr_log_to_bs(
    enum wlr_log_importance importance,
    const char *fmt,
    va_list args)
{
    bs_log_severity_t severity = BS_DEBUG;

    switch (importance) {
    case WLR_SILENT:  // Fall-through to DEBUG severity.
    case WLR_DEBUG: severity = BS_DEBUG; break;
    case WLR_INFO: severity = BS_INFO; break;
    case WLR_ERROR: severity = BS_ERROR; break;
    default: severity = BS_INFO; break;
    }

    if (!bs_will_log(severity)) return;

    // Log to buffer. Ignores overflows.
    char buf[BS_LOG_MAX_BUF_SIZE];
    vsnprintf(buf, sizeof(buf), fmt, args);

    regmatch_t matches[4];
    if (0 != regexec(&_wlm_util_wlr_log_regex, buf, 4, &matches[0], 0) ||
        matches[0].rm_so != 0 ||
        !(matches[0].rm_eo >= 6) ||  // Minimum "[x:1] ".
        matches[1].rm_so != 1 ||
        !(matches[2].rm_so > 2) ||
        matches[3].rm_so != -1) {
        bs_log(severity, "%s (wlr_log unexpected format!)", buf);
        return;
    }

    buf[matches[1].rm_eo] = '\0';
    buf[matches[2].rm_eo] = '\0';
    uint64_t line_no = 0;
    bs_strconvert_uint64(&buf[matches[2].rm_so], &line_no, 10);
    line_no = BS_MIN((uint64_t)INT_MAX, line_no);

    bs_log_write(severity, &buf[matches[1].rm_so], (int)line_no, "%s",
        &buf[matches[0].rm_eo]);
}

/* == End of wlr_log.c ===================================================== */
