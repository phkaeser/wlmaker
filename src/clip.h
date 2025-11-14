/* ========================================================================= */
/**
 * @file clip.h
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
 *
 * Creates the wlmaker clip. A view, with server-bound surfaces, that act
 * as a workspace-local dock and a workspace pager.
 *
 * Corresponding Window Maker documentation:
 * http://www.windowmaker.org/docs/guidedtour/clip.html
 */
#ifndef __CLIP_H__
#define __CLIP_H__

#include <libbase/plist.h>
#include <libbase/libbase.h>

/** Forward definition: Clip handle. */
typedef struct _wlmaker_clip_t wlmaker_clip_t;

#include "config.h"
#include "server.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates the Clip. Needs the server to be up with workspaces running.
 *
 * @param server_ptr
 * @param state_dict_ptr
 * @param style_ptr
 *
 * @return Pointer to the Clip handle, or NULL on error.
 */
wlmaker_clip_t *wlmaker_clip_create(
    wlmaker_server_t *server_ptr,
    bspl_dict_t *state_dict_ptr,
    const wlmaker_config_style_t *style_ptr);

/**
 * Destroys the Clip.
 *
 * @param clip_ptr
 */
void wlmaker_clip_destroy(wlmaker_clip_t *clip_ptr);

/** Unit test set. */
extern const bs_test_set_t wlmaker_clip_test_set;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __CLIP_H__ */
/* == End of clip.h ======================================================== */
