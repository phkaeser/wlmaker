/* ========================================================================= */
/**
 * @file icon_manager.c
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

#include "icon_manager.h"

#include <fcntl.h>
#include <drm_fourcc.h>
#include <inttypes.h>
#include <libbase/libbase.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-server-core.h>
#define WLR_USE_UNSTABLE
#include <wlr/render/drm_format_set.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#undef WLR_USE_UNSTABLE

#include "config.h"
#include "toolkit/toolkit.h"
#include "wlmaker-icon-unstable-v1-server-protocol.h"
#include "xdg_shell.h"
#include "backend/backend.h"

struct wl_client;
struct wl_resource;

/* == Declarations ========================================================= */

/** State of the toplevel icon manager. */
struct _wlmaker_icon_manager_t {
    /** Back-link to the server. */
    wlmaker_server_t          *server_ptr;
    /** Back-link to the wayland Display. */
    struct wl_display         *wl_display_ptr;

    /** The global holding the icon manager's interface. */
    struct wl_global          *wl_global_ptr;

    struct wlr_renderer       *wlr_renderer_ptr;
    struct wlr_allocator      *wlr_allocator_ptr;
};

/** State of a toplevel icon. */
struct _wlmaker_toplevel_icon_t {
    /** The icon is also a toolkit tile. */
    wlmtk_tile_t              super_tile;
    /** The surface element, being the content of the tile. */
    wlmtk_surface_t           *content_surface_ptr;

    /** Back-link to the client requesting the toplevel. */
    struct wl_client          *wl_client_ptr;
    /** Back-link to the icon manager. */
    wlmaker_icon_manager_t    *icon_manager_ptr;
    struct wl_resource        *icon_manager_wl_resource_ptr;
    /** The provided ID. */
    uint32_t                  id;
    /** The XDG toplevel for which the icon is specified. */
    struct wlr_xdg_toplevel   *wlr_xdg_toplevel_ptr;
    /** The surface to use for the icon of this toplevel. */
    struct wlr_surface        *wlr_surface_ptr;

    /** The resource associated with this icon. */
    struct wl_resource        *wl_resource_ptr;

    /** Whether the configuration sequence was acknowledged. */
    bool                      acknowledged;
    /** Serial that needs to be acknowledged. */
    uint32_t                  pending_serial;

    /** Listener for the `commit` event of `wlr_surface_ptr`. */
    struct wl_listener        surface_commit_listener;
    /** Listener for the `destroy` event of `wlr_surface_ptr`. */
    struct wl_listener        surface_destroy_listener;
};

static wlmaker_icon_manager_t *icon_manager_from_resource(
    struct wl_resource *wl_resource_ptr);
static wlmaker_toplevel_icon_t *wlmaker_toplevel_icon_from_resource(
    struct wl_resource *wl_resource_ptr);

static void bind_icon_manager(
    struct wl_client *wl_client_ptr,
    void *data_ptr,
    uint32_t version,
    uint32_t id);

static void handle_resource_destroy(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr);

static void handle_get_toplevel_icon(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_icon_manager_resource_ptr,
    uint32_t id,
    struct wl_resource *wl_toplevel_resource_ptr,
    struct wl_resource *wl_surface_resource_ptr);

static wlmaker_toplevel_icon_t *wlmaker_toplevel_icon_create(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_icon_manager_resource_ptr,
    uint32_t id,
    int version,
    struct wlr_xdg_toplevel *wlr_xdg_toplevel_ptr,
    struct wlr_surface *wlr_surface_ptr);

static void wlmaker_toplevel_icon_destroy(
    wlmaker_toplevel_icon_t *toplevel_icon_ptr);
static void toplevel_icon_resource_destroy(
    struct wl_resource *wl_resource_ptr);

static void handle_icon_ack_configure(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr,
    uint32_t serial);
static void handle_surface_commit(
    struct wl_listener *listener_ptr,
    void *data_ptr);
static void handle_surface_destroy(
    struct wl_listener *listener_ptr,
    void *data_ptr);

static void _wlmaker_toplevel_icon_element_destroy(
    wlmtk_element_t *element_ptr);

/* == Data ================================================================= */

