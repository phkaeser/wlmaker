/* ========================================================================= */
/**
 * @file view.h
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
 * The view is an abstraction to handle windows, eg. XDG shells.
 *
 * A view has the following properties:
 * - A position, width and height.
 * - It has a surface.
 * - It has a position in the stack of other views & can be raised or lowered.
 * - It may be activated (or be configured to not be activate-able)
 * - It may be mapped (visible) or unmapped (not visible)
 * - It may be maximized, minimized, full-screen, (normal) or rolled up.
 *
 * TODO: finalize.
 *
 * Should have a state, which can be:
 * - unmapped
 * - fullscreen
 * - maximized
 * - shaded    (only applies to server-side decorated views)
 * - organic
 */
#ifndef __VIEW_H__
#define __VIEW_H__

#include <stdbool.h>
#include <stdint.h>

#include <libbase/libbase.h>
#include <wayland-server-core.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

/** Type definition of the state of a view. */
typedef struct _wlmaker_view_t wlmaker_view_t;

#include "interactive.h"
#include "server.h"
#include "workspace.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Callback: Activate the view. */
typedef uint32_t (*wlmaker_view_activate_callback_t)(wlmaker_view_t *view_ptr, bool activated);
/** Callback: Close the view. */
typedef void (*wlmaker_view_send_close_callback_t)(wlmaker_view_t *view_ptr);
/** Callback: Set size. */
typedef void (*wlmaker_view_set_size_callback_t)(wlmaker_view_t *view_ptr,
                                                 int width, int height);

/** Information regarding a client. Drawn from `struct wl_client`. */
typedef struct {
    /** Process ID. */
    pid_t                     pid;
    /** User ID. */
    uid_t                     uid;
    /** Group ID. */
    gid_t                     gid;
} wlmaker_client_t;

/** Anchor bitfield. */
typedef enum {
    /** Anchored to the top edge. */
    WLMAKER_VIEW_ANCHOR_TOP   = (UINT32_C(1) << 0),
    /** Anchored to the bottom edge. */
    WLMAKER_VIEW_ANCHOR_BOTTOM= (UINT32_C(1) << 1),
    /** Anchored to the left edge. */
    WLMAKER_VIEW_ANCHOR_LEFT  = (UINT32_C(1) << 2),
    /** Anchored to the right edge. */
    WLMAKER_VIEW_ANCHOR_RIGHT = (UINT32_C(1) << 3)
} wlmaker_view_anchor_t;

/** Implementation methods for the view. */
typedef struct {
    /**
     * Sets the `activated` status for the view. "Activated" denotes the visual
     * appearance when the view has keyboard focus.
     *
     * Required for an implementation.
     */
    uint32_t (*set_activated)(wlmaker_view_t *view_ptr,
                              bool activated);

    /**
     * Retrieves the size of the view's surface owned by the implementation.
     *
     * This ignores elements owned by the `wlmaker_view_t`, eg. server-side
     * decoration elements. Both |width_ptr| and |height_ptr| may be NULL,
     * if the caller is not interested in the particular value.
     *
     * Required for an implementation.
     */
    void (*get_size)(wlmaker_view_t *view_ptr,
                     uint32_t *width_ptr, uint32_t *height_ptr);

    /**
     * Sets the size of the view's surface owned by the implementation.
     *
     * This sets width and height of the view, excluding elements owned by
     * `wlmaker_view_t`, such as server-side decoration. Will not change the
     * position of the view; the position is iwned by `wlmaker_view_t`.
     *
     * Optional for an implementation.
     */
    void (*set_size)(wlmaker_view_t *view_ptr, int width, int height);

    // TODO(kaeser@gubbe.ch): Consider merging the set_maximized &
    // set_fullscreen methods.

    /**
     * Sets the implementation maximization state.
     *
     * Optional for an implementation.
     */
    void (*set_maximized)(wlmaker_view_t *view_ptr, bool maximize);

    /**
     * Sets the implementation's fullscreen state.
     *
     * Optional for an implementation.
     */
    void (*set_fullscreen)(wlmaker_view_t *view_ptr, bool maximize);

    /** Handles an axis event.
     *
     * Optional for an implementation.
     */
    void (*handle_axis)(wlmaker_view_t *view_ptr,
                        struct wlr_pointer_axis_event *event_ptr);

} wlmaker_view_impl_t;

