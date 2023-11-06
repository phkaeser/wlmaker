/* ========================================================================= */
/**
 * @file resizebar.h
 * Copyright (c) 2023 by Philipp Kaeser <kaeser@gubbe.ch>
 */
#ifndef __WLMTK_RESIZEBAR_H__
#define __WLMTK_RESIZEBAR_H__

/** Forward declaration: Title bar. */
typedef struct _wlmtk_resizebar_t wlmtk_resizebar_t;

#include "element.h"
#include "primitives.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Style options for the resizebar. */
typedef struct {
    /** Fill style for the complete resizebar. */
    wlmtk_style_fill_t        fill;
    /** Height of the resize bar. */
    unsigned                  height;
} wlmtk_resizebar_style_t;

/**
 * Creates the resize bar.
 *
 * @param width
 * @param style_ptr
 *
 * @return Pointer to the resizebar state, or NULL on error.
 */
wlmtk_resizebar_t *wlmtk_resizebar_create(
    unsigned width,
    const wlmtk_resizebar_style_t *style_ptr);

/**
 * Destroys the resize bar.
 *
 * @param resizebar_ptr
 */
void wlmtk_resizebar_destroy(wlmtk_resizebar_t *resizebar_ptr);

/**
 * Sets the width of the resize bar.
 *
 * @param resizebar_ptr
 * @param width
 *
 * @return true on success.
 */
bool wlmtk_resizebar_set_width(
    wlmtk_resizebar_t * resizebar_ptr,
    unsigned width);

/**
 * Returns the super Element of the resizebar.
 *
 * @param resizebar_ptr
 *
 * @return Pointer to the element.
 */
wlmtk_element_t *wlmtk_resizebar_element(wlmtk_resizebar_t *resizebar_ptr);

/** Unit test cases. */
extern const bs_test_case_t wlmtk_resizebar_test_cases[];

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_RESIZEBAR_H__ */
/* == End of resizebar.h ================================================== */
