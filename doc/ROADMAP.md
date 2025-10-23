# wlmaker - Roadmap

Vision: A lightweight and fast Wayland compositor, visually inspired by Window
Maker, and fully theme-able and configurable.

Support for visual effects to improve usability, but not for pure show.

See the [Detailed Feature List](FEATURES.md) for details.

## Plan for 0.7

**Focus**: Smooth resizing for surfaces and clean implementation.

* Clip & Dock handling
  * Add option to save state (Dock, Clip, Output).
  * Toplevel windows show an icon, unless started from dock.
  * There is a means to attach an icon to Dock or Clip (eg. via menu action).

* Cleanups:
  * Update wlmtk_window_t to use wlmtk_pane_t as principal container.
    * [done] Add wlmtk_window2_t as the updated window version.
    * Extend wlmtk_window2_t to support menu, popups, shade, maximize, minimize, client.
    * Replace wlmtk_window_t with wlmtk_window2_t for XWL windows.
    * Replace wlmtk_window_t with wlmtk_window2_t for menu windows.
    * Remove all references of wlmtk_window_t, wlmkt_content_t, wlmtk_pane_t.
  * Recompute pointer focus max once per frame.

* Bug fixes
  * Resize-from-left jitter observed on the raspi or with gnome-terminal.
  * Particularly when using large decorations, there is resize jitter.
  * Propagate decoration mode and toplevel configure() only after first surface commit.
  * [#322](https://github.com/phkaeser/wlmaker/issues/322): Fix lost click with root menu.
  * [#275](https://github.com/phkaeser/wlmaker/issues/275): Fix crash with early configure.
  * [#258](https://github.com/phkaeser/wlmaker/issues/258): Fix crash on early non-fullscreen

* Infrastructure
  * [done] Make it compile for wlroots 0.19, and update tests accordingly.
  * [done] Add Debian Forky as tested distribution, and update from Bookworm to Trixie.
  * [done] Add `pointer-position` experimental protocol, and a `wlmeyes` app.
  * Embed version and have a `--version` argument to print out.

## [0.6](https://github.com/phkaeser/wlmaker/releases/tag/v0.6)

**Focus**: Multiple outputs.

* [done] Support for dynamic output configurations.
  * [done] Support `wlr-output-management-unstable-v1` protocol.
    * [done] Verify that `wlr-randr` works, for `test` and `apply`.
    * [done] Fix: Report output position correctly.
    * [done] Verify that setting output position works as desired.
    * [done] Fix: Handle --on and --off, should remove output and re-position dock & clip.
  * [done] Support `xdg-output-unstable-v1` protocol.
    * [done] Verify that `wdisplays` works.
      * [done] Fix `wdisplays` crash when unsetting `Overlay Screen Names`: [Known](https://github.com/artizirk/wdisplays/issues/17) `wdisplays` issue.
      * [done] Fix positioning of overlaid screen names.
  * [done] Add `wlr-screencopy-unstable-v1` support.
  * [done] Test and verify: Multiple monitors supported. Supporting:
    * [done] per-monitor fractional scale.
    * [done] per-monitor transformation setting.
  * [done] `wlr-layer-shell-unstable-v1` implementation fixes:
    * [done] Update layer positioning to be respective to the panel's configured output.
  * [done] Permit `wlmaker.plist` per-output configuration, to persist layout.
  * [done] Explore if wlroots permits mirroring layouts. If yes: Implement.
    (Via outputs sharing the same position, through `wlr-randr` or `wdisplays`).
  * [done] Window (toplevel) handling on multiple outputs:
    * [done] Support and handle `wl_output` arg to `xdg_toplevel::set_fullscreen`.
    * [done] 'fullscreen': Fill the configured (or active) output.
    * [done] 'maximized': Maximize on configured (or active) output.
    * [done] When an output is removed: Re-position toplevels into visible area.
  * [done] Fix screen lock behaviour: Ensure the unlock surface is shown on all outputs.
  * [done] Permit specifying output for dock, clip and icon area (similar `KeepDockOnPrimaryHead`)
  * [done] Add "scaling" actions, configurable as hotkey and in root menu.
  * [done] Add "output configuration" item to the root menu. (eg. XF86Display key?)

* [done] Menu
  * [done] Permit navigation by keys
  * [done] Generate from XDG repository ([#90](https://github.com/phkaeser/wlmaker/issues/90)).
  * [done] Documentation for menu configuration.
  * [done] Draw a submenu hint (small triangle) on items expanding into a submenu.

* [done] Bug fixes
  * [done] Verify subprocess from action have stdout & stderr captured and logged.
  * [done] Fix keyboard input not working for Firefox.
  * [done] When switching workspace, pointer state appears to be reset.
  * [done] Test handling of mouse position when changing element visibility. Making
    an element visible should re-trigger focus computation.
  * [done] Verify handling of element motion() and button() return values.
  * [done] Fix non-updating wlmclock observed on non-accelerated graphics stack.

## [0.5](https://github.com/phkaeser/wlmaker/releases/tag/v0.5)

**Focus**: Add root menu and window menu.

* [done] Menu, based on toolkit.
  * [done] Root menu: Basic actions (quit, lock, next- or previous workspace).
  * [done] Menu shown on right-button-down, items trigger on right-button-up.
  * [done] When invoked on unclaimed button, exits menu on button release.
  * [done] Available as window menu in windows.
  * [done] Available also for X11 windows.
  * [done] Available as (hardcoded) application menu.
  * [done] Menu with submenus.
  * [done] Window menu adapting to window state.
    (Eg. "Maximize" shown when not maximized, otherwise: "restore".)
  * [done] When positioning the root menu, keep it entirely within the desktop area.
  * [done] Lookup root menu plist file in home or system directory.

* [done] Support `xdg_shell`, based on toolkit.
  * [done] show window menu.

* [done] Support `layer_shell`, based on toolkit.
  * [done] Fixed [#158](https://github.com/phkaeser/wlmaker/issues/158), an setup issue triggered when running fuzzel.
  * [done] preliminary support for keyboard interactivity.

* Bug fixes
  * [done] Fix crash when closing a shaded window.

## [0.4](https://github.com/phkaeser/wlmaker/releases/tag/v0.4)

**Focus**: Make it ready for "Early-Access".

* [done] Thorough tests of both pointer and keyboard state.
  * [done] Issue found when killing saylock that keyboard focus is incorrect.
  * [done] Re-activate workspace & windows after lock.

* [done] Screensaver support.
  * [done] Magic corner to lock immediately.
  * [done] Magic corner to inhibit locking.
  * [done] Configurable corners & timeout.

* [done] Documentation updates
  * [done] Update README to reflect "early-access" vs. "early development".
  * [done] Screenshots included.

* [done] Update build system to use libraries from the base system rather than
  the `dependencies/` subdirectory, if versions are avaialble.
  * [done] Upgrade to wlroots 0.18. (support both 0.17 and 0.18 in code).
  * [done] Have github actions compile on trixie, using the host library.
  * [done] Have github actions compile not just 0.17, but also 0.18.
  * [done] Verify if that & libdrm update works with lightdm. It
    [does not](https://github.com/canonical/lightdm/issues/267).

* [done] Support different output scale & transformations
  * [done] Add a style file that has dimensions suitably for a Hi-Res screen (eg. Retina) ([#99](https://github.com/phkaeser/wlmaker/issues/99))
  * [done] Scale icons to tile size.
  * [done] Add option to specify an output transformation ([#97](https://github.com/phkaeser/wlmaker/issues/87)). Note: Will not work well in X11 window mode.
  * [done] Add commandline arguments to configure size of window ([#98](https://github.com/phkaeser/wlmaker/issues/98))

* [done] Misc
  * [done] Expose the decoration manager configurables through the config file.
  * [done] Add support for switching virtual terminals ([#6](https://github.com/phkaeser/wlmaker/issues/6)).

* [done] Bug fixes
  * [done] Investigate & fix handling of axis (touchpad) on tty.
  * [done] Fix wrong size for lock surface when Output scale != 1.0 on tty.
  * [done] Fix leak / double free with config_dict_ptr.

## [0.3](https://github.com/phkaeser/wlmaker/releases/tag/v0.3)

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

## [0.2](https://github.com/phkaeser/wlmaker/releases/tag/v0.2)

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

## [0.1 - MVP milestone](https://github.com/phkaeser/wlmaker/releases/tag/v0.1)

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

## Features and work items pending roadmap placement

### Major feature milestones

* Initial multi-monitor support: Mirrored.
* Toplevel windows represented as icons & minimized (iconified) windows.
* Drag-n-drop of icons into & from dock & clip.

### Features for further versions, not ordered by priority nor timeline.

## Window Maker features

## Overall

* Window attributes
  * Build and test a clear model for `organic`/`maximized`/`fullscreen` state
    switches and precedence.

* Application support.
  * Icons retrieved and used for iconified windows. See [themes](https://specifications.freedesktop.org/icon-theme-spec/icon-theme-spec-latest.html).
  * Make use of XDG Desktop Entry [specification](https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html).

* Configuration file and parser:
  * Support different background styles (fill, image).
  * Make semicolon-after-value required, for consistency with GNUstep.
  * Theme.
    * [done] Added ADGRADIENT fill style, aligned with Window Maker's diagonal.
    * Adds support for textures as fill (tiled, scaled, maximized, centered, filled?)
    * Add drag-modifier option, to configure when a drag makes a window move.

* Configurable keyboard map.
  * Verify support of multi-layout configurations (eg. `shift_caps_toggle`)
  * Support ChromeOS layout switch hotkey (`Ctrl+Shift+Space`)

* Configuration tool, similar to WPrefs.

* Compositor features
  * Bindable hotkeys.
  * Pointer position, to support apps like wmscreen or xeyes.

* Commandline flags to support:
  * icon lookup paths beyond the hardcoded defaults.

* Reduce Technical Debt
  * Move the setenv calls for DISPLAY and WAYLAND_DISPLAY into subprocess
    creation, just after fork. These should not impact the parent process.
  * subprocess_monitor: Use listeners, not callback.

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

## Non-Goals

* Do not (re)create a GNUStep environment.
* Creating a dedicated toolkit.
