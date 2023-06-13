# wlmaker - Roadmap

Vision: A lightweight and fast Wayland compositor, visually inspired by Window
Maker, and fully theme-able and configurable.

Support for visual effects to improve usability, but not for pure show.

## 0.1 - MVP milestone

### Features 

* Support `xdg_shell`.
  
* Support `layer_shell`.

* Support window decoration protocol.
  * Style of title bar, iconify and close buttons similar to Window Maker.
  * Window menu, with basic window actions (not required to adapt to state).

* Multiple workspaces
  * Navigate via keys (ctrl-window-alt-arrows, hardcoded).
  
* Dock, visible across workspaces.
  * Style similar to Window Maker.
  * With application launchers (hardcoded).
  
* Clip
  * Display the current workspace.
  * Buttons to switch between workspaces.
  
* Application launchers
  * Display an icon.
  * Display application status (*starting*, *running*).
  * Configurable (in code).

* Window actions
  * Move (drag via title bar, or window-alt-click)
  * Resize windows, including a resize bar.
  * Fullscreen windows.
  * Maximize windows.
  * Minimize (*iconify*) windows.
  * Roll up (*shade*) windows.
  * Raise window when activated.
  
* Visuualization of iconified applications.
  
* Task list (window-alt-esc), cycling through windows.

* Auto-start of configured applications.
  * Configurable in code.

* Verify minimal application set to run:
  * Terminal: `foot`
  * Google Chrome
  * Mozilla Firefox
  
* Works as a X11 window, Wayland client or standalone compositor.

### Internals and code organization

* git submodule for direct and critical dependencies.
* CMake as build system.
* `test` and `doc` targets.
* Published as open source.

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
