/* ========================================================================= */
/**
 * @file backend.c
 *
 * @copyright
 * Copyright 2025 Google LLC
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

#include "backend.h"

#include <conf/decode.h>
#include <libbase/libbase.h>
#include <toolkit/toolkit.h>

#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/version.h>
#undef WLR_USE_UNSTABLE

#include "output.h"
#include "output_manager.h"

/* == Declarations ========================================================= */

/** State of the server's backend. */
struct _wlmbe_backend_t {
    /** wlroots backend. */
    struct wlr_backend        *wlr_backend_ptr;
    /** wlroots session. Populated from wlr_backend_autocreate(). */
    struct wlr_session        *wlr_session_ptr;
    /** wlroots renderer. */
    struct wlr_renderer       *wlr_renderer_ptr;
    /** The allocator. */
    struct wlr_allocator      *wlr_allocator_ptr;
    /** The scene output layout. */
    struct wlr_scene_output_layout *wlr_scene_output_layout_ptr;
    /** The compositor is necessary for clients to allocate surfaces. */
    struct wlr_compositor     *wlr_compositor_ptr;
    /**
     * The subcompositor allows to assign the role of subsurfaces to
     * surfaces.
     */
    struct wlr_subcompositor  *wlr_subcompositor_ptr;
    /** The output manager(s). */
    wlmbe_output_manager_t    *output_manager_ptr;

    /** Listener for wlr_backend::events::new_input. */
    struct wl_listener        new_output_listener;

    /** Desired output width, for windowed mode. 0 for no preference. */
    uint32_t                  width;
    /** Desired output height, for windowed mode. 0 for no preference. */
    uint32_t                  height;
    /** Default configuration for outputs. */
    wlmbe_output_config_t     output_config;
    /** List of outputs. Connects @ref wlmaker_output_t::node. */
    bs_dllist_t               outputs;

    // Elements below not owned by @ref wlmbe_backend_t.
    /** Back-link to the wlroots scene. */
    struct wlr_scene          *wlr_scene_ptr;
    /** Points to struct wlr_output_layout. */
    struct wlr_output_layout  *wlr_output_layout_ptr;
};

static bool _wlmbe_output_config_parse(
    wlmcfg_dict_t *config_dict_ptr,
    wlmbe_output_config_t *config_ptr);
static void _wlmbe_backend_handle_new_output(
    struct wl_listener *listener_ptr,
    void *data_ptr);

/* == Data ================================================================= */

/** Name of the plist dict describing the (default) output configuration. */
static const char *_wlmbe_output_dict_name = "Output";

/** Descriptor for output transformations. */
static const wlmcfg_enum_desc_t _wlmbe_output_transformation_desc[] = {
    WLMCFG_ENUM("Normal", WL_OUTPUT_TRANSFORM_NORMAL),
    WLMCFG_ENUM("Rotate90", WL_OUTPUT_TRANSFORM_90),
    WLMCFG_ENUM("Rotate180", WL_OUTPUT_TRANSFORM_180),
    WLMCFG_ENUM("Rotate270", WL_OUTPUT_TRANSFORM_270),
    WLMCFG_ENUM("Flip", WL_OUTPUT_TRANSFORM_FLIPPED),
    WLMCFG_ENUM("FlipAndRotate90", WL_OUTPUT_TRANSFORM_FLIPPED_90),
    WLMCFG_ENUM("FlipAndRotate180", WL_OUTPUT_TRANSFORM_FLIPPED_180),
    WLMCFG_ENUM("FlipAndRotate270", WL_OUTPUT_TRANSFORM_FLIPPED_270),
    WLMCFG_ENUM_SENTINEL(),
};

