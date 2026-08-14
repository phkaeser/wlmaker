/* ========================================================================= */
/**
 * @file wlr_log.h
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
#ifndef __WLMAKER_UTIL_WLR_LOG_H__
#define __WLMAKER_UTIL_WLR_LOG_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

enum wlr_log_importance;  // IWYU pragma: keep

/**
 * Initializes wlroots logging to redirect into bs_log().
 *
 * @param verbosity
 */
bool wlm_util_wlr_log_init(enum wlr_log_importance verbosity);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // __WLMAKER_UTIL_WLR_LOG_H__
/* == End of wlr_log.h ===================================================== */
