/* ========================================================================= */
/**
 * @file style.c
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

#include <libbase/libbase.h>
#include <libbase/plist.h>
#include <stddef.h>
#include <toolkit/style.h>

/* == Data ================================================================= */

const bspl_desc_t wlmtk_style_margin_desc[] = {
    BSPL_DESC_UINT64(
        "Width", true, struct wlmtk_margin_style, width, width, 0),
    BSPL_DESC_ARGB32(
        "Color", true, struct wlmtk_margin_style, color, color, 0xff000000),
    BSPL_DESC_SENTINEL()
};

/** Plist decoding descriptor for font weight. */
static const bspl_enum_desc_t _wlmtk_style_font_weight_desc[] = {
    BSPL_ENUM("Normal", WLMTK_FONT_WEIGHT_NORMAL),
    BSPL_ENUM("Bold", WLMTK_FONT_WEIGHT_BOLD),
    BSPL_ENUM_SENTINEL()
};

const bspl_desc_t wlmtk_style_font_desc[] = {
    BSPL_DESC_CHARBUF(
        "Face", true, wlmtk_style_font_t, face, face,
        WLMTK_STYLE_FONT_FACE_LENGTH, NULL),
    BSPL_DESC_ENUM(
        "Weight", true, wlmtk_style_font_t, weight, weight,
        WLMTK_FONT_WEIGHT_NORMAL, _wlmtk_style_font_weight_desc),
    BSPL_DESC_UINT64(
        "Size", true, wlmtk_style_font_t, size, size, 10),
    BSPL_DESC_SENTINEL()
};

/** Plist decoding descriptor of the fill type. */
static const bspl_enum_desc_t _wlmtk_style_fill_type_desc[] = {
    BSPL_ENUM("SOLID", WLMTK_STYLE_COLOR_SOLID),
    BSPL_ENUM("HGRADIENT", WLMTK_STYLE_COLOR_HGRADIENT),
    BSPL_ENUM("VGRADIENT", WLMTK_STYLE_COLOR_VGRADIENT),
    BSPL_ENUM("DGRADIENT", WLMTK_STYLE_COLOR_DGRADIENT),
    BSPL_ENUM("ADGRADIENT", WLMTK_STYLE_COLOR_ADGRADIENT),
    BSPL_ENUM_SENTINEL()
};

/** Plist decoding descriptor of the fill style. */
static const bspl_desc_t _wlmtk_style_fill_style_desc[] = {
    BSPL_DESC_ENUM("Type", true, wlmtk_style_fill_t, type, type,
                     WLMTK_STYLE_COLOR_SOLID,
                     _wlmtk_style_fill_type_desc),
    BSPL_DESC_SENTINEL()
};

/** Plist decoding descriptor of the solid color. */
static const bspl_desc_t _wlmtk_style_fill_color_solid_desc[] = {
    BSPL_DESC_ARGB32(
        "Color", true, wlmtk_style_color_solid_data_t, color, color, 0),
    BSPL_DESC_SENTINEL()
};