/** Implementation of the toplevel icon manager interface. */
static const struct zwlmaker_icon_manager_v1_interface
icon_manager_v1_implementation = {
    .destroy = handle_resource_destroy,
    .get_toplevel_icon = handle_get_toplevel_icon
};

/** Implementation of the toplevel icon interface. */
static const struct zwlmaker_toplevel_icon_v1_interface
toplevel_icon_v1_implementation = {
    .destroy = handle_resource_destroy,
    .ack_configure = handle_icon_ack_configure,
};

/** The icon's extension to @ref wlmtk_element_t virtual method table. */
static const wlmtk_element_vmt_t _wlmaker_toplevel_icon_element_vmt = {
    .destroy = _wlmaker_toplevel_icon_element_destroy,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmaker_icon_manager_t *wlmaker_icon_manager_create(
    struct wl_display *wl_display_ptr,
    wlmaker_server_t *server_ptr)
{
    wlmaker_icon_manager_t *icon_manager_ptr =
        logged_calloc(1, sizeof(wlmaker_icon_manager_t));
    if (NULL == icon_manager_ptr) return NULL;
    icon_manager_ptr->server_ptr = server_ptr;
    icon_manager_ptr->wl_display_ptr = wl_display_ptr;

    icon_manager_ptr->wl_global_ptr = wl_global_create(
        wl_display_ptr,
        &zwlmaker_icon_manager_v1_interface,
        1,
        icon_manager_ptr,
        bind_icon_manager);
    if (NULL == icon_manager_ptr->wl_global_ptr) {
        wlmaker_icon_manager_destroy(icon_manager_ptr);
        return NULL;
    }

    icon_manager_ptr->wlr_renderer_ptr = wlr_renderer_autocreate(
        wlmbe_backend_wlr(server_ptr->backend_ptr));
    icon_manager_ptr->wlr_allocator_ptr = wlr_allocator_autocreate(
        wlmbe_backend_wlr(server_ptr->backend_ptr),
        icon_manager_ptr->wlr_renderer_ptr);


    return icon_manager_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmaker_icon_manager_destroy(
    wlmaker_icon_manager_t *icon_manager_ptr)
{
    if (NULL != icon_manager_ptr->wl_global_ptr) {
        wl_global_destroy(icon_manager_ptr->wl_global_ptr);
        icon_manager_ptr->wl_global_ptr = NULL;
    }

    free(icon_manager_ptr);
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Returns the toplevel icon manager from the resource, with type check.
 *
 * @param wl_resource_ptr
 *
 * @return Pointer to the @ref wlmaker_icon_manager_t.
 */
wlmaker_icon_manager_t *icon_manager_from_resource(
    struct wl_resource *wl_resource_ptr)
{
    BS_ASSERT(wl_resource_instance_of(
                  wl_resource_ptr,
                  &zwlmaker_icon_manager_v1_interface,
                  &icon_manager_v1_implementation));
    return wl_resource_get_user_data(wl_resource_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Returns the toplevel icon from the resource, with type check.
 *
 * @param wl_resource_ptr
 *
 * @return Pointer to the @ref wlmaker_toplevel_icon_t.
 */
wlmaker_toplevel_icon_t *wlmaker_toplevel_icon_from_resource(
    struct wl_resource *wl_resource_ptr)
{
    BS_ASSERT(wl_resource_instance_of(
                  wl_resource_ptr,
                  &zwlmaker_toplevel_icon_v1_interface,
                  &toplevel_icon_v1_implementation));
    return wl_resource_get_user_data(wl_resource_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Binds an icon manager for the client.
 *
 * @param wl_client_ptr
 * @param data_ptr
 * @param version
 * @param id
 */
void bind_icon_manager(
    struct wl_client *wl_client_ptr,
    void *data_ptr,
    uint32_t version,
    uint32_t id)
{
    struct wl_resource *wl_resource_ptr = wl_resource_create(
        wl_client_ptr,
        &zwlmaker_icon_manager_v1_interface,
        version, id);
    if (NULL == wl_resource_ptr) {
        wl_client_post_no_memory(wl_client_ptr);
        return;
    }
   wlmaker_icon_manager_t *icon_manager_ptr = data_ptr;

    wl_resource_set_implementation(
        wl_resource_ptr,
        &icon_manager_v1_implementation,  // implementation.
        icon_manager_ptr,  // data
        NULL);  // dtor. We don't have an explicit one.
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the 'destroy' method: Destroys the resource.
 *
 * @param wl_client_ptr
 * @param wl_resource_ptr
 */
void handle_resource_destroy(
    __UNUSED__ struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr)
{
    wl_resource_destroy(wl_resource_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the 'get_toplevel_icon' method.
 *
 * @param wl_client_ptr
 * @param wl_icon_manager_resource_ptr
 * @param id
 * @param wl_toplevel_resource_ptr
 * @param wl_surface_resource_ptr
 */
void handle_get_toplevel_icon(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_icon_manager_resource_ptr,
    uint32_t id,
    struct wl_resource *wl_toplevel_resource_ptr,
    struct wl_resource *wl_surface_resource_ptr)
{
    struct wlr_xdg_toplevel *wlr_xdg_toplevel_ptr = NULL;
    if (NULL != wl_toplevel_resource_ptr) {
        wlr_xdg_toplevel_ptr = wlr_xdg_toplevel_from_resource(
            wl_toplevel_resource_ptr);
    }
    struct wlr_surface *wlr_surface_ptr =
        wlr_surface_from_resource(wl_surface_resource_ptr);

    wlmaker_toplevel_icon_t *toplevel_icon_ptr = wlmaker_toplevel_icon_create(
        wl_client_ptr,
        wl_icon_manager_resource_ptr,
        id,
        wl_resource_get_version(wl_icon_manager_resource_ptr),
        wlr_xdg_toplevel_ptr,
        wlr_surface_ptr);
    if (NULL == toplevel_icon_ptr) {
        wl_client_post_no_memory(wl_client_ptr);
        return;
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Creates a new toplevel icon.
 *
 * @param wl_client_ptr
 * @param icon_manager_ptr
 * @param id
 * @param version
 * @param wlr_xdg_toplevel_ptr
 * @param wlr_surface_ptr
 *
 * @return A pointer to the new toplevel icon or NULL on error. The toplevel
 *     icon's resources must be free'd via @ref wlmaker_toplevel_icon_destroy.
 */
wlmaker_toplevel_icon_t *wlmaker_toplevel_icon_create(
    struct wl_client *wl_client_ptr,
    struct wl_resource *wl_icon_manager_resource_ptr,
    uint32_t id,
    int version,
    struct wlr_xdg_toplevel *wlr_xdg_toplevel_ptr,
    struct wlr_surface *wlr_surface_ptr)
{
    wlmaker_icon_manager_t *icon_manager_ptr =
        icon_manager_from_resource(
            wl_icon_manager_resource_ptr);

    wlmaker_toplevel_icon_t *toplevel_icon_ptr = logged_calloc(
        1, sizeof(wlmaker_toplevel_icon_t));
    if (NULL == toplevel_icon_ptr) return NULL;

    toplevel_icon_ptr->wl_client_ptr = wl_client_ptr;
    toplevel_icon_ptr->icon_manager_ptr = icon_manager_ptr;
    toplevel_icon_ptr->id = id;
    toplevel_icon_ptr->wlr_xdg_toplevel_ptr = wlr_xdg_toplevel_ptr;
    toplevel_icon_ptr->wlr_surface_ptr = wlr_surface_ptr;
    toplevel_icon_ptr->icon_manager_wl_resource_ptr =
        wl_icon_manager_resource_ptr;

    toplevel_icon_ptr->wl_resource_ptr = wl_resource_create(
        wl_client_ptr,
        &zwlmaker_toplevel_icon_v1_interface,
        version,
        id);
    if (NULL == toplevel_icon_ptr->wl_resource_ptr) {
        bs_log(BS_ERROR, "Failed wl_resource_create(%p, %p, %d, %"PRIu32")",
               wl_client_ptr, &zwlmaker_toplevel_icon_v1_interface, version,
               id);
        wlmaker_toplevel_icon_destroy(toplevel_icon_ptr);
        return NULL;
    }
    wl_resource_set_implementation(
        toplevel_icon_ptr->wl_resource_ptr,
        &toplevel_icon_v1_implementation,
        toplevel_icon_ptr,
        toplevel_icon_resource_destroy);

    if (!wlmtk_tile_init(
            &toplevel_icon_ptr->super_tile,
            &icon_manager_ptr->server_ptr->style.tile)) {
        wlmaker_toplevel_icon_destroy(toplevel_icon_ptr);
        return NULL;
    }
    wlmtk_element_extend(
        wlmtk_tile_element(&toplevel_icon_ptr->super_tile),
        &_wlmaker_toplevel_icon_element_vmt);
    wlmtk_element_set_visible(
        wlmtk_tile_element(&toplevel_icon_ptr->super_tile),
        true);
    wlmtk_dock_add_tile(
        icon_manager_ptr->server_ptr->clip_dock_ptr,
        &toplevel_icon_ptr->super_tile);

    toplevel_icon_ptr->content_surface_ptr = wlmtk_surface_create(
        wlr_surface_ptr,
        icon_manager_ptr->server_ptr->wlr_seat_ptr);
    if (NULL == toplevel_icon_ptr->content_surface_ptr) {
        wlmaker_toplevel_icon_destroy(toplevel_icon_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_surface_element(toplevel_icon_ptr->content_surface_ptr),
        true);

    // Hack: Connect this listener after wlmtk_surface creation, so that the
    // surface knows it's size before added...
    wlmtk_util_connect_listener_signal(
        &toplevel_icon_ptr->wlr_surface_ptr->events.commit,
        &toplevel_icon_ptr->surface_commit_listener,
        handle_surface_commit);
    wlmtk_util_connect_listener_signal(
        &toplevel_icon_ptr->wlr_surface_ptr->events.destroy,
        &toplevel_icon_ptr->surface_destroy_listener,
        handle_surface_destroy);

    bs_log(BS_INFO,
           "created toplevel icon %p for toplevel %p, surface %p, "
           "wlmtk surface %p",
           toplevel_icon_ptr, wlr_xdg_toplevel_ptr, wlr_surface_ptr,
           toplevel_icon_ptr->content_surface_ptr);

    return toplevel_icon_ptr;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the toplevel icon.
 *
 * @param toplevel_icon_ptr
 */
void wlmaker_toplevel_icon_destroy(
    wlmaker_toplevel_icon_t *toplevel_icon_ptr)
{
    bs_log(BS_INFO, "Destroying toplevel icon %p", toplevel_icon_ptr);

    _wlmaker_toplevel_icon_element_destroy(
        wlmtk_tile_element(&toplevel_icon_ptr->super_tile));
    wlmtk_tile_fini(&toplevel_icon_ptr->super_tile);

    // Note: Not destroying toplevel_icon_ptr->resource, since that causes
    // cycles...

    free(toplevel_icon_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Destructor for the toplevel icon's resource.
 *
 * @param wl_resource_ptr
 */
void toplevel_icon_resource_destroy(struct wl_resource *wl_resource_ptr)
{
    wlmaker_toplevel_icon_t *toplevel_icon_ptr =
        wlmaker_toplevel_icon_from_resource(wl_resource_ptr);
    wlmaker_toplevel_icon_destroy(toplevel_icon_ptr);
}

/* ------------------------------------------------------------------------- */
/**
 * Handles the `ack_configure` request by the icon.
 *
 * @param wl_client_ptr
 * @param wl_resource_ptr
 * @param serial
 */
void handle_icon_ack_configure(
    __UNUSED__ struct wl_client *wl_client_ptr,
    struct wl_resource *wl_resource_ptr,
    uint32_t serial)
{
    wlmaker_toplevel_icon_t *toplevel_icon_ptr =
        wlmaker_toplevel_icon_from_resource(wl_resource_ptr);

    if (serial == toplevel_icon_ptr->pending_serial) {
        toplevel_icon_ptr->acknowledged = true;
        toplevel_icon_ptr->pending_serial = 0;
    }
}

/* ------------------------------------------------------------------------- */
void into_shm(
    struct wlr_client_buffer *wlr_client_buffer_ptr,
    struct wl_resource *wl_icon_manager_resource_ptr)
{
    __UNUSED__ wlmaker_icon_manager_t *icon_manager_ptr =
        icon_manager_from_resource(
            wl_icon_manager_resource_ptr);

#if 0
    // FIXME: This might be a means of rendering into a buffer.
    uint64_t modifier = DRM_FORMAT_MOD_LINEAR;
    struct wlr_drm_format format = {
        .format = DRM_FORMAT_ARGB8888,
        .len = 1, // Number of modifiers
        .modifiers = &modifier
    };
    struct wlr_buffer *wlr_buffer_ptr = wlr_allocator_create_buffer(
        icon_manager_ptr->wlr_allocator_ptr,
        64, 64, &format);
    if (NULL == wlr_buffer_ptr) {
        bs_log(BS_ERROR, "Failed wlr_allocator_create_buffer(...)");
        return;
    }

    struct wlr_render_pass *pass_ptr = wlr_renderer_begin_buffer_pass(
        icon_manager_ptr->wlr_renderer_ptr, wlr_buffer_ptr, NULL);
    if (NULL == pass_ptr) {
        bs_log(BS_ERROR, "Failed wlr_renderer_begin_buffer_pass");
        return;
    }

    struct wlr_render_texture_options to = {
        .texture = wlr_client_buffer_ptr->texture,
        .transform = WL_OUTPUT_TRANSFORM_NORMAL,
        .filter_mode = WLR_SCALE_FILTER_BILINEAR,
        .blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED,
    };
    wlr_render_pass_add_texture(pass_ptr, &to);
    wlr_render_pass_submit(pass_ptr);

    struct wlr_dmabuf_attributes dmabuf_attribs;
    if (!wlr_buffer_get_dmabuf(wlr_buffer_ptr, &dmabuf_attribs)) {
        bs_log(BS_ERROR, "Not dmabuf.");
        return;
    }
#endif


    int fd = shm_open("wlmFIXME", O_RDWR|O_CREAT|O_EXCL, 0600);
    if (0 >= fd) {
        bs_log(BS_ERROR, "Failed shm_open(...)");
        return;
    }
    shm_unlink("wlmFIXME");

    if (0 != ftruncate(fd, 16384)) {
        bs_log(BS_ERROR, "Failed ftruncate(%d, 16384)", fd);
        close(fd);
        return;
    }

    void *data_ptr = mmap(NULL, 16384, PROT_READ|PROT_WRITE,
                          MAP_SHARED, fd, 0);
    if (MAP_FAILED == data_ptr) {
        bs_log(BS_ERROR | BS_ERRNO, "Failed mmap()");
        data_ptr = NULL;
        close(fd);
        return;
    }

    if (false) {

        // Read from the input's wlr_buffer data_ptr_access.
        void *d_ptr;
        uint32_t format;
        size_t stride;
        if (!wlr_buffer_begin_data_ptr_access(
                wlr_client_buffer_ptr->source,
                WLR_BUFFER_DATA_PTR_ACCESS_READ,
                &d_ptr, &format, &stride)) {
            bs_log(BS_ERROR, "Failed wlr_buffer_begin_data_ptr_access()");
            return;
        }

        memcpy(data_ptr, d_ptr, wlr_client_buffer_ptr->source->height * stride);

        static uint8_t x = 0;
        x += 0x10;
        //memset(data_ptr, x, 16384);
        bs_log(BS_ERROR, "FIXME: x = %d", x);
        wlr_buffer_end_data_ptr_access(wlr_client_buffer_ptr->source);

    } else {

        // Reads from the texture. Slow, but should also work with GL.
        struct wlr_texture_read_pixels_options options = {
            .data = data_ptr,
            .format = DRM_FORMAT_XRGB8888,
            .stride = 4 * 64,
            .dst_x = 0,
            .dst_y = 0
        };

        if (!wlr_texture_read_pixels(wlr_client_buffer_ptr->texture, &options)) {
            bs_log(BS_ERROR, "Failed wlr_texture_read_pixels(%p, %p)",
                   wlr_client_buffer_ptr->texture, &options);
            return;
        }
    }

    bs_log(BS_ERROR, "FIXME: Texture stored.");
    zwlmaker_icon_manager_v1_send_get_buffer(
        wl_icon_manager_resource_ptr,
        fd);
    close(fd);
}

/* ------------------------------------------------------------------------- */
/**
 * Event handler for the `commit` signal of the icon's surface.
 *
 * The protocol expects a first `commit` with a NULL-buffer attached to the
 * surface. This will trigger a `configure` event, informing the client of the
 * suggested icon size.
 *
 * Only when the configuration was suggested and acknowledged a first time,
 * will we accept `commit` with attached buffers.
 *
 * @param listener_ptr
 * @param data_ptr            Points to the `struct wlr_surface` of the icon.
 */
void handle_surface_commit(
    struct wl_listener *listener_ptr,
    void *data_ptr)
{
    wlmaker_toplevel_icon_t *toplevel_icon_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_toplevel_icon_t, surface_commit_listener);
    struct wlr_surface *wlr_surface_ptr = data_ptr;
    BS_ASSERT(toplevel_icon_ptr->wlr_surface_ptr == wlr_surface_ptr);

    if (NULL == wlr_surface_ptr->buffer) {
        // An initial commit is expected with a NULL buffer, so we can
        // respond with a `configure` event.
        toplevel_icon_ptr->pending_serial = wl_display_next_serial(
            toplevel_icon_ptr->icon_manager_ptr->wl_display_ptr);
        zwlmaker_toplevel_icon_v1_send_configure(
            toplevel_icon_ptr->wl_resource_ptr,
            toplevel_icon_ptr->super_tile.style.size,
            toplevel_icon_ptr->super_tile.style.size,
            toplevel_icon_ptr->pending_serial);
        return;
    }
    BS_ASSERT(NULL != wlr_surface_ptr->buffer);

    into_shm(
        wlr_surface_ptr->buffer,
        toplevel_icon_ptr->icon_manager_wl_resource_ptr);

    if (!toplevel_icon_ptr->acknowledged) {
        wl_resource_post_error(
            toplevel_icon_ptr->wl_resource_ptr,
            1,
            "Commit non-NULL buffer without configure sequence.");
        return;
    }

    wlmtk_tile_set_content(
        &toplevel_icon_ptr->super_tile,
        wlmtk_surface_element(toplevel_icon_ptr->content_surface_ptr));
}


/* ------------------------------------------------------------------------- */
/** Handles when the surface is destroyed. */
static void handle_surface_destroy(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmaker_toplevel_icon_t *toplevel_icon_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmaker_toplevel_icon_t, surface_destroy_listener);
    _wlmaker_toplevel_icon_element_destroy(
        wlmtk_tile_element(&toplevel_icon_ptr->super_tile));
}

/* ------------------------------------------------------------------------- */
/**
 * Destructor of the icon's corresponding tile element.
 *
 * This is a hack: The `wlmaker_toplevel_icon_t` is owned by the wl_resource
 * and must only be freed when that dtor is caller. But, the element dtor may
 * be called on wlmaker shutdown, when eg. an icon app is still running.
 * So, for that case, we just detach the icon here.
 *
 * Leaving as is, since the icon model will need to be revamped anyway, to
 * line up with the new XDG icon protocol.
 *
 * @param element_ptr
 */
void _wlmaker_toplevel_icon_element_destroy(wlmtk_element_t *element_ptr)
{
    wlmaker_toplevel_icon_t *toplevel_icon_ptr = BS_CONTAINER_OF(
        element_ptr, wlmaker_toplevel_icon_t,
        super_tile.super_container.super_element);

    if (NULL != toplevel_icon_ptr->content_surface_ptr) {

        wlmtk_util_disconnect_listener(
            &toplevel_icon_ptr->surface_destroy_listener);
        wlmtk_util_disconnect_listener(
            &toplevel_icon_ptr->surface_commit_listener);

        wlmtk_tile_set_content(&toplevel_icon_ptr->super_tile, NULL);
        wlmtk_surface_destroy(toplevel_icon_ptr->content_surface_ptr);
        toplevel_icon_ptr->content_surface_ptr = NULL;
    }

    if (wlmtk_tile_element(&toplevel_icon_ptr->super_tile
            )->parent_container_ptr) {
        wlmtk_dock_remove_tile(
            toplevel_icon_ptr->icon_manager_ptr->server_ptr->clip_dock_ptr,
            &toplevel_icon_ptr->super_tile);
    }
}

/* == End of icon_manager.c ================================================ */
