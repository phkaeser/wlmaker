/* ========================================================================= */
/**
 * @file container.c
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

#include "container.h"

#include "util.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

static struct wlr_scene_node *element_create_scene_node(
    wlmtk_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr);
static void element_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr);
static void element_get_pointer_area(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr);
static bool element_pointer_motion(
    wlmtk_element_t *element_ptr,
    double x, double y,
    uint32_t time_msec);
static bool element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);

static void handle_wlr_scene_tree_node_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr);
static bool update_pointer_focus_at(
    wlmtk_container_t *container_ptr,
    double x,
    double y,
    uint32_t time_msec);
static void _wlmtk_container_update_layout(wlmtk_container_t *container_ptr);

/** Virtual method table for the container's super class: Element. */
static const wlmtk_element_vmt_t container_element_vmt = {
    .create_scene_node = element_create_scene_node,
    .get_dimensions = element_get_dimensions,
    .get_pointer_area = element_get_pointer_area,
    .pointer_motion = element_pointer_motion,
    .pointer_button = element_pointer_button,
};

/** Default virtual method table. Initializes non-abstract methods. */
static const wlmtk_container_vmt_t container_vmt = {
    .update_layout = _wlmtk_container_update_layout,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_container_init(
    wlmtk_container_t *container_ptr,
    wlmtk_env_t *env_ptr)
{
    BS_ASSERT(NULL != container_ptr);
    memset(container_ptr, 0, sizeof(wlmtk_container_t));
    container_ptr->vmt = container_vmt;

    if (!wlmtk_element_init(&container_ptr->super_element, env_ptr)) {
        return false;
    }
    container_ptr->orig_super_element_vmt = wlmtk_element_extend(
        &container_ptr->super_element, &container_element_vmt);

    return true;
}

/* ------------------------------------------------------------------------- */
bool wlmtk_container_init_attached(
    wlmtk_container_t *container_ptr,
    wlmtk_env_t *env_ptr,
    struct wlr_scene_tree *root_wlr_scene_tree_ptr)
{
    if (!wlmtk_container_init(container_ptr, env_ptr)) return false;

    container_ptr->super_element.wlr_scene_node_ptr =
        element_create_scene_node(
            &container_ptr->super_element, root_wlr_scene_tree_ptr);
    if (NULL == container_ptr->super_element.wlr_scene_node_ptr) {
        wlmtk_container_fini(container_ptr);
        return false;
    }

    BS_ASSERT(NULL != container_ptr->super_element.wlr_scene_node_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
wlmtk_container_vmt_t wlmtk_container_extend(
    wlmtk_container_t *container_ptr,
    const wlmtk_container_vmt_t *container_vmt_ptr)
{
    wlmtk_container_vmt_t orig_vmt = container_ptr->vmt;

    if (NULL != container_vmt_ptr->update_layout) {
        container_ptr->vmt.update_layout = container_vmt_ptr->update_layout;
    }
    return orig_vmt;
}

/* ------------------------------------------------------------------------- */
void wlmtk_container_fini(wlmtk_container_t *container_ptr)
{
    bs_dllist_node_t *dlnode_ptr;
    while (NULL != (dlnode_ptr = container_ptr->elements.head_ptr)) {
        wlmtk_element_t *element_ptr = wlmtk_element_from_dlnode(dlnode_ptr);
        wlmtk_container_remove_element(container_ptr, element_ptr);
        BS_ASSERT(container_ptr->elements.head_ptr != dlnode_ptr);
        wlmtk_element_destroy(element_ptr);
    }

    // For containers created with wlmtk_container_init_attached(): We also
    // need to remove references to the WLR scene tree.
    if (NULL != container_ptr->wlr_scene_tree_ptr) {
        BS_ASSERT(NULL == container_ptr->super_element.parent_container_ptr);
        wlr_scene_node_destroy(&container_ptr->wlr_scene_tree_ptr->node);
        container_ptr->wlr_scene_tree_ptr = NULL;
        container_ptr->super_element.wlr_scene_node_ptr = NULL;
    }

    wlmtk_element_fini(&container_ptr->super_element);
    memset(container_ptr, 0, sizeof(wlmtk_container_t));
}

/* ------------------------------------------------------------------------- */
void wlmtk_container_add_element(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr)
{
    BS_ASSERT(NULL == element_ptr->parent_container_ptr);
    BS_ASSERT(NULL == element_ptr->wlr_scene_node_ptr);

    bs_dllist_push_front(
        &container_ptr->elements,
        wlmtk_dlnode_from_element(element_ptr));
    wlmtk_element_set_parent_container(element_ptr, container_ptr);

    wlmtk_container_update_layout(container_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_container_add_element_before(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *reference_element_ptr,
    wlmtk_element_t *element_ptr)
{
    BS_ASSERT(NULL == element_ptr->parent_container_ptr);
    BS_ASSERT(NULL == element_ptr->wlr_scene_node_ptr);
    BS_ASSERT(
        NULL == reference_element_ptr ||
        container_ptr == reference_element_ptr->parent_container_ptr);

    if (NULL == reference_element_ptr) {
        bs_dllist_push_back(
            &container_ptr->elements,
            wlmtk_dlnode_from_element(element_ptr));
    } else {
        bs_dllist_insert_node_before(
            &container_ptr->elements,
            wlmtk_dlnode_from_element(reference_element_ptr),
            wlmtk_dlnode_from_element(element_ptr));
    }

    wlmtk_element_set_parent_container(element_ptr, container_ptr);
    wlmtk_container_update_layout(container_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_container_remove_element(
    wlmtk_container_t *container_ptr,
    wlmtk_element_t *element_ptr)
{
    BS_ASSERT(element_ptr->parent_container_ptr == container_ptr);

    wlmtk_element_set_parent_container(element_ptr, NULL);
    bs_dllist_remove(
        &container_ptr->elements,
        wlmtk_dlnode_from_element(element_ptr));

    if (container_ptr->left_button_element_ptr == element_ptr) {
        container_ptr->left_button_element_ptr = NULL;
    }

    wlmtk_container_update_layout(container_ptr);
    BS_ASSERT(element_ptr != container_ptr->pointer_focus_element_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_container_update_pointer_focus(wlmtk_container_t *container_ptr)
{
    if (NULL != container_ptr->super_element.parent_container_ptr) {
        wlmtk_container_update_pointer_focus(
            container_ptr->super_element.parent_container_ptr);
    } else {
        update_pointer_focus_at(
            container_ptr,
            container_ptr->super_element.last_pointer_x,
            container_ptr->super_element.last_pointer_y,
            container_ptr->super_element.last_pointer_time_msec);
    }
}

/* ------------------------------------------------------------------------- */
struct wlr_scene_tree *wlmtk_container_wlr_scene_tree(
    wlmtk_container_t *container_ptr)
{
    return container_ptr->wlr_scene_tree_ptr;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the superclass wlmtk_element_t::create_scene_node method.
 *
 * Creates the wlroots scene graph tree for the container, and will attach all
 * already-contained elements to the scene graph, as well.
 *
 * @param element_ptr
 * @param wlr_scene_tree_ptr
 *
 * @return Pointer to the scene graph API node.
 */
struct wlr_scene_node *element_create_scene_node(
    wlmtk_element_t *element_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);

    BS_ASSERT(NULL == container_ptr->wlr_scene_tree_ptr);
    container_ptr->wlr_scene_tree_ptr = wlr_scene_tree_create(
        wlr_scene_tree_ptr);
    BS_ASSERT(NULL != container_ptr->wlr_scene_tree_ptr);

    for (bs_dllist_node_t *dlnode_ptr = container_ptr->elements.head_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmtk_element_t *element_ptr = wlmtk_element_from_dlnode(dlnode_ptr);
        BS_ASSERT(NULL == element_ptr->wlr_scene_node_ptr);
        wlmtk_element_attach_to_scene_graph(element_ptr);
    }

    wlmtk_util_connect_listener_signal(
        &container_ptr->wlr_scene_tree_ptr->node.events.destroy,
        &container_ptr->wlr_scene_tree_node_destroy_listener,
        handle_wlr_scene_tree_node_destroy);
    return &container_ptr->wlr_scene_tree_ptr->node;
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the element's get_dimensions method: Return dimensions.
 *
 * @param element_ptr
 * @param left_ptr            Leftmost position. May be NULL.
 * @param top_ptr             Topmost position. May be NULL.
 * @param right_ptr           Rightmost position. Ma be NULL.
 * @param bottom_ptr          Bottommost position. May be NULL.
 */
void element_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);

    int left = 0, top = 0, right = 0, bottom = 0;
    for (bs_dllist_node_t *dlnode_ptr = container_ptr->elements.head_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmtk_element_t *element_ptr = wlmtk_element_from_dlnode(dlnode_ptr);

        int x_pos, y_pos;
        wlmtk_element_get_position(element_ptr, &x_pos, &y_pos);
        int x1, y1, x2, y2;
        wlmtk_element_get_dimensions(element_ptr, &x1, &y1, &x2, &y2);
        left = BS_MIN(left, x_pos + x1);
        top = BS_MIN(top, y_pos + y1);
        right = BS_MAX(right, x_pos + x2);
        bottom = BS_MAX(bottom, y_pos + y2);
    }

    if (NULL != left_ptr) *left_ptr = left;
    if (NULL != top_ptr) *top_ptr = top;
    if (NULL != right_ptr) *right_ptr = right;
    if (NULL != bottom_ptr) *bottom_ptr = bottom;
}

/* ------------------------------------------------------------------------- */
/**
 * Returns the minimal rectangle covering all element's pointer areas.
 *
 * @param element_ptr
 * @param left_ptr            Leftmost position. May be NULL.
 * @param top_ptr             Topmost position. May be NULL.
 * @param right_ptr           Rightmost position. Ma be NULL.
 * @param bottom_ptr          Bottommost position. May be NULL.
 */
void element_get_pointer_area(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);

    int left = 0, top = 0, right = 0, bottom = 0;
    for (bs_dllist_node_t *dlnode_ptr = container_ptr->elements.head_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmtk_element_t *element_ptr = wlmtk_element_from_dlnode(dlnode_ptr);

        int x_pos, y_pos;
        wlmtk_element_get_position(element_ptr, &x_pos, &y_pos);
        int x1, y1, x2, y2;
        wlmtk_element_get_pointer_area(element_ptr, &x1, &y1, &x2, &y2);
        left = BS_MIN(left, x_pos + x1);
        top = BS_MIN(top, y_pos + y1);
        right = BS_MAX(right, x_pos + x2);
        bottom = BS_MAX(bottom, y_pos + y2);
    }

    if (NULL != left_ptr) *left_ptr = left;
    if (NULL != top_ptr) *top_ptr = top;
    if (NULL != right_ptr) *right_ptr = right;
    if (NULL != bottom_ptr) *bottom_ptr = bottom;
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the element's motion method: Handle pointer moves.
 *
 * @param element_ptr
 * @param x
 * @param y
 * @param time_msec
 *
 * @return Whether this container has an element that accepts the emotion.
 */
bool element_pointer_motion(
    wlmtk_element_t *element_ptr,
    double x,
    double y,
    uint32_t time_msec)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);
    container_ptr->orig_super_element_vmt.pointer_motion(
        element_ptr, x, y, time_msec);

    return update_pointer_focus_at(container_ptr, x, y, time_msec);
}

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the element's pointer_button() method. Forwards it to the
 * element currently having pointer focus.
 *
 * @param element_ptr
 * @param button_event_ptr
 *
 * @return true if the button was handled.
 */
bool element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_container_t, super_element);
    bool accepted = false;

    // TODO: Generalize this for non-LEFT buttons.
    if (BTN_LEFT == button_event_ptr->button) {

        switch (button_event_ptr->type) {
        case WLMTK_BUTTON_DOWN:
            // Forward to the pointer focus element, if any. If it
            // was accepted: remember the element.
            if (NULL != container_ptr->pointer_focus_element_ptr) {
                accepted = wlmtk_element_pointer_button(
                    container_ptr->pointer_focus_element_ptr,
                    button_event_ptr);
                if (accepted) {
                    container_ptr->left_button_element_ptr =
                        container_ptr->pointer_focus_element_ptr;
                } else {
                    container_ptr->left_button_element_ptr = NULL;
                }
            }
            break;

        case WLMTK_BUTTON_UP:
            // Forward to the element that received the DOWN, if any.
            if (NULL != container_ptr->left_button_element_ptr) {
                accepted = wlmtk_element_pointer_button(
                    container_ptr->left_button_element_ptr,
                    button_event_ptr);
            }
            break;

        case WLMTK_BUTTON_CLICK:
        case WLMTK_BUTTON_DOUBLE_CLICK:
            // Will only be forwarded, if the element still (or again)
            // has pointer focus.
            if (NULL != container_ptr->left_button_element_ptr &&
                container_ptr->left_button_element_ptr ==
                container_ptr->pointer_focus_element_ptr) {
                accepted = wlmtk_element_pointer_button(
                    container_ptr->left_button_element_ptr,
                    button_event_ptr);
            }
            break;


        default:  // Uh, don't know about this...
            bs_log(BS_FATAL, "Unhandled button type %d",
                   button_event_ptr->type);
        }

        return accepted;
    }


    if (NULL == container_ptr->pointer_focus_element_ptr) return false;

    return wlmtk_element_pointer_button(
        container_ptr->pointer_focus_element_ptr,
        button_event_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handles the 'destroy' callback of wlr_scene_tree_ptr->node.
 *
 * Will also detach (but not destroy) each of the still-contained elements.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void handle_wlr_scene_tree_node_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_container_t *container_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_container_t, wlr_scene_tree_node_destroy_listener);

    container_ptr->wlr_scene_tree_ptr = NULL;
    for (bs_dllist_node_t *dlnode_ptr = container_ptr->elements.head_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmtk_element_t *element_ptr = wlmtk_element_from_dlnode(dlnode_ptr);
        // Will read the parent container's wlr_scene_tree_ptr == NULL.
        wlmtk_element_attach_to_scene_graph(element_ptr);
    }

    // Since this is a callback from the tree node dtor, the tree is going to
    // be destroyed. We are using this to reset the container's reference.
    wl_list_remove(&container_ptr->wlr_scene_tree_node_destroy_listener.link);
}

/* ------------------------------------------------------------------------- */
/**
 * Updates pointer focus of container for position (x, y).
 *
 * Updates wlmtk_container_t::pointer_focus_element_ptr.
 *
 * @param container_ptr
 * @param x
 * @param y
 * @param time_msec
 *
 * @return Whether there was an element acception the motion at (x, y).
 */
bool update_pointer_focus_at(
    wlmtk_container_t *container_ptr,
    double x,
    double y,
    uint32_t time_msec)
{
    for (bs_dllist_node_t *dlnode_ptr = container_ptr->elements.head_ptr;
         dlnode_ptr != NULL;
         dlnode_ptr = dlnode_ptr->next_ptr) {
        wlmtk_element_t *element_ptr = wlmtk_element_from_dlnode(dlnode_ptr);

        if (!element_ptr->visible) continue;

        int x_pos, y_pos;
        wlmtk_element_get_position(element_ptr, &x_pos, &y_pos);
        int x1, y1, x2, y2;
        wlmtk_element_get_pointer_area(element_ptr, &x1, &y1, &x2, &y2);
        if (x_pos + x1 <= x && x < x_pos + x2 &&
            y_pos + y1 <= y && y < y_pos + y2) {
            if (!wlmtk_element_pointer_motion(
                    element_ptr, x - x_pos, y - y_pos, time_msec)) {
                continue;
            }

            // There is a focus change. Invalidate coordinates in old element.
            if (container_ptr->pointer_focus_element_ptr != element_ptr &&
                NULL != container_ptr->pointer_focus_element_ptr) {
                wlmtk_element_pointer_motion(
                    container_ptr->pointer_focus_element_ptr,
                    NAN, NAN, time_msec);

            }
            container_ptr->pointer_focus_element_ptr = element_ptr;
            return true;
        }
    }

    // Getting here implies we didn't have an element catching the motion,
    // so it must have happened outside our araea. We also should reset
    // pointer focus element now.
    if (NULL != container_ptr->pointer_focus_element_ptr) {
        wlmtk_element_pointer_motion(
            container_ptr->pointer_focus_element_ptr,
            NAN, NAN, time_msec);
        container_ptr->pointer_focus_element_ptr = NULL;
    }
    return false;
}

/* ------------------------------------------------------------------------- */
/**
 * Base implementation of wlmtk_container_vmt_t::update_layout. If there's
 * a parent, will call @ref wlmtk_container_update_layout. Otherwise, will
 * update the pointer focus.
 *
 * @param container_ptr
 */
void _wlmtk_container_update_layout(wlmtk_container_t *container_ptr)
{
    if (NULL != container_ptr->super_element.parent_container_ptr) {
        wlmtk_container_update_layout(
            container_ptr->super_element.parent_container_ptr);
    } else {
        wlmtk_container_update_pointer_focus(container_ptr);
    }
}

/* == Helper for unit tests: A fake container with a tree, as parent ======= */

/** State of the "fake" parent container. Refers to a scene graph. */
typedef struct {
    /** The actual container */
    wlmtk_container_t         container;
    /** A scene graph. Not attached to any output, */
    struct wlr_scene          *wlr_scene_ptr;
} fake_parent_container_t;

/* ------------------------------------------------------------------------- */
wlmtk_container_t *wlmtk_container_create_fake_parent(void)
{
    fake_parent_container_t *fake_parent_container_ptr = logged_calloc(
        1, sizeof(fake_parent_container_t));
    if (NULL == fake_parent_container_ptr) return NULL;

    fake_parent_container_ptr->wlr_scene_ptr = wlr_scene_create();
    if (NULL == fake_parent_container_ptr->wlr_scene_ptr) {
        wlmtk_container_destroy_fake_parent(
            &fake_parent_container_ptr->container);
        return NULL;
    }

    if (!wlmtk_container_init_attached(
            &fake_parent_container_ptr->container,
            NULL,
            &fake_parent_container_ptr->wlr_scene_ptr->tree)) {
        wlmtk_container_destroy_fake_parent(
            &fake_parent_container_ptr->container);
        return NULL;
    }

    return &fake_parent_container_ptr->container;
}

/* ------------------------------------------------------------------------- */
void wlmtk_container_destroy_fake_parent(wlmtk_container_t *container_ptr)
{
    fake_parent_container_t *fake_parent_container_ptr = BS_CONTAINER_OF(
        container_ptr, fake_parent_container_t, container);

    wlmtk_container_fini(&fake_parent_container_ptr->container);

    if (NULL != fake_parent_container_ptr->wlr_scene_ptr) {
        // Note: There is no "wlr_scene_destroy()" method; as of 2023-09-02,
        // this is just a flat allocated struct.
        free(fake_parent_container_ptr->wlr_scene_ptr);
        fake_parent_container_ptr->wlr_scene_ptr = NULL;
    }

    free(fake_parent_container_ptr);
}

/* == Unit tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);
static void test_add_remove(bs_test_t *test_ptr);
static void test_add_remove_with_scene_graph(bs_test_t *test_ptr);
static void test_pointer_motion(bs_test_t *test_ptr);
static void test_pointer_focus(bs_test_t *test_ptr);
static void test_pointer_focus_move(bs_test_t *test_ptr);
static void test_pointer_focus_layered(bs_test_t *test_ptr);
static void test_pointer_button(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_container_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 1, "add_remove", test_add_remove },
    { 1, "add_remove_with_scene_graph", test_add_remove_with_scene_graph },
    { 1, "pointer_motion", test_pointer_motion },
    { 1, "pointer_focus", test_pointer_focus },
    { 1, "pointer_focus_move", test_pointer_focus_move },
    { 1, "pointer_focus_layered", test_pointer_focus_layered },
    { 1, "pointer_button", test_pointer_button },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises init() and fini() methods, verifies dtor forwarding. */
void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_container_t container;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_container_init(&container, NULL));
    // Also expect the super element to be initialized.
    BS_TEST_VERIFY_NEQ(
        test_ptr, NULL, container.super_element.vmt.pointer_motion);

    wlmtk_container_fini(&container);
    // Also expect the super element to be un-initialized.
    BS_TEST_VERIFY_EQ(
        test_ptr, NULL, container.super_element.vmt.pointer_motion);
}

/* ------------------------------------------------------------------------- */
/** Exercises adding and removing elements, verifies destruction on fini. */
void test_add_remove(bs_test_t *test_ptr)
{
    wlmtk_container_t container;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_container_init(&container, NULL));

    wlmtk_fake_element_t *elem1_ptr, *elem2_ptr, *elem3_ptr;
    elem1_ptr = wlmtk_fake_element_create();
    BS_ASSERT(NULL != elem1_ptr);
    elem2_ptr = wlmtk_fake_element_create();
    BS_ASSERT(NULL != elem2_ptr);
    elem3_ptr = wlmtk_fake_element_create();
    BS_ASSERT(NULL != elem3_ptr);

    // Build sequence: 3 -> 2 -> 1.
    wlmtk_container_add_element(&container, &elem1_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr, &container, elem1_ptr->element.parent_container_ptr);
    wlmtk_container_add_element(&container, &elem2_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr, &container, elem2_ptr->element.parent_container_ptr);
    wlmtk_container_add_element(&container, &elem3_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr, &container, elem3_ptr->element.parent_container_ptr);

    // Remove 2, then add at the end: 3 -> 1 -> 2.
    wlmtk_container_remove_element(&container, &elem2_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, elem2_ptr->element.parent_container_ptr);
    wlmtk_container_add_element_before(&container, NULL, &elem2_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, &container, elem2_ptr->element.parent_container_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_element(&elem1_ptr->element)->next_ptr,
        wlmtk_dlnode_from_element(&elem2_ptr->element));

    // Remove elem3 and add before elem2: 1 -> 3 -> 2.
    wlmtk_container_remove_element(&container, &elem3_ptr->element);
    wlmtk_container_add_element_before(
        &container, &elem2_ptr->element, &elem3_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        wlmtk_dlnode_from_element(&elem3_ptr->element)->next_ptr,
        wlmtk_dlnode_from_element(&elem2_ptr->element));

    wlmtk_container_remove_element(&container, &elem2_ptr->element);
    wlmtk_element_destroy(&elem2_ptr->element);

    // Will destroy contained elements.
    wlmtk_container_fini(&container);
}

/* ------------------------------------------------------------------------- */
/** Tests that elements are attached, resp. detached from scene graph. */
void test_add_remove_with_scene_graph(bs_test_t *test_ptr)
{
    wlmtk_container_t *fake_parent_ptr = wlmtk_container_create_fake_parent();
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fake_parent_ptr);
    wlmtk_container_t container;
    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_container_init(&container, NULL));
    wlmtk_element_set_parent_container(
        &container.super_element, fake_parent_ptr);

    // Want to have the node.
    BS_TEST_VERIFY_NEQ(
        test_ptr, NULL, container.super_element.wlr_scene_node_ptr);

    // Fresh element: No scene graph node yet.
    wlmtk_fake_element_t *fe_ptr = wlmtk_fake_element_create();
    BS_TEST_VERIFY_EQ(test_ptr, NULL, fe_ptr->element.wlr_scene_node_ptr);

    // Add to container with attached graph: Element now has a graph node.
    wlmtk_container_add_element(&container, &fe_ptr->element);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, fe_ptr->element.wlr_scene_node_ptr);

    // Remove: The element's graph node must be destroyed & cleared..
    wlmtk_container_remove_element(&container, &fe_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, fe_ptr->element.wlr_scene_node_ptr);
    wlmtk_element_destroy(&fe_ptr->element);

    wlmtk_element_set_parent_container(&container.super_element, NULL);
    wlmtk_container_fini(&container);
    wlmtk_container_destroy_fake_parent(fake_parent_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests the 'motion' method for container. */
void test_pointer_motion(bs_test_t *test_ptr)
{
    wlmtk_container_t container;
    BS_ASSERT(wlmtk_container_init(&container, NULL));
    wlmtk_element_set_visible(&container.super_element, true);

    // Note: pointer area extends by (-1, -2, 3, 4) on each fake element.
    wlmtk_fake_element_t *elem1_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_position(&elem1_ptr->element, -20, -40);
    elem1_ptr->width = 10;
    elem1_ptr->height = 5;
    wlmtk_element_set_visible(&elem1_ptr->element, true);
    wlmtk_container_add_element(&container, &elem1_ptr->element);
    wlmtk_fake_element_t *elem2_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_position(&elem2_ptr->element, 100, 200);
    elem2_ptr->width = 10;
    elem2_ptr->height = 5;
    wlmtk_element_set_visible(&elem2_ptr->element, true);
    wlmtk_container_add_element(&container, &elem2_ptr->element);

    // Verify 'dimensions' and 'pointer_area', derived from children.
    int l, t, r, b;
    wlmtk_element_get_dimensions(&container.super_element, &l, &t, &r, &b);
    BS_TEST_VERIFY_EQ(test_ptr, -20, l);
    BS_TEST_VERIFY_EQ(test_ptr, -40, t);
    BS_TEST_VERIFY_EQ(test_ptr, 110, r);
    BS_TEST_VERIFY_EQ(test_ptr, 205, b);

    wlmtk_element_get_pointer_area(&container.super_element, &l, &t, &r, &b);
    BS_TEST_VERIFY_EQ(test_ptr, -21, l);
    BS_TEST_VERIFY_EQ(test_ptr, -42, t);
    BS_TEST_VERIFY_EQ(test_ptr, 113, r);
    BS_TEST_VERIFY_EQ(test_ptr, 209, b);

    // Same must hold for the parent container.
    wlmtk_container_t parent_container;
    BS_ASSERT(wlmtk_container_init(&parent_container, NULL));
    wlmtk_container_add_element(&parent_container,
                                &container.super_element);

    wlmtk_element_get_dimensions(
        &parent_container.super_element, &l, &t, &r, &b);
    BS_TEST_VERIFY_EQ(test_ptr, -20, l);
    BS_TEST_VERIFY_EQ(test_ptr, -40, t);
    BS_TEST_VERIFY_EQ(test_ptr, 110, r);
    BS_TEST_VERIFY_EQ(test_ptr, 205, b);

    wlmtk_element_get_pointer_area(
        &parent_container.super_element, &l, &t, &r, &b);
    BS_TEST_VERIFY_EQ(test_ptr, -21, l);
    BS_TEST_VERIFY_EQ(test_ptr, -42, t);
    BS_TEST_VERIFY_EQ(test_ptr, 113, r);
    BS_TEST_VERIFY_EQ(test_ptr, 209, b);

    // There's nothing at (0, 0).
    wlmtk_element_pointer_motion(&container.super_element, 0, 0, 7);
    BS_TEST_VERIFY_FALSE(test_ptr, elem1_ptr->pointer_motion_called);
    BS_TEST_VERIFY_FALSE(test_ptr, elem2_ptr->pointer_motion_called);

    wlmtk_element_pointer_motion(&parent_container.super_element, 0, 0, 7);
    BS_TEST_VERIFY_FALSE(test_ptr, elem1_ptr->pointer_motion_called);
    BS_TEST_VERIFY_FALSE(test_ptr, elem2_ptr->pointer_motion_called);

    // elem1 is at (-20, -40).
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(&container.super_element, -20, -40, 7));
    BS_TEST_VERIFY_TRUE(test_ptr, elem1_ptr->pointer_enter_called);
    elem1_ptr->pointer_enter_called = false;
    BS_TEST_VERIFY_TRUE(test_ptr, elem1_ptr->pointer_motion_called);
    elem1_ptr->pointer_motion_called = false;
    BS_TEST_VERIFY_FALSE(test_ptr, elem2_ptr->pointer_motion_called);
    BS_TEST_VERIFY_EQ(test_ptr, 0, elem1_ptr->element.last_pointer_x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, elem1_ptr->element.last_pointer_y);

    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(
            &parent_container.super_element, -20, -40, 7));
    BS_TEST_VERIFY_FALSE(test_ptr, elem1_ptr->pointer_enter_called);
    BS_TEST_VERIFY_TRUE(test_ptr, elem1_ptr->pointer_motion_called);
    elem1_ptr->pointer_motion_called = false;
    BS_TEST_VERIFY_FALSE(test_ptr, elem2_ptr->pointer_motion_called);
    BS_TEST_VERIFY_EQ(test_ptr, 0, elem1_ptr->element.last_pointer_x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, elem1_ptr->element.last_pointer_y);

    // elem2 is covering the area at (107, 302).
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(
            &parent_container.super_element, 107, 203, 7));
    BS_TEST_VERIFY_TRUE(test_ptr, elem1_ptr->pointer_motion_called);
    BS_TEST_VERIFY_TRUE(test_ptr, isnan(elem1_ptr->element.last_pointer_x));
    elem1_ptr->pointer_motion_called = false;
    BS_TEST_VERIFY_TRUE(test_ptr, elem1_ptr->pointer_leave_called);
    elem1_ptr->pointer_leave_called = false;
    BS_TEST_VERIFY_TRUE(test_ptr, elem2_ptr->pointer_enter_called);
    elem2_ptr->pointer_enter_called = false;
    BS_TEST_VERIFY_TRUE(test_ptr, elem2_ptr->pointer_motion_called);
    elem2_ptr->pointer_motion_called = false;
    BS_TEST_VERIFY_EQ(test_ptr, 7, elem2_ptr->element.last_pointer_x);
    BS_TEST_VERIFY_EQ(test_ptr, 3, elem2_ptr->element.last_pointer_y);

    // The pointer area of elem2 is covering the area at (112, 208).
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(
            &parent_container.super_element, 112, 208, 7));
    BS_TEST_VERIFY_FALSE(test_ptr, elem1_ptr->pointer_leave_called);
    BS_TEST_VERIFY_FALSE(test_ptr, elem1_ptr->pointer_motion_called);
    BS_TEST_VERIFY_TRUE(test_ptr, elem2_ptr->pointer_motion_called);
    elem2_ptr->pointer_motion_called = false;
    BS_TEST_VERIFY_EQ(test_ptr, 12, elem2_ptr->element.last_pointer_x);
    BS_TEST_VERIFY_EQ(test_ptr, 8, elem2_ptr->element.last_pointer_y);

    // The pointer area of elem2 does not include (113, 209).
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_element_pointer_motion(
            &parent_container.super_element, 113, 209, 7));
    BS_TEST_VERIFY_FALSE(test_ptr, elem1_ptr->pointer_motion_called);
    BS_TEST_VERIFY_TRUE(test_ptr, elem2_ptr->pointer_motion_called);
    BS_TEST_VERIFY_TRUE(test_ptr, isnan(elem2_ptr->element.last_pointer_x));
    BS_TEST_VERIFY_TRUE(test_ptr, elem2_ptr->pointer_leave_called);
    elem2_ptr->pointer_leave_called = false;

    // All set. clean it up.
    wlmtk_container_remove_element(&container, &elem1_ptr->element);
    wlmtk_element_destroy(&elem1_ptr->element);
    wlmtk_container_remove_element(&container, &elem2_ptr->element);
    wlmtk_element_destroy(&elem2_ptr->element);

    wlmtk_container_remove_element(&parent_container,
                                   &container.super_element);
    wlmtk_container_fini(&parent_container);
    wlmtk_container_fini(&container);
}