/** Plist decoding descriptor of a color gradient. */
static const bspl_desc_t _wlmtk_style_fill_color_gradient_desc[] = {
    BSPL_DESC_ARGB32(
        "From", true, wlmtk_style_color_gradient_data_t, from, from, 0),
    BSPL_DESC_ARGB32(
        "To", true, wlmtk_style_color_gradient_data_t, to, to, 0),
    BSPL_DESC_SENTINEL()
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
cairo_font_weight_t wlmtk_style_font_weight_cairo_from_wlmtk(
    wlmtk_style_font_weight_t weight)
{
    switch (weight) {
    case WLMTK_FONT_WEIGHT_NORMAL: return CAIRO_FONT_WEIGHT_NORMAL;
    case WLMTK_FONT_WEIGHT_BOLD: return CAIRO_FONT_WEIGHT_BOLD;
    default:
        bs_log(BS_FATAL, "Unhandled font weight %d", weight);
        BS_ABORT();
    }
    return CAIRO_FONT_WEIGHT_NORMAL;
}

/* ------------------------------------------------------------------------- */
bool wlmtk_style_decode_fill(
    bspl_object_t *object_ptr,
    __UNUSED__ const union bspl_desc_value *desc_value_ptr,
    void *value_ptr)
{
    wlmtk_style_fill_t *fill_ptr = value_ptr;
    bspl_dict_t *dict_ptr = bspl_dict_from_object(object_ptr);
    if (NULL == dict_ptr) return false;

    if (!bspl_decode_dict(
            dict_ptr,
            _wlmtk_style_fill_style_desc,
            fill_ptr)) return false;

    switch (fill_ptr->type) {
    case WLMTK_STYLE_COLOR_SOLID:
        return bspl_decode_dict(
            dict_ptr,
            _wlmtk_style_fill_color_solid_desc,
            &fill_ptr->param.solid);
    case WLMTK_STYLE_COLOR_HGRADIENT:
        return bspl_decode_dict(
            dict_ptr,
            _wlmtk_style_fill_color_gradient_desc,
            &fill_ptr->param.hgradient);
    case WLMTK_STYLE_COLOR_VGRADIENT:
        return bspl_decode_dict(
            dict_ptr,
            _wlmtk_style_fill_color_gradient_desc,
            &fill_ptr->param.vgradient);
    case WLMTK_STYLE_COLOR_DGRADIENT:
        return bspl_decode_dict(
            dict_ptr,
            _wlmtk_style_fill_color_gradient_desc,
            &fill_ptr->param.dgradient);
    case WLMTK_STYLE_COLOR_ADGRADIENT:
        return bspl_decode_dict(
            dict_ptr,
            _wlmtk_style_fill_color_gradient_desc,
            &fill_ptr->param.adgradient);
    default:
        bs_log(BS_ERROR, "Unhandled fill type %d", fill_ptr->type);
        return false;
    }
    bs_log(BS_FATAL, "Uh... no idea how this got here.");
    return false;
}

/* == Unit Tests =========================================================== */

static void _wlmtk_style_test_decode_fill(bs_test_t *test_ptr);
static void _wlmtk_style_test_decode_font(bs_test_t *test_ptr);

/** Unit test cases. */
const bs_test_case_t wlmtk_style_test_cases[] = {
    { true, "decode_fill", _wlmtk_style_test_decode_fill },
    { true, "decode_font", _wlmtk_style_test_decode_font },
    BS_TEST_CASE_SENTINEL()
};

/* ------------------------------------------------------------------------- */
/** Tests the decoder for the fill style. */
void _wlmtk_style_test_decode_fill(bs_test_t *test_ptr)
{
    const char *s = ("{"
                     "Type = DGRADIENT;"
                     "From = \"argb32:0x01020304\";"
                     "To = \"argb32:0x0204080c\""
                     "}");
    wlmtk_style_fill_t fill;
    bspl_object_t *object_ptr;

    object_ptr = bspl_create_object_from_plist_string(s);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, object_ptr);

    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_style_decode_fill(object_ptr, NULL, &fill));
    BS_TEST_VERIFY_EQ(test_ptr, WLMTK_STYLE_COLOR_DGRADIENT, fill.type);
    BS_TEST_VERIFY_EQ(test_ptr, 0x01020304, fill.param.dgradient.from);
    BS_TEST_VERIFY_EQ(test_ptr, 0x0204080c, fill.param.dgradient.to);
    bspl_object_unref(object_ptr);

    s = ("{"
         "Type = HGRADIENT;"
         "From = \"argb32:0x04030201\";"
         "To = \"argb32:0x40302010\""
         "}");
    object_ptr = bspl_create_object_from_plist_string(s);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, object_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_style_decode_fill(object_ptr, NULL, &fill));
    BS_TEST_VERIFY_EQ(test_ptr, WLMTK_STYLE_COLOR_HGRADIENT, fill.type);
    BS_TEST_VERIFY_EQ(test_ptr, 0x04030201, fill.param.hgradient.from);
    BS_TEST_VERIFY_EQ(test_ptr, 0x40302010, fill.param.hgradient.to);
    bspl_object_unref(object_ptr);

    s = ("{"
         "Type = SOLID;"
         "Color = \"argb32:0x11223344\""
         "}");
    object_ptr = bspl_create_object_from_plist_string(s);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, object_ptr);
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        wlmtk_style_decode_fill(object_ptr, NULL, &fill));
    BS_TEST_VERIFY_EQ(test_ptr, WLMTK_STYLE_COLOR_SOLID, fill.type);
    BS_TEST_VERIFY_EQ(test_ptr, 0x11223344, fill.param.solid.color);
    bspl_object_unref(object_ptr);
}

/* ------------------------------------------------------------------------- */
/** Tests the decoder for a font descriptor. */
void _wlmtk_style_test_decode_font(bs_test_t *test_ptr)
{
    bspl_object_t *object_ptr;
    const char *s = ("{"
                     "Face = Helvetica;"
                     "Weight = Bold;"
                     "Size = 12;"
                     "}");

    object_ptr = bspl_create_object_from_plist_string(s);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, object_ptr);
    bspl_dict_t *dict_ptr = bspl_dict_from_object(object_ptr);
    BS_TEST_VERIFY_NEQ_OR_RETURN(test_ptr, NULL, dict_ptr);

    wlmtk_style_font_t font = {};
    BS_TEST_VERIFY_TRUE(
        test_ptr,
        bspl_decode_dict(dict_ptr, wlmtk_style_font_desc, &font));

    BS_TEST_VERIFY_STREQ(test_ptr, "Helvetica", font.face);
    BS_TEST_VERIFY_EQ(test_ptr, WLMTK_FONT_WEIGHT_BOLD, font.weight);
    BS_TEST_VERIFY_EQ(test_ptr, 12, font.size);

    bspl_object_unref(object_ptr);
}

/* == End of style.c ======================================================= */
