/* ========================================================================= */
/**
 * @file fsm.c
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

#include "fsm.h"

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
void wlmtk_fsm_init(
    wlmtk_fsm_t *fsm_ptr,
    const wlmtk_fsm_transition_t *transitions,
    int initial_state)
{
    fsm_ptr->transitions = transitions;
    fsm_ptr->state = initial_state;
}

/* ------------------------------------------------------------------------- */
bool wlmtk_fsm_event(
    wlmtk_fsm_t *fsm_ptr,
    int event,
    void *ud_ptr)
{
    for (const wlmtk_fsm_transition_t *transition_ptr = fsm_ptr->transitions;
         0 <= transition_ptr->state;
         ++transition_ptr) {
        if (transition_ptr->state == fsm_ptr->state &&
            transition_ptr->event == event) {
            if (NULL != transition_ptr->handler) {
                bool rv = transition_ptr->handler(fsm_ptr, ud_ptr);
                fsm_ptr->state = transition_ptr->to_state;
                return rv;
            } else {
                return true;
            }
        }
    }
    return false;
}

/* == End of fsm.c ========================================================= */
