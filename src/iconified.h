/* ========================================================================= */
/**
 * @file iconified.h
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
 * TODO:
 * Interface for @ref wlmaker_iconified_t. An iconified is the representation
 * for an iconified view (An XDG toplevel or an Xwayland ... surface?).
 *
 * The "iconified" can be created from a wlmaker_view_t. Following properties:
 * - position       get_position and set_position
 * - scene node     (or tree?)
 * - workspace      (that it is shown on) => this should *probably* be
 *                  an "tile_holder"  (dock, clip, drawer, icon-area)
 *
 * tile_set, tile_container
 *
 * Note: An "iconified" should be derived from a "tile". Whereas a tile always
 * has a background and edge. As in WM.
 * Note: A 'tile' is also an interactive, since it can be clicked.  And we may
 * pass other events along to it.
 * => The ability to hold multiple "interactives" is a shared property between
 *    a view and the tile_container (and other layer elements).
 *
 * [parent] (view, container, layer element)
 *      +--> view
 *           +--> xdg toplevel
 *           +--> x11?   (we should check xwayland, maybe it's not?)
 *      +--> layer element
 *           +--> tile container (we keep this on layers only)
 *
 * [interactive]  (handlers for enter/leave/motion/focus/button)
 *      +--> tile (oh, the tile is an interactive already!)
 *                (but, should be merged/migrated to app (app launcher).
 *           +--> iconified
 *           +--> app (launcher)
 *           +--> clip
 *           +--> (optionally later: drawer => open a tile container)
 *      +--> menu
 *      +--> (window) button
 *      +--> resizebar
 *      +--> titlebar
 *
 * As for current status:
 * -> see if the iconified can be hacked as an interactive, and make use of
 *    the (tile container's) view for event forwarding.
 *    (should be OK with the interactive being a scene buffer directly)
 */
#ifndef __ICONIFIED_H__
#define __ICONIFIED_H__

/** Forward declaration of the iconified. */
typedef struct _wlmaker_iconified_t wlmaker_iconified_t;

/** TODO(kaeser@gubbe.ch): Cleanup, this is prototype. */
typedef struct _wlmaker_dockapp_iconified_t wlmaker_dockapp_iconified_t;

#include "server.h"
#include "view.h"

#include <libbase/libbase.h>  // TODO: consider removing.

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Creates an iconified, ie. a minimized representation of `view_ptr`.
 *
 * @param view_ptr
 */
wlmaker_iconified_t *wlmaker_iconified_create(
    wlmaker_view_t *view_ptr);

/**
 * Destroys the iconified.
 *
 * @param iconified_ptr
 */
void wlmaker_iconified_destroy(wlmaker_iconified_t *iconified_ptr);

/**
 * Conversion: Retrieves the @ref wlmaker_view_t represented by this iconified.
 *
 * @param iconified_ptr
 *
 * @return Pointer to the view.
 */
wlmaker_view_t *wlmaker_view_from_iconified(
    wlmaker_iconified_t *iconified_ptr);

// TODO(kaeser@gubbe.ch): Migrate this to 'tile'.
/**
 * Sets the position of the iconified, relative to the tile container.
 *
 * @param iconified_ptr
 * @param x
 * @param y
 */
void wlmaker_iconified_set_position(
    wlmaker_iconified_t *iconified_ptr,
    uint32_t x, uint32_t y);

// TODO(kaeser@gubbe.ch): Remove if/when deriving from tile.
/**
 * Conversion: Gets a pointer to the `dlnode` of the iconified
 *
 * @param iconified_ptr
 *
 * @return Pointer.
 */
bs_dllist_node_t *wlmaker_dlnode_from_iconified(
    wlmaker_iconified_t *iconified_ptr);

// TODO(kaeser@gubbe.ch): Remove if/when deriving from tile.
/**
 * Conversion: Gets a pointer to the avlnode of the iconified's interactive.
 *
 * @param iconified_ptr
 *
 * @return Pointer.
 */
bs_avltree_node_t *wlmaker_avlnode_from_iconified(
    wlmaker_iconified_t *iconified_ptr);

// TODO(kaeser@gubbe.ch): Remove if/when deriving from tile.
/**
 * Conversion: Returns the iconified, given a pointer to it's `dlnode`.
 *
 * @param dlnode_ptr
 *
 * @return Pointer.
 */
wlmaker_iconified_t *wlmaker_iconified_from_dlnode(
    bs_dllist_node_t *dlnode_ptr);

// TODO(kaeser@gubbe.ch): Remove if/when deriving from tile.
/**
 * Conversion: Lookups up the scene node of the iconified's interactive.
 *
 * @param iconified_ptr
 *
 * @return Pointer.
 */
struct wlr_scene_node *wlmaker_wlr_scene_node_from_iconified(
    wlmaker_iconified_t *iconified_ptr);

/**
 * Conversion: Gets the scene node from the scene buffer.
 *
 * TODO(kaeser@gubbe.ch): Remove, once the dockapp prototype is cleaned up.
 *
 * @param iconified_ptr
 *
 * @return Pointer.
 */
struct wlr_scene_node *wlmaker_wlr_scene_node_from_iconified_scene_buffer(
    wlmaker_iconified_t *iconified_ptr);

// TODO(kaeser@gubbe.ch): Remove, once designed and implemented properly.  */
/** Creates the iconified dockapp. */
wlmaker_dockapp_iconified_t *wlmaker_dockapp_iconified_create(
    wlmaker_server_t *server_ptr);
/** Destroys the iconified dockapp. */
void wlmaker_dockapp_iconified_destroy(wlmaker_dockapp_iconified_t *dai_ptr);
/** Gets the iconified from the dockapp. */
wlmaker_iconified_t *wlmaker_iconified_from_dockapp(
    wlmaker_dockapp_iconified_t *dai_ptr);
/** Attaches the surface to the dockapp. */
void wlmaker_dockapp_iconified_attach(
    wlmaker_dockapp_iconified_t *dai_ptr,
    struct wlr_surface *wlr_surface_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __ICONIFIED_H__ */
/* == End of iconified.h =================================================== */
