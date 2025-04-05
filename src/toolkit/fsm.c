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

#include <toolkit/fsm.h>

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
            bool rv = true;
            if (NULL != transition_ptr->handler) {
                rv = transition_ptr->handler(fsm_ptr, ud_ptr);
            }
            fsm_ptr->state = transition_ptr->to_state;
            return rv;
        }
    }
    return false;
}

/* == Unit tests =========================================================== */

static void test_event(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_fsm_test_cases[] = {
    { 1, "event", test_event },
    { 0, NULL, NULL }
};

/** Test handler for the FSM unit test: Sets the bool to true. */
static bool test_fsm_handler(__UNUSED__ wlmtk_fsm_t *fsm_ptr, void *ud_ptr) {
    *((bool*)ud_ptr) = true;
    return true;
}
/** Test transition table for the FSM unit test. */
static const wlmtk_fsm_transition_t test_transitions[] = {
    { 1, 100, 2, test_fsm_handler },
    { 2, 101, 3, NULL },
    WLMTK_FSM_TRANSITION_SENTINEL
};

/* ------------------------------------------------------------------------- */
/** Tests FSM. */
void test_event(bs_test_t *test_ptr)
{
    wlmtk_fsm_t fsm;
    bool called = false;

    wlmtk_fsm_init(&fsm, test_transitions, 1);
    BS_TEST_VERIFY_EQ(test_ptr, 1, fsm.state);

    // (1, 100) should trigger call to handler and move to (2).
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_fsm_event(&fsm, 100, &called));
    BS_TEST_VERIFY_EQ(test_ptr, 2, fsm.state);
    BS_TEST_VERIFY_TRUE(test_ptr, called);
    called = false;

    // (2, 100) is not defined. return false.
    BS_TEST_VERIFY_FALSE(test_ptr, wlmtk_fsm_event(&fsm, 100, &called));

    // (2, 101) is defined. No handler == no crash. moves to (3).
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_fsm_event(&fsm, 101, &called));
    BS_TEST_VERIFY_EQ(test_ptr, 3, fsm.state);
    BS_TEST_VERIFY_FALSE(test_ptr, called);
}

/* == End of fsm.c ========================================================= */
