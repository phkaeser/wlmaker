/* ========================================================================= */
/**
 * @file popup.h
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
#ifndef __WLMTK_POPUP_H__
#define __WLMTK_POPUP_H__

/** Forward declaration: Popup. */
typedef struct _wlmtk_popup_t wlmtk_popup_t;

#include "container.h"
#include "env.h"
#include "pubase.h"
#include "surface.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * State of a popup.
 *
 * A popup contains a @ref wlmtk_surface_t, and may contain further popups.
 * These further popups will be stacked above the principal surface, in order
 * of them being added.
 */
struct _wlmtk_popup_t {
    /** Super class of the panel. */
    wlmtk_container_t         super_container;

    /** And the popup base. Popups can contain child popups. */
    wlmtk_pubase_t            pubase;

    /** The contained surface. */
    wlmtk_surface_t           *surface_ptr;

    /** The parent popup base. */
    wlmtk_pubase_t            *parent_pubase_ptr;

    /** Listener for the `map` signal of `wlr_surface`. */
    struct wl_listener        surface_map_listener;
    /** Listener for the `map` signal of `wlr_surface`. */
    struct wl_listener        surface_unmap_listener;

    /** Node element of @ref wlmtk_pubase_t::popups. */
    bs_dllist_node_t          dlnode;
};

/**
 * Initializes the popup.
 *
 * @param popup_ptr
 * @param env_ptr
 * @param surface_ptr
 *
 * @return true on success.
 */
bool wlmtk_popup_init(
    wlmtk_popup_t *popup_ptr,
    wlmtk_env_t *env_ptr,
    wlmtk_surface_t *surface_ptr);

/**
 * Un-initializes the popup. Will remove it from the parent pubase.
 *
 * @param popup_ptr
 */
void wlmtk_popup_fini(wlmtk_popup_t *popup_ptr);

/**
 * Sets the popup base for `popup_ptr`.
 *
 * @param popup_ptr
 * @param pubase_ptr
 */
void wlmtk_popup_set_pubase(wlmtk_popup_t *popup_ptr,
                            wlmtk_pubase_t *pubase_ptr);

/** Returns the base @ref wlmtk_element_t. */
wlmtk_element_t *wlmtk_popup_element(wlmtk_popup_t *popup_ptr);

/** Gets the pointer to @ref wlmtk_popup_t::dlnode. */
bs_dllist_node_t *wlmtk_dlnode_from_popup(wlmtk_popup_t *popup_ptr);
/** Gets the @ref wlmtk_popup_t from the @ref wlmtk_popup_t::dlnode. */
wlmtk_popup_t *wlmtk_popup_from_dlnode(bs_dllist_node_t *dlnode_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_POPUP_H__ */
/* == End of popup.h ======================================================= */
