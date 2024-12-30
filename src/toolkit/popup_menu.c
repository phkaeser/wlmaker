/* ========================================================================= */
/**
 * @file popup_menu.c
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

#include "popup_menu.h"

#include "menu.h"

/* == Declarations ========================================================= */

/** State of the popup menu. */
struct _wlmtk_popup_menu_t {
    /** Wrapped as a popup. */
    wlmtk_popup_t             super_popup;
    /** The contained menu. */
    wlmtk_menu_t              menu;

    /** Events of the popup menu. */
    wlmtk_popup_menu_events_t events;

    /** The element's original virtual method table. */
    wlmtk_element_vmt_t       orig_element_vmt;
};

static bool _wlmtk_popup_menu_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);

/* == Data ================================================================= */

/** The superclass' element virtual method table. */
static const wlmtk_element_vmt_t _wlmtk_popup_menu_element_vmt = {
    .pointer_button = _wlmtk_popup_menu_element_pointer_button
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
wlmtk_popup_menu_t *wlmtk_popup_menu_create(
    const wlmtk_menu_style_t *style_ptr,
    wlmtk_env_t *env_ptr)
{
    wlmtk_popup_menu_t *popup_menu_ptr = logged_calloc(
        1, sizeof(wlmtk_popup_menu_t));
    if (NULL == popup_menu_ptr) return NULL;

    if (!wlmtk_menu_init(&popup_menu_ptr->menu, style_ptr, env_ptr)) {
        wlmtk_popup_menu_destroy(popup_menu_ptr);
        return NULL;
    }
    wlmtk_element_set_visible(
        wlmtk_menu_element(&popup_menu_ptr->menu), true);

    if (!wlmtk_popup_init(&popup_menu_ptr->super_popup,
                          env_ptr,
                          wlmtk_menu_element(&popup_menu_ptr->menu))) {
        wlmtk_popup_menu_destroy(popup_menu_ptr);
        return NULL;
    }
    popup_menu_ptr->orig_element_vmt = wlmtk_element_extend(
        wlmtk_popup_element(&popup_menu_ptr->super_popup),
        &_wlmtk_popup_menu_element_vmt);

    wl_signal_init(&popup_menu_ptr->events.request_close);
    return popup_menu_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_popup_menu_destroy(wlmtk_popup_menu_t *popup_menu_ptr)
{
    wlmtk_popup_fini(&popup_menu_ptr->super_popup);
    wlmtk_menu_fini(&popup_menu_ptr->menu);
    free(popup_menu_ptr);
}

/* ------------------------------------------------------------------------- */
wlmtk_popup_menu_events_t *wlmtk_popup_menu_events(
    wlmtk_popup_menu_t *popup_menu_ptr)
{
    return &popup_menu_ptr->events;
}

/* ------------------------------------------------------------------------- */
wlmtk_popup_t *wlmtk_popup_menu_popup(wlmtk_popup_menu_t *popup_menu_ptr)
{
    return &popup_menu_ptr->super_popup;
}

/* ------------------------------------------------------------------------- */
wlmtk_menu_t *wlmtk_popup_menu_menu(wlmtk_popup_menu_t *popup_menu_ptr)
{
    return &popup_menu_ptr->menu;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * If the menu is in right-click mode, acts on right-button events and signals
 * the menu to close.
 *
 * Implementation of @ref wlmtk_element_vmt_t::pointer_button.
 *
 * @param element_ptr
 * @param button_event_ptr
 *
 * @return whether the button event was claimed.
 */
bool _wlmtk_popup_menu_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmtk_popup_menu_t *popup_menu_ptr = BS_CONTAINER_OF(
        element_ptr,
        wlmtk_popup_menu_t,
        super_popup.super_container.super_element);

    bool rv = popup_menu_ptr->orig_element_vmt.pointer_button(
        element_ptr, button_event_ptr);

    if (WLMTK_MENU_MODE_RIGHTCLICK == popup_menu_ptr->menu.mode &&
        BTN_RIGHT == button_event_ptr->button) {
        wl_signal_emit(&popup_menu_ptr->events.request_close, NULL);
        rv = true;
    }

    return rv;
}

// TODO(kaeser@gubbe.ch): Add tests.

/* == End of popup_menu.c ================================================== */
