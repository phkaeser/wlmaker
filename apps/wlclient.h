/* ========================================================================= */
/**
 * @file wlclient.h
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
#ifndef __WLCLIENT_H__
#define __WLCLIENT_H__

#include <inttypes.h>
#include <stdbool.h>
#include <libbase/libbase.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef struct _wlclient_t wlclient_t;

wlclient_t *wlclient_create(void);
void wlclient_destroy(wlclient_t *wlclient_ptr);

void wlclient_add_timer(
    wlclient_t *wlclient_ptr,
    uint64_t msec,
    void (*callback)(wlclient_t *wlclient_ptr, void *ud_ptr),
    void *ud_ptr);

bool wlclient_icon_supported(wlclient_t *wlclient_ptr);
bs_gfxbuf_t *wlclient_icon_gfxbuf(wlclient_t *wlclient_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLCLIENT_H__ */
/* == End of wlclient.h ================================================== */
