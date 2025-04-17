/* ========================================================================= */
/**
 * @file output_config.c
 * Copyright (c) 2025 by Philipp Kaeser <kaeser@gubbe.ch>
 */

#include <backend/output_config.h>
#include <libbase/libbase.h>

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_output.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

static bool _wlmbe_output_position_decode(
    bspl_object_t *object_ptr,
    void *dest_ptr);
static bool _wlmbe_output_position_decode_init(void *dest_ptr);

static bool _wlmbe_output_mode_decode(
    bspl_object_t *object_ptr,
    void *dest_ptr);
static bool _wlmbe_output_mode_decode_init(void *dest_ptr);

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
    BSPL_DESC_CUSTOM(
        "Mode", false, wlmbe_output_config_t,
        attr.mode, attr.has_mode,
        _wlmbe_output_mode_decode,
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
wlmbe_output_config_t *wlmbe_output_config_create_from_wlr(
    struct wlr_output *wlr_output_ptr)
{
    BS_ASSERT(NULL != wlr_output_ptr);
    BS_ASSERT(NULL != wlr_output_ptr->name);
        wlmbe_output_config_t *config_ptr = logged_calloc(
        1, sizeof(wlmbe_output_config_t));
    if (NULL == config_ptr) return NULL;

    struct strings { char **d; const char *s; } s[4] = {
        { .d = &config_ptr->name_ptr, .s = wlr_output_ptr->name },
        { .d = &config_ptr->manufacturer_ptr, .s = wlr_output_ptr->make },
        { .d = &config_ptr->model_ptr, .s = wlr_output_ptr->model },
        { .d = &config_ptr->serial_ptr, .s = wlr_output_ptr->serial },
    };
    for (size_t i = 0; i < sizeof(s) / sizeof(struct strings); ++i) {
        if (NULL == s[i].s) continue;
        *s[i].d = logged_strdup(s[i].s);
        if (NULL == *s[i].d) {
            wlmbe_output_config_destroy(config_ptr);
            return NULL;
        }
    }
    config_ptr->has_name = NULL != config_ptr->name_ptr;
    config_ptr->has_manufacturer = NULL != config_ptr->manufacturer_ptr;
    config_ptr->has_model = NULL != config_ptr->model_ptr;
    config_ptr->has_serial = NULL != config_ptr->serial_ptr;

    config_ptr->attr.transformation = wlr_output_ptr->transform;
    config_ptr->attr.scale = wlr_output_ptr->scale;
    config_ptr->attr.enabled = wlr_output_ptr->enabled;

    config_ptr->attr.position.x = 0;
    config_ptr->attr.position.y = 0;
    config_ptr->attr.has_position = false;

    config_ptr->attr.mode.width = wlr_output_ptr->width;
    config_ptr->attr.mode.height = wlr_output_ptr->height;
    config_ptr->attr.mode.refresh = wlr_output_ptr->refresh;
    config_ptr->attr.has_mode = true;

    return config_ptr;
}

/* ------------------------------------------------------------------------- */
wlmbe_output_config_t *wlmbe_output_config_create_from_plist(
    bspl_dict_t *dict_ptr)
{
    wlmbe_output_config_t *config_ptr = logged_calloc(
        1, sizeof(wlmbe_output_config_t));
    if (NULL != config_ptr) {
        if (bspl_decode_dict(
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
    if (NULL != config_ptr->serial_ptr) {
        free(config_ptr->serial_ptr);
        config_ptr->serial_ptr = NULL;
    }
    if (NULL != config_ptr->model_ptr) {
        free(config_ptr->model_ptr);
        config_ptr->model_ptr = NULL;
    }
    if (NULL != config_ptr->manufacturer_ptr) {
        free(config_ptr->manufacturer_ptr);
        config_ptr->manufacturer_ptr = NULL;
    }
    if (NULL != config_ptr->name_ptr) {
        free(config_ptr->name_ptr);
        config_ptr->name_ptr = NULL;
    }
    free(config_ptr);
}

/* == Local (static) methods =============================================== */

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

/* ------------------------------------------------------------------------- */
/** Decodes a plist "WxH@R" string into @ref wlmbe_output_config_mode_t. */
bool _wlmbe_output_mode_decode(
    bspl_object_t *object_ptr,
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

    int64_t w, h, r;
    if (!bs_strconvert_int64(width, &w, 10) ||
        !bs_strconvert_int64(height, &h, 10) ||
        !bs_strconvert_int64(s, &r, 10)) {
        bs_log(BS_WARNING, "Failed to decode mode \"%s\"", full_s);
        return false;
    }
    if (w < INT32_MIN || w > INT32_MAX ||
        h < INT32_MIN || h > INT32_MAX ||
        r < INT32_MIN || r > INT32_MAX) {
        bs_log(BS_WARNING, "Mode values out of range for \"%s\"", full_s);
        return false;
    }
    wlmbe_output_config_mode_t *mode_ptr = dest_ptr;
    *mode_ptr = (wlmbe_output_config_mode_t){
        .width = w, .height = h, .refresh = r
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

const bs_test_case_t          wlmbe_output_config_test_cases[] = {
    { 1, "config_parse", _wlmbe_output_test_config_parse },
    { 1, "decode_position", _wlmbe_output_test_decode_position },
    { 1, "decode_mode", _wlmbe_output_test_decode_mode },
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

    BS_TEST_VERIFY_STREQ(test_ptr, "X11", c->name_ptr);
    BS_TEST_VERIFY_EQ(
        test_ptr, WL_OUTPUT_TRANSFORM_FLIPPED,
        c->attr.transformation);
    BS_TEST_VERIFY_EQ(test_ptr, 1.0, c->attr.scale);

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

/* ------------------------------------------------------------------------- */
/** Tests decoding of a position field. */
void _wlmbe_output_test_decode_mode(bs_test_t *test_ptr)
{
    wlmbe_output_config_mode_t m;
    _wlmbe_output_mode_decode_init(&m);
    BS_TEST_VERIFY_EQ(test_ptr, 0, m.width);
    BS_TEST_VERIFY_EQ(test_ptr, 0, m.height);
    BS_TEST_VERIFY_EQ(test_ptr, 0, m.refresh);

    bspl_object_t *o = bspl_object_from_string(bspl_string_create("1x2@3"));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, o);
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmbe_output_mode_decode(o, &m));
    BS_TEST_VERIFY_EQ(test_ptr, 1, m.width);
    BS_TEST_VERIFY_EQ(test_ptr, 2, m.height);
    BS_TEST_VERIFY_EQ(test_ptr, 3, m.refresh);
    bspl_object_unref(o);

    o = bspl_object_from_string(
        bspl_string_create("2147483647x-2147483648@2147483647"));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, o);
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmbe_output_mode_decode(o, &m));
    BS_TEST_VERIFY_EQ(test_ptr, INT32_MAX, m.width);
    BS_TEST_VERIFY_EQ(test_ptr, INT32_MIN, m.height);
    BS_TEST_VERIFY_EQ(test_ptr, INT32_MAX, m.refresh);
    bspl_object_unref(o);

    o = bspl_object_from_string(
        bspl_string_create("2147483648x-2147483649@2147483648"));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, o);
    BS_TEST_VERIFY_FALSE(test_ptr, _wlmbe_output_mode_decode(o, &m));
    bspl_object_unref(o);

    o = bspl_object_from_string(bspl_string_create("3x4"));
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, o);
    BS_TEST_VERIFY_TRUE(test_ptr, _wlmbe_output_mode_decode(o, &m));
    BS_TEST_VERIFY_EQ(test_ptr, 3, m.width);
    BS_TEST_VERIFY_EQ(test_ptr, 4, m.height);
    BS_TEST_VERIFY_EQ(test_ptr, 0, m.refresh);
    bspl_object_unref(o);
}

/* == End of output_config.c =============================================== */
