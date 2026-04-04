/* ========================================================================= */
/**
 * @file wlmbattery.c
 *
 * Displays battery capacity from /sys/class/power_supply in a DockApp.
 *
 * TODO(kaeser@gubbe.ch):
 * - Only update when the numbers have actually changed.
 * - Support more than one battery (eg. by clicking through?)
 * - Use graphics for visualization, this initial version is very crude.
 * - Permit dynamic size of the app, currently it's fixed to 64x64.
 *
 * @copyright
 * Copyright 2026 Google LLC
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

#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <primitives/primitives.h>
#include <libwlclient/libwlclient.h>

#include <libbase/libbase.h>

/* == Declarations ========================================================= */

/** Sufficient for the typical paths, although there is no strict maximum. */
#define SYSFS_BATTERY_PATH_MAX_LEN 256
/** Small buffer for reading single values (like capacity). */
#define VAL_BUF_LEN 64

/** Internal battery status enum. */
enum battery_status {
    BATTERY_STATUS_UNKNOWN = 0,
    BATTERY_STATUS_CHARGING,
    BATTERY_STATUS_DISCHARGING,
    BATTERY_STATUS_NOT_CHARGING,
    BATTERY_STATUS_FULL
};

/** Power supply state container. */
struct wlm_power_supply {
    /** List of batteries. Elements are @ref wlm_battery::dlnode. */
    bs_dllist_t batteries;
    /** List of adapters. Elements are @ref wlm_power_adapter::dlnode. */
    bs_dllist_t adapters;
};

/** Power adapter state container. */
struct wlm_power_adapter {
    /** List node, member of  @ref wlm_power_supply::adapters. */
    bs_dllist_node_t dlnode;
    /** Adapter name (e.g. "AC0"). */
    char *name;
    /** Absolute sysfs path. */
    char *sysfs_path;
    /** Whether the power adapter is online. */
    bool online;
};

/** Battery state container. */
struct wlm_battery {
    /** List node, member of @ref wlm_power_supply::batteries. */
    bs_dllist_node_t dlnode;
    /** Battery name (e.g. "BAT0"). */
    char *name;
    /** Absolute sysfs path. */
    char *sysfs_path;

    /** Loaded battery properties */
    uint64_t capacity;
    /** Current battery status. */
    enum battery_status status;

    /** Current energy. */
    uint64_t energy_now;
    /** Energy when full */
    uint64_t energy_full;
    /** power being added currently. */
    uint64_t power_now;

    /** Current charge. */
    uint64_t charge_now;
    /** Full charge. */
    uint64_t charge_full;
    /** current adding charge currently. */
    uint64_t current_now;

    /** Precalculated remaining time in minutes, based on availability of energy/charge records (-1 if unavailable). */
    int time_remaining_min;
};

static struct wlm_power_supply *wlm_power_supply_create(void);
static void wlm_power_supply_destroy(struct wlm_power_supply *ps);
static bool wlm_power_supply_read(struct wlm_power_supply *ps);
static size_t wlm_power_supply_num_batteries(struct wlm_power_supply *ps);
static struct wlm_battery *wlm_power_supply_battery(
    struct wlm_power_supply *ps,
    size_t index);
static bool wlm_power_supply_connected(struct wlm_power_supply *ps);

static struct wlm_battery *wlm_battery_create(
    const char *name_ptr,
    const char *power_supply_dir);
