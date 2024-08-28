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
  * [done] Fix issue on fullscreen: The window border is kept, having the window off by 1 pixel.
  * [done] Add commandline flag to enable/disable XWayland start.
  * [done] Verify startup on console works.

* [done] Screensaver support.
  * [done] Implement ext-session-lock-v1 protocol.
  * [done] Verify screen lock works with eg. swaylock.
  * [done] Implement timer for lock, and support zwp_idle_inhibit_manager_v1 to inhibit.
  * [done] Verify this still works after the to-toolkit move.

* [done] Configuration file support
  * [done] Pick or implement parser for configuration file.
  * [done] File for basic configuration: Keyboard map & config, auto-started apps.
  * [done] Configure idle monitor and screensaver command via config file.
  * [done] Configurable key combinations for basic window actions (minimize, ...).
  * [done] File for visual style (theme): decoration style, background.
  * [done] File to define workspaces and dock, falling back to default if not provided.
  * [done] Include at least one additional theme.

* [done] Theme details
  * [done] Style for resizebar.
  * [done] Style for the window's margin.
  * [done] Style for the window border.
  * [done] Titlebar icons centered.
  * [done] Titlebar icons with text color, blurred or focussed.
  * ~~Bezel 'off' color so it is visible on black (not doing).~~
  * [done] Titlebar font and size.
  * [done] Style for clip.
  * [done] Style for task list fill and text color.

* [done] Support `layer_shell`, based on toolkit.
  * [done] XDG Popups.

* [done] Multiple workspaces, based on toolkit.
  * [done] Remove the earlier non-toolkit code.
  * [done] Background color for separate workspaces, configured in state.
  * [done] Default background color, picked up from style file.
  * [done] Navigate via keys (ctrl-window-alt-arrows, configurable in plist).

* [done] Dock, visible across workspaces, based on toolkit.
  * [done] Keep track of subprocesses and the corresponding windows.
  * [done] Style similar to Window Maker.
  * [done] With application launchers (configurable in file).

* [done] Clip, based on toolkit.
  * [done] Display the current workspace.
  * [done] Buttons to switch between workspaces.

* [done] Application launchers, based on toolkit.
  * [done] Display an icon.
  * [done] Display application status (*starting*, *running*).
  * [done] Configurable (in code).

* [done] Task list (window-alt-esc), cycling through windows.
  * [done] Migrate implementation to wlmtk.
  * [done]Key combination configurable in the config file.

* [done] Build & compile off released dependency versions.

## Plan for 0.4

**Focus**: Add menus & make it ready for  "Early-Access".

* Thorough tests of both pointer and keyboard state.
  * [done] Issue found when killing saylock that keyboard focus is incorrect.
  * [done] Re-activate workspace & windows after lock.
  * Fix bug: resize-from-left jitter observed on the raspi or with gnome-terminal.
  * Fix bug: When switching workspace, pointer state appears to be reset.
  * Fix bug: Particularly when using large decorations, there is resize jitter.

* Menu, based on toolkit.
  * Available as window menu in windows.
  * Available as (hardcoded) application menu.
  * Root menu.
  * Menu with submenus.
  * Window menu adapting to window state.
    (Eg. "Maximize" shown when not maximized, otherwise: "restore".)

* Screensaver support.
  * Magic corner to lock immediately.
  * Magic corner to inhibit locking.
  * Configurable corners, timeout and visualization.

* Documentation updates
  * Update README to reflect "early-access" vs. "early development".
  * [done] Screenshots included.

* Update build system to use libraries from the base system rather than
  the `dependencies/` subdirectory, if versions are avaialble.
  * Upgrade to wlroots 0.18. Verify if that & libdrm update works with lightdm.

* Support different output scale & transformations
  * [done] Add a style file that has dimensions suitably for a Hi-Res screen (eg. Retina) (#99)
  * [done] Scale icons to tile size.
  * Add commandline arguments to configure size of window (#98)
  * Add option to specify an output transformation (#97)

* Misc
  * [done] Expose the decoration manager configurables through the config file.

## Plan for 0.5

* Support for dynamic output configurations.
  * Multiple monitors, with output mirrored across.
  * Per-monitor fractional scale.

## Pending

### Major feature milestones

* Initial multi-monitor support: Mirrored.
* Toplevel windows represented as icons & minimized (iconified) windows.
* Drag-n-drop of icons into & from dock & clip.

### Features for further versions, not ordered by priority nor timeline.

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
  * Permit identifying the real X client, not the XWayland connection.
  * Have a test matrix build that verifies the build works without XWayland.
  * Fix bug with emacs hanging on saving clipboard (observed once).

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
  * Use SVG as principal icon format, and scale without quality loss.

* A logo and info panel.

* Window actions
  * Send to another workspace, using menu or key combinations.
  * Window *shade* triggered by double-click, and animated.
  * Minimize (*iconify*) windows, using wlmtk.
  * Window menu.
  * Application ID (from XDG shell and/or X11).

* Configuration file and parser:
  * Permit dock and clip to save state to configuration files.
  * Support different background styles (fill, image).
  * Make semicolon-after-value required, for consistency with GNUstep.
  * Theme.
    * [done] Added ADGRADIENT fill style, aligned with Window Maker's diagonal.
    * Adds support for textures as fill (tiled, scaled, maximized, centered, filled?)
    * Add drag-modifier option, to configure when a drag makes a window move.

* Configurable keyboard map.
  * Verify support of multi-layout configurations (eg. `shift_caps_toggle`)

* Window placement
  * Automatic placement on a free spot.
  * Gravity to snap to and stick to borders.
  * Mouse pull to sides or corners will set window to half or quarter screen.

* Configuration tool, similar to WPrefs.

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

* Exploratory ideas
  * Stretch: Consider supporting XScreenSaver (or visualization modules).

## Visualization and effects

* Animations
  * Launching applications, when *starting*.
  * Size changes (maximize, minimize, fullscreen).

* Task switcher
  * Application icons or minimized surfaces to visualize applications.
  * Animations when switching focus.
  * Clean up signal handling (emit on task switch, enable, disable).

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

* Run static checks and enforce them on pull requests (eg. https://www.kitware.com/static-checks-with-cmake-cdash-iwyu-clang-tidy-lwyu-cpplint-and-cppcheck/).
* Provide binary package of wlmaker.
* Run github workflows to build with GCC and Clang, x86_64 and arm64, and Linux + *BSD.
* Look into whether the screenshots can be scripted.

## Non-Goals

* Do not (re)create a GNUStep environment.
* Creating a dedicated toolkit.
