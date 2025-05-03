/* ========================================================================= */
/**
 * @file dblbuf.c
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

#include "dblbuf.h"

#include <errno.h>
#include <fcntl.h>
#include <libbase/libbase.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <wayland-client.h>

/* == Declarations ========================================================= */

/** How many buffers to hold for the double buffer: Two. */
#define _WLCL_DBLBUF_NUM 2

/** A single buffer. Two of these are backing the double-buffer. */
typedef struct {
    /** The wayland buffer structure. */
    struct wl_buffer          *wl_buffer_ptr;
    /** Pixel buffer we're using for clients. */
    bs_gfxbuf_t               *gfxbuf_ptr;
    /** Back-link to the double-buffer. */
    wlcl_dblbuf_t             *dblbuf_ptr;
} wlcl_buffer_t;

/** State of double-buffered shared memory. */
struct _wlcl_dblbuf_t {
    /** With of the buffer, in pixels. */
    unsigned                  width;
    /** Height of the buffer, in pixels. */
    unsigned                  height;

    /** Holds the two @ref wlcl_buffer_t backing this double buffer. */
    wlcl_buffer_t             buffers[_WLCL_DBLBUF_NUM];

    /** Holds @ref wlcl_dblbuf_t::buffers items that are released. */
    wlcl_buffer_t             *released_buffer_ptrs[_WLCL_DBLBUF_NUM];
    /** Number of items in @ref wlcl_dblbuf_t::released_buffer_ptrs. */
    int                       released;
    /** Indicates that a frame is due to be drawn. */
    bool                      frame_is_due;

    /** Blob of memory-mapped buffer data. */
    void                      *data_ptr;
    /** Size of @ref wlcl_dblbuf_t::data_ptr. */
    size_t                    data_size;

    /** Will be called when the buffer is ready to draw into. */
    wlcl_dblbuf_ready_callback_t callback;
    /** Argument to @ref wlcl_dblbuf_t::callback. */
    void                      *callback_ud_ptr;

    /** Surface that this double buffer is operating on. */
    struct wl_surface         *wl_surface_ptr;
};

static void _wlcl_dblbuf_callback_if_ready(wlcl_dblbuf_t *dblbuf_ptr);
static void _wlcl_dblbuf_handle_frame_done(
    void *data_ptr,
    struct wl_callback *callback,
    __UNUSED__ uint32_t time);

static bool _wlcl_dblbuf_create_buffer(
    wlcl_buffer_t *buffer_ptr,
    wlcl_dblbuf_t *dblbuf_ptr,
    struct wl_shm_pool *wl_shm_pool_ptr,
    unsigned page,
    unsigned width,
    unsigned height);

static void _wlcl_dblbuf_handle_wl_buffer_release(
    void *data_ptr,
    struct wl_buffer *wl_buffer_ptr);
static int _wlcl_dblbuf_shm_create(const char *app_id_ptr, size_t size);

/* == Data ================================================================= */

/** How many attempts to try shm_open before giving up. */
static const uint32_t         SHM_OPEN_RETRIES = 256;

/** Listener implementation for the `wl_buffer`. */
static const struct wl_buffer_listener _wlcl_dblbuf_wl_buffer_listener = {
    .release = _wlcl_dblbuf_handle_wl_buffer_release,
};

