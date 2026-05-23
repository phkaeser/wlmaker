# Implementation Plan - Dynamic Keyboard Layout Synchronization

Listen to the `keymap` event of `wl_keyboard` on the client side (`wlclient`), and propagate keymap updates to the nested compositor (`wlmdock`) input manager, replacing the hardcoded layout configuration.

## Proposed Changes

### WLClient Component

#### [wlclient.h](file:///home/furball/furbot-code/wlmaker/src/wlclient/wlclient.h)
- Add a new signal `keymap` to the `wlmcl_client_events` struct:
  ```c
  struct wl_signal          keymap;
  ```
- Declare a getter function to access the current `struct xkb_keymap`:
  ```c
  struct xkb_keymap *wlmcl_client_get_keymap(wlmcl_client_t *wlmcl_client_ptr);
  ```

#### [wlclient.c](file:///home/furball/furbot-code/wlmaker/src/wlclient/wlclient.c)
- Initialize the `keymap` signal inside `wlmcl_client_create`:
  ```c
  wl_signal_init(&wlclient_ptr->events.keymap);
  ```
- Implement `wlmcl_client_get_keymap` to return `wlmcl_client_ptr->xkb_keymap_ptr`.
- Emit the `keymap` signal from `_wlmcl_client_keyboard_handle_keymap` once a new keymap is successfully loaded and parsed:
  ```c
  wl_signal_emit(&client_ptr->events.keymap, client_ptr->xkb_keymap_ptr);
  ```

---

### Input Manager Component

#### [manager.h](file:///home/furball/furbot-code/wlmaker/src/input/manager.h)
- Declare getter and setter functions for the input manager's keymap:
  ```c
  struct xkb_keymap *wlmim_input_manager_get_keymap(wlmim_t *input_manager_ptr);
  void wlmim_input_manager_set_keymap(
      wlmim_t *input_manager_ptr,
      struct xkb_keymap *xkb_keymap_ptr);
  ```

#### [manager.c](file:///home/furball/furbot-code/wlmaker/src/input/manager.c)
- Add an `xkb_keymap_ptr` field to `struct _wlmim_t` to store the active keymap.
- Reference the keymap using `xkb_keymap_ref` on assignment and release it using `xkb_keymap_unref` when destroyed or replaced.
- Implement the getter `wlmim_input_manager_get_keymap`.
- Implement `wlmim_input_manager_set_keymap` to update the stored keymap and call `wlmim_keyboard_set_keymap` on all registered keyboard devices.

#### [keyboard.h](file:///home/furball/furbot-code/wlmaker/src/input/keyboard.h)
- Declare `wlmim_keyboard_set_keymap`:
  ```c
  void wlmim_keyboard_set_keymap(
      wlmim_keyboard_t *keyboard_ptr,
      struct xkb_keymap *xkb_keymap_ptr);
  ```

#### [keyboard.c](file:///home/furball/furbot-code/wlmaker/src/input/keyboard.c)
- Implement `wlmim_keyboard_set_keymap` to update the underlying `wlr_keyboard` keymap:
  ```c
  void wlmim_keyboard_set_keymap(
      wlmim_keyboard_t *keyboard_ptr,
      struct xkb_keymap *xkb_keymap_ptr) {
      wlr_keyboard_set_keymap(keyboard_ptr->wlr_keyboard_ptr, xkb_keymap_ptr);
  }
  ```
- Modify `wlmim_keyboard_create` to check for a non-NULL keymap from the input manager:
  ```c
  struct xkb_keymap *xkb_keymap_ptr = wlmim_input_manager_get_keymap(input_manager_ptr);
  if (NULL != xkb_keymap_ptr) {
      wlr_keyboard_set_keymap(keyboard_ptr->wlr_keyboard_ptr, xkb_keymap_ptr);
  } else {
      // (Fallback to parsing from rules configuration dictionary as before)
  }
  ```

---

### WLMDock Component

#### [wlmdock.c](file:///home/furball/furbot-code/wlmaker/src/dock/wlmdock.c)
- Add a new listener `client_keymap_listener` and callback `handle_client_keymap`.
- Register the listener to the client's `keymap` signal in `_wlmdock_create`.
- When the listener is called, update the input manager using `wlmim_input_manager_set_keymap`.
- Check if the keymap is already loaded during startup and apply it immediately.
- Simplify `config_dict_ptr` layout settings since layout configuration is now retrieved dynamically.

---

## Verification Plan

### Automated Tests
- Run `make` and `ctest` inside `build/` to verify tests pass.
- Run `make doc` in `build/` to ensure Doxygen docs compile cleanly.
- Run `make` in `build-iwyu/` to check for Include-What-You-Use compliance.
