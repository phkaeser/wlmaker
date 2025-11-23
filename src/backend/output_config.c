/* ========================================================================= */
/**
 * @file output_config.c
 * Copyright (c) 2025 by Philipp Kaeser <kaeser@gubbe.ch>
 */

#include "output_config.h"

#include <fnmatch.h>
#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#undef WLR_USE_UNSTABLE

#include "toolkit/toolkit.h"

/* == Declarations ========================================================= */

/** Output configuration. */
struct _wlmbe_output_config_t {
    /**
     * List node, element of @ref wlmbe_backend_t::output_configs, or
     * @ref wlmbe_backend_t::ephemeral_output_configs.
     */
    bs_dllist_node_t          dlnode;

    /** The output's description. */
    wlmbe_output_description_t description;
    /** The attributes. */
    wlmbe_output_config_attributes_t attributes;
};

static bool _wlmbe_output_position_decode(
    bspl_object_t *object_ptr,
    __UNUSED__ const union bspl_desc_value *desc_value_ptr,
    void *dest_ptr);
static bool _wlmbe_output_position_decode_init(void *dest_ptr);

static bool _wlmbe_output_mode_decode(
    bspl_object_t *object_ptr,
    __UNUSED__ const union bspl_desc_value *desc_value_ptr,
    void *dest_ptr);
static bool _wlmbe_output_mode_decode_init(void *dest_ptr);

/* == Data ================================================================= */

/** Descriptor for output transformations. */
/** [OutputTransformation] */
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
/** [OutputTransformation] */

/** Plist descriptor for @ref wlmbe_output_description_t. */
static const bspl_desc_t    _wlmbe_output_description_desc[] = {
    BSPL_DESC_STRING(
        "Name", false, wlmbe_output_description_t,
        name_ptr, has_name, "*"),
    BSPL_DESC_STRING(
        "Manufacturer", false, wlmbe_output_description_t,
        manufacturer_ptr, has_manufacturer, ""),
    BSPL_DESC_STRING(
        "Model", false, wlmbe_output_description_t,
        model_ptr, has_model, ""),
    BSPL_DESC_STRING(
        "Serial", false, wlmbe_output_description_t,
        serial_ptr, has_serial, ""),
    BSPL_DESC_SENTINEL()
};