/** Listener implementation for the frame. */
static const struct wl_callback_listener _wlcl_dblbuf_frame_listener = {
    .done = _wlcl_dblbuf_handle_frame_done
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlcl_dblbuf_t *wlcl_dblbuf_create(
    const char *app_id_ptr,
    struct wl_surface *wl_surface_ptr,
    struct wl_shm *wl_shm_ptr,
    unsigned width,
    unsigned height)
{
    wlcl_dblbuf_t *dblbuf_ptr = logged_calloc(1, sizeof(wlcl_dblbuf_t));
    if (NULL == dblbuf_ptr) return NULL;
    dblbuf_ptr->width = width;
    dblbuf_ptr->height = height;
    dblbuf_ptr->wl_surface_ptr = BS_ASSERT_NOTNULL(wl_surface_ptr);

    dblbuf_ptr->data_size = 2 * width * height * sizeof(uint32_t);
    int fd = _wlcl_dblbuf_shm_create(app_id_ptr, dblbuf_ptr->data_size);
    if (0 >= fd) goto error;

    dblbuf_ptr->data_ptr = mmap(
        NULL, dblbuf_ptr->data_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (MAP_FAILED == dblbuf_ptr->data_ptr) {
        bs_log(BS_ERROR | BS_ERRNO, "Failed mmap(NULL, %zu, "
               "PROT_READ|PROT_WRITE, MAP_SHARED, %d, 0)",
               dblbuf_ptr->data_size, fd);
        close(fd);
        goto error;
    }

    struct wl_shm_pool *wl_shm_pool_ptr = wl_shm_create_pool(
        wl_shm_ptr, fd, dblbuf_ptr->data_size);
    close(fd);
    if (NULL == wl_shm_pool_ptr) {
        bs_log(BS_ERROR, "Failed wl_shm_create_pool(%p, %d, %zu)",
               wl_shm_ptr, fd, dblbuf_ptr->data_size);
        goto error;
    }

    for (int i = 0; i < _WLCL_DBLBUF_NUM; ++i) {
        if (_wlcl_dblbuf_create_buffer(
                &dblbuf_ptr->buffers[i], dblbuf_ptr,
                wl_shm_pool_ptr, i, width, height)) {
            _wlcl_dblbuf_handle_wl_buffer_release(
                &dblbuf_ptr->buffers[i],
                dblbuf_ptr->buffers[i].wl_buffer_ptr);
        }
    }
    if (dblbuf_ptr->released != _WLCL_DBLBUF_NUM) goto error;

    dblbuf_ptr->frame_is_due = true;
    return dblbuf_ptr;

error:
    wlcl_dblbuf_destroy(dblbuf_ptr);
    return NULL;
}

/* ------------------------------------------------------------------------- */
void wlcl_dblbuf_destroy(wlcl_dblbuf_t *dblbuf_ptr)
{
    for (int i = 0; i < _WLCL_DBLBUF_NUM; ++i) {
        wlcl_buffer_t *buffer_ptr = &dblbuf_ptr->buffers[i];
        if (NULL != buffer_ptr->wl_buffer_ptr) {
            wl_buffer_destroy(buffer_ptr->wl_buffer_ptr);
            buffer_ptr->wl_buffer_ptr = NULL;
        }
        if (NULL != buffer_ptr->gfxbuf_ptr) {
            bs_gfxbuf_destroy(buffer_ptr->gfxbuf_ptr);
            buffer_ptr->gfxbuf_ptr = NULL;
        }
    }

    if (NULL != dblbuf_ptr->data_ptr) {
        munmap(dblbuf_ptr->data_ptr, dblbuf_ptr->data_size);
        dblbuf_ptr->data_ptr = NULL;
    }
    free(dblbuf_ptr);
}

/* ------------------------------------------------------------------------- */
void wlcl_dblbuf_register_ready_callback(
    wlcl_dblbuf_t *dblbuf_ptr,
    wlcl_dblbuf_ready_callback_t callback,
    void *callback_ud_ptr)
{
    dblbuf_ptr->callback = callback;
    dblbuf_ptr->callback_ud_ptr = callback_ud_ptr;

    _wlcl_dblbuf_callback_if_ready(dblbuf_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Calls @ref wlcl_dblbuf_t::callback, if it is registered, a frame is due,
 * and if there are available buffers. If so, and if the callback returns
 * true, the corresponding buffer will be attached to the surface and the
 * surface is committed.
 *
 * @param dblbuf_ptr
 */
void _wlcl_dblbuf_callback_if_ready(wlcl_dblbuf_t *dblbuf_ptr)
{
    // Only proceed a frame is due, the client asked, and we have a buffer.
    if (!dblbuf_ptr->callback ||
        !dblbuf_ptr->frame_is_due ||
        0 >= dblbuf_ptr->released) return;

    wlcl_buffer_t *buffer_ptr =
        dblbuf_ptr->released_buffer_ptrs[--dblbuf_ptr->released];
    dblbuf_ptr->frame_is_due = false;
    wlcl_dblbuf_ready_callback_t callback = dblbuf_ptr->callback;
    dblbuf_ptr->callback = NULL;
    if (!callback(
            buffer_ptr->gfxbuf_ptr,
            dblbuf_ptr->callback_ud_ptr)) {
        dblbuf_ptr->released_buffer_ptrs[dblbuf_ptr->released++] = buffer_ptr;
        dblbuf_ptr->frame_is_due = true;
        return;
    }

    wl_surface_damage_buffer(
        dblbuf_ptr->wl_surface_ptr, 0, 0, INT32_MAX, INT32_MAX);

    struct wl_callback *wl_callback = wl_surface_frame(
        dblbuf_ptr->wl_surface_ptr);
    wl_callback_add_listener(
        wl_callback,
        &_wlcl_dblbuf_frame_listener,
        dblbuf_ptr);
    dblbuf_ptr->frame_is_due = false;

    wl_surface_attach(
        dblbuf_ptr->wl_surface_ptr,
        buffer_ptr->wl_buffer_ptr, 0, 0);

    wl_surface_commit(dblbuf_ptr->wl_surface_ptr);
}

/* ------------------------------------------------------------------------- */
/** Callback for when the compositor indicates a frame is due. */
void _wlcl_dblbuf_handle_frame_done(
    void *data_ptr,
    struct wl_callback *callback,
    __UNUSED__ uint32_t time)
{
    wl_callback_destroy(callback);

    wlcl_dblbuf_t *dblbuf_ptr = data_ptr;
    dblbuf_ptr->frame_is_due = true;
    _wlcl_dblbuf_callback_if_ready(dblbuf_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Helper: Creates a `struct wl_buffer` from the `wl_shm_pool_ptr` at given
 * dimensions and page number, and stores all into `buffer_ptr`.
 *
 * @param buffer_ptr
 * @param dblbuf_ptr
 * @param wl_shm_pool_ptr
 * @param page
 * @param width
 * @param height
 *
 * @return true on success.
 */
bool _wlcl_dblbuf_create_buffer(
    wlcl_buffer_t *buffer_ptr,
    wlcl_dblbuf_t *dblbuf_ptr,
    struct wl_shm_pool *wl_shm_pool_ptr,
    unsigned page,
    unsigned width,
    unsigned height)
{
    buffer_ptr->dblbuf_ptr = dblbuf_ptr;
    buffer_ptr->wl_buffer_ptr = wl_shm_pool_create_buffer(
        wl_shm_pool_ptr,
        page * width * height * sizeof(uint32_t),
        width,
        height,
        width * sizeof(uint32_t),
        WL_SHM_FORMAT_ARGB8888);
    if (NULL == buffer_ptr->wl_buffer_ptr) {
        bs_log(BS_ERROR, "Failed wl_shm_pool_create_buffer(%p, %zu, %u, %u, "
               "%zu, WL_SHM_FORMAT_ARGB8888)",
               wl_shm_pool_ptr,
               page * width * height * sizeof(uint32_t),
               width,
               height,
               width * sizeof(uint32_t));
        return false;
    }

    buffer_ptr->gfxbuf_ptr = bs_gfxbuf_create_unmanaged(
        width, height, width,
        (uint32_t*)dblbuf_ptr->data_ptr + page * width * height);
    if (NULL == buffer_ptr->gfxbuf_ptr) return false;

    wl_buffer_add_listener(
        buffer_ptr->wl_buffer_ptr,
        &_wlcl_dblbuf_wl_buffer_listener,
        buffer_ptr);
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Handles the `release` notification of the wl_buffer interface.
 *
 * @param data_ptr
 * @param wl_buffer_ptr
 */
static void _wlcl_dblbuf_handle_wl_buffer_release(
    void *data_ptr,
    struct wl_buffer *wl_buffer_ptr)
{
    wlcl_buffer_t *buffer_ptr = data_ptr;
    BS_ASSERT(buffer_ptr->wl_buffer_ptr == wl_buffer_ptr);
    wlcl_dblbuf_t *dblbuf_ptr = buffer_ptr->dblbuf_ptr;
    dblbuf_ptr->released_buffer_ptrs[dblbuf_ptr->released++] = buffer_ptr;

    _wlcl_dblbuf_callback_if_ready(dblbuf_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a POSIX shared memory object and allocates `size` bytes to it.
 *
 * @param app_id_ptr
 * @param size
 *
 * @return The file descriptor (a non-negative integer) on success, or -1 on
 *     failure. The file descriptor must be closed with close(2).
 */
int _wlcl_dblbuf_shm_create(const char *app_id_ptr, size_t size)
{
    char shm_name[NAME_MAX];
    int fd = -1;

    shm_name[0] = '\0';
    for (uint32_t sequence = 0; sequence < SHM_OPEN_RETRIES; ++sequence) {
        snprintf(shm_name, NAME_MAX, "/%s_%"PRIdMAX"_shm_%"PRIx64"_%"PRIu32,
                 app_id_ptr ? app_id_ptr : "wlclient",
                 (intmax_t)getpid(), bs_usec(), sequence);
        fd = shm_open(shm_name, O_RDWR|O_CREAT|O_EXCL, 0600);
        if (0 > fd && errno == EEXIST) continue;
        if (0 < fd) break;
        bs_log(BS_WARNING | BS_ERRNO,
               "Failed shm_open(\"%s\", O_RDWR|O_CREAT|O_EXCL, 0600)",
               shm_name);
        return -1;
    }

    if (0 != shm_unlink(shm_name)) {
        bs_log(BS_ERROR | BS_ERRNO, "Failed shm_unlink(\"%s\")", shm_name);
        close(fd);
        return -1;
    }

    while (0 != ftruncate(fd, size)) {
        if (EINTR == errno) continue;  // try again...
        bs_log(BS_ERROR | BS_ERRNO, "Failed ftruncate(%d, %zu)", fd, size);
        close(fd);
        return -1;
    }

    return fd;
}

/* == End of dblbuf.c ====================================================== */