/** State of a view. */
struct _wlmaker_view_t {
    /** Points to the view's implementation methods. */
    const wlmaker_view_impl_t *impl_ptr;

    /** Node within the stack of views, defining it's position. */
    bs_dllist_node_t          views_node;
    /** Back-link to the server. */
    wlmaker_server_t          *server_ptr;
    /** Workspace this view belongs to. Non-NULL when mapped. */
    wlmaker_workspace_t       *workspace_ptr;

    /** The surface. TODO: Clarify. */
    struct wlr_surface        *wlr_surface_ptr;

    /**
     * Scene graph tree, holding all the window elements.
     *
     * Will hold the scene node of the view's surfaces & sub-surfaces (as
     * provided to @ref wlmaker_view_init and re-parented), the decorations.
     *
     * The `node.data` field of the tree's scene node is a back-link pointing
     * to @ref wlmaker_view_t.
     */
    struct wlr_scene_tree     *elements_wlr_scene_tree_ptr;
    /** Scene graph tree of the surface (the shell). */
    struct wlr_scene_tree     *view_wlr_scene_tree_ptr;

    /** "Sending close event" callback. */
    wlmaker_view_send_close_callback_t send_close_callback;

    /** Anchor of the view. */
    uint32_t                  anchor;

    /** Whether this view is currently active (focussed). */
    bool                      active;
    /**
     * Stores the 'organic' position and size of the view.
     *
     * This is used to store the position & size of the view before entering
     * maximized (or fullscreen) state, and to restore the dimensions once
     * that state is terminated.
     */
    struct wlr_box            organic_box;
    /** Whether the view is currently maximized. */
    bool                      maximized;
    /** Whether the view is currently in full-screen mode. */
    bool                      fullscreen;
    /** Whether the view is currently shaded. */
    bool                      shaded;
    /** Default layer (unless the view is in fullscreen). */
    wlmaker_workspace_layer_t default_layer;

    /**
     * AVL tree holding decoration interactives.
     * Lookup key: the `wlr_scene_buffer.node`.
     * */
    bs_avltree_t              *interactive_tree_ptr;

    /** Listener for "button release" signals. To catch releases off focus. */
    struct wl_listener        button_release_listener;

    /** Scene node currently having pointer focus, or NULL. */
    struct wlr_scene_node     *pointer_focussed_wlr_scene_node_ptr;

    /** Application ID, as a UTF-8 string. */
    char                      *app_id_ptr;
    /** Window title, as a UTF-8 string. */
    char                      *title_ptr;

    /** Client information. */
    wlmaker_client_t          client;
};

/**
 * Initializes the |view_ptr| state.
 *
 * @param view_ptr
 * @param view_impl_ptr
 * @param server_ptr
 * @param wlr_surface_ptr
 * @param wlr_scene_tree_ptr
 * @param send_close_callback
 */
void wlmaker_view_init(
    wlmaker_view_t *view_ptr,
    const wlmaker_view_impl_t *view_impl_ptr,
    wlmaker_server_t *server_ptr,
    struct wlr_surface *wlr_surface_ptr,
    struct wlr_scene_tree *wlr_scene_tree_ptr,
    wlmaker_view_send_close_callback_t send_close_callback);

/**
 * Un-initializes the |view_ptr| state.
 *
 * @param view_ptr
 */
void wlmaker_view_fini(wlmaker_view_t *view_ptr);

/**
 * Raises the view to the top of the stack.
 *
 * @param view_ptr
 */
void wlmaker_view_raise_to_top(wlmaker_view_t *view_ptr);

