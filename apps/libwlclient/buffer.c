/* ========================================================================= */
/**
 * @file buffer.c
 *
 * @copyright
 * Copyright 2023 Google LLC
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

#include "buffer.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <wayland-client.h>

/* == Declarations ========================================================= */

/** Actual buffer. TODO(kaeser@gubbe.ch): Clean this up. */
typedef struct {
    /** Points to the data area, ie. the pixels. */
    uint32_t                  *data_ptr;
    /** Corresponding wayland buffer. */
    struct wl_buffer          *wl_buffer_ptr;
    /** Corresponding (unmanaged) `bs_gfxbuf_t`. */
    bs_gfxbuf_t               *bs_gfxbuf_ptr;

    /** Indicates that the buffer is committed, and not ready to draw into. */
    bool                      committed;
    /** Back-link to the client buffer state. */
    wlclient_buffer_t         *client_buffer_ptr;
} buffer_t;

/** All elements contributing to a wl_buffer. */
struct _wlclient_buffer_t {
    /** Mapped data. */
    void                      *data_ptr;
    /** Shared memory pool. */
    struct wl_shm_pool        *wl_shm_pool_ptr;

    /** Width of the buffer, in pixels. */
    unsigned                  width;
    /** Height of the buffer, in pixels. */
    unsigned                  height;

    /** Actual buffer. */
    buffer_t                  *buffer_ptr;

    /** Callback to indicate the buffer is ready to draw into. */
    wlclient_buffer_ready_callback_t ready_callback;
    /** Argument to said callback. */
    void                      *ready_callback_ud_ptr;
};

static void handle_wl_buffer_release(
    void *data_ptr,
    struct wl_buffer *wl_buffer_ptr);
static int shm_creat(const char *app_id_ptr, size_t size);

static buffer_t *create_buffer(
    struct wl_shm_pool *wl_shm_pool_ptr,
    void *data_base_ptr,
    size_t ofs,
    unsigned width,
    unsigned height,
    unsigned bytes_per_line);
static void buffer_destroy(buffer_t *buffer_ptr);

/* == Data ================================================================= */

/** How many attempts to try shm_open before giving up. */
static const uint32_t         SHM_OPEN_RETRIES = 256;

