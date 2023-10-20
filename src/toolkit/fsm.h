/* ========================================================================= */
/**
 * @file fsm.h
 * Event-driven finite state machine.
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
#ifndef __WLTMK_FSM_H__
#define __WLTMK_FSM_H__

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Forward declaration. */
typedef struct _wlmtk_fsm_t wlmtk_fsm_t;

/** State machine definition. */
typedef struct {
    /** State before receiving the event. */
    int                       state;
    /** Event. */
    int                       event;
    /** Upon having (state, event): State to transition to. */
    int                       to_state;
    /** Handler for the activity at (state, event). */
    bool                      (*handler)(wlmtk_fsm_t *fsm_ptr, void *ud_ptr);
} wlmtk_fsm_transition_t;

/** Finite state machine. State. */
struct _wlmtk_fsm_t {
    /** The transitions table. */
    const wlmtk_fsm_transition_t *transitions;
    /** Current state. */
    int                       state;
};

/** Sentinel element for state transition table. */
#define WLMTK_FSM_TRANSITION_SENTINEL {         \
        .state = -1,                            \
        .event = -1,                            \
        .to_state = -1,                         \
        .handler = NULL,                        \
    }

/**
 * Initializes the finite-state machine.
 *
 * @param fsm_ptr
 * @param transitions
 * @param initial_state
 */
void wlmtk_fsm_init(
    wlmtk_fsm_t *fsm_ptr,
    const wlmtk_fsm_transition_t *transitions,
    int initial_state);

/**
 * Handles an event for the finite-state machine.
 *
 * Will search for the transition matching (current state, event) and call the
 * associate handler.
 *
 * @param fsm_ptr
 * @param event
 * @param ud_ptr
 *
 * @return If a matching transition was found: The return value of the
 *     associated handler (or true, if no handler was given). Otherwise,
 *     returns false.
 */
bool wlmtk_fsm_event(
    wlmtk_fsm_t *fsm_ptr,
    int event,
    void *ud_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLTMK_FSM_H__ */
/* == End of fsm.h ====================================================== */
