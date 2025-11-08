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

#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <toolkit/toolkit.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#define WLR_USE_UNSTABLE
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/version.h>
#undef WLR_USE_UNSTABLE

#include "output.h"
#include "output_config.h"
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

    /**
     * A list of @ref wlmbe_output_config_t items, configured through
     * wlmaker's configuration file.
     *
     * Discovered outputs are attempted to matched through
     * @ref wlmbe_output_config_fnmatches for configured attributes.
     */
    bs_dllist_t               output_configs;
    /**
     * Another list of @ref wlmbe_output_config_t items. This is
     * initialized from wlmaker's state file.
     *
     * If a discovered output equals to one of the nodes here, it's attributes
     * will be taken from here. Otherwise, a new entry is created in this list.
     * The intent is to memorize state of connected configs, so that
     * re-connected outputs are using the same attributes they left with.
     */
    bs_dllist_t               ephemeral_output_configs;

    /** List of outputs. Connects @ref wlmbe_output_t::dlnode. */
    bs_dllist_t               outputs;

    // Elements below not owned by @ref wlmbe_backend_t.
    /** Back-link to the wlroots scene. */
    struct wlr_scene          *wlr_scene_ptr;
    /** Points to struct wlr_output_layout. */
    struct wlr_output_layout  *wlr_output_layout_ptr;

    /** Name of the file for storing the output's state. */
    char                      *state_fname_ptr;
};

static void _wlmbe_backend_handle_new_output(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static bool _wlmbe_backend_decode_item(
    bspl_object_t *obj_ptr,
    size_t i,
    void *dest_ptr);
static bspl_object_t *_wlmbe_backend_encode_all(
    const union bspl_desc_value *desc_value_ptr,
    const void *value_ptr);
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
                    _wlmbe_backend_encode_all,
                    NULL,  // init.
                    _wlmbe_backend_decode_fini),
    BSPL_DESC_SENTINEL(),
};

/** Descriptor for the output state, stored as plist. */
static const bspl_desc_t _wlmbe_outputs_state_desc[] = {
    BSPL_DESC_ARRAY("Outputs", true, wlmbe_backend_t, ephemeral_output_configs,
                    ephemeral_output_configs,
                    _wlmbe_backend_decode_item,
                    _wlmbe_backend_encode_all,
                    NULL,
                    _wlmbe_backend_decode_fini),
    BSPL_DESC_SENTINEL(),
};

