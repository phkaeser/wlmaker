/* ========================================================================= */
/**
 * @file version.c
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

#include "version.h"

/* == Data ================================================================= */

#if !defined(WLMAKER_VERSION_MAJOR) || !defined(WLMAKER_VERSION_MINOR)
#error "WLMAKER_VERSION_MAJOR or WLMAKER_VERSION_MINOR... not defined!"
#else
// Patch level is optional.
#if defined(WLMAKER_VERSION_PATCH)
const char *wlm_util_version =
    WLMAKER_VERSION_MAJOR "." WLMAKER_VERSION_MINOR "." WLMAKER_VERSION_PATCH;
#else
const char *wlm_util_version =
    WLMAKER_VERSION_MAJOR "." WLMAKER_VERSION_MINOR;
#endif  // !defined(WLMAKER_VERSION_PATCH)
#endif  // !defined(WLMAKER_VERSION_MAJOR) || !defined(WLMAKER_VERSION_MINOR)

#if !defined(WLMAKER_VERSION_FULL)
#error "WLMAKER_VERSION_FULL not defined!"
#else
const char *wlm_util_version_full = WLMAKER_VERSION_FULL;
#endif  //  !defined(WLMAKER_VERSION_FULL)

/* == End of version.c ===================================================== */
