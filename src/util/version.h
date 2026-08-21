/* ========================================================================= */
/**
 * @file version.h
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
#ifndef __WLMAKER_VERSION_H__
#define __WLMAKER_VERSION_H__

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Version string: `MAJOR.MINOR[.PATCH]` */
extern const char *wlm_util_version;

/** Full version string: May include git tag and version. */
extern const char *wlm_util_version_full;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // __WLMAKER_VERSION_H__
/* == End of version.h ===================================================== */