/** Magnification factor: 3rd square root of 2.0. ~10, doubles exactly. */
static const double _wlmbke_backend_magnification = 1.0905077326652577;

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmbe_backend_t *wlmbe_backend_create(
    struct wl_display *wl_display_ptr,
    struct wlr_scene *wlr_scene_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr,
    int width,
    int height,
    bspl_dict_t *config_dict_ptr,
    const char *state_fname_ptr)
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

    // Loads state (if available).
    backend_ptr->state_fname_ptr = logged_strdup(state_fname_ptr);
    if (NULL == backend_ptr->state_fname_ptr) {
        wlmbe_backend_destroy(backend_ptr);
        return NULL;
    }
    bspl_object_t *o = bspl_create_object_from_plist_file(
        backend_ptr->state_fname_ptr);
    if (NULL != o) {
        bool rv = bspl_decode_dict(
            bspl_dict_from_object(o),
            _wlmbe_outputs_state_desc,
            backend_ptr);
        bspl_object_unref(o);
        if (!rv) {
            wlmbe_backend_destroy(backend_ptr);
            return NULL;
        }
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

    if (NULL != backend_ptr->state_fname_ptr) {
        free(backend_ptr->state_fname_ptr);
        backend_ptr->state_fname_ptr = NULL;
    }

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

/* ------------------------------------------------------------------------- */
void wlmbe_backend_magnify(wlmbe_backend_t *backend_ptr)
{
    wlmbe_output_manager_scale(
        backend_ptr->output_manager_ptr,
        _wlmbke_backend_magnification);
}

/* ------------------------------------------------------------------------- */
void wlmbe_backend_reduce(wlmbe_backend_t *backend_ptr)
{
    wlmbe_output_manager_scale(
        backend_ptr->output_manager_ptr,
        1.0 / _wlmbke_backend_magnification);
}

/* ------------------------------------------------------------------------- */
bool wlmbe_backend_save_ephemeral_state(wlmbe_backend_t *backend_ptr)
{
    bspl_dict_t *dict_ptr = bspl_encode_dict(
        _wlmbe_outputs_state_desc,
        backend_ptr);
    if (NULL != dict_ptr) {
        bs_dynbuf_t dynbuf = {};
        if (bs_dynbuf_init(&dynbuf, getpagesize(), SIZE_MAX)) {
            if (bspl_object_write(
                    bspl_object_from_dict(dict_ptr),
                    &dynbuf) &&
                bs_dynbuf_write_file(
                    &dynbuf,
                    backend_ptr->state_fname_ptr,
                    S_IRUSR | S_IWUSR)) {
                return true;
            }
            bs_dynbuf_fini(&dynbuf);
        }
        bspl_dict_unref(dict_ptr);
    }
    return false;
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
    const wlmbe_output_config_attributes_t *attr_ptr =
        wlmbe_output_attributes(output_ptr);
    BS_ASSERT(NULL != attr_ptr);

    struct wlr_output *wlrop = wlmbe_wlr_output_from_output(output_ptr);
    struct wlr_output_layout_output *wlr_output_layout_output_ptr = NULL;
    if (attr_ptr->has_position) {
        wlr_output_layout_output_ptr = wlr_output_layout_add(
            backend_ptr->wlr_output_layout_ptr, wlrop,
            attr_ptr->position.x, attr_ptr->position.y);
    } else {
        wlr_output_layout_output_ptr = wlr_output_layout_add_auto(
            backend_ptr->wlr_output_layout_ptr, wlrop);
    }
    if (NULL == wlr_output_layout_output_ptr) {
        bs_log(BS_WARNING,
               "Failed wlr_output_layout_add(%p, %p, %d, %d) for \"%s\"",
               backend_ptr->wlr_output_layout_ptr, wlrop,
               attr_ptr->position.x, attr_ptr->position.y,
               wlrop->name);
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

    bs_log(BS_INFO,
           "Created: Output <%s> %s to %dx%d@%.2f position (%d,%d) %s",
           wlmbe_output_description(output_ptr),
           wlrop->enabled ? "enabled" : "disabled",
           wlrop->width, wlrop->height,
           1e-3 * wlrop->refresh,
           wlr_output_layout_output_ptr->x,
           wlr_output_layout_output_ptr->y,
           attr_ptr->has_position ? "explicit" : "auto");
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

    // See if there is an exact match among the ephemeral output configs. If
    // yes, pick that configuration. Otherwise, create a new one.
    wlmbe_output_config_t *config_ptr = wlmbe_output_config_from_dlnode(
        bs_dllist_find(
            &backend_ptr->ephemeral_output_configs,
            wlmbe_output_config_equals,
            wlr_output_ptr));
    if (NULL != config_ptr &&
        !wlmbe_output_config_attributes(config_ptr)->enabled) {
        // Explicitly configured to be disabled. Skip that output.
        wlr_output_destroy(wlr_output_ptr);
        return;
    }
    if (NULL == config_ptr) {
        config_ptr = wlmbe_output_config_create_from_wlr(wlr_output_ptr);
        bs_dllist_push_front(
            &backend_ptr->ephemeral_output_configs,
            wlmbe_dlnode_from_output_config(config_ptr));
    }

    // See if we have a corresponding entry among configured outputs. If yes,
    // apply the attributes to our new config.
    wlmbe_output_config_t *outputs_config_ptr =
        wlmbe_output_config_from_dlnode(
            bs_dllist_find(
                &backend_ptr->output_configs,
                wlmbe_output_config_fnmatches,
                wlr_output_ptr));
    if (NULL != outputs_config_ptr) {
        wlmbe_output_config_apply_attributes(
            config_ptr,
            wlmbe_output_config_attributes(outputs_config_ptr));
    }

    wlmbe_output_t *output_ptr = wlmbe_output_create(
        wlr_output_ptr,
        backend_ptr->wlr_allocator_ptr,
        backend_ptr->wlr_renderer_ptr,
        backend_ptr->wlr_scene_ptr,
        config_ptr,
        backend_ptr->width,
        backend_ptr->height);
    if (NULL != output_ptr) {
        if (_wlmbe_backend_add_output(backend_ptr, output_ptr)) return;
        wlmbe_output_destroy(output_ptr);
    }
    wlr_output_destroy(wlr_output_ptr);
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

    wlmbe_output_config_t *config_ptr =
        wlmbe_output_config_create_from_plist(dict_ptr);
    if (NULL != config_ptr) {
        bs_dllist_push_back(
            dllist_ptr,
            wlmbe_dlnode_from_output_config(config_ptr));

        return true;
    }
    return false;
}

/* ------------------------------------------------------------------------- */
/** Encodes the dllist item and stores it into the array at `ud_ptr`. */
bool _wlmbe_backend_encode_item(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    bspl_array_t *array_ptr = ud_ptr;

    wlmbe_output_config_t *c = wlmbe_output_config_from_dlnode(dlnode_ptr);

    bspl_dict_t *dict_ptr = wlmbe_output_config_create_into_plist(c);
    if (NULL == dict_ptr) return false;

    bool rv = bspl_array_push_back(array_ptr, bspl_object_from_dict(dict_ptr));
    bspl_dict_unref(dict_ptr);
    return rv;
}

/* ------------------------------------------------------------------------- */
/** Encodes all array (items) into a plist array. */
bspl_object_t *_wlmbe_backend_encode_all(
    __UNUSED__ const union bspl_desc_value *desc_value_ptr,
    const void *value_ptr)
{
    const bs_dllist_t *output_configs_ptr = value_ptr;

    bspl_array_t *array_ptr = bspl_array_create();
    if (NULL != array_ptr &&
        bs_dllist_all(
            output_configs_ptr,
            _wlmbe_backend_encode_item,
            array_ptr)) {
        return bspl_object_from_array(array_ptr);
    }

    bspl_array_unref(array_ptr);
    return NULL;
}

/* ------------------------------------------------------------------------- */
/** Frees all resources of the @ref wlmbe_output_config_t in the list. */
void _wlmbe_backend_decode_fini(void *dest_ptr)
{
    bs_dllist_t *dllist_ptr = dest_ptr;
    bs_dllist_for_each(dllist_ptr,
        _wlmbe_backend_config_dlnode_destroy,
        NULL);
}

/* ------------------------------------------------------------------------- */
/**
 * Iterator callback: Destroys a @ref wlmbe_output_config_t.
 *
 * @param dlnode_ptr          To @ref wlmbe_output_config_t::dlnode, and
 *                            identifies the config node to destroy.
 * @param ud_ptr              unused.
 */
void _wlmbe_backend_config_dlnode_destroy(
    bs_dllist_node_t *dlnode_ptr,
    __UNUSED__ void *ud_ptr)
{
    wlmbe_output_config_destroy(wlmbe_output_config_from_dlnode(dlnode_ptr));
}

/* == Unit tests =========================================================== */

static void _wlmbe_backend_test_find(bs_test_t *test_ptr);
static void _wlmbe_backend_test_encode_state(bs_test_t *test_ptr);

const bs_test_case_t          wlmbe_backend_test_cases[] = {
    { 1, "find", _wlmbe_backend_test_find },
    { 1, "encode_state", _wlmbe_backend_test_encode_state },
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
    BS_TEST_VERIFY_EQ(
        test_ptr, 2, bs_dllist_size(&be.ephemeral_output_configs));

    wlmbe_output_config_t *o;
    struct wlr_output wlr_output = {};

    // These are found in the configured state.
    wlr_output.name = "DP-0";
    o = wlmbe_output_config_from_dlnode(
        bs_dllist_find(
            &be.ephemeral_output_configs,
            wlmbe_output_config_equals,
            &wlr_output));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, o);
    BS_TEST_VERIFY_EQ(test_ptr, 2.0, wlmbe_output_config_attributes(o)->scale);


    wlr_output.name = "DP-1";
    o = wlmbe_output_config_from_dlnode(
        bs_dllist_find(
            &be.ephemeral_output_configs,
            wlmbe_output_config_equals,
            &wlr_output));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, o);
    BS_TEST_VERIFY_EQ(test_ptr, 3.0, wlmbe_output_config_attributes(o)->scale);

    // Only has a fit through config match. Will add a state entry.
    wlr_output.name = "DP-2";
    o = wlmbe_output_config_from_dlnode(
        bs_dllist_find(
            &be.output_configs,
            wlmbe_output_config_fnmatches,
            &wlr_output));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, o);
    BS_TEST_VERIFY_EQ(test_ptr, 1.0, wlmbe_output_config_attributes(o)->scale);

    bspl_decoded_destroy(_wlmbe_outputs_state_desc, &be);
    bspl_decoded_destroy(_wlmbe_output_configs_desc, &be);
    bspl_dict_unref(state_dict_ptr);
    bspl_dict_unref(config_dict_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests encoding of the output state */
void _wlmbe_backend_test_encode_state(bs_test_t *test_ptr)
{
    wlmbe_backend_t backend = {};

    struct wlr_output w = {
        .name = "Name0",
        .width = 1024,
        .height = 768,
        .scale = 2.0,
        .refresh = 60000,
    };

    wlmbe_output_config_t *c1 = wlmbe_output_config_create_from_wlr(&w);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, c1);
    bs_dllist_push_back(
        &backend.ephemeral_output_configs,
        wlmbe_dlnode_from_output_config(c1));

    w = (struct wlr_output){
        .enabled = true,
        .name = "Name1",
        .width = 640,
        .height = 480,
        .scale = 1.0,
        .refresh = 80000
    };
    wlmbe_output_config_t *c2 = wlmbe_output_config_create_from_wlr(&w);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, c2);
    bs_dllist_push_back(
        &backend.ephemeral_output_configs,
        wlmbe_dlnode_from_output_config(c2));


    bspl_dict_t *dict_ptr = bspl_encode_dict(
        _wlmbe_outputs_state_desc,
        &backend);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, dict_ptr);

    bs_dynbuf_t dynbuf = {};
    BS_TEST_VERIFY_TRUE(test_ptr, bs_dynbuf_init(&dynbuf, 1024, SIZE_MAX));
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        bspl_object_write(bspl_object_from_dict(dict_ptr), &dynbuf));

    static const char *expected =
        "{\n"
        "  Outputs = (\n"
        "    {\n"
        "      Enabled = False;\n"
        "      Mode = \"1024x768@60.0\";\n"
        "      Name = Name0;\n"
        "      Scale = \"2.000000e+00\";\n"
        "      Transformation = Normal;\n"
        "    },\n"
        "    {\n"
        "      Enabled = True;\n"
        "      Mode = \"640x480@80.0\";\n"
        "      Name = Name1;\n"
        "      Scale = \"1.000000e+00\";\n"
        "      Transformation = Normal;\n"
        "    }\n"
        "  );\n"
        "}\n";
    BS_TEST_VERIFY_MEMEQ(test_ptr, expected, dynbuf.data_ptr, dynbuf.length);

    bs_dynbuf_fini(&dynbuf);
    bspl_dict_unref(dict_ptr);
    wlmbe_output_config_destroy(c2);
    wlmbe_output_config_destroy(c1);
}

/* == End of backend.c ===================================================== */