/** Descriptor for the output configuration. */
static const bspl_desc_t    _wlmbe_output_config_desc[] = {
    BSPL_DESC_ENUM(
        "Transformation", true, wlmbe_output_config_t,
        attributes.transformation, attributes.transformation,
        WL_OUTPUT_TRANSFORM_NORMAL, _wlmbe_output_transformation_desc),
    BSPL_DESC_DOUBLE(
        "Scale", true, wlmbe_output_config_t,
        attributes.scale, attributes.scale, 1.0),
    BSPL_DESC_BOOL(
        "Enabled", false, wlmbe_output_config_t,
        attributes.enabled, attributes.enabled, true),
    BSPL_DESC_CUSTOM(
        "Position", false, wlmbe_output_config_t,
        attributes.position, attributes.has_position,
        _wlmbe_output_position_decode,
        NULL,
        _wlmbe_output_position_decode_init,
        NULL),
    BSPL_DESC_CUSTOM(
        "Mode", false, wlmbe_output_config_t,
        attributes.mode, attributes.has_mode,
        _wlmbe_output_mode_decode,
        NULL,
        _wlmbe_output_mode_decode_init,
        NULL),
    BSPL_DESC_SENTINEL()
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmbe_output_config_t *wlmbe_output_config_from_dlnode(
    bs_dllist_node_t *dlnode_ptr)
{
    if (NULL == dlnode_ptr) return NULL;
    return BS_CONTAINER_OF(dlnode_ptr, wlmbe_output_config_t, dlnode);
}

/* ------------------------------------------------------------------------- */
bs_dllist_node_t *wlmbe_dlnode_from_output_config(
    wlmbe_output_config_t *config_ptr)
{
    return &config_ptr->dlnode;
}

/* ------------------------------------------------------------------------- */
wlmbe_output_config_attributes_t *wlmbe_output_config_attributes(
    wlmbe_output_config_t *config_ptr)
{
    return &config_ptr->attributes;
}

/* ------------------------------------------------------------------------- */
wlmbe_output_config_t *wlmbe_output_config_create_from_wlr(
    struct wlr_output *wlr_output_ptr)
{
    BS_ASSERT(NULL != wlr_output_ptr);
    BS_ASSERT(NULL != wlr_output_ptr->name);
        wlmbe_output_config_t *config_ptr = logged_calloc(
        1, sizeof(wlmbe_output_config_t));
    if (NULL == config_ptr) return NULL;

    wlmbe_output_description_t *desc_ptr = &config_ptr->description;
    struct strings { char **d; const char *s; } s[4] = {
        { .d = &desc_ptr->name_ptr, .s = wlr_output_ptr->name },
        { .d = &desc_ptr->manufacturer_ptr, .s = wlr_output_ptr->make },
        { .d = &desc_ptr->model_ptr, .s = wlr_output_ptr->model },
        { .d = &desc_ptr->serial_ptr, .s = wlr_output_ptr->serial },
    };
    for (size_t i = 0; i < sizeof(s) / sizeof(struct strings); ++i) {
        if (NULL == s[i].s) continue;
        *s[i].d = logged_strdup(s[i].s);
        if (NULL == *s[i].d) {
            wlmbe_output_config_destroy(config_ptr);
            return NULL;
        }
    }
    desc_ptr->has_name = NULL != desc_ptr->name_ptr;
    desc_ptr->has_manufacturer = NULL != desc_ptr->manufacturer_ptr;
    desc_ptr->has_model = NULL != desc_ptr->model_ptr;
    desc_ptr->has_serial = NULL != desc_ptr->serial_ptr;

    config_ptr->attributes.transformation = wlr_output_ptr->transform;
    config_ptr->attributes.scale = wlr_output_ptr->scale;
    config_ptr->attributes.enabled = wlr_output_ptr->enabled;

    config_ptr->attributes.position.x = 0;
    config_ptr->attributes.position.y = 0;
    config_ptr->attributes.has_position = false;

    config_ptr->attributes.mode.width = wlr_output_ptr->width;
    config_ptr->attributes.mode.height = wlr_output_ptr->height;
    config_ptr->attributes.mode.refresh = wlr_output_ptr->refresh;
    config_ptr->attributes.has_mode = true;

    return config_ptr;
}

/* ------------------------------------------------------------------------- */
wlmbe_output_config_t *wlmbe_output_config_create_from_plist(
    bspl_dict_t *dict_ptr)
{
    wlmbe_output_config_t *config_ptr = logged_calloc(
        1, sizeof(wlmbe_output_config_t));
    if (NULL != config_ptr) {
        if (wlmbe_output_description_init_from_plist(
                &config_ptr->description, dict_ptr) &&
            bspl_decode_dict(
                dict_ptr,
                _wlmbe_output_config_desc,
                config_ptr)) {
            return config_ptr;
        }
        free(config_ptr);
    }
    return NULL;
}

/* ------------------------------------------------------------------------- */
void wlmbe_output_config_destroy(wlmbe_output_config_t *config_ptr)
{
    wlmbe_output_description_fini(&config_ptr->description);
    free(config_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmbe_output_config_equals(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    wlmbe_output_config_t *config_ptr =
        wlmbe_output_config_from_dlnode(dlnode_ptr);
    struct wlr_output *wlr_output_ptr = ud_ptr;

    return wlmbe_output_description_equals(
        &config_ptr->description,
        wlr_output_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmbe_output_config_fnmatches(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr)
{
    wlmbe_output_config_t *config_ptr =
        wlmbe_output_config_from_dlnode(dlnode_ptr);
    struct wlr_output *wlr_output_ptr = ud_ptr;

    return wlmbe_output_description_fnmatches(
        &config_ptr->description, wlr_output_ptr);
}

/* ------------------------------------------------------------------------- */
bool wlmbe_output_description_init_from_plist(
    wlmbe_output_description_t *desc_ptr,
    bspl_dict_t *dict_ptr)
{
    return bspl_decode_dict(
        dict_ptr, _wlmbe_output_description_desc, desc_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmbe_output_description_fini(wlmbe_output_description_t *desc_ptr)
{
    if (NULL != desc_ptr->serial_ptr) {
        free(desc_ptr->serial_ptr);
        desc_ptr->serial_ptr = NULL;
    }
    if (NULL != desc_ptr->model_ptr) {
        free(desc_ptr->model_ptr);
        desc_ptr->model_ptr = NULL;
    }
    if (NULL != desc_ptr->manufacturer_ptr) {
        free(desc_ptr->manufacturer_ptr);
        desc_ptr->manufacturer_ptr = NULL;
    }
    if (NULL != desc_ptr->name_ptr) {
        free(desc_ptr->name_ptr);
        desc_ptr->name_ptr = NULL;
    }
}

/* ------------------------------------------------------------------------- */
bool wlmbe_output_description_equals(
    wlmbe_output_description_t *desc_ptr,
    struct wlr_output *wlr_output_ptr)
{
    // Maps the presence indicator and fields to match.
    struct fields { bool present; char *cfg_val; char *o_val; } f[4] = {
        {
            .present = desc_ptr->has_name,
            .cfg_val = desc_ptr->name_ptr,
            .o_val = wlr_output_ptr->name,
        },
        {
            .present = desc_ptr->has_manufacturer,
            .cfg_val = desc_ptr->manufacturer_ptr,
            .o_val = wlr_output_ptr->make,
        },
        {
            .present = desc_ptr->has_model,
            .cfg_val = desc_ptr->model_ptr,
            .o_val = wlr_output_ptr->model,
        },
        {
            .present = desc_ptr->has_serial,
            .cfg_val = desc_ptr->serial_ptr,
            .o_val = wlr_output_ptr->serial,
        },
    };

    // Presence of the field must match in both config and output.
    bool any_field_equals = false;
    for (size_t i = 0; i < sizeof(f) / sizeof(struct fields); ++i) {

        if (f[i].present != (NULL != f[i].o_val)) return false;
        if (!f[i].present) continue;
        if (0 != strcmp(f[i].cfg_val, f[i].o_val)) return false;
        any_field_equals = true;
    }
    return any_field_equals;
}

/* ------------------------------------------------------------------------- */
bool wlmbe_output_description_fnmatches(
    wlmbe_output_description_t *desc_ptr,
    struct wlr_output *wlr_output_ptr)
{
    // Maps the presence indicator and fields to match.
    struct fields { bool present; char *cfg_val; char *o_val; } f[4] = {
        {
            .present = desc_ptr->has_name,
            .cfg_val = desc_ptr->name_ptr,
            .o_val = wlr_output_ptr->name,
        },
        {
            .present = desc_ptr->has_manufacturer,
            .cfg_val = desc_ptr->manufacturer_ptr,
            .o_val = wlr_output_ptr->make,
        },
        {
            .present = desc_ptr->has_model,
            .cfg_val = desc_ptr->model_ptr,
            .o_val = wlr_output_ptr->model,
        },
        {
            .present = desc_ptr->has_serial,
            .cfg_val = desc_ptr->serial_ptr,
            .o_val = wlr_output_ptr->serial,
        },
    };
    for (size_t i = 0; i < sizeof(f) / sizeof(struct fields); ++i) {
        if (!f[i].present) continue;
        if (NULL == f[i].cfg_val || NULL == f[i].o_val) return false;
        if (0 != fnmatch(f[i].cfg_val, f[i].o_val, 0)) return false;
    }
    return true;
}

/* ------------------------------------------------------------------------- */
struct wlr_output *wlmbe_output_description_first_fnmatch(
    wlmbe_output_description_t *desc_ptr,
    struct wlr_output_layout *wlr_output_layout_ptr)
{
    struct wl_list *link_ptr;
    for (link_ptr = wlr_output_layout_ptr->outputs.next;
         link_ptr != &wlr_output_layout_ptr->outputs;
         link_ptr = link_ptr->next) {
        struct wlr_output_layout_output *wlr_output_layout_output_ptr =
            BS_CONTAINER_OF(link_ptr, struct wlr_output_layout_output, link);
        if (wlmbe_output_description_fnmatches(
                desc_ptr,
                wlr_output_layout_output_ptr->output)) {
            return wlr_output_layout_output_ptr->output;
        }
    }
    return NULL;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/** Decodes a plist "x,y" string into @ref wlmbe_output_config_position_t. */
bool _wlmbe_output_position_decode(
    bspl_object_t *object_ptr,
    __UNUSED__ const union bspl_desc_value *desc_value_ptr,
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

/* ------------------------------------------------------------------------- */
/** Decodes a plist "WxH@R" string into @ref wlmbe_output_config_mode_t. */
bool _wlmbe_output_mode_decode(
    bspl_object_t *object_ptr,
    __UNUSED__ const union bspl_desc_value *desc_value_ptr,
    void *dest_ptr)
{
    const char *s = bspl_string_value(bspl_string_from_object(object_ptr));
    const char *full_s = s;
    if (NULL == s) return false;

    // Extracts the first arg into buf. Large enough for a INT32_MIN.
    char width[12], height[12];
    size_t i = 0;
    for (i = 0; s[i] != '\0' && s[i] != 'x' && i < sizeof(width) - 1; ++i) {
        width[i] = s[i];
    }
    width[i] = '\0';
    s += i;
    if (*s == 'x') ++s;

    for (i = 0; s[i] != '\0' && s[i] != '@' && i < sizeof(height) - 1; ++i) {
        height[i] = s[i];
    }
    height[i] = '\0';
    if (s[i] == '@') {
        s = s + i + 1;
    } else if (s[i] == '\0') {
        s = "0";
    } else {
        bs_log(BS_WARNING, "Failed to decode mode \"%s\"", full_s);
        return false;
    }

    int64_t w, h;
    double r;
    if (!bs_strconvert_int64(width, &w, 10) ||
        !bs_strconvert_int64(height, &h, 10) ||
        !bs_strconvert_double(s, &r)) {
        bs_log(BS_WARNING, "Failed to decode mode \"%s\"", full_s);
        return false;
    }
    if (w < INT32_MIN || w > INT32_MAX ||
        h < INT32_MIN || h > INT32_MAX ||
        r < 0 || r > INT32_MAX / 1000) {
        bs_log(BS_WARNING, "Mode values out of range for \"%s\"", full_s);
        return false;
    }
    wlmbe_output_config_mode_t *mode_ptr = dest_ptr;
    *mode_ptr = (wlmbe_output_config_mode_t){
        .width = w, .height = h, .refresh = r * 1000  // Hz -> mHz.
    };
    return true;
}

/* ------------------------------------------------------------------------- */
/** Initializes @ref wlmbe_output_config_mode_t at `dest_ptr`. */
bool _wlmbe_output_mode_decode_init(void *dest_ptr)
{
    wlmbe_output_config_mode_t *mode_ptr = dest_ptr;
    *mode_ptr = (wlmbe_output_config_mode_t){};
    return true;
}

/* == Unit tests =========================================================== */

static void _wlmbe_output_test_config_parse(bs_test_t *test_ptr);
static void _wlmbe_output_test_decode_position(bs_test_t *test_ptr);
static void _wlmbe_output_test_decode_mode(bs_test_t *test_ptr);
static void _wlmbe_output_test_first_fnmatch(bs_test_t *test_ptr);

const bs_test_case_t          wlmbe_output_config_test_cases[] = {
    { 1, "config_parse", _wlmbe_output_test_config_parse },
    { 1, "decode_position", _wlmbe_output_test_decode_position },
    { 1, "decode_mode", _wlmbe_output_test_decode_mode },
    { 1, "first_fnmatch", _wlmbe_output_test_first_fnmatch },
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

    wlmbe_output_config_t *c = wlmbe_output_config_create_from_plist(dict_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, c);

    BS_TEST_VERIFY_STREQ(test_ptr, "X11", c->description.name_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr, WL_OUTPUT_TRANSFORM_FLIPPED,
        c->attributes.transformation);
    BS_TEST_VERIFY_EQ(test_ptr, 1.0, c->attributes.scale);

    wlmbe_output_config_destroy(c);
    bspl_dict_unref(dict_ptr);
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
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmbe_output_position_decode(o, NULL, &p));
    BS_TEST_VERIFY_EQ(test_ptr, 1, p.x);
    BS_TEST_VERIFY_EQ(test_ptr, 2, p.y);
    bspl_object_unref(o);

    o = bspl_object_from_string(bspl_string_create("2147483647,-2147483648"));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, o);
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmbe_output_position_decode(o, NULL, &p));
    BS_TEST_VERIFY_EQ(test_ptr, INT32_MAX, p.x);
    BS_TEST_VERIFY_EQ(test_ptr, INT32_MIN, p.y);
    bspl_object_unref(o);

    o = bspl_object_from_string(bspl_string_create("2147483648,-2147483649"));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, o);
    BS_TEST_VERIFY_FALSE(test_ptr, _wlmbe_output_position_decode(o, NULL, &p));
    bspl_object_unref(o);
}

/* ------------------------------------------------------------------------- */
/** Tests decoding of a position field. */
void _wlmbe_output_test_decode_mode(bs_test_t *test_ptr)
{
    wlmbe_output_config_mode_t m;
    _wlmbe_output_mode_decode_init(&m);
    BS_TEST_VERIFY_EQ(test_ptr, 0, m.width);
    BS_TEST_VERIFY_EQ(test_ptr, 0, m.height);
    BS_TEST_VERIFY_EQ(test_ptr, 0, m.refresh);

    bspl_object_t *o = bspl_object_from_string(bspl_string_create("1x2@3.4"));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, o);
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmbe_output_mode_decode(o, NULL, &m));
    BS_TEST_VERIFY_EQ(test_ptr, 1, m.width);
    BS_TEST_VERIFY_EQ(test_ptr, 2, m.height);
    BS_TEST_VERIFY_EQ(test_ptr, 3400, m.refresh);
    bspl_object_unref(o);

    o = bspl_object_from_string(
        bspl_string_create("2147483647x-2147483648@2147483"));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, o);
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmbe_output_mode_decode(o, NULL, &m));
    BS_TEST_VERIFY_EQ(test_ptr, INT32_MAX, m.width);
    BS_TEST_VERIFY_EQ(test_ptr, INT32_MIN, m.height);
    BS_TEST_VERIFY_EQ(test_ptr, 2147483000, m.refresh);
    bspl_object_unref(o);

    o = bspl_object_from_string(
        bspl_string_create("2147483648x-2147483649@2147483648"));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, o);
    BS_TEST_VERIFY_FALSE(test_ptr, _wlmbe_output_mode_decode(o, NULL, &m));
    bspl_object_unref(o);

    o = bspl_object_from_string(bspl_string_create("3x4"));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, o);
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmbe_output_mode_decode(o, NULL, &m));
    BS_TEST_VERIFY_EQ(test_ptr, 3, m.width);
    BS_TEST_VERIFY_EQ(test_ptr, 4, m.height);
    BS_TEST_VERIFY_EQ(test_ptr, 0, m.refresh);
    bspl_object_unref(o);
}

