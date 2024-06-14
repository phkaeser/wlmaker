# wlmaker - Roadmap

Vision: A lightweight and fast Wayland compositor, visually inspired by Window
Maker, and fully theme-able and configurable.

Support for visual effects to improve usability, but not for pure show.

## 0.1 - MVP milestone

### Features

* [done] Support `xdg_shell`.

* [done] Support `layer_shell`.

* [done] Support window decoration protocol.
  * [done] Style of title bar, iconify and close buttons similar to Window Maker.
  * [done] Window menu, with basic window actions (not required to adapt to state).

* [done] Multiple workspaces
  * [done] Navigate via keys (ctrl-window-alt-arrows, hardcoded).

* [done] Dock, visible across workspaces.
  * [done] Style similar to Window Maker.
  * [done] With application launchers (hardcoded).

* [done] Clip
  * [done] Display the current workspace.
  * [done] Buttons to switch between workspaces.

* [done] Application launchers
  * [done] Display an icon.
  * [done] Display application status (*starting*, *running*).
  * [done] Configurable (in code).

* [done] Window actions
  * [done] Move (drag via title bar, or window-alt-click)
  * [done] Resize windows, including a resize bar.
  * [done] Fullscreen windows.
  * [done] Maximize windows.
  * [done] Minimize (*iconify*) windows.
  * [done] Roll up (*shade*) windows.
  * [done] Raise window when activated.

* [done] Visualization of iconified applications.

* [done] Task list (window-alt-esc), cycling through windows.

* [done] Auto-start of configured applications.
  * [done] Configurable in code.

* [done] Verify minimal application set to run:
  * [done] Terminal: `foot`
  * [done] Google Chrome
  * [done] Mozilla Firefox

* [done] Works as a X11 window, Wayland client or standalone compositor.

### Internals and code organization

* [done] git submodule for direct and critical dependencies.
* [done] CMake as build system.
* [done] `test` and `doc` targets.
* [done] Published as open source.

## Plan for 0.2

* [done] Issues to fix:
  * [done] Fix out-of-sync display of server-side decoration and window content when resizing.
  * [done] Fix assertion crash when mouse is pressed, then moved to another toplevel, then released.
  * [done] Hide window border when not having server-side decoration.
  * [done] Fix issue with Chrome: Enabling "Use system title and boders" will pick a slightly small decoration.
  * [done] Fix resize issue with Chrome & Firefox: The content size and actual window size can differ, which led to jumpy resizes as get_size and request_size operations did not base on the same.
  * [not reproducible] Fix issue on resizing: When moving the mouse too quickly, focus is lost and the resizing stops.

* [done] Experimental support for Dock Apps
  * [done] Experimental wayland protocol for Apps to declare icon surfaces.
  * [done] Surfaces will be shown in either tile container, clip or dock area,
    depending from where the app was started.
  * [done] Demo DockApps included (digital clock)

* [done] Initial XWayland support
  * [done] Cover enough functionality to support xterm
  * [done] Enough functionality to support emacs in X11.
    * [done] Support for child surfaces.
    * [done] Positioning of popups
    * [done] Ensure stacking order is respected and used.
    * [done] Popups do not contribute to window extensions (no border hops)
    * [done] Cursor set appropriately.
    * [done] Set DISPLAY env variable appropriately.
    * [done] Handling of modal windows: Should have decorations, stay on top.

* [done] Configurable keyboard map (in code ~or commandline arg~)

* Support `xdg_shell`, based on toolkit.
  * [done] XDG Popups.
  * [done] Move and Resize, compliant with asynchronous ops.
  * [done] Maximize.
  * [done] Set title.
  * [done] fullscreen.
  * [done] Fix positioning of popups.
  * [regression, not supported] show window menu.

* [done] Support window decoration protocol, based on toolkit.
  * [done] Style of title bar, iconify and close buttons similar to Window Maker.
  * [done] No border shown when windows are not decorated (eg. chrome, firefox)

* [done] Task List
  * [done] Listing windows, rather than views.

* [done] Window actions, based on toolkit.
  * [done] Move ([done] drag via title bar, or [done] window-alt-click)
  * [done] Resize windows, including a resize bar.
  * [done] Fullscreen windows.
  * [done] Maximize windows.
  * [done] Roll up (*shade*) windows.
  * [done] Raise window when activated.

* [done] App Launcher: Update status for wlmtk_window_t, instead of
  wlmaker_view_t.

### Internals and code organization

* [done] Design a toolkit and re-factor the codebase to make use of it.
  * Ensure the main features (eg. all explicit actions and features above) are
    tested.

## Plan for 0.3

* Bugfixes
  * Fix issue on fullscreen: The window border is kept, having the window off by 1 pixel.

* [done] Screensaver support.
  * [done] Implement ext-session-lock-v1 protocol.
  * [done] Verify screen lock works with eg. swaylock.
  * [done] Implement timer for lock, and support zwp_idle_inhibit_manager_v1 to inhibit.

* Configuration file support
  * [done] Pick or implement parser for configuration file.
  * [done] File for basic configuration: Keyboard map & config, auto-started apps.
  * [done] Configure idle monitor and screensaver command via config file.
  * File for visual style (theme): decoration style, background.
  * File to define workspaces and dock.

* [done] Support `layer_shell`, based on toolkit.
  * [done] XDG Popups.

* Multiple workspaces, based on toolkit.
  * Navigate via keys (ctrl-window-alt-arrows, hardcoded).

* [done] Dock, visible across workspaces, based on toolkit.
  * [done] Keep track of subprocesses and the corresponding windows.
  * [done] Style similar to Window Maker.
  * [done] With application launchers (configurable in file).

