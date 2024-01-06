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

* Issues to fix:
  * [done] Fix out-of-sync display of server-side decoration and window content when resizing.
  * Fix assertion crash when mouse is pressed, then moved to another toplevel, then released.
  * Hide window border when not having server-side decoration.
  * Fix issue with Chrome: Enabling "Use system title and boders" will pick a slightly small decoration.
  * Fix issue on resizing: When moving the mouse too quickly, focus is lost and the resizing stops.
  * Fix issue on fullscreen: The window border is kept, having the window off by 1 pixel. 

* Experimental support for Dock Apps
  * [done] Experimental wayland protocol for Apps to declare icon surfaces.
  * Surfaces will be shown in either tile container, clip or dock area,
    depending from where the app was started.
  * Two demo DockApps included (digital clock; julia set).

* Initial XWayland support
  * Cover enough functionality to support xterm, emacs in X11.

* Configurable keyboard map (in code or commandline arg)

* Support `xdg_shell`, based on toolkit.
  * [done] XDG Popups.
  * [done] Move and Resize, compliant with asynchronous ops.
  * [done] Maximize.
  * [done] Set title.
  * [done] fullscreen.
  * minimize.
  * show window menu.
  * set_parent.
  * set app ID.

* Support `layer_shell`, based on toolkit.

* Support window decoration protocol, based on toolkit.
  * [done] Style of title bar, iconify and close buttons similar to Window Maker.
  * Window menu, with basic window actions (not required to adapt to state).

* Multiple workspaces, based on toolkit.
  * Navigate via keys (ctrl-window-alt-arrows, hardcoded).

* Dock, visible across workspaces, based on toolkit.
  * Style similar to Window Maker.
  * With application launchers (hardcoded).

*  Clip, based on toolkit.
  *  Display the current workspace.
  *  Buttons to switch between workspaces.

*  Application launchers, based on toolkit.
  *  Display an icon.
  *  Display application status (*starting*, *running*).
  *  Configurable (in code).

* Window actions, based on toolkit.
  * Move (drag via title bar, or window-alt-click)
  * [done] Resize windows, including a resize bar.
  * Fullscreen windows.
  * [done] Maximize windows.
  * Minimize (*iconify*) windows.
  * Roll up (*shade*) windows.
  * Raise window when activated.

*  Visualization of iconified applications, based on toolkit.

*  Task list (window-alt-esc), cycling through windows, based on toolkit.

### Internals and code organization

* [done] Design a toolkit and re-factor the codebase to make use of it.
  * Ensure the main features (eg. all explicit actions and features above) are
    tested.

## Pending

Features for further versions, not ordered by priority nor timeline.

* XWayland support (X11 clients).

* Dock Apps.
  * Attached to dock (visible across workspaces) or clip (per workspace).
  * Configurable to show permanently also in clip.
  * Drag-and-drop between clip and dock.

* Visualization / icons for running apps.
  * Show in 'iconified' area.
  * Drag-and-drop into clip or dock area.

* Support for dynamic output configurations.
  * Multiple monitors.
  * Per-monitor fractional scale.
  * Work with hot-plugged monitor, remember configurations.

* Window attributes
  * Determine how to detect client preferences.
  * Configurable and overridable (titlebar, resizebar, buttons, ...).
  * Scaling factor per application.

* Application support.
  * Icons retrieved and used for iconified windows. See [themes](https://specifications.freedesktop.org/icon-theme-spec/icon-theme-spec-latest.html).
  * Make use of XDG Desktop Entry [specification](https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html).

* XDG Complianace
  * Review and define what to support from https://specifications.freedesktop.org.
  * Autostart.
  * System Tray (potentially through a Dock App)
  * Icon Themes
  * Notifications (potentially through a Dock App)
  * Fullscreen: Hide all other visuals when a window takes fullscreen.

* Application launcher
  * Show icon from XDG desktop entry.
  * For running apps, consider showing the surface on the tile.
  * Configuration menu: Commandline, and further settings.

* A logo and info panel.

* Window actions
  * Send to another workspace, using menu or key combinations.
  * Configurable key combinations for basic actions (minimize, ...).

* Configuration file, for:
  * Application launchers of the dock.
  * Application launchers for the clip.
  * Workspace configuration.
  * Background.
  * Theme.
  * Auto-started applications.

* Configurable keyboard map.

* Root menu.

* Menu with submenus.

* Window menu adapting to window state.
  * Eg. "Maximize" shown when not maximized, otherwise: "restore".

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

## Non-Goals

* Do not (re)create a GNUStep environment.
* Creating a dedicated toolkit.
