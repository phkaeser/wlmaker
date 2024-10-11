/* ========================================================================= */
/**
 * @file corner.h
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
#ifndef __CORNER_H__
#define __CORNER_H__

/** Forward declaration: State of hot corner monitor. */
typedef struct _wlmaker_corner_t wlmaker_corner_t;

#include "cursor.h"
#include "server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates the hot-corner handler.
 *
 * @param hot_corner_config_dict_ptr
 * @param wl_event_loop_ptr
 * @param wlr_output_layout_ptr
 * @param cursor_ptr
 * @param server_ptr
 *
 * @return Pointer to the hot-corner monitor.
 */
wlmaker_corner_t *wlmaker_corner_create(
    wlmcfg_dict_t *hot_corner_config_dict_ptr,
    struct wl_event_loop *wl_event_loop_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    wlmaker_cursor_t *cursor_ptr,
    wlmaker_server_t *server_ptr);

/**
 * Destroys the hot-corner handler.
 *
 * @param corner_ptr
 */
void wlmaker_corner_destroy(wlmaker_corner_t *corner_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmaker_corner_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __CORNER_H__ */
/* == End of corner.h ====================================================== */