/* ------------------------------------------------------------------------- */
/** Tests that pointer focus is updated when elements are updated. */
void test_pointer_focus(bs_test_t *test_ptr)
{
    wlmtk_container_t container;
    BS_ASSERT(wlmtk_container_init(&container, NULL));

    wlmtk_fake_element_t *elem1_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&elem1_ptr->element, true);
    wlmtk_fake_element_t *elem2_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&elem2_ptr->element, true);

    // Case 1: An empty container, will not have a pointer-focussed element.
    BS_TEST_VERIFY_EQ(test_ptr, NULL, container.pointer_focus_element_ptr);

    // Case 2: Adding a visible element at (0, 0): Focus remains NULL, since
    // motion() was not called yet and we don't have known pointer position.
    wlmtk_container_add_element(&container, &elem1_ptr->element);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, container.pointer_focus_element_ptr);
    wlmtk_container_remove_element(&container, &elem1_ptr->element);

    // Case 3: Call motion() first, then add a visible element at (0, 0). Focus
    // should switch there.
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_element_pointer_motion(&container.super_element, 0, 0, 7));
    wlmtk_container_add_element(&container, &elem1_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem1_ptr->element,
        container.pointer_focus_element_ptr);

    // Case 4: Add another visible element. Focus changes, since in front.
    wlmtk_container_add_element(&container, &elem2_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem2_ptr->element,
        container.pointer_focus_element_ptr);

    // Case 5: Elem2 (added last = in front) becomes invisible. Focus changes.
    wlmtk_element_set_visible(&elem2_ptr->element, false);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem1_ptr->element,
        container.pointer_focus_element_ptr);

    // Case 6: Elem1 becomes invisible. Focus changes to NULL.
    wlmtk_element_set_visible(&elem1_ptr->element, false);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        container.pointer_focus_element_ptr);

    // Case 7: Elem1 becomes visible. Focus changes to elem1 again.
    wlmtk_element_set_visible(&elem1_ptr->element, true);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem1_ptr->element,
        container.pointer_focus_element_ptr);

    // Case 8: Remove Elem1. Focus changes to NULL.
    wlmtk_container_remove_element(&container, &elem1_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        container.pointer_focus_element_ptr);

    // Case 9: Elem2 becomes visible, focus changes there.
    wlmtk_element_set_visible(&elem2_ptr->element, true);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem2_ptr->element,
        container.pointer_focus_element_ptr);

    // Case 10: Elem2 is removed. Focus is now NULL, and leave() is called for
    // the element that was removed.
    elem2_ptr->pointer_leave_called = false;
    wlmtk_container_remove_element(&container, &elem2_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        container.pointer_focus_element_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        elem2_ptr->pointer_leave_called);

    wlmtk_element_destroy(&elem2_ptr->element);
    wlmtk_element_destroy(&elem1_ptr->element);
    wlmtk_container_fini(&container);
}

