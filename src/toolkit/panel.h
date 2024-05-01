/* ========================================================================= */
/**
 * @file panel.h
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
#ifndef __PANEL_H__
#define __PANEL_H__

#include <libbase/libbase.h>

/** Forward declaration: An element of a layer, we call it: Panel. */
typedef struct _wlmtk_panel_t wlmtk_panel_t;
/** Forward declaration: The panel's virtual method table. */
typedef struct _wlmtk_panel_vmt_t wlmtk_panel_vmt_t;

#include "container.h"
#include "layer.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** The panel's virtual method table. */
struct _wlmtk_panel_vmt_t {
    /**
     * Requests the panel to change to the specified size.
     *
     * This may be implemented as an asynchronous implementation. Once the
     * panel has committed the adapted size, @ref wlmtk_panel_commit should
     * be called with the corresponding serial.
     *
     * @param content_ptr
     * @param width_ptr
     * @param height
     *
     * @return WLR Layer Shell configuration serial.
     */
    uint32_t (*request_size)(wlmtk_panel_t *panel_ptr,
                             int width,
                             int size);
};

/** State of the panel. */
struct _wlmtk_panel_t {
    /** Super class of the panel. */
    wlmtk_container_t         super_container;
    /** The panel's virtual method table. */
    wlmtk_panel_vmt_t         vmt;

    /** Virtual method table of the superclass' container. */
    wlmtk_container_vmt_t     orig_super_container_vmt;

    /** The layer that this panel belongs to. NULL if none. */
    wlmtk_layer_t             *layer_ptr;
    /** Node of @ref wlmtk_layer_t::panels. */
    bs_dllist_node_t          dlnode;

    /** Width of the panel. */
    int                       width;
    /** Height of the panel. */
    int                       height;
    /** Edges the panel is anchored to. See `enum wlr_edges`. */
    uint32_t                  anchor;

    /** Margin on the left of the panel. */
    int                       margin_left;
    /** Margin on the right of the panel. */
    int                       margin_right;
    /** Margin on the top of the panel. */
    int                       margin_top;
    /** Margin on the bottom of the panel. */
    int                       margin_bottom;
};

/**
 * Initializes the panel.
 *
 * @param panel_ptr
 * @param env_ptr
 *
 * @return true on success.
 */
bool wlmtk_panel_init(wlmtk_panel_t *panel_ptr,
                      wlmtk_env_t *env_ptr);

/**
 * Un-initializes the panel.
 *
 * @param panel_ptr
 */
void wlmtk_panel_fini(wlmtk_panel_t *panel_ptr);

/**
 * Extends the panel by the specified virtual methods.
 *
 * @param panel_ptr
 * @param panel_vmt_ptr
 *
 * @return The original virtual method table.
 */
wlmtk_panel_vmt_t wlmtk_panel_extend(
    wlmtk_panel_t *panel_ptr,
    const wlmtk_panel_vmt_t *panel_vmt_ptr);

/** @return pointer to the super @ref wlmtk_element_t of the panel. */
wlmtk_element_t *wlmtk_panel_element(wlmtk_panel_t *panel_ptr);

/** @return Pointer to @ref wlmtk_panel_t::dlnode. */
bs_dllist_node_t *wlmtk_dlnode_from_panel(wlmtk_panel_t *panel_ptr);
/** @return Pointer to @ref wlmtk_panel_t for the given dlnode. */
wlmtk_panel_t *wlmtk_panel_from_dlnode(bs_dllist_node_t *dlnode_ptr);

/**
 * Sets the layer for the `panel_ptr`.
 *
 * @protected This method must only be called from @ref wlmtk_layer_t.
 *
 * @param panel_ptr
 * @param layer_ptr
 */
void wlmtk_panel_set_layer(wlmtk_panel_t *panel_ptr,
                           wlmtk_layer_t *layer_ptr);

/** @return the wlmtk_layer_t this panel belongs to. Or NULL, if unmapped. */
wlmtk_layer_t *wlmtk_panel_get_layer(wlmtk_panel_t *panel_ptr);

/** Requests new size. See @ref wlmtk_panel_vmt_t::request_size. */
static inline uint32_t wlmtk_panel_request_size(
    wlmtk_panel_t *panel_ptr,
    int width,
    int height) {
    return panel_ptr->vmt.request_size(panel_ptr, width, height);
}

/**
 * Commits serial
 *
 * @param panel_ptr
 * @param serial
 */
void wlmtk_panel_commit(
    wlmtk_panel_t *panel_ptr,
    uint32_t serial);

/**
 * Computes the requested dimension for the panel.
 *
 * @param panel_ptr
 * @param full_area_ptr
 * @param usable_area_ptr     Are that remains usable from the output and layer
 *                            after factoring in other panels. *usable_area_ptr
 *                            will be updated with this panel's exclusive area
 *                            (if any) subtracted.
 *
 * @return A wlr_box with the requested position and size for this panel. The
 * caller is advised to issue a call to @ref wlmtk_panel_request_size call and
 * update the panel element's position with the box' information.
 */
struct wlr_box wlmtk_panel_compute_dimensions(
    const wlmtk_panel_t *panel_ptr,
    const struct wlr_box *full_area_ptr,
    struct wlr_box *usable_area_ptr);

/** Unit test cases of panel. */
extern const bs_test_case_t wlmtk_panel_test_cases[];

/** Forward declaration: Fake panel, for tests. */
typedef struct _wlmtk_fake_panel_t wlmtk_fake_panel_t;
/** State of fake panel. */
struct _wlmtk_fake_panel_t {
    /** Superclass: panel. */
    wlmtk_panel_t             panel;
    /** Serial to return on next request_size call. */
    uint32_t                  serial;
    /** `width` argument eof last @ref wlmtk_content_request_size call. */
    int                       requested_width;
    /** `height` argument of last @ref wlmtk_content_request_size call. */
    int                       requested_height;
};
/** Creates a fake panel, for tests. */
wlmtk_fake_panel_t *wlmtk_fake_panel_create(
    uint32_t anchor, int width, int height);
/** Destroys the fake panel. */
void wlmtk_fake_panel_destroy(wlmtk_fake_panel_t *fake_panel_ptr);


#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __PANEL_H__ */
/* == End of panel.h ======================================================= */
