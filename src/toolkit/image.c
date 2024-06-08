/* ========================================================================= */
/**
 * @file image.c
 * Copyright (c) 2024 by Philipp Kaeser <kaeser@gubbe.ch>
 */

#include "image.h"

#include "buffer.h"

/* == Declarations ========================================================= */

/** State of the image. */
struct _wlmtk_image_t {
    /** The image's superclass: A buffer. */
    wlmtk_buffer_t            super_buffer;
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_image_t *wlmtk_image_create(
    __UNUSED__ const char *image_path_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmtk_image_t *image_ptr = logged_calloc(1, sizeof(wlmtk_image_t));
    if (NULL == image_ptr) return NULL;

    if (!wlmtk_buffer_init(&image_ptr->super_buffer, env_ptr)) {
        wlmtk_image_destroy(image_ptr);
        return NULL;
    }

    return image_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_image_destroy(wlmtk_image_t *image_ptr)
{
    wlmtk_buffer_fini(&image_ptr->super_buffer);
    free(image_ptr);
}

/* == Local (static) methods =============================================== */

static void test_create_destroy(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_image_test_cases[] = {
    { 1, "create_destroy", test_create_destroy },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Exercises ctor and dtor. */
void test_create_destroy(bs_test_t *test_ptr)
{
    wlmtk_image_t *image_ptr = wlmtk_image_create(NULL, NULL);
    BS_TEST_VERIFY_NEQ(test_ptr, NULL, image_ptr);

    wlmtk_image_destroy(image_ptr);
}

/* == End of image.c ======================================================= */