/* ------------------------------------------------------------------------- */
/** Tests that pointer focus is updated when elements are moved. */
void test_pointer_focus_move(bs_test_t *test_ptr)
{
    wlmtk_container_t container;
    BS_ASSERT(wlmtk_container_init(&container, NULL));

    // Setup to span an area where the container catches pointer coordinates.
    wlmtk_fake_element_t *elem1_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&elem1_ptr->element, true);
    wlmtk_element_set_position(&elem1_ptr->element, -20, 0);
    wlmtk_container_add_element(&container, &elem1_ptr->element);
    wlmtk_fake_element_t *elem2_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&elem2_ptr->element, true);
    wlmtk_element_set_position(&elem2_ptr->element, 20, 0);
    wlmtk_container_add_element(&container, &elem2_ptr->element);

    // Need the container to pick up the cursor position.
    wlmtk_element_pointer_motion(&container.super_element, 0, 0, 7);

    // Is off the cursor, will get focus.
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        container.pointer_focus_element_ptr);

    // Now moves below the cursor, will get focus.
    wlmtk_element_set_position(&elem1_ptr->element, 0, 0);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem1_ptr->element,
        container.pointer_focus_element_ptr);

    wlmtk_container_remove_element(&container, &elem2_ptr->element);
    wlmtk_container_remove_element(&container, &elem1_ptr->element);

    wlmtk_element_destroy(&elem2_ptr->element);
    wlmtk_element_destroy(&elem1_ptr->element);
    wlmtk_container_fini(&container);
}


