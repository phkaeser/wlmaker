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

#include <backend/backend.h>
#include <backend/output.h>
#include <backend/output_manager.h>
#include <fnmatch.h>
#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <toolkit/toolkit.h>

#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/version.h>
#undef WLR_USE_UNSTABLE

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
    /** The screencopy manager. */
    struct wlr_screencopy_manager_v1 *wlr_screencopy_manager_v1_ptr;
    /** The output manager(s). */
    wlmbe_output_manager_t    *output_manager_ptr;

    /** Listener for wlr_backend::events::new_input. */
    struct wl_listener        new_output_listener;

    /** Desired output width, for windowed mode. 0 for no preference. */
    uint32_t                  width;
    /** Desired output height, for windowed mode. 0 for no preference. */
    uint32_t                  height;

    /** A list of @ref wlmbe_output_config_node_t items. */
    bs_dllist_t               output_configs;
    /** A list of @ref wlmbe_output_config_node_t items. */
    bs_dllist_t               state_output_configs;

    /** List of outputs. Connects @ref wlmbe_output_t::dlnode. */
    bs_dllist_t               outputs;

    // Elements below not owned by @ref wlmbe_backend_t.
    /** Back-link to the wlroots scene. */
    struct wlr_scene          *wlr_scene_ptr;
    /** Points to struct wlr_output_layout. */
    struct wlr_output_layout  *wlr_output_layout_ptr;
};

/** One output configuration. */
typedef struct {
    /** Element of @ref wlmbe_backend_t::output_configs. */
    bs_dllist_node_t          dlnode;
    /** The configurtation. */
    wlmbe_output_config_t     config;
} wlmbe_output_config_node_t;

