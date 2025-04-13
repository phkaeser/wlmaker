/* ========================================================================= */
/**
 * @file output.c
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

#include <backend/output.h>
#include <backend/output_manager.h>
#include <libbase/libbase.h>
#include <toolkit/toolkit.h>

#define WLR_USE_UNSTABLE
#include <wlr/backend/wayland.h>
#include <wlr/backend/x11.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_scene.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

/** Handle for a compositor output device. */
struct _wlmbe_output_t {
    /** List node for insertion in @ref wlmbe_backend_t::outputs. */
    bs_dllist_node_t          dlnode;

    /** Listener for `destroy` signals raised by `wlr_output`. */
    struct wl_listener        output_destroy_listener;
    /** Listener for `frame` signals raised by `wlr_output`. */
    struct wl_listener        output_frame_listener;
    /** Listener for `request_state` signals raised by `wlr_output`. */
    struct wl_listener        output_request_state_listener;

    // Below: Not owned by @ref wlmbe_output_t.
    /** Refers to the compositor output region, from wlroots. */
    struct wlr_output         *wlr_output_ptr;
    /** Refers to the scene graph used. */
    struct wlr_scene          *wlr_scene_ptr;
    /** Output configuration. */
    wlmbe_output_config_t     *config_ptr;
};

static void _wlmbe_output_handle_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmbe_output_handle_frame(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void _wlmbe_output_handle_request_state(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static bool _wlmbe_output_position_decode(
    bspl_object_t *object_ptr,
    void *dest_ptr);
static bool _wlmbe_output_position_decode_init(void *dest_ptr);

/* == Data ================================================================= */

/** Descriptor for output transformations. */
static const bspl_enum_desc_t _wlmbe_output_transformation_desc[] = {
    BSPL_ENUM("Normal", WL_OUTPUT_TRANSFORM_NORMAL),
    BSPL_ENUM("Rotate90", WL_OUTPUT_TRANSFORM_90),
    BSPL_ENUM("Rotate180", WL_OUTPUT_TRANSFORM_180),
    BSPL_ENUM("Rotate270", WL_OUTPUT_TRANSFORM_270),
    BSPL_ENUM("Flip", WL_OUTPUT_TRANSFORM_FLIPPED),
    BSPL_ENUM("FlipAndRotate90", WL_OUTPUT_TRANSFORM_FLIPPED_90),
    BSPL_ENUM("FlipAndRotate180", WL_OUTPUT_TRANSFORM_FLIPPED_180),
    BSPL_ENUM("FlipAndRotate270", WL_OUTPUT_TRANSFORM_FLIPPED_270),
    BSPL_ENUM_SENTINEL(),
};

/** Descriptor for the output configuration. */
static const bspl_desc_t    _wlmbe_output_config_desc[] = {
    BSPL_DESC_STRING(
        "Name", false, wlmbe_output_config_t,
        name_ptr, has_name, "*"),
    BSPL_DESC_STRING(
        "Manufacturer", false, wlmbe_output_config_t,
        manufacturer_ptr, has_manufacturer, ""),
    BSPL_DESC_STRING(
        "Model", false, wlmbe_output_config_t,
        model_ptr, has_model, ""),
    BSPL_DESC_STRING(
        "Serial", false, wlmbe_output_config_t,
        serial_ptr, has_serial, ""),
    BSPL_DESC_ENUM(
        "Transformation", true, wlmbe_output_config_t,
        attr.transformation, attr.transformation, WL_OUTPUT_TRANSFORM_NORMAL,
        _wlmbe_output_transformation_desc),
    BSPL_DESC_DOUBLE(
        "Scale", true, wlmbe_output_config_t,
        attr.scale, attr.scale, 1.0),
    BSPL_DESC_BOOL(
        "Enabled", false, wlmbe_output_config_t,
        attr.enabled, attr.enabled, true),
    BSPL_DESC_CUSTOM(
        "Position", false, wlmbe_output_config_t,
        attr.position, attr.has_position,
        _wlmbe_output_position_decode,
        _wlmbe_output_position_decode_init,
        NULL),
    BSPL_DESC_SENTINEL()
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmbe_output_t *wlmbe_output_create(
    struct wlr_output *wlr_output_ptr,
    struct wlr_allocator *wlr_allocator_ptr,
    struct wlr_renderer *wlr_renderer_ptr,
    struct wlr_scene *wlr_scene_ptr,
    wlmbe_output_config_t *config_ptr,
    int width,
    int height)
{
    wlmbe_output_t *output_ptr = logged_calloc(1, sizeof(wlmbe_output_t));
    if (NULL == output_ptr) return NULL;
    output_ptr->wlr_output_ptr = wlr_output_ptr;
    output_ptr->wlr_scene_ptr = wlr_scene_ptr;
    output_ptr->config_ptr = config_ptr;
    output_ptr->wlr_output_ptr->data = output_ptr;

    wlmtk_util_connect_listener_signal(
        &output_ptr->wlr_output_ptr->events.destroy,
        &output_ptr->output_destroy_listener,
        _wlmbe_output_handle_destroy);
    wlmtk_util_connect_listener_signal(
        &output_ptr->wlr_output_ptr->events.frame,
        &output_ptr->output_frame_listener,
        _wlmbe_output_handle_frame);
    wlmtk_util_connect_listener_signal(
        &output_ptr->wlr_output_ptr->events.request_state,
        &output_ptr->output_request_state_listener,
        _wlmbe_output_handle_request_state);

    // From tinwywl: Configures the output created by the backend to use our
    // allocator and our renderer. Must be done once, before commiting the
    // output.
    if (!wlr_output_init_render(
            output_ptr->wlr_output_ptr,
            wlr_allocator_ptr,
            wlr_renderer_ptr)) {
        bs_log(BS_ERROR, "Failed wlr_output_init_renderer() on %s",
               output_ptr->wlr_output_ptr->name);
        wlmbe_output_destroy(output_ptr);
        return NULL;
    }

    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, output_ptr->config_ptr->attr.enabled);
    wlr_output_state_set_scale(&state, output_ptr->config_ptr->attr.scale);

    // Issue #97: Found that X11 and transformations do not translate
    // cursor coordinates well. Force it to 'Normal'.
    enum wl_output_transform transformation =
        output_ptr->config_ptr->attr.transformation;
    if (wlr_output_is_x11(wlr_output_ptr) &&
        transformation != WL_OUTPUT_TRANSFORM_NORMAL) {
        bs_log(BS_WARNING, "X11 backend: Transformation changed to 'Normal'.");
        transformation = WL_OUTPUT_TRANSFORM_NORMAL;
    }
    wlr_output_state_set_transform(&state, transformation);

    // Set modes for backends that have them.
    if (!wl_list_empty(&output_ptr->wlr_output_ptr->modes)) {
        struct wlr_output_mode *mode_ptr = wlr_output_preferred_mode(
            output_ptr->wlr_output_ptr);
        bs_log(BS_INFO, "Setting mode %dx%d @ %.2fHz",
               mode_ptr->width, mode_ptr->height, 1e-3 * mode_ptr->refresh);
        wlr_output_state_set_mode(&state, mode_ptr);
    } else {
        bs_log(BS_INFO, "No modes available on %s",
               output_ptr->wlr_output_ptr->name);
    }

    if ((wlr_output_is_x11(wlr_output_ptr) ||
         wlr_output_is_wl(wlr_output_ptr))
        && 0 < width && 0 < height) {
        bs_log(BS_INFO, "Overriding output dimensions to %"PRIu32"x%"PRIu32,
               width, height);
        wlr_output_state_set_custom_mode(
            &state, width, height, 0);
    }

    if (!wlr_output_test_state(output_ptr->wlr_output_ptr, &state)) {
        bs_log(BS_ERROR, "Failed wlr_output_test_state() on %s",
               output_ptr->wlr_output_ptr->name);
        wlmbe_output_destroy(output_ptr);
        wlr_output_state_finish(&state);
        return NULL;
    }

    // Enable the output and commit.
    bool rv = wlr_output_commit_state(output_ptr->wlr_output_ptr, &state);
    wlr_output_state_finish(&state);
    if (!rv) {
        bs_log(BS_ERROR, "Failed wlr_output_commit_state() on %s",
               output_ptr->wlr_output_ptr->name);
        wlmbe_output_destroy(output_ptr);
        return NULL;
    }

    return output_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmbe_output_destroy(wlmbe_output_t *output_ptr)
{
    if (NULL != output_ptr->wlr_output_ptr) {
        bs_log(BS_INFO, "Destroy output %s", output_ptr->wlr_output_ptr->name);
    }

    _wlmbe_output_handle_destroy(&output_ptr->output_destroy_listener, NULL);
    free(output_ptr);
}

/* ------------------------------------------------------------------------- */
struct wlr_output *wlmbe_wlr_output_from_output(wlmbe_output_t *output_ptr)
{
    return output_ptr->wlr_output_ptr;
}

/* ------------------------------------------------------------------------- */
wlmbe_output_config_t *wlmbe_config_from_output(wlmbe_output_t *output_ptr)
{
    return output_ptr->config_ptr;
}

/* ------------------------------------------------------------------------- */
bs_dllist_node_t *wlmbe_dlnode_from_output(wlmbe_output_t *output_ptr)
{
    return &output_ptr->dlnode;
}

/* ------------------------------------------------------------------------- */
wlmbe_output_t *wlmbe_output_from_dlnode(bs_dllist_node_t *dlnode_ptr)
{
    if (NULL == dlnode_ptr) return NULL;
    return BS_CONTAINER_OF(dlnode_ptr, wlmbe_output_t, dlnode);
}

/* ------------------------------------------------------------------------- */
bool wlmbe_output_config_init_from_config(
    wlmbe_output_config_t *config_ptr,
    const wlmbe_output_config_t *source_config_ptr)
{
    *config_ptr = (wlmbe_output_config_t){
        .name_ptr = logged_strdup(source_config_ptr->name_ptr),
        .attr = source_config_ptr->attr,
    };
    return config_ptr->name_ptr != NULL;
}

/* ------------------------------------------------------------------------- */
bool wlmbe_output_config_init_from_plist(
    wlmbe_output_config_t *config_ptr,
    bspl_dict_t *dict_ptr)
{
    *config_ptr = (wlmbe_output_config_t){};
    return bspl_decode_dict(
        dict_ptr,
        _wlmbe_output_config_desc,
        config_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmbe_output_config_init_from_wlr(
    wlmbe_output_config_t *config_ptr,
    struct wlr_output *wlr_output_ptr)
{
    if (NULL == wlr_output_ptr || NULL == wlr_output_ptr->name) return false;

    *config_ptr = (wlmbe_output_config_t){
        .name_ptr = logged_strdup(wlr_output_ptr->name),
        .attr.transformation = wlr_output_ptr->transform,
        .attr.scale = wlr_output_ptr->scale,
        .attr.enabled = wlr_output_ptr->enabled,
    };
    return config_ptr->name_ptr != NULL;
}

/* ------------------------------------------------------------------------- */
void wlmbe_output_config_fini(wlmbe_output_config_t *config_ptr)
{
    bspl_decoded_destroy(_wlmbe_output_config_desc, config_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Event handler for the `destroy` signal raised by `wlr_output`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmbe_output_handle_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmbe_output_t *output_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmbe_output_t, output_destroy_listener);

    wl_list_remove(&output_ptr->output_request_state_listener.link);
    wl_list_remove(&output_ptr->output_frame_listener.link);
    wl_list_remove(&output_ptr->output_destroy_listener.link);
    output_ptr->wlr_output_ptr = NULL;
}

/* ------------------------------------------------------------------------- */
/**
 * Event handler for the `frame` signal raised by `wlr_output`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmbe_output_handle_frame(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmbe_output_t *output_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmbe_output_t, output_frame_listener);

    struct wlr_scene_output *wlr_scene_output_ptr = wlr_scene_get_scene_output(
        output_ptr->wlr_scene_ptr,
        output_ptr->wlr_output_ptr);
    wlr_scene_output_commit(wlr_scene_output_ptr, NULL);

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(wlr_scene_output_ptr, &now);
}

/* ------------------------------------------------------------------------- */
/**
 * Event handler for the `request_state` signal raised by `wlr_output`.
 *
 * @param listener_ptr
 * @param data_ptr
 */
void _wlmbe_output_handle_request_state(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmbe_output_t *output_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmbe_output_t, output_request_state_listener);

    const struct wlr_output_event_request_state *event_ptr = data_ptr;

    if (wlr_output_commit_state(output_ptr->wlr_output_ptr,
                                event_ptr->state)) {
        output_ptr->config_ptr->attr.transformation =
            event_ptr->state->transform;
        output_ptr->config_ptr->attr.scale = event_ptr->state->scale;
        output_ptr->config_ptr->attr.enabled = event_ptr->state->enabled;
    } else {
        bs_log(BS_WARNING, "Failed wlr_output_commit_state('%s', %p)",
               output_ptr->wlr_output_ptr->name, event_ptr->state);
    }
}

/* ------------------------------------------------------------------------- */
/** Decodes a plist "x,y" string into @ref wlmbe_output_config_position_t. */
bool _wlmbe_output_position_decode(
    bspl_object_t *object_ptr,
    void *dest_ptr)
{
    const char *s = bspl_string_value(bspl_string_from_object(object_ptr));
    if (NULL == s) return false;

    // Extracts the first arg into buf. Large enough for a INT32_MIN.
    char buf[12];
    size_t i = 0;
    for (; s[i] != '\0' && s[i] != ',' && i < sizeof(buf) - 1; ++i) {
        buf[i] = s[i];
    }
    buf[i] = '\0';
    if (s[i] == ',') ++i;

    int64_t x, y;
    if (!bs_strconvert_int64(buf, &x, 10) ||
        !bs_strconvert_int64(s + i, &y, 10)) {
        bs_log(BS_WARNING, "Failed to decode position \"%s\"", s);
        return false;
    }
    if (x < INT32_MIN || x > INT32_MAX || y < INT32_MIN || y > INT32_MAX) {
        bs_log(BS_WARNING, "Position out of range for \"%s\"", s);
        return false;
    }
    wlmbe_output_config_position_t *pos_ptr = dest_ptr;
    *pos_ptr = (wlmbe_output_config_position_t){ .x = x, .y = y };
    return true;
}

/* ------------------------------------------------------------------------- */
/** Initializes @ref wlmbe_output_config_position_t at `dest_ptr`. */
bool _wlmbe_output_position_decode_init(void *dest_ptr)
{
    wlmbe_output_config_position_t *pos_ptr = dest_ptr;
    *pos_ptr = (wlmbe_output_config_position_t){};
    return true;
}

/* == Unit tests =========================================================== */

static void _wlmbe_output_test_config_parse(bs_test_t *test_ptr);
static void _wlmbe_output_test_config_init(bs_test_t *test_ptr);
static void _wlmbe_output_test_decode_position(bs_test_t *test_ptr);

const bs_test_case_t          wlmbe_output_test_cases[] = {
    { 1, "config_parse", _wlmbe_output_test_config_parse },
    { 1, "config_init", _wlmbe_output_test_config_init },
    { 1, "decode_position", _wlmbe_output_test_decode_position },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Verifies parsing. */
void _wlmbe_output_test_config_parse(bs_test_t *test_ptr)
{
    bspl_dict_t *dict_ptr = bspl_dict_from_object(
        bspl_create_object_from_plist_string(
            "{Transformation=Flip;Scale=1;Name=X11}"));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, dict_ptr);

    wlmbe_output_config_t c;
    BS_TEST_VERIFY_TRUE_OR_RETURN(
        test_ptr, wlmbe_output_config_init_from_plist(&c, dict_ptr));

    BS_TEST_VERIFY_STREQ(test_ptr, "X11", c.name_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr, WL_OUTPUT_TRANSFORM_FLIPPED,
        c.attr.transformation);
    BS_TEST_VERIFY_EQ(test_ptr, 1.0, c.attr.scale);

    wlmbe_output_config_fini(&c);
    bspl_dict_unref(dict_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests the various flavours of initializing a @ref wlmbe_output_config_t. */
void _wlmbe_output_test_config_init(bs_test_t *test_ptr)
{
    wlmbe_output_config_t c, src = { .name_ptr = "name" };

    BS_TEST_VERIFY_TRUE_OR_RETURN(
        test_ptr,
        wlmbe_output_config_init_from_config(&c, &src));
    BS_TEST_VERIFY_STREQ(test_ptr, "name", c.name_ptr);
    wlmbe_output_config_fini(&c);
}

/* ------------------------------------------------------------------------- */
/** Tests decoding of a position field. */
void _wlmbe_output_test_decode_position(bs_test_t *test_ptr)
{
    wlmbe_output_config_position_t p;
    _wlmbe_output_position_decode_init(&p);
    BS_TEST_VERIFY_EQ(test_ptr, 0, p.x);
    BS_TEST_VERIFY_EQ(test_ptr, 0, p.y);

    bspl_object_t *o = bspl_object_from_string(bspl_string_create("1,2"));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, o);
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmbe_output_position_decode(o, &p));
    BS_TEST_VERIFY_EQ(test_ptr, 1, p.x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, p.y);
    bspl_object_unref(o);

    o = bspl_object_from_string(bspl_string_create("2147483647,-2147483648"));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, o);
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmbe_output_position_decode(o, &p));
    BS_TEST_VERIFY_EQ(test_ptr, INT32_MAX, p.x);
    BS_TEST_VERIFY_EQ(test_ptr, INT32_MIN, p.y);
    bspl_object_unref(o);

    o = bspl_object_from_string(bspl_string_create("2147483648,-2147483649"));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, o);
    BS_TEST_VERIFY_FALSE(test_ptr, _wlmbe_output_position_decode(o, &p));
    bspl_object_unref(o);
}

/* == End of output.c ====================================================== */