/* ------------------------------------------------------------------------- */
/** Tests that pointer focus is updated across layers of containers. */
void test_pointer_focus_layered(bs_test_t *test_ptr)
{
    wlmtk_container_t container1;
    BS_ASSERT(wlmtk_container_init(&container1, NULL));
    wlmtk_container_t container2;
    BS_ASSERT(wlmtk_container_init(&container2, NULL));
    wlmtk_element_set_visible(&container2.super_element, true);

    wlmtk_fake_element_t *elem1_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&elem1_ptr->element, true);
    wlmtk_fake_element_t *elem2_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&elem2_ptr->element, true);

    // Prepare: Motion was called, will not have any focus.
    wlmtk_element_pointer_motion(&container1.super_element, 0, 0, 7);
    BS_TEST_VERIFY_EQ(test_ptr, NULL, container1.pointer_focus_element_ptr);

    // Case 1: Add element 2 to second container, then add this container.
    // this must re-trigger focus and pass it to elem2.
    wlmtk_container_add_element(&container2, &elem2_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        container1.pointer_focus_element_ptr);
    wlmtk_container_add_element(&container1, &container2.super_element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &container2.super_element,
        container1.pointer_focus_element_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem2_ptr->element, container2.pointer_focus_element_ptr);

    // Case 2: Add elem1 to container1. Must change focus there, and call
    // leave for container2 and elem2.
    elem2_ptr->pointer_leave_called = false;
    wlmtk_container_add_element(&container1, &elem1_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem1_ptr->element,
        container1.pointer_focus_element_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, elem2_ptr->pointer_leave_called);

    // Case 3: Bring container2 in front. Now elem2 has focus.
    elem1_ptr->pointer_leave_called = false;
    wlmtk_container_remove_element(&container1, &container2.super_element);
    wlmtk_container_add_element(&container1, &container2.super_element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &container2.super_element,
        container1.pointer_focus_element_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem2_ptr->element,
        container2.pointer_focus_element_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, elem1_ptr->pointer_leave_called);

    // Case 4: Remove elem2, drop focus back to elem1.
    elem2_ptr->pointer_leave_called = false;
    wlmtk_container_remove_element(&container2, &elem2_ptr->element);
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem1_ptr->element,
        container1.pointer_focus_element_ptr);
    BS_TEST_VERIFY_TRUE(test_ptr, elem2_ptr->pointer_leave_called);

    wlmtk_container_remove_element(&container1, &elem1_ptr->element);
    wlmtk_element_destroy(&elem2_ptr->element);
    wlmtk_element_destroy(&elem1_ptr->element);

    wlmtk_container_remove_element(&container1, &container2.super_element);
    wlmtk_container_fini(&container2);
    wlmtk_container_fini(&container1);
}

