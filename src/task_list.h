/* ========================================================================= */
/**
 * @file task_list.h
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
#ifndef __TASK_LIST_H__
#define __TASK_LIST_H__

/** Forward definition: Task list handle. */
typedef struct _wlmaker_task_list_t wlmaker_task_list_t;

#include "server.h"
#include "workspace.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// TODO(kaeser@gubbe.ch): Move this to wlmaker_keyboard.
// Taskswitch:
// modifer = ALT, and PRESSED is TAB enables it, and will switch focus
// to the next-open View.
//
// - TAB will switch focus one further
// - Shift-TAB will switch focus one back
// (- Cursor left/right will also switch focus further/one back)
// - Esc will restore focus of the view that has it before switcher.
//
// will remain active until:
// - ALT is released
// - any key outside the handled keys are pressed
// - mouse is presset outside the task switch window
// - workspace is switched.
//
// Means: It needs a means of...
// - grabbing keyboard focus and holding it until release.
// - grabbing mouse focus and holding it until release.
// - not losing focus and top-of-stack until release.
// => Should be atop each layer -> have it's own layer? or OVERLAY ?
//   (likely go with overlay)
//
// => means, this is like a "layer view" except the extra focus constraints.

/**
 * Creates a task list for the server.
 *
 * Will allocate the task list handle, and register signal handlers so the task
 * list reacts to `task_list_enabled_event` and `task_list_disabled_event` of
 * the `wlmaker_server_t`.
 *
 * @param server_ptr
 *
 * @return The task list handle or NULL on error. Must be released by calling
 *     @ref wlmaker_task_list_destroy.
 */
wlmaker_task_list_t *wlmaker_task_list_create(
    wlmaker_server_t *server_ptr);

/**
 * Destroys the task list, as created by @ref wlmaker_task_list_create.
 *
 * @param task_list_ptr
 */
void wlmaker_task_list_destroy(
    wlmaker_task_list_t *task_list_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __TASK_LIST_H__ */
/* == End of task_list.h =================================================== */