/**
 * Sets the state of the view. Active == focussed, inactive == blurred.
 *
 * @param view_ptr
 * @param active
 */
void wlmaker_view_set_active(wlmaker_view_t *view_ptr, bool active);

/**
 * Type conversion: Gets the wlmaker_view_t from the given `bs_dllist_node_t`.
 *
 * @param node_ptr
 *
 * @return The pointer to `wlmaker_view_t` holding that node.
 */
wlmaker_view_t *wlmaker_view_from_dlnode(bs_dllist_node_t *node_ptr);

/**
 * Type conversion: Gets the `bs_dllist_node_t` from the given wlmaker_view_t.
 *
 * @param view_ptr
 *
 * @return The pointer to the `bs_dllist_node_t`.
 */
bs_dllist_node_t *wlmaker_dlnode_from_view(wlmaker_view_t *view_ptr);

/**
 * Type conversion: Gets the `struct wr_scene_node` for the view.
 *
 * @param view_ptr
 *
 * @return The pointer to wlr_scene_tree_ptr->node.
 */
struct wlr_scene_node *wlmaker_wlr_scene_node_from_view(
    wlmaker_view_t *view_ptr);

/**
 * Gets the `struct wlr_surface` associated with this view.
 *
 * @param view_ptr
 */
struct wlr_surface *wlmaker_view_get_wlr_surface(wlmaker_view_t *view_ptr);

/**
 * Returns the view that has a surface at the given position. Updates
 * wlr_surface_ptr_ptr to point to the surface.
 *
 * @param server_ptr
 * @param x
 * @param y
 * @param wlr_surface_ptr_ptr
 * @param rel_x_ptr
 * @param rel_y_ptr
 *
 * @return The view or NULL if there is no view there.
 */
wlmaker_view_t *wlmaker_view_at(
    wlmaker_server_t *server_ptr,
    double x,
    double y,
    struct wlr_surface **wlr_surface_ptr_ptr,
    double *rel_x_ptr,
    double *rel_y_ptr);

/**
 * Handles cursor motion for the view, ie. for the decoration elements.
 *
 * @param view_ptr
 * @param x
 * @param y
 */
void wlmaker_view_handle_motion(
    wlmaker_view_t *view_ptr,
    double x,
    double y);

/** Handles a button event for the view.
 *
 * Any button press on the view will trigger "raise_to_top" and "activate".
 * If server-side decorations are enabled: Button events on the decoration
 * control surfaces may trigger respective events.
 *
 * @param view_ptr
 * @param x
 * @param y
 * @param event_ptr
 */
void wlmaker_view_handle_button(
    wlmaker_view_t *view_ptr,
    double x,
    double y,
    struct wlr_pointer_button_event *event_ptr);

/** Handles an axis event for the view.
 *
 * Axis events are eg. scroll-wheel actions. Some of the wlmaker elements
 * (eg. Clip) will take scroll-wheel events.
 *
 * @param view_ptr
 * @param x
 * @param y
 * @param event_ptr
 */
void wlmaker_view_handle_axis(
    wlmaker_view_t *view_ptr,
    double x,
    double y,
    struct wlr_pointer_axis_event *event_ptr);

/**
 * Handles when the |view_ptr| loses pointer focus.
 *
 * Used to update control surfaces of server side decoration. Will not be
 * passed to the client: wlr_seat_pointer_notify_enter does that.
 *
 * @param view_ptr
 */
void wlmaker_view_cursor_leave(wlmaker_view_t *view_ptr);

/**
 * Shades (rolls up) the view.
 *
 * @param view_ptr
 */
void wlmaker_view_shade(wlmaker_view_t *view_ptr);

/**
 * Retrieves the dimensions of the view, including server-side decoration
 * (if any).
 *
 * @param view_ptr
 * @param width_ptr           May be NULL.
 * @param height_ptr          May be NULL.
 */
void wlmaker_view_get_size(wlmaker_view_t *view_ptr,
                           uint32_t *width_ptr,
                           uint32_t *height_ptr);

