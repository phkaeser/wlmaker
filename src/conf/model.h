/* ========================================================================= */
/**
 * @file model.h
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
#ifndef __WLMCFG_MODEL_H__
#define __WLMCFG_MODEL_H__

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef struct _wlmcfg_object_t wlmcfg_object_t;
typedef struct _wlmcfg_string_t wlmcfg_string_t;

void wlmcfg_object_destroy(wlmcfg_object_t *object_ptr);

wlmcfg_string_t *wlmcfg_string_create(const char *value_ptr);


#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMCFG_MODEL_H__ */
/* == End of model.h ================================================== */