/** Descriptor for the output configuration. */
static const wlmcfg_desc_t    _wlmbe_output_config_desc[] = {
    WLMCFG_DESC_ENUM("Transformation", true,
                     wlmbe_output_config_t, transformation,
                     WL_OUTPUT_TRANSFORM_NORMAL,
                     _wlmbe_output_transformation_desc),
    WLMCFG_DESC_DOUBLE("Scale", true, wlmbe_output_config_t, scale, 1.0),
    WLMCFG_DESC_SENTINEL()
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmbe_backend_t *wlmbe_backend_create(
    struct wl_display *wl_display_ptr,
    struct wlr_scene *wlr_scene_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    int width,
    int height,
    wlmcfg_dict_t *config_dict_ptr)
{
    wlmbe_backend_t *backend_ptr = logged_calloc(1, sizeof(wlmbe_backend_t));
    if (NULL == backend_ptr) return NULL;
    backend_ptr->wlr_scene_ptr = wlr_scene_ptr;
    backend_ptr->wlr_output_layout_ptr = wlr_output_layout_ptr;
    backend_ptr->width = width;
    backend_ptr->height = height;

    if (!_wlmbe_output_config_parse(
            config_dict_ptr,
            &backend_ptr->output_config)) {
        wlmbe_backend_destroy(backend_ptr);
        return NULL;
    }

    // Auto-create the wlroots backend. Can be X11 or direct.
    backend_ptr->wlr_backend_ptr = wlr_backend_autocreate(
#if WLR_VERSION_NUM >= (18 << 8)
        wl_display_get_event_loop(wl_display_ptr),
#else // WLR_VERSION_NUM >= (18 << 8)
        wl_display_ptr,
#endif // WLR_VERSION_NUM >= (18 << 8)
        &backend_ptr->wlr_session_ptr);
    if (NULL == backend_ptr->wlr_backend_ptr) {
        bs_log(BS_ERROR, "Failed wlr_backend_autocreate()");
        wlmbe_backend_destroy(backend_ptr);
        return NULL;
    }

    // Auto-create a renderer. Can be specified using WLR_RENDERER env var.
    backend_ptr->wlr_renderer_ptr = wlr_renderer_autocreate(
        backend_ptr->wlr_backend_ptr);
    if (NULL == backend_ptr->wlr_renderer_ptr) {
        bs_log(BS_ERROR, "Failed wlr_renderer_autocreate()");
        wlmbe_backend_destroy(backend_ptr);
        return NULL;
    }
    if (!wlr_renderer_init_wl_display(
            backend_ptr->wlr_renderer_ptr, wl_display_ptr)) {
        bs_log(BS_ERROR, "Failed wlr_render_init_wl_display()");
        wlmbe_backend_destroy(backend_ptr);
        return NULL;
    }

    backend_ptr->wlr_allocator_ptr = wlr_allocator_autocreate(
        backend_ptr->wlr_backend_ptr,
        backend_ptr->wlr_renderer_ptr);
    if (NULL == backend_ptr->wlr_allocator_ptr) {
        bs_log(BS_ERROR, "Failed wlr_allocator_autocreate()");
        wlmbe_backend_destroy(backend_ptr);
        return NULL;
    }

    backend_ptr->wlr_scene_output_layout_ptr =
        wlr_scene_attach_output_layout(
            wlr_scene_ptr,
            backend_ptr->wlr_output_layout_ptr);
    if (NULL == backend_ptr->wlr_scene_output_layout_ptr) {
        bs_log(BS_ERROR, "Failed wlr_scene_attach_output_layout()");
        wlmbe_backend_destroy(backend_ptr);
        return NULL;
    }

    backend_ptr->wlr_compositor_ptr = wlr_compositor_create(
        wl_display_ptr, 5, backend_ptr->wlr_renderer_ptr);
    if (NULL == backend_ptr->wlr_compositor_ptr) {
        bs_log(BS_ERROR, "Failed wlr_compositor_create()");
        wlmbe_backend_destroy(backend_ptr);
        return NULL;
    }
    backend_ptr->wlr_subcompositor_ptr = wlr_subcompositor_create(
        wl_display_ptr);
    if (NULL == backend_ptr->wlr_subcompositor_ptr) {
        bs_log(BS_ERROR, "Failed wlr_subcompositor_create()");
        wlmbe_backend_destroy(backend_ptr);
        return NULL;
    }

    backend_ptr->output_manager_ptr = wlmbe_output_manager_create(
        wl_display_ptr,
        wlr_scene_ptr,
        wlr_output_layout_ptr,
        backend_ptr->wlr_backend_ptr);
    if (NULL == backend_ptr->output_manager_ptr) {
        wlmbe_backend_destroy(backend_ptr);
        return NULL;
    }

    wlmtk_util_connect_listener_signal(
        &backend_ptr->wlr_backend_ptr->events.new_output,
        &backend_ptr->new_output_listener,
        _wlmbe_backend_handle_new_output);

    return backend_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmbe_backend_destroy(wlmbe_backend_t *backend_ptr)
{
    wlmtk_util_disconnect_listener(&backend_ptr->new_output_listener);

    if (NULL != backend_ptr->output_manager_ptr) {
        wlmbe_output_manager_destroy(backend_ptr->output_manager_ptr);
        backend_ptr->output_manager_ptr = NULL;
    }

    if (NULL != backend_ptr->wlr_renderer_ptr) {
        wlr_renderer_destroy(backend_ptr->wlr_renderer_ptr);
        backend_ptr->wlr_renderer_ptr = NULL;
    }

     if (NULL != backend_ptr->wlr_allocator_ptr) {
        wlr_allocator_destroy(backend_ptr->wlr_allocator_ptr);
        backend_ptr->wlr_allocator_ptr = NULL;
     }

    // @ref wlmbe_backend_t::wlr_backend_ptr is destroyed from wl_display.

    free(backend_ptr);
}

/* ------------------------------------------------------------------------- */
size_t wlmbe_backend_outputs(wlmbe_backend_t *backend_ptr)
{
    return bs_dllist_size(&backend_ptr->outputs);
}

/* ------------------------------------------------------------------------- */
struct wlr_output *wlmbe_backend_primary_output(
    wlmbe_backend_t *backend_ptr)
{
    if (bs_dllist_empty(&backend_ptr->outputs)) return NULL;

    return wlmbe_wlr_output_from_output(
        wlmbe_output_from_dlnode(backend_ptr->outputs.head_ptr));
}

/* ------------------------------------------------------------------------- */
void wlmbe_backend_switch_to_vt(wlmbe_backend_t *backend_ptr, unsigned vt_num)
{
    // Guard clause: @ref wlmbe_backend_t::session_ptr will be populated only
    // if wlroots created a session, eg. when running from the terminal.
    if (NULL == backend_ptr->wlr_session_ptr) {
        bs_log(BS_DEBUG, "wlmbe_backend_switch_to_vt: No session, ignored.");
        return;
    }

    if (!wlr_session_change_vt(backend_ptr->wlr_session_ptr, vt_num)) {
        bs_log(BS_WARNING, "Failed wlr_session_change_vt(, %u)", vt_num);
    }
}

/* ------------------------------------------------------------------------- */
struct wlr_backend *wlmbe_backend_wlr(wlmbe_backend_t *backend_ptr)
{
    return backend_ptr->wlr_backend_ptr;
}

/* ------------------------------------------------------------------------- */
struct wlr_compositor *wlmbe_backend_compositor(wlmbe_backend_t *backend_ptr)
{
    return backend_ptr->wlr_compositor_ptr;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Adds the output to the backend.
 *
 * @param backend_ptr
 * @param output_ptr
 *
 * @return true on success.
 */
bool _wlmbe_backend_add_output(
    wlmbe_backend_t *backend_ptr,
    wlmbe_output_t *output_ptr)
{
    // tinywl: Adds this to the output layout. The add_auto function arranges
    // outputs from left-to-right in the order they appear. A sophisticated
    // compositor would let the user configure the arrangement of outputs in
    // the layout.
    struct wlr_output *wlrop = wlmbe_wlr_output_from_output(output_ptr);
    struct wlr_output_layout_output *wlr_output_layout_output_ptr =
        wlr_output_layout_add_auto(backend_ptr->wlr_output_layout_ptr, wlrop);
    if (NULL == wlr_output_layout_output_ptr) {
        bs_log(BS_ERROR, "Failed wlr_output_layout_add_auto(%p, %p) for '%s'",
               backend_ptr->wlr_output_layout_ptr, wlrop, wlrop->name);
        return false;
    }

    struct wlr_scene_output *wlr_scene_output_ptr = wlr_scene_output_create(
        backend_ptr->wlr_scene_ptr, wlrop);
    wlr_scene_output_layout_add_output(
        backend_ptr->wlr_scene_output_layout_ptr,
        wlr_output_layout_output_ptr,
        wlr_scene_output_ptr);

    bs_dllist_push_back(
        &backend_ptr->outputs,
        wlmbe_dlnode_from_output(output_ptr));

    const char *tname_ptr = "Unknown";
    wlmcfg_enum_value_to_name(
        _wlmbe_output_transformation_desc, wlrop->transform, &tname_ptr);
    bs_log(BS_INFO, "Added output '%s' (%dx%d). Trsf '%s', Scale %.2f.",
           wlrop->name, wlrop->width, wlrop->height, tname_ptr, wlrop->scale);
    return true;
}

/* ------------------------------------------------------------------------- */
/** Parses the plist dictionnary into the @ref wlmbe_output_config_t. */
bool _wlmbe_output_config_parse(
    wlmcfg_dict_t *config_dict_ptr,
    wlmbe_output_config_t *config_ptr)
{
    wlmcfg_dict_t *output_dict_ptr = wlmcfg_dict_get_dict(
        config_dict_ptr, _wlmbe_output_dict_name);
    if (NULL == output_dict_ptr) {
        bs_log(BS_ERROR, "No '%s' dict.", _wlmbe_output_dict_name);
        return false;
    }

    if (!wlmcfg_decode_dict(
            output_dict_ptr,
            _wlmbe_output_config_desc,
            config_ptr)) {
        bs_log(BS_ERROR, "Failed to decode '%s' dict",
               _wlmbe_output_dict_name);
        return false;
    }

    return true;
}

/* ------------------------------------------------------------------------- */
/** Handles new output events: Creates @ref wlmbe_output_t and adds them.  */
void _wlmbe_backend_handle_new_output(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmbe_backend_t *backend_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmbe_backend_t, new_output_listener);
    struct wlr_output *wlr_output_ptr = data_ptr;

    wlmbe_output_t *output_ptr = wlmbe_output_create(
        wlr_output_ptr,
        backend_ptr->wlr_allocator_ptr,
        backend_ptr->wlr_renderer_ptr,
        backend_ptr->wlr_scene_ptr,
        backend_ptr->width,
        backend_ptr->height,
        &backend_ptr->output_config);
    if (NULL == output_ptr) return;

    if (!_wlmbe_backend_add_output(backend_ptr, output_ptr)) {
        wlmbe_output_destroy(output_ptr);
    }
}

/* == End of backend.c ===================================================== */