/* ------------------------------------------------------------------------- */
/** Tests that pointer DOWN is forwarded to element with pointer focus. */
void test_pointer_button(bs_test_t *test_ptr)
{
    wlmtk_container_t container;
    BS_ASSERT(wlmtk_container_init(&container, NULL));

    wlmtk_fake_element_t *elem1_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_visible(&elem1_ptr->element, true);
    elem1_ptr->width = 1;
    elem1_ptr->height = 1;
    wlmtk_container_add_element(&container, &elem1_ptr->element);

    wlmtk_fake_element_t *elem2_ptr = wlmtk_fake_element_create();
    wlmtk_element_set_position(&elem2_ptr->element, 10, 10);
    wlmtk_element_set_visible(&elem2_ptr->element, true);
    wlmtk_container_add_element_before(&container, NULL, &elem2_ptr->element);

    wlmtk_button_event_t button = {
        .button = BTN_LEFT, .type = WLMTK_BUTTON_DOWN
    };
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_element_pointer_button(&container.super_element, &button));

    // DOWN events go to the focussed element.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(&container.super_element, 0, 0, 7));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(&container.super_element, &button));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem1_ptr->element,
        container.left_button_element_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr, elem1_ptr->pointer_button_called);

    // Moves, pointer focus is now on elem2.
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_motion(&container.super_element, 10, 10, 7));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        &elem2_ptr->element,
        container.pointer_focus_element_ptr);

    // The UP event is still received by elem1.
    elem1_ptr->pointer_button_called = false;
    button.type = WLMTK_BUTTON_UP;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(&container.super_element, &button));
    BS_TEST_VERIFY_TRUE(
        test_ptr, elem1_ptr->pointer_button_called);

    // Click will be ignored
    button.type = WLMTK_BUTTON_CLICK;
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_element_pointer_button(&container.super_element, &button));

    // New DOWN event goes to elem2, though.
    elem2_ptr->pointer_button_called = false;
    button.type = WLMTK_BUTTON_DOWN;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(&container.super_element, &button));
    BS_TEST_VERIFY_TRUE(
        test_ptr, elem2_ptr->pointer_button_called);

    // And UP event now goes to elem2.
    elem2_ptr->pointer_button_called = false;
    button.type = WLMTK_BUTTON_UP;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(&container.super_element, &button));
    BS_TEST_VERIFY_TRUE(
        test_ptr, elem2_ptr->pointer_button_called);

    // Here, CLICK goes to elem2.
    elem2_ptr->pointer_button_called = false;
    button.type = WLMTK_BUTTON_CLICK;
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_element_pointer_button(&container.super_element, &button));
    BS_TEST_VERIFY_TRUE(
        test_ptr, elem2_ptr->pointer_button_called);

    // After removing, further UP events won't be accidentally sent there.
    wlmtk_container_remove_element(&container, &elem1_ptr->element);
    wlmtk_container_remove_element(&container, &elem2_ptr->element);
    button.type = WLMTK_BUTTON_UP;
    BS_TEST_VERIFY_FALSE(
        test_ptr,
        wlmtk_element_pointer_button(&container.super_element, &button));
    BS_TEST_VERIFY_EQ(
        test_ptr,
        NULL,
        container.left_button_element_ptr);
    wlmtk_element_destroy(&elem2_ptr->element);
    wlmtk_element_destroy(&elem1_ptr->element);
    wlmtk_container_fini(&container);
}


/* == End of container.c =================================================== */
