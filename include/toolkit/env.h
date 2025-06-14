/* ========================================================================= */
/**
 * @file env.h
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
#ifndef __WLMTK_ENV_H__
#define __WLMTK_ENV_H__

/** Forward declaration: Environment. */
typedef struct _wlmtk_env_t wlmtk_env_t;

/** Forward declaration. */
struct wlr_cursor;
/** Forward declaration. */
struct wlr_xcursor_manager;

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Cursor types. */
typedef enum {
    /** Default. */
    WLMTK_CURSOR_DEFAULT,
    /** Resizing, southern border. */
    WLMTK_CURSOR_RESIZE_S,
    /** Resizing, south-eastern corner. */
    WLMTK_CURSOR_RESIZE_SE,
    /** Resizing, south-western corner. */
    WLMTK_CURSOR_RESIZE_SW,
} wlmtk_env_cursor_t;

/**
 * Creates an environment state from the cursor.
 *
 * @param wlr_cursor_ptr
 * @param wlr_xcursor_manager_ptr
 *
 * @return An environment state or NULL on error.
 */
wlmtk_env_t *wlmtk_env_create(
    struct wlr_cursor *wlr_cursor_ptr,
    struct wlr_xcursor_manager *wlr_xcursor_manager_ptr);

/**
 * Destroys the environment state.
 *
 * @param env_ptr
 */
void wlmtk_env_destroy(wlmtk_env_t *env_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_ENV_H__ */
/* == End of env.h ========================================================= */