/**
 * Sets the size of the view, including server-side decoration (if any).
 *
 * @param view_ptr
 * @param width
 * @param height
 */
void wlmaker_view_set_size(wlmaker_view_t *view_ptr,
                           int width, int height);

/**
 * Sets, respectively unsets this view as maximized.
 *
 * @param view_ptr
 * @param maximize
 */
void wlmaker_view_set_maximized(wlmaker_view_t *view_ptr, bool maximize);

/**
 * Sets, respectively unsets this view as fullscreen.
 *
 * @param view_ptr
 * @param fullscreen
 */
void wlmaker_view_set_fullscreen(wlmaker_view_t *view_ptr, bool fullscreen);

/**
 * Retrieves the position of the view, including server-side decoration
 * (if any).
 *
 * @param view_ptr
 * @param x_ptr
 * @param y_ptr
 */
void wlmaker_view_get_position(wlmaker_view_t *view_ptr,
                               int *x_ptr, int *y_ptr);

/**
 * Sets the position of the view, including server-side decoration
 * (if any).
 *
 * @param view_ptr
 * @param x
 * @param y
 */
void wlmaker_view_set_position(wlmaker_view_t *view_ptr,
                               int x, int y);

/**
 * Sets the title string.
 *
 * @param view_ptr
 * @param title_ptr
 */
void wlmaker_view_set_title(wlmaker_view_t *view_ptr, const char *title_ptr);

/**
 * Gets the title string.
 *
 * Will point to a memory area that remains valid until either the view is
 * destroyed, or @ref wlmaker_view_set_title is called again.
 *
 * @param view_ptr
 *
 * @return Pointer to the title string. May be NULL if no title has been set.
 *     @see wlmaker_view_set_title.
 */
const char *wlmaker_view_get_title(wlmaker_view_t *view_ptr);

/**
 * Sets the application ID for the view.
 *
 * @param view_ptr
 * @param app_id_ptr
 */
void wlmaker_view_set_app_id(wlmaker_view_t *view_ptr, const char *app_id_ptr);

/**
 * Gets the application ID of the veiw.
 *
 * Will point to a memory area that remains valid until either the view is
 * destroyed, or @ref wlmaker_view_set_app_id is called again.
 *
 * @param view_ptr
 *
 * @return Pointer to the application ID as a string. May be NULL if no
 *     application ID has been set. @see wlmaker_view_set_app_id.
 */
const char *wlmaker_view_get_app_id(wlmaker_view_t *view_ptr);

/**
 * Maps the view to the specified layer of the given workspace.
 *
 * @param view_ptr
 * @param workspace_ptr
 * @param layer
 */
void wlmaker_view_map(wlmaker_view_t *view_ptr,
                      wlmaker_workspace_t *workspace_ptr,
                      wlmaker_workspace_layer_t layer);

/**
 * Unmaps the view.
 *
 * @param view_ptr
 */
void wlmaker_view_unmap(wlmaker_view_t *view_ptr);

/**
 * Returns the anchoring edges for this view.
 *
 * @param view_ptr
 *
 * @returns The anchoring edges, as a bitmask. See `wlmaker_view_anchor_t`.
 */
uint32_t wlmaker_view_get_anchor(wlmaker_view_t *view_ptr);

/**
 * Returns the `struct wlr_output` that the `wlmaker_view_t` is on.
 *
 * @param view_ptr
 *
 * @returns A pointer to the `struct wlr_output`.
 */
struct wlr_output *wlmaker_view_get_wlr_output(wlmaker_view_t *view_ptr);

/**
 * Returns a pointer to details about the client, if available.
 *
 * @param view_ptr
 *
 * @return Pointer to client details. Will remain valid throughout the lifetime
 *     of `view_ptr`.
 */
const wlmaker_client_t *wlmaker_view_get_client(wlmaker_view_t *view_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __VIEW_H__ */
/* == End of view.h ======================================================== */
