/* ========================================================================= */
/**
 * @file backtrace.h
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
#ifndef __BACKTRACE_H__
#define __BACKTRACE_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Sets up signal handlers to catch issues and log a backtrace.
 *
 * @param filename_ptr        path name of the executable file; if it is NULL
 *                            the library will try system-specific path names.
 *                            If not NULL, FILENAME must point to a permanent
 *                            buffer.
 *
 * @return true on success.
 */
bool wlmaker_backtrace_setup(const char *filename_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __BACKTRACE_H__ */
/* == End of backtrace.h =================================================== */