* Clip, based on toolkit.
  * Display the current workspace.
  * Buttons to switch between workspaces.

* Application launchers, based on toolkit.
  * Display an icon.
  * Display application status (*starting*, *running*).
  * Configurable (in code).

*  Task list (window-alt-esc), cycling through windows, based on toolkit.

## Pending

Features for further versions, not ordered by priority nor timeline.

* Wayland protocol adherence.
  * Support XDG `wm_capabilities` and advertise the compositor features.
  * Fullscreen: Hide all other visuals when a window takes fullscreen.
  * `xdg-shell`: set_parent, by child wlmtk_window_t.
  * Test `idle-inhibit-unstable-v1`. Didn't have a tool when adding.xs
  * Add `ext-idle-notify-v1` support.
  * Add `xdg-activation-v1` support.
  * Add `wlr-foreign-toplevel-management-unstable-v1` support.
  * Support `keyboard_interactivity` for `wlr-layer-shell-unstable-v1`.

* XWayland support (X11 clients).
  * Proper handling of modal windows: Should be a child wlmtk_window_t to itself.
  * Permit conditional compilation of XWayland support, and enabling/disabling via flag.
  * Permit identifying the real X client, not the XWayland connection.

* Dock Apps.
  * Attached to dock (visible across workspaces) or clip (per workspace).
  * Configurable to show permanently also in clip.
  * Drag-and-drop between clip and dock.
  * Ideally: With a Wayland protocol that permits running the dock and clip as
    separate binary, independent of the compositor.
  * Second Demo DockApp (julia set).

* Visualization / icons for running apps.
  * Re-build this unsing wlmtk.
  * Show in 'iconified' area.
  * Drag-and-drop into clip or dock area.
  * Consider running this as task selector, as separate binary.

* Support for dynamic output configurations.
  * Multiple monitors.
  * Per-monitor fractional scale.
  * Work with hot-plugged monitor, remember configurations.

* Window attributes
  * Determine how to detect client preferences.
  * Configurable and overridable (titlebar, resizebar, buttons, ...).
  * Scaling factor per application.
  * Build and test a clear model for `organic`/`maximized`/`fullscreen` state
    switches and precedence.
  * Window menu, adapting to state (eg. no "maximize" when maximized).

* Application support.
  * Icons retrieved and used for iconified windows. See [themes](https://specifications.freedesktop.org/icon-theme-spec/icon-theme-spec-latest.html).
  * Make use of XDG Desktop Entry [specification](https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html).

* XDG Complianace
  * Review and define what to support from https://specifications.freedesktop.org.
  * Autostart.
  * System Tray (potentially through a Dock App)
  * Icon Themes
  * Notifications (potentially through a Dock App)

* Application launcher
  * Show icon from XDG desktop entry.
  * For running apps, consider showing the surface on the tile.
  * Configuration menu: Commandline, and further settings.

* A logo and info panel.

* Window actions
  * Send to another workspace, using menu or key combinations.
  * Configurable key combinations for basic actions (minimize, ...).
  * Window *shade* triggered by double-click, and animated.
  * Minimize (*iconify*) windows, using wlmtk.
  * Window menu.
  * Application ID (from XDG shell and/or X11).

* Configuration file, for:
  * Application launchers of the dock.
  * Application launchers for the clip.
  * Workspace configuration.
  * Background.
  * Theme.
  * Auto-started applications.

* Configurable keyboard map.

* Menu, based on toolkit
  * Available as window menu in windows.
  * Available as (hardcoded) application menu.
  * Root menu.
  * Menu with submenus.
  * Window menu adapting to window state.
    (Eg. "Maximize" shown when not maximized, otherwise: "restore".)

* Window placement
  * Automatic placement on a free spot.
  * Gravity to snap to and stick to borders.
  * Mouse pull to sides or corners will set window to half or quarter screen.

* Configuration tool, similar to WPrefs.

* Screensaver support.
  * Magic corner to lock immediately.
  * Magic corner to inhibit locking.
  * Configurable corners, timeout and visualization.
  * Stretch: Consider supporting XScreenSaver (or visualization modules).

* Compositor features
  * Bindable hotkeys.
  * Pointer position, to support apps like wmscreen or xeyes.

* Internationalization and solid font support
  * Move from cairo toy interface to using pango proper.
  * All text strings to be configurable and swappable.

* Commandline flags to support:
  * icon lookup paths beyond the hardcoded defaults.

* Reduce Technical Debt
  * Move the setenv calls for DISPLAY and WAYLAND_DISPLAY into subprocess
    creation, just after fork. These should not impact the parent process.

## Visualization and effects

* Animations
  * Launching applications, when *starting*.
  * Size changes (maximize, minimize, fullscreen).

* Task switcher
  * Application icons or minimized surfaces to visualize applications.
  * Animations when switching focus.

* Resizing & moving
  * Consider visualizing windows partially transparent when resizing or moving.

## Dock Apps

* Sensors.
* Clock
* System tray.
* CPU load.
* Network monitor.
* Laptop battery status.
* Julia set.

## Build, compile, deployment

* Build & compile off dependency versions found in recent distros (libwlroot, ...).
* Run static checks and enforce them on pull requests (eg. https://www.kitware.com/static-checks-with-cmake-cdash-iwyu-clang-tidy-lwyu-cpplint-and-cppcheck/).
* Provide binary package of wlmaker.
* Run github workflows to build with GCC and Clang, x86_64 and arm64, and Linux + *BSD.

## Non-Goals

* Do not (re)create a GNUStep environment.
* Creating a dedicated toolkit.
