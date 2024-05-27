/* ========================================================================= */
/**
 * @file plist.h
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
#ifndef __WLMCFG_PLIST_H__
#define __WLMCFG_PLIST_H__

#include <libbase/libbase.h>

#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Parses the plist string `buf_ptr` and returns the de-serialized object.
 *
 * @param buf_ptr
 *
 * @return The de-serialized object, or NULL on error.
 */
wlmcfg_object_t *wlmcfg_create_object_from_plist_string(const char *buf_ptr);

/**
 * Parses the plist data with given size and returns the de-serialized object.
 *
 * @param data_ptr
 * @param data_size
 *
 * @return The de-serialized object, or NULL on error.
 */
wlmcfg_object_t *wlmcfg_create_object_from_plist_data(
    const uint8_t *data_ptr, size_t data_size);

/**
 * Parses the file `fname_ptr` and returns the de-serialized object.
 *
 * @param fname_ptr
 *
 * @return The de-serialized object, or NULL on error.
 */
wlmcfg_object_t *wlmcfg_create_object_from_plist_file(const char *fname_ptr);

/** Unit tests for the plist parser. */
extern const bs_test_case_t wlmcfg_plist_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMCFG_PLIST_H__ */
/* == End of plist.h ======================================================= */