/* ------------------------------------------------------------------------- */
/** Tests @ref wlmbe_output_description_first_fnmatch. */
void _wlmbe_output_test_first_fnmatch(bs_test_t *test_ptr)
{
    struct wl_display *display = wl_display_create();
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, display);
    struct wlr_output_layout *wol_ptr = wlr_output_layout_create(display);

    struct wlr_output o1 = { .name = "Name1" };
    wlmtk_test_wlr_output_init(&o1);
    wlr_output_layout_add_auto(wol_ptr, &o1);
    struct wlr_output o2 = { .name = "Other1" };
    wlmtk_test_wlr_output_init(&o2);
    wlr_output_layout_add_auto(wol_ptr, &o2);
    struct wlr_output o3 = { .name = "Other2" };
    wlmtk_test_wlr_output_init(&o3);
    wlr_output_layout_add_auto(wol_ptr, &o3);

    wlmbe_output_description_t d = {};
    BS_TEST_VERIFY_EQ(
        test_ptr, &o1, wlmbe_output_description_first_fnmatch(&d, wol_ptr));
    d = (wlmbe_output_description_t){ .name_ptr = "Oth*", .has_name = true };
    BS_TEST_VERIFY_EQ(
        test_ptr, &o2, wlmbe_output_description_first_fnmatch(&d, wol_ptr));
    d.name_ptr = "Name1";
    BS_TEST_VERIFY_EQ(
        test_ptr, &o1, wlmbe_output_description_first_fnmatch(&d, wol_ptr));
    d.name_ptr = "*2";
    BS_TEST_VERIFY_EQ(
        test_ptr, &o3, wlmbe_output_description_first_fnmatch(&d, wol_ptr));

    wl_display_destroy(display);
}

/* == End of output_config.c =============================================== */