static void wlm_battery_destroy(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static bool wlm_battery_read(struct wlm_battery *bat);

static struct wlm_power_adapter *wlm_power_adapter_create(
    const char *name_ptr,
    const char *power_supply_dir);
static void wlm_power_adapter_destroy(
    bs_dllist_node_t *dlnode_ptr,
    void *ud_ptr);
static bool wlm_power_adapter_connected(
    bs_dllist_node_t *node_ptr,
    void *ud_ptr);
static bool wlm_power_adapter_read(struct wlm_power_adapter *adapter);

static bool wlm_vread_buffer(
    char *v, size_t v_size, const char *fmt_ptr, va_list ap);
static bool wlm_read_buffer(
    char *v, size_t v_size, const char *fmt_ptr, ...);
static bool wlm_read_uint64(
    uint64_t *u64_ptr, const char *fmt_ptr, ...);
static enum battery_status parse_battery_status(const char *status_str);


/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Iterates through /sys/class/power_supply and creates corresponding
 * battery and adapter items.
 *
 * @return Pointer to the power supply, or NULL on error.
 */
struct wlm_power_supply *wlm_power_supply_create(void)
{
    struct wlm_power_supply *ps = logged_calloc(1, sizeof(*ps));
    if (!ps) return NULL;

    static const char *power_supply_dir = "/sys/class/power_supply";
    DIR *dir = opendir(power_supply_dir);
    if (!dir) {
        bs_log(BS_ERROR, "Failed to open %s", power_supply_dir);
        free(ps);
        return NULL;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (bs_str_startswith(ent->d_name, ".")) {
            continue;
        }

        if (bs_str_startswith(ent->d_name, "BAT")) {
            struct wlm_battery *bat = wlm_battery_create(ent->d_name, power_supply_dir);
            if (bat) {
                bs_dllist_push_back(&ps->batteries, &bat->dlnode);
            }
        } else {
            struct wlm_power_adapter *adapter = wlm_power_adapter_create(ent->d_name, power_supply_dir);
            if (adapter) {
                bs_dllist_push_back(&ps->adapters, &adapter->dlnode);
            }
        }
    }
    closedir(dir);
    return ps;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys the power supply container and internal batteries/adapters.
 *
 * @param ps Pointer to the target power supply.
 */
void wlm_power_supply_destroy(struct wlm_power_supply *ps)
{
    if (!ps) return;

    bs_dllist_for_each(&ps->batteries, wlm_battery_destroy, NULL);
    bs_dllist_for_each(&ps->adapters, wlm_power_adapter_destroy, NULL);
    free(ps);
}

/* ------------------------------------------------------------------------- */
/**
 * Reads current state of the power supply (batteries and adapters).
 *
 * @param ps
 *
 * @return true on success.
 */
bool wlm_power_supply_read(struct wlm_power_supply *ps)
{
    bs_dllist_node_t *dlnode_ptr;
    bool failure = false;

    for (dlnode_ptr = ps->batteries.head_ptr;
         NULL != dlnode_ptr;
         dlnode_ptr = bs_dllist_node_iterator_forward(dlnode_ptr)) {
        struct wlm_battery *bat = BS_CONTAINER_OF(
            dlnode_ptr, struct wlm_battery, dlnode);
        failure |= !(wlm_battery_read(bat));
    }

    for (dlnode_ptr = ps->adapters.head_ptr;
         NULL != dlnode_ptr;
         dlnode_ptr = bs_dllist_node_iterator_forward(dlnode_ptr)) {
        struct wlm_power_adapter *a = BS_CONTAINER_OF(
            dlnode_ptr, struct wlm_power_adapter, dlnode);
        failure |= !(wlm_power_adapter_read(a));
    }

    return !failure;
}

/* ------------------------------------------------------------------------- */
/**
 * Returns the tracked amount of batteries.
 *
 * @param ps Pointer to the target power supply.
 *
 * @return the tracked amount of batteries.
 */
size_t wlm_power_supply_num_batteries(struct wlm_power_supply *ps)
{
    return bs_dllist_size(&ps->batteries);
}

/* ------------------------------------------------------------------------- */
/**
 * Extract battery handle at specified `index` linearly.
 *
 * @param ps Pointer to the structured system state container.
 * @param index Index integer for node traversal limit bounding.
 *
 * @return Sliced battery memory, NULL if invalid parameter passed.
 */
struct wlm_battery *wlm_power_supply_battery(
    struct wlm_power_supply *ps,
    size_t index)
{
    bs_dllist_node_t *node = ps->batteries.head_ptr;
    for (size_t i = 0; i < index && node != NULL; ++i) {
        node = bs_dllist_node_iterator_forward(node);
    }
    if (!node) return NULL;
    return BS_CONTAINER_OF(node, struct wlm_battery, dlnode);
}

/* ------------------------------------------------------------------------- */
/**
 * Indicates if any of the power adapters is online.
 *
 * @param ps Pointer to the target power supply.
 *
 * @return True if at least one adapter is connected and online, otherwise false.
 */
bool wlm_power_supply_connected(struct wlm_power_supply *ps)
{
    return bs_dllist_any(&ps->adapters, wlm_power_adapter_connected, NULL);
}

/* ------------------------------------------------------------------------- */
/**
 * Creates battery struct spanning the specific `name_ptr`.
 *
 * @param name_ptr Null-terminated property name segment (e.g. "BAT0").
 * @param power_supply_dir The root directory where sysfs devices live.
 *
 * @return Pointer to allocated battery structure, or NULL on error.
 */
struct wlm_battery *wlm_battery_create(
    const char *name_ptr,
    const char *power_supply_dir)
{
    struct wlm_battery *bat = logged_calloc(1, sizeof(*bat));
    if (!bat) return NULL;
    bat->name = logged_strdup(name_ptr);
    if (!bat->name) {
        wlm_battery_destroy(&bat->dlnode, NULL);
        return NULL;
    }

    bat->sysfs_path = bs_strdupf("%s/%s", power_supply_dir, name_ptr);
    if (!bat->sysfs_path) {
        wlm_battery_destroy(&bat->dlnode, NULL);
        return NULL;
    }
    return bat;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys battery struct.
 *
 * @param dlnode_ptr
 * @param ud_ptr
 */
void wlm_battery_destroy(
    bs_dllist_node_t *dlnode_ptr,
    __UNUSED__ void *ud_ptr)
{
    struct wlm_battery *bat = BS_CONTAINER_OF(
        dlnode_ptr, struct wlm_battery, dlnode);
    if (bat->name) {
        free(bat->name);
    }
    if (bat->sysfs_path) {
        free(bat->sysfs_path);
    }
    free(bat);
}

/* ------------------------------------------------------------------------- */
/**
 * Helper to pull all readings into the structural cache representation.
 *
 * @param bat Pointer to the target battery object tracking the metric states.
 *
 * @return True on success.
 */
bool wlm_battery_read(struct wlm_battery *bat)
{
    // Capacity
    if (!wlm_read_uint64(&bat->capacity, "%s/capacity", bat->sysfs_path)) {
        return false;
    }

    // Status
    char buf[VAL_BUF_LEN];
    if (!wlm_read_buffer(buf, sizeof(buf), "%s/status", bat->sysfs_path)) {
        return false;
    } else {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') {
            buf[len - 1] = '\0';
        }
    }
    bat->status = parse_battery_status(buf);

    // Advanced hardware stats gathering
    bat->time_remaining_min = -1;
    bat->charge_now = bat->charge_full = bat->current_now = 0;
    bat->energy_now = bat->energy_full = bat->power_now = 0;

    uint64_t val_now = 0, val_full = 0, rate = 0;

    /*
     * Linux power_supply exposes either charge-based (µAh) or energy-based (µWh)
     * parameters depending on the specific hardware controller. Reading charge_now
     * will naturally fail on systems utilizing energy parameters.
     */
    if (wlm_read_uint64(&bat->charge_now, "%s/charge_now", bat->sysfs_path)) {
        val_now = bat->charge_now;

        if (wlm_read_uint64(&bat->charge_full, "%s/charge_full", bat->sysfs_path)) {
            val_full = bat->charge_full;
        }

        if (wlm_read_uint64(&bat->current_now, "%s/current_now", bat->sysfs_path)) {
            rate = bat->current_now;
        }
    } else {
        if (wlm_read_uint64(&bat->energy_now, "%s/energy_now", bat->sysfs_path)) {
            val_now = bat->energy_now;

            if (wlm_read_uint64(&bat->energy_full, "%s/energy_full", bat->sysfs_path)) {
                val_full = bat->energy_full;
            }

            if (wlm_read_uint64(&bat->power_now, "%s/power_now", bat->sysfs_path)) {
                rate = bat->power_now;
            }
        }
    }

    if (rate > 0 && (bat->status == BATTERY_STATUS_CHARGING ||
                     bat->status == BATTERY_STATUS_DISCHARGING)) {
        double hours = 0;
        if (bat->status == BATTERY_STATUS_DISCHARGING) {
            hours = (double)val_now / rate;
        } else if (bat->status == BATTERY_STATUS_CHARGING) {
            hours = (double)(val_full > val_now ? val_full - val_now : 0) / rate;
        }
        bat->time_remaining_min = (int)(hours * 60.0);
    }

    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Creates adapter struct spanning the specific `name_ptr`.
 *
 * @param name_ptr Null-terminated property name segment (e.g. "AC0").
 * @param power_supply_dir The root directory where sysfs devices live.
 *
 * @return Pointer to allocated adapter structure, or NULL on error.
 */
struct wlm_power_adapter *wlm_power_adapter_create(
    const char *name_ptr,
    const char *power_supply_dir)
{
    struct wlm_power_adapter *adapter = logged_calloc(1, sizeof(*adapter));
    if (!adapter) return NULL;
    adapter->name = logged_strdup(name_ptr);
    if (!adapter->name) {
        wlm_power_adapter_destroy(&adapter->dlnode, NULL);
        return NULL;
    }

    adapter->sysfs_path = bs_strdupf("%s/%s", power_supply_dir, name_ptr);
    if (!adapter->sysfs_path) {
        wlm_power_adapter_destroy(&adapter->dlnode, NULL);
        return NULL;
    }
    return adapter;
}

/* ------------------------------------------------------------------------- */
/**
 * Destroys power adapter struct.
 *
 * @param dlnode_ptr The list node of the power adapter to destroy.
 * @param ud_ptr
 */
void wlm_power_adapter_destroy(
    bs_dllist_node_t *dlnode_ptr,
    __UNUSED__ void *ud_ptr)
{
    struct wlm_power_adapter *adapter = BS_CONTAINER_OF(
        dlnode_ptr, struct wlm_power_adapter, dlnode);
    if (adapter->name) {
        free(adapter->name);
    }
    if (adapter->sysfs_path) {
        free(adapter->sysfs_path);
    }
    free(adapter);
}

/**
 * Iterator function to verify whether a power adapter node is connected.
 *
 * @param node_ptr The doubly-linked list node embedded in a wlm_power_adapter.
 * @param ud_ptr Ignored user data.
 *
 * @return True if the adapter is online.
 */
bool wlm_power_adapter_connected(
    bs_dllist_node_t *node_ptr,
    __UNUSED__ void *ud_ptr)
{
    struct wlm_power_adapter *adapter = BS_CONTAINER_OF(node_ptr, struct wlm_power_adapter, dlnode);
    return adapter->online;
}

/* ------------------------------------------------------------------------- */
/**
 * Reads power adapter properties into the structural cache representation.
 *
 * @param adapter Pointer to the target power adapter object.
 *
 * @return True on success.
 */
bool wlm_power_adapter_read(struct wlm_power_adapter *adapter)
{
    uint64_t v;
    if (!wlm_read_uint64(&v, "%s/online", adapter->sysfs_path)) {
        return false;
    }

    adapter->online = (v != 0);
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Reads content of file named by the formatted path into a buffer.
 *
 * @param v Output buffer.
 * @param v_size Length boundary for `v`.
 * @param fmt_ptr Format string for the path.
 * @param ap Variadic arguments list.
 *
 * @return True if read safely, otherwise false.
 */
bool wlm_vread_buffer(char *v, size_t v_size, const char *fmt_ptr, va_list ap)
{
    char path[SYSFS_BATTERY_PATH_MAX_LEN];
    int len = vsnprintf(path, sizeof(path), fmt_ptr, ap);

    if (len < 0 || (size_t)len >= sizeof(path)) {
        bs_log(BS_ERROR, "Path too long or format error");
        return false;
    }

    if (bs_file_read_buffer(path, v, v_size) < 0) {
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Reads content of file named by the formatted path into the buffer.
 *
 * @param v Output buffer.
 * @param v_size Length boundary for `v`.
 * @param fmt_ptr Format string for the path.
 * @param ... Arguments for the format string.
 *
 * @return True if read safely, otherwise false.
 */
bool wlm_read_buffer(char *v, size_t v_size, const char *fmt_ptr, ...)
{
    va_list ap;
    va_start(ap, fmt_ptr);
    bool result = wlm_vread_buffer(v, v_size, fmt_ptr, ap);
    va_end(ap);
    return result;
}

/* ------------------------------------------------------------------------- */
/**
 * Reads content of file named by the formatted path and parses it as uint64.
 *
 * @param u64_ptr Output pointer for the parsed integer.
 * @param fmt_ptr Format string for the path.
 * @param ... Arguments for the format string.
 *
 * @return True on successful parse, otherwise false.
 */
bool wlm_read_uint64(uint64_t *u64_ptr, const char *fmt_ptr, ...)
{
    char buf[VAL_BUF_LEN];
    va_list ap;

    va_start(ap, fmt_ptr);
    bool result = wlm_vread_buffer(buf, sizeof(buf), fmt_ptr, ap);
    va_end(ap);

    return result && bs_strconvert_uint64(buf, u64_ptr, 10);
}

/* ------------------------------------------------------------------------- */
/**
 * Maps standard sysfs status string to internal enum.
 *
 * @param status_str String literal describing the battery status.
 *
 * @return The corresponding parsed internal enum status.
 */
static enum battery_status parse_battery_status(const char *status_str)
{
    if (strcmp(status_str, "Charging") == 0) return BATTERY_STATUS_CHARGING;
    if (strcmp(status_str, "Discharging") == 0) return BATTERY_STATUS_DISCHARGING;
    if (strcmp(status_str, "Not charging") == 0) return BATTERY_STATUS_NOT_CHARGING;
    if (strcmp(status_str, "Full") == 0) return BATTERY_STATUS_FULL;
    return BATTERY_STATUS_UNKNOWN;
}

/* ------------------------------------------------------------------------- */
/** Argument to @ref icon_callback and @ref timer_callback. */
struct callback_arg {
    /** The icon handle */
    wlclient_icon_t           *icon_ptr;
    /** Power supply handle */
    struct wlm_power_supply   *ps;
};

/* ------------------------------------------------------------------------- */
/**
 * Draws current byttery status into the icon buffer.
 *
 * @param gfxbuf_ptr
 * @param ud_ptr
 */
bool icon_callback(
    bs_gfxbuf_t *gfxbuf_ptr,
    void *ud_ptr)
{
    struct callback_arg *arg_ptr = ud_ptr;

    bs_gfxbuf_clear(gfxbuf_ptr, 0);

    cairo_t *cairo_ptr = cairo_create_from_bs_gfxbuf(gfxbuf_ptr);
    if (NULL == cairo_ptr) {
        bs_log(BS_ERROR, "Failed cairo_create_from_bs_gfxbuf(%p)", gfxbuf_ptr);
        return false;
    }

    float r, g, b, alpha;
    bs_gfxbuf_argb8888_to_floats(0xff111111, &r, &g, &b, &alpha);
    cairo_pattern_t *pattern_ptr = cairo_pattern_create_rgba(r, g, b, alpha);
    BS_ASSERT(NULL != pattern_ptr);
    cairo_set_source(cairo_ptr, pattern_ptr);
    cairo_pattern_destroy(pattern_ptr);
    cairo_rectangle(cairo_ptr, 4, 4, 56, 56);
    cairo_fill(cairo_ptr);

    wlm_primitives_draw_bezel_at(cairo_ptr, 4, 4, 56, 56, 1.0, false);

    wlm_power_supply_read(arg_ptr->ps);

    cairo_select_font_face(
        cairo_ptr, "Helvetica", CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cairo_ptr, 12);
    cairo_set_source_argb8888(cairo_ptr, 0xffffffff);


    if (0 < wlm_power_supply_num_batteries(arg_ptr->ps)) {
        struct wlm_battery *bat = wlm_power_supply_battery(arg_ptr->ps, 0);

        char buf[16];

        cairo_move_to(cairo_ptr, 12, 19);
        snprintf(buf, sizeof(buf), "%3"PRIu64"%%", bat->capacity);
        cairo_show_text(cairo_ptr, buf);

        cairo_move_to(cairo_ptr, 12, 31);
        switch (bat->status) {
        case BATTERY_STATUS_CHARGING: cairo_show_text(cairo_ptr, "CHRG"); break;
        case BATTERY_STATUS_DISCHARGING: cairo_show_text(cairo_ptr, "DISC"); break;
        case BATTERY_STATUS_NOT_CHARGING: cairo_show_text(cairo_ptr, "----"); break;
        case BATTERY_STATUS_FULL: cairo_show_text(cairo_ptr, "FULL"); break;
        default: cairo_show_text(cairo_ptr, "UNKN"); break;
        }

        cairo_move_to(cairo_ptr, 12, 43);
        if (0 <= bat->time_remaining_min) {
            snprintf(buf, sizeof(buf), "% 2d:%02d",
                     bat->time_remaining_min / 60,
                     bat->time_remaining_min % 60);
            cairo_show_text(cairo_ptr, buf);
        }
    }

    cairo_move_to(cairo_ptr, 12, 55);
    if (wlm_power_supply_connected(arg_ptr->ps)) {
        cairo_show_text(cairo_ptr, " AC ");
    } else {
        cairo_show_text(cairo_ptr, "no pwr");
    }


    cairo_destroy(cairo_ptr);

    return true;
}

/* ------------------------------------------------------------------------- */
/** Called once per second. */
void timer_callback(wlclient_t *client_ptr, void *ud_ptr)
{
    struct callback_arg *arg_ptr = ud_ptr;

    wlclient_icon_register_ready_callback(
        arg_ptr->icon_ptr, icon_callback, arg_ptr);
    wlclient_register_timer(
        client_ptr, bs_usec() + 1000000, timer_callback,
        arg_ptr);
}

/* == Main program ========================================================= */
/** @return `EXIT_SUCCESS` on success. */
int main(void)
{
    struct wlm_power_supply *ps = wlm_power_supply_create();
    if (!ps) {
        return EXIT_FAILURE;
    }

    wlclient_t *wlclient_ptr = wlclient_create("wlmaker.wlmbattery");
    if (NULL == wlclient_ptr) {
        wlm_power_supply_destroy(ps);
        return EXIT_FAILURE;
    }

    if (wlclient_icon_supported(wlclient_ptr)) {
        wlclient_icon_t *icon_ptr = wlclient_icon_create(wlclient_ptr);
        struct callback_arg arg = { .ps = ps, .icon_ptr = icon_ptr };
        if (NULL == icon_ptr) {
            bs_log(BS_ERROR, "Failed wlclient_icon_create(%p)", wlclient_ptr);
        } else {
            wlclient_icon_register_ready_callback(
                icon_ptr, icon_callback, &arg);
            wlclient_register_timer(
                wlclient_ptr, bs_usec() + 1000000, timer_callback, &arg);

            wlclient_run(wlclient_ptr);
            wlclient_icon_destroy(icon_ptr);
        }
    } else {
        bs_log(BS_ERROR, "icon protocol is not supported.");
    }

    wlclient_destroy(wlclient_ptr);

    wlm_power_supply_destroy(ps);
    return EXIT_SUCCESS;
}
/* == End of wlmbattery.c ================================================== */