/** Listener implementation for the `wl_buffer`. */
static const struct wl_buffer_listener wl_buffer_listener = {
    .release = handle_wl_buffer_release,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlclient_buffer_t *wlclient_buffer_create(
    const wlclient_t *wlclient_ptr,
    unsigned width,
    unsigned height,
    wlclient_buffer_ready_callback_t ready_callback,
    void *ready_callback_ud_ptr)
{
    wlclient_buffer_t *client_buffer_ptr = logged_calloc(
        1, sizeof(wlclient_buffer_t));
    if (NULL == client_buffer_ptr) return NULL;
    client_buffer_ptr->ready_callback = ready_callback;
    client_buffer_ptr->ready_callback_ud_ptr = ready_callback_ud_ptr;

    client_buffer_ptr->width = width;
    client_buffer_ptr->height = height;

    size_t shm_pool_size = width * height * sizeof(uint32_t);
    int fd = shm_creat(
        wlclient_attributes(wlclient_ptr)->app_id_ptr, shm_pool_size);
    if (0 >= fd) {
        wlclient_buffer_destroy(client_buffer_ptr);
        return NULL;
    }
    client_buffer_ptr->data_ptr = mmap(
        NULL, shm_pool_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (MAP_FAILED == client_buffer_ptr->data_ptr) {
        bs_log(BS_ERROR | BS_ERRNO, "Failed mmap(NULL, %zu, "
               "PROT_READ|PROT_WRITE, MAP_SHARED, %d, 0)",
               shm_pool_size, fd);
        close(fd);
        wlclient_buffer_destroy(client_buffer_ptr);
        return NULL;
    }

    struct wl_shm_pool *wl_shm_pool_ptr = wl_shm_create_pool(
        wlclient_attributes(wlclient_ptr)->wl_shm_ptr, fd, shm_pool_size);
    close(fd);
    if (NULL == wl_shm_pool_ptr) {
        bs_log(BS_ERROR, "Failed wl_shm_create_pool(%p, %d, %zu)",
               wlclient_attributes(wlclient_ptr)->wl_shm_ptr,
               fd, shm_pool_size);
        wlclient_buffer_destroy(client_buffer_ptr);
        return NULL;
    }

    client_buffer_ptr->buffer_ptr = create_buffer(
        wl_shm_pool_ptr,
        client_buffer_ptr->data_ptr,
        0,
        width,
        height,
        width * sizeof(uint32_t));
    if (NULL == client_buffer_ptr->buffer_ptr) {
        wlclient_buffer_destroy(client_buffer_ptr);
        return NULL;
    }
    client_buffer_ptr->buffer_ptr->client_buffer_ptr = client_buffer_ptr;

    wl_shm_pool_destroy(wl_shm_pool_ptr);

    if (NULL != client_buffer_ptr->ready_callback) {
        client_buffer_ptr->ready_callback(
            client_buffer_ptr->ready_callback_ud_ptr);
    }
    return client_buffer_ptr;
}

/* ------------------------------------------------------------------------- */
void wlclient_buffer_destroy(wlclient_buffer_t *client_buffer_ptr)
{
    if (NULL != client_buffer_ptr->buffer_ptr) {
        buffer_destroy(client_buffer_ptr->buffer_ptr);
        client_buffer_ptr->buffer_ptr = NULL;
    }

    bs_log(BS_WARNING, "Destroyed %p", client_buffer_ptr);
    free(client_buffer_ptr);
}

/* ------------------------------------------------------------------------- */
bs_gfxbuf_t *bs_gfxbuf_from_wlclient_buffer(
    wlclient_buffer_t *client_buffer_ptr)
{
    return client_buffer_ptr->buffer_ptr->bs_gfxbuf_ptr;
}

/* ------------------------------------------------------------------------- */
void wlclient_buffer_attach_to_surface_and_commit(
    wlclient_buffer_t *client_buffer_ptr,
    struct wl_surface *wl_surface_ptr)
{
    BS_ASSERT(!client_buffer_ptr->buffer_ptr->committed);
    wl_surface_attach(
        wl_surface_ptr,
        client_buffer_ptr->buffer_ptr->wl_buffer_ptr,
        0, 0);
    client_buffer_ptr->buffer_ptr->committed = true;
    wl_surface_commit(wl_surface_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Handles the `release` notification of the wl_buffer interface.
 *
 * @param data_ptr
 * @param wl_buffer_ptr
 */
static void handle_wl_buffer_release(
    void *data_ptr,
    __UNUSED__ struct wl_buffer *wl_buffer_ptr)
{
    buffer_t *buffer_ptr = data_ptr;
    buffer_ptr->committed = false;

    // Signal a potential user that this buffer is ready to draw into.
    if (NULL != buffer_ptr->client_buffer_ptr->ready_callback) {
        buffer_ptr->client_buffer_ptr->ready_callback(
            buffer_ptr->client_buffer_ptr->ready_callback_ud_ptr);
    }
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
int shm_creat(const char *app_id_ptr, size_t size)
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
               "Failed shm_open(%s, O_RDWR|O_CREAT|O_EXCL, 0600)",
               shm_name);
        return -1;
    }

    if (0 != shm_unlink(shm_name)) {
        bs_log(BS_ERROR | BS_ERRNO, "Failed shm_unlink(%s)", shm_name);
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

/* ------------------------------------------------------------------------- */
/** Creates the actual buffer. */
buffer_t *create_buffer(struct wl_shm_pool *wl_shm_pool_ptr,
                        void *data_base_ptr,
                        size_t ofs,
                        unsigned width,
                        unsigned height,
                        unsigned bytes_per_line)
{
    buffer_t *buffer_ptr = logged_calloc(1, sizeof(buffer_t));
    if (NULL == buffer_ptr) return buffer_ptr;

    buffer_ptr->data_ptr = (uint32_t*)((uint8_t*)data_base_ptr + ofs);

    buffer_ptr->wl_buffer_ptr = wl_shm_pool_create_buffer(
        wl_shm_pool_ptr, ofs, width, height, bytes_per_line,
        WL_SHM_FORMAT_ARGB8888);
    if (NULL == buffer_ptr->wl_buffer_ptr) {
        buffer_destroy(buffer_ptr);
        return NULL;
    }

    buffer_ptr->bs_gfxbuf_ptr = bs_gfxbuf_create_unmanaged(
        width, height, bytes_per_line / sizeof(uint32_t), buffer_ptr->data_ptr);
    if (NULL == buffer_ptr->bs_gfxbuf_ptr) {
        buffer_destroy(buffer_ptr);
        return NULL;
    }

    wl_buffer_add_listener(
        buffer_ptr->wl_buffer_ptr,
        &wl_buffer_listener,
        buffer_ptr);

    return buffer_ptr;
}

/* ------------------------------------------------------------------------- */
/** Destroys the actual buffer. */
void buffer_destroy(buffer_t *buffer_ptr)
{
    if (NULL != buffer_ptr->bs_gfxbuf_ptr) {
        bs_gfxbuf_destroy(buffer_ptr->bs_gfxbuf_ptr);
        buffer_ptr->bs_gfxbuf_ptr = NULL;
    }
    if (NULL != buffer_ptr->wl_buffer_ptr) {
        wl_buffer_destroy(buffer_ptr->wl_buffer_ptr);
        buffer_ptr->wl_buffer_ptr = NULL;
    }
    free(buffer_ptr);
}

/* == End of buffer.c ====================================================== */