static void _wlmbe_backend_handle_new_output(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static bool _wlmbe_output_config_equals(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static bool _wlmbe_output_config_fnmatches(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static wlmbe_output_config_t *_wlmbe_backend_get_config(
    wlmbe_backend_t *backend_ptr,
    struct wlr_output *wlr_output_ptr);

static bool _wlmbe_backend_decode_item(
    bspl_object_t *obj_ptr,
    size_t i,
    void *dest_ptr);
static void _wlmbe_backend_decode_fini(void *dest_ptr);
static void _wlmbe_backend_config_dlnode_destroy(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);

/* == Data ================================================================= */

/** Descriptor for the output configuration. */
static const bspl_desc_t _wlmbe_output_configs_desc[] = {
    BSPL_DESC_ARRAY("Outputs", true, wlmbe_backend_t, output_configs,
                    output_configs,
                    _wlmbe_backend_decode_item,
                    NULL,
                    _wlmbe_backend_decode_fini),
    BSPL_DESC_SENTINEL(),
};

/** Descriptor for the output state, stored as plist. */
static const bspl_desc_t _wlmbe_outputs_state_desc[] = {
    BSPL_DESC_ARRAY("Outputs", true, wlmbe_backend_t, state_output_configs,
                    state_output_configs,
                    _wlmbe_backend_decode_item,
                    NULL,
                    _wlmbe_backend_decode_fini),
    BSPL_DESC_SENTINEL(),
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmbe_backend_t *wlmbe_backend_create(
    struct wl_display *wl_display_ptr,
    struct wlr_scene *wlr_scene_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    int width,
    int height,
    bspl_dict_t *config_dict_ptr)
{
    wlmbe_backend_t *backend_ptr = logged_calloc(1, sizeof(wlmbe_backend_t));
    if (NULL == backend_ptr) return NULL;
    backend_ptr->wlr_scene_ptr = wlr_scene_ptr;
    backend_ptr->wlr_output_layout_ptr = wlr_output_layout_ptr;
    backend_ptr->width = width;
    backend_ptr->height = height;

    if (!bspl_decode_dict(
            config_dict_ptr,
            _wlmbe_output_configs_desc,
            backend_ptr)) {
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

    backend_ptr->wlr_screencopy_manager_v1_ptr =
        wlr_screencopy_manager_v1_create(wl_display_ptr);
    if (NULL == backend_ptr->wlr_screencopy_manager_v1_ptr) {
        bs_log(BS_ERROR, "Failed wlr_screencopy_manager_v1_create()");
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

    bspl_decoded_destroy(_wlmbe_outputs_state_desc, backend_ptr);
    bspl_decoded_destroy(_wlmbe_output_configs_desc, backend_ptr);

    // @ref wlmbe_backend_t::wlr_backend_ptr is destroyed from wl_display.

    free(backend_ptr);
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

/* ------------------------------------------------------------------------- */
struct wlr_output *wlmbe_primary_output(
    struct wlr_output_layout *wlr_output_layout_ptr)
{
    if (0 >= wl_list_length(&wlr_output_layout_ptr->outputs)) return NULL;

    struct wlr_output_layout_output* wolo = BS_CONTAINER_OF(
        wlr_output_layout_ptr->outputs.next,
        struct wlr_output_layout_output,
        link);
    return wolo->output;
}

/* ------------------------------------------------------------------------- */
size_t wlmbe_num_outputs(struct wlr_output_layout *wlr_output_layout_ptr)
{
    return wl_list_length(&wlr_output_layout_ptr->outputs);
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
    wlmbe_output_config_t *config_ptr = wlmbe_config_from_output(output_ptr);
    BS_ASSERT(NULL != config_ptr);

    struct wlr_output *wlrop = wlmbe_wlr_output_from_output(output_ptr);
    struct wlr_output_layout_output *wlr_output_layout_output_ptr = NULL;
    if (config_ptr->attr.has_position) {
        wlr_output_layout_output_ptr = wlr_output_layout_add(
            backend_ptr->wlr_output_layout_ptr, wlrop,
            config_ptr->attr.position.x, config_ptr->attr.position.y);
    } else {
        wlr_output_layout_output_ptr = wlr_output_layout_add_auto(
            backend_ptr->wlr_output_layout_ptr, wlrop);
    }
    if (NULL == wlr_output_layout_output_ptr) {
        bs_log(BS_WARNING,
               "Failed wlr_output_layout_add(%p, %p, %d, %d) for \"%s\"",
               backend_ptr->wlr_output_layout_ptr, wlrop,
               config_ptr->attr.position.x, config_ptr->attr.position.y,
               wlrop->name);
        return false;
    }
    config_ptr->attr.position.x = wlr_output_layout_output_ptr->x;
    config_ptr->attr.position.y = wlr_output_layout_output_ptr->y;

    struct wlr_scene_output *wlr_scene_output_ptr = wlr_scene_output_create(
        backend_ptr->wlr_scene_ptr, wlrop);
    wlr_scene_output_layout_add_output(
        backend_ptr->wlr_scene_output_layout_ptr,
        wlr_output_layout_output_ptr,
        wlr_scene_output_ptr);

    bs_dllist_push_back(
        &backend_ptr->outputs,
        wlmbe_dlnode_from_output(output_ptr));

    bs_log(BS_INFO,
           "Created: Output '%s' %s to %dx%d@%.2f position (%d,%d) %s",
           wlrop->name,
           wlrop->enabled ? "enabled" : "disabled",
           wlrop->width, wlrop->height,
           1e-3 * wlrop->refresh,
           wlr_output_layout_output_ptr->x,
           wlr_output_layout_output_ptr->y,
           config_ptr->attr.has_position ? "explicit" : "auto");
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Returns if the backend configuration equals the wlr_output attributes.
 *
 * @param dlnode_ptr          To @ref wlmbe_output_config_node_t::dlnode.
 * @param ud_ptr              To a `struct wlr_output`.
 *
 * @return true if it equals.
 */
bool _wlmbe_output_config_equals(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    wlmbe_output_config_node_t *config_node_ptr = BS_CONTAINER_OF(
        dlnode_ptr, wlmbe_output_config_node_t, dlnode);
    struct wlr_output *wlr_output_ptr = ud_ptr;

    if (config_node_ptr->config.has_name) {
        if (NULL == wlr_output_ptr->name) return false;
        if (0 != strcmp(config_node_ptr->config.name_ptr,
                        wlr_output_ptr->name)) {
            return false;
        }
    }

    if (config_node_ptr->config.has_manufacturer) {
        if (NULL == wlr_output_ptr->make) return false;
        if (0 != strcmp(config_node_ptr->config.manufacturer_ptr,
                        wlr_output_ptr->make)) {
            return false;
        }
    }

    if (config_node_ptr->config.has_model) {
        if (NULL == wlr_output_ptr->model) return false;
        if (0 != strcmp(config_node_ptr->config.model_ptr,
                        wlr_output_ptr->model)) {
            return false;
        }
    }

    if (config_node_ptr->config.has_serial) {
        if (NULL == wlr_output_ptr->serial) return false;
        if (0 != strcmp(config_node_ptr->config.serial_ptr,
                        wlr_output_ptr->serial)) {
            return false;
        }
    }

    return (config_node_ptr->config.has_name ||
            config_node_ptr->config.has_manufacturer ||
            config_node_ptr->config.has_model ||
            config_node_ptr->config.has_serial);
}

/* ------------------------------------------------------------------------- */
/**
 * Returns if the backend configuration fnmatches the wlr_output attributes.
 *
 * @param dlnode_ptr          To @ref wlmbe_output_config_node_t::dlnode.
 * @param ud_ptr              To a `struct wlr_output`.
 *
 * @return true if it matches.
 */
bool _wlmbe_output_config_fnmatches(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    wlmbe_output_config_node_t *config_node_ptr = BS_CONTAINER_OF(
        dlnode_ptr, wlmbe_output_config_node_t, dlnode);
    struct wlr_output *wlr_output_ptr = ud_ptr;

    if (config_node_ptr->config.has_name) {
        if (NULL == wlr_output_ptr->name) return false;
        if (0 != fnmatch(config_node_ptr->config.name_ptr,
                         wlr_output_ptr->name,
                         0)) {
            return false;
        }
    }

    if (config_node_ptr->config.has_manufacturer) {
        if (NULL == wlr_output_ptr->make) return false;
        if (0 != fnmatch(config_node_ptr->config.manufacturer_ptr,
                         wlr_output_ptr->make,
                         0)) {
            return false;
        }
    }

    if (config_node_ptr->config.has_model) {
        if (NULL == wlr_output_ptr->model) return false;
        if (0 != fnmatch(config_node_ptr->config.model_ptr,
                         wlr_output_ptr->model,
                         0)) {
            return false;
        }
    }

    if (config_node_ptr->config.has_serial) {
        if (NULL == wlr_output_ptr->serial) return false;
        if (0 != fnmatch(config_node_ptr->config.serial_ptr,
                         wlr_output_ptr->serial,
                         0)) {
            return false;
        }
    }

    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Retrieves (or creates) an output configuration for `wlr_output_ptr`.
 *
 * Will first look if @ref wlmbe_backend_t::state_output_configs contains a
 * corresponding configuration, and return if found.
 *
 * If not, creates a new @ref wlmbe_output_config_node_t and adds it to
 * @ref wlmbe_backend_t::state_output_configs. If a matching configuration can
 * be found in @ref wlmbe_backend_t::output_configs, that new node will get
 * it's attributes initalized from there.
 *
 * @param backend_ptr
 * @param wlr_output_ptr
 *
 * @return Pointer to the output configuration, or NULL on error.
 */
wlmbe_output_config_t *_wlmbe_backend_get_config(
    wlmbe_backend_t *backend_ptr,
    struct wlr_output *wlr_output_ptr)
{
    wlmbe_output_config_node_t *config_node_ptr;
    bs_dllist_node_t *dlnode_ptr = bs_dllist_find(
        &backend_ptr->state_output_configs,
        _wlmbe_output_config_equals,
        wlr_output_ptr);
    if (NULL != dlnode_ptr) {
        // Found this output in @ref wlmbe_backend_t::state_output_configs,
        // meaning it had been connected earlier. In order for the output
        // management to find them, we always re-enable the output.
        config_node_ptr = BS_CONTAINER_OF(
            dlnode_ptr, wlmbe_output_config_node_t, dlnode);
        config_node_ptr->config.attr.enabled = true;
    } else {
        // This output is seen from wlmaker the first time. Note that the
        // configuration in @ref wlmbe_backend_t::output_configs may have that
        // output disabled. We will not overwrite that.
        config_node_ptr = logged_calloc(1, sizeof(wlmbe_output_config_node_t));
        if (NULL == config_node_ptr) return false;
        if (!wlmbe_output_config_init_from_wlr(
                &config_node_ptr->config,
                wlr_output_ptr)) {
            free(config_node_ptr);
            return false;
        }

        dlnode_ptr = bs_dllist_find(
            &backend_ptr->output_configs,
            _wlmbe_output_config_fnmatches,
            wlr_output_ptr);
        if (NULL != dlnode_ptr) {
            wlmbe_output_config_node_t *state_node_ptr = BS_CONTAINER_OF(
                dlnode_ptr, wlmbe_output_config_node_t, dlnode);
            config_node_ptr->config.attr = state_node_ptr->config.attr;
        }

        bs_dllist_push_back(&backend_ptr->state_output_configs,
                            &config_node_ptr->dlnode);
    }

    return &config_node_ptr->config;
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

    wlmbe_output_config_t *output_config_ptr =
        _wlmbe_backend_get_config(backend_ptr, wlr_output_ptr);
    if (NULL == output_config_ptr) return;

    if (!output_config_ptr->attr.enabled) {
        wlr_output_destroy(wlr_output_ptr);
        return;
    }

    wlmbe_output_t *output_ptr = wlmbe_output_create(
        wlr_output_ptr,
        backend_ptr->wlr_allocator_ptr,
        backend_ptr->wlr_renderer_ptr,
        backend_ptr->wlr_scene_ptr,
        output_config_ptr,
        backend_ptr->width,
        backend_ptr->height);
    if (NULL == output_ptr) return;

    if (!_wlmbe_backend_add_output(backend_ptr, output_ptr)) {
        wlmbe_output_destroy(output_ptr);
    }
}

/* ------------------------------------------------------------------------- */
/** Decodes an item of `Outputs`. */
bool _wlmbe_backend_decode_item(
    bspl_object_t *obj_ptr,
    size_t i,
    void *dest_ptr)
{
    bs_dllist_t *dllist_ptr = dest_ptr;
    bspl_dict_t *dict_ptr = bspl_dict_from_object(obj_ptr);
    if (NULL == dict_ptr) {
        bs_log(BS_WARNING, "Element %zu is not a dict", i);
        return false;
    }

    wlmbe_output_config_node_t *config_node_ptr = logged_calloc(
        1, sizeof(wlmbe_output_config_node_t));
    if (NULL == config_node_ptr) return false;

    if (wlmbe_output_config_init_from_plist(
            &config_node_ptr->config,
            dict_ptr)) {
        bs_dllist_push_back(dllist_ptr, &config_node_ptr->dlnode);
        return true;
    }

    _wlmbe_backend_config_dlnode_destroy(&config_node_ptr->dlnode, NULL);
    return true;
}

/* ------------------------------------------------------------------------- */
/** Frees all resources of the @ref wlmbe_output_config_node_t in the list. */
void _wlmbe_backend_decode_fini(void *dest_ptr)
{
    bs_dllist_t *dllist_ptr = dest_ptr;
    bs_dllist_for_each(dllist_ptr,
        _wlmbe_backend_config_dlnode_destroy,
        NULL);
}

/* ------------------------------------------------------------------------- */
/**
 * Iterator callback: Destroys a @ref wlmbe_output_config_node_t.
 *
 * @param dlnode_ptr          To @ref wlmbe_output_config_node_t::dlnode, and
 *                            identifies the config node to destroy.
 * @param ud_ptr              unused.
 */
void _wlmbe_backend_config_dlnode_destroy(
    bs_dllist_node_t *dlnode_ptr,
    __UNUSED__ void *ud_ptr)
{
    wlmbe_output_config_node_t *config_node_ptr = BS_CONTAINER_OF(
        dlnode_ptr, wlmbe_output_config_node_t, dlnode);
    wlmbe_output_config_fini(&config_node_ptr->config);
    free(config_node_ptr);
}

/* == Unit tests =========================================================== */

static void _wlmbe_backend_test_find(bs_test_t *test_ptr);

const bs_test_case_t          wlmbe_backend_test_cases[] = {
    { 1, "find", _wlmbe_backend_test_find },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests that output configurations are found as desired. */
void _wlmbe_backend_test_find(bs_test_t *test_ptr)
{
    wlmbe_backend_t be = {};
    bspl_dict_t *config_dict_ptr = bspl_dict_from_object(
        bspl_create_object_from_plist_string(
            "{Outputs = ("
            "{Transformation=Normal; Scale=1.0; Name=\"DP-*\"},"
            ")}"));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, config_dict_ptr);
    BS_TEST_VERIFY_TRUE_OR_RETURN(
        test_ptr,
        bspl_decode_dict(config_dict_ptr, _wlmbe_output_configs_desc, &be));

    bspl_dict_t *state_dict_ptr = bspl_dict_from_object(
        bspl_create_object_from_plist_string(
            "{Outputs = ("
            "{Transformation=Normal; Scale=2.0; Name=\"DP-0\"},"
            "{Transformation=Flip; Scale=3.0; Name=\"DP-1\"},"
            ")}"));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, state_dict_ptr);
    BS_TEST_VERIFY_TRUE_OR_RETURN(
        test_ptr,
        bspl_decode_dict(state_dict_ptr, _wlmbe_outputs_state_desc, &be));
    BS_TEST_VERIFY_EQ(test_ptr, 2, bs_dllist_size(&be.state_output_configs));

    wlmbe_output_config_t *o;
    struct wlr_output wlr_output = {};

    // These are found in the configured state.
    wlr_output.name = "DP-0";
    o = _wlmbe_backend_get_config(&be, &wlr_output);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, o);
    BS_TEST_VERIFY_EQ(test_ptr, 2.0, o->attr.scale);

    wlr_output.name = "DP-1";
    o = _wlmbe_backend_get_config(&be, &wlr_output);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, o);
    BS_TEST_VERIFY_EQ(test_ptr, 3.0, o->attr.scale);

    // Only has a fit through config match. Will add a state entry.
    wlr_output.name = "DP-2";
    o = _wlmbe_backend_get_config(&be, &wlr_output);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, o);
    BS_TEST_VERIFY_STREQ(test_ptr, "DP-2", o->name_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 1.0, o->attr.scale);
    BS_TEST_VERIFY_EQ(test_ptr, 3, bs_dllist_size(&be.state_output_configs));

    // No match, will create a new entry.
    wlr_output.name = "HDMI-0";
    o = _wlmbe_backend_get_config(&be, &wlr_output);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, o);
    BS_TEST_VERIFY_STREQ(test_ptr, "HDMI-0", o->name_ptr);
    BS_TEST_VERIFY_EQ(test_ptr, 0, o->attr.scale);
    BS_TEST_VERIFY_EQ(test_ptr, 4, bs_dllist_size(&be.state_output_configs));

    bspl_decoded_destroy(_wlmbe_outputs_state_desc, &be);
    bspl_decoded_destroy(_wlmbe_output_configs_desc, &be);
    bspl_dict_unref(state_dict_ptr);
    bspl_dict_unref(config_dict_ptr);
}

/* == End of backend.c ===================================================== */
