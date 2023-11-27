/* ========================================================================= */
/**
 * @file input.h
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
#ifndef __WLMTK_INPUT_H__
#define __WLMTK_INPUT_H__

// BTN_LEFT, BTN_RIGHT, ...
#include <linux/input-event-codes.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration: Button event. */
typedef struct _wlmtk_button_event_t wlmtk_button_event_t;
/** Forward declaration: Environment. */
typedef struct _wlmtk_env_t wlmtk_env_t;

/** Button state. */
typedef enum {
    WLMTK_BUTTON_DOWN,
    WLMTK_BUTTON_UP,
    WLMTK_BUTTON_CLICK,
    WLMTK_BUTTON_DOUBLE_CLICK,
} wlmtk_button_event_type_t;

/** Button events. */
struct _wlmtk_button_event_t {
    /** Button for which the event applies: linux/input-event-codes.h */
    uint32_t                  button;
    /** Type of the event: DOWN, UP, ... */
    wlmtk_button_event_type_t type;
    /** Time of the button event, in milliseconds. */
    uint32_t                  time_msec;
};

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_INPUT_H__ */
/* == End of input.h ======================================================= */
