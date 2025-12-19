# wlmaker - Detailed Feature List

This document lists features implemented, planned or proposed for wlmaker,
and sets them in reference to documented [Window Maker](https://www.windowmaker.org/)
features, where available.

Features should be in any of the following state(s):

* [ ] -- **Listed**: A desired feature for the future.
* [ ] :clock3: -- **Planned**: Listed on the roadmap for an upcoming version.
* [ ] :construction: -- **In progress**: Is currently being worked on.
* [x] :white_check_mark: -- **Implemented**: Has been implemented.
* [x] :books: -- **Documented**: Implemented and has user-facing documentation.

If a feature is partially implemented, it is suggested to break it down into parts
that map to above states.

## Windows

### Anatomy of a Window

**Reference**: [Window Maker: User Guide, "Anatomy of a Window](https://www.windowmaker.org/docs/chap2.html).

> [!NOTE]
> Window Maker specifies the default layout of an application (toplevel window),
> and does so by decorating each window consistently. The default Wayland shell
> behaviour is for clients to provide their own decoration, with an optional
> [XDG decoration protocol](https://wayland.app/protocols/xdg-decoration-unstable-v1)
> to permit server-side decorations.
>
> wlmaker implements that protocol for window decorations.

#### Elements of the window decoration

* [x] :white_check_mark: XDG decoration protocol implemented, applies decoration as negotiated.
* [ ] Override decoration setting per application.
* [ ] Configure applying window decoration in case application does not support decoration protocol.

* Titlebar
  * [x] :white_check_mark: Holds the name of the application or window.
  * [x] :white_check_mark: The color indicates active or inactive status (keyboard focus).
  * [x] :white_check_mark: Dragging the titlebar moves the window.
  * [x] :white_check_mark: Right-click displays the window commands menu.
  * [ ] The color indicates if a (modal) child dialog has keyboard focus.
  * [ ] Applies ellipsize on long names.
  * [ ] Left- or right-aligned, depending on language/charset flow of text.

* Miniaturize button
  * [x] :white_check_mark: Is shown on the titlebar.
  * [ ] Miniaturize/iconify the window when clicked ([#244](https://github.com/phkaeser/wlmaker/issues/244)).
  * [ ] Hide the window when clicked.

* Close button
  * [x] :white_check_mark: Requests the application to close.
  * [ ] Figure out how to forcibly kill the application (kill or close the connection).

* Resizebar
  * [x] :white_check_mark: Drag with the left button resizes the window.
  * [ ] Drag with the middle mouse button resizes the window *without raising it*.
  * [ ] Drag while holding the control key resizes the window *without focusing it*.
  * [ ] Drag while holding the meta key moves the window.

* Client Area: Holds the application's toplevel surface(s).
  * [x] :white_check_mark: Clicking into the client area focuses the window.
  * [x] :white_check_mark: Left button drag while holding Meta: Moves the window.
  * [ ] Do not pass the activating click to the application if *IgnoreFocusClick* is set.
  * [ ] Right button drag while holding Meta resizes the window.

### Focusing a Window

A window can be *active* (has *keyboard focus*) or *inactive* (not having *keyboard
focus*). Only one window can be active at a time.

* [x] :white_check_mark: Click-to-Focus mode.
  * [x] :white_check_mark: Activates + raises the window when left-, middle- or
    right-clicking on titlebar, resizebar or in the client area.
  * [ ] Add configuration option to permit "Activate" without raising on click.
  * [ ] Middle-click on the titelbar activates the window, but does not raise it.

* [ ] Add Focus-Follow-Mouse mode. Essentially, having *pointer focus* implies
  *keyboard focus*, and inactivate window when losing *pointer focus*.

* [ ] Add Sloppy-Focus. Activate a window when obtaining *pointer focus*,
  but retain it unless another window becomes activated ([#245](https://github.com/phkaeser/wlmaker/issues/245)).

* [ ] Add a config-file setting for these options.

### Reordering Windows

TBD: Raise/Lower.

### Moving a Window

* [x] :white_check_mark: Left-dragging the title bar moves the window.
* [x] :white_check_mark: Holding Alt and left-dragging while anywhere on the window
  moves the window.
* [ ] The modifier key for left-dragging anywhere on the content is configurable.
* [ ] Drag resizebar while holding Meta: Moves the window.
* [ ] Drag titlebar with middle mouse button: Moves window without changing stacking order.
* [ ] Drag titlebar while holding Control: Moves window without focussing.

### Resizing a Window

* [x] :white_check_mark: Drag left, middle or right region on the resizebar resizes the window.
* [ ] Drag window in the client area with right mouse button, holding meta: Resizes the window.
* [ ] Drag resizebar with the middle mouse button: Resize window without bringing it to the front.
* [ ] Drag resizebar while holding the control key: Resize window without focussing it.

### Shading a Window

* [x] :white_check_mark: Shade or unshade the window by the mouse's scrollwheel.
* [ ] Double-click on the titlebar shades or unshades the window ([#246](https://github.com/phkaeser/wlmaker/issues/246)).
* [ ] Permit a configurable keybinding for shading/unshading the window.
* [ ] Shading and unshading has a short animation to enlarge the window.

### Miniaturizing a Window

* [ ] Left-click on the miniaturize button collapses the window into a mini-window.
* [ ] The transition to a mini-window (and back) is animated.
* [ ] The mini-window is shown in the *Icon Area* and is distinguishable
  from an application icon.

### Closing a Window

* [x] :white_check_mark: Left-click on the window's close button requests the application to close it.
* [ ] Define whether "forcibly" closing means to kill the client process, or to close
  the wayland connection.
* [ ] Holding Control while left-clicking the window's close button forcibly closes the window.
* [ ] Double-clicking the close button forcibly closes the window.

### Maximizing a Window

* [x] :white_check_mark: Select 'Maximize' from the window commands menu.
* [x] :white_check_mark: A configurable key shortcut requests the window to be maximized.
* [ ] Define whether control-double-click should do resize height to full screen.
* [ ] Define whether shift-double-click should do resize height to full screen.
* [ ] Define whether control-shift-double-click should do maximize.
* [ ] Add a configuration option whether 'Maximize' may obscure Dock, Clip and Icons ([#328](https://github.com/phkaeser/wlmaker/issues/328)).

### Window Placement

* [ ] New windows are placed on a suitable free spot, if available.
* [ ] Once screen is full, windows are stacked with a moderate displacement between each.
* [ ] "Gravity" to snap and stick to borders.
* [ ] Pulling a window towards an edge of an output sets window to occupy that edge.

### Window attributes

* [ ] Permit to configure attributes by application ID and/or window title.
* [ ] Permit to disable titlebar, resizebar, close button, miniaturize button.
* [ ] Option to keep on top.
* [ ] Option to be "omnipresent", ie. shown across all workspaces.
* [ ] To determine: Start miniaturized.
* [ ] To determine: Skip window list.
* [ ] Specify an icon, where not provided.
* [ ] Specify initial workspace.
* [ ] Scaling factor (fractional scale).
* [ ] Use *XDG Shell* `wm_capabilities` to advertise capabilities, as attributes permit.

## Workspaces

* [x] :white_check_mark: Workspaces for startup are configurable in the *state*
  configuration file.
* [x] :white_check_mark: Navigate between workspaces using a configurable key
  combination (ctrl-arrow).
* [x] :white_check_mark: Navigate between workspaces through scrollwheel or buttons on the *Clip*.
* [ ] When saving state, the current workspace configuation is saved to the *state*
  configuration file.

## Workspaces menu

* [x] :white_check_mark: "New", to create a new workspace.
* [x] :white_check_mark: "Destroy" or "Destroy last" for destroying a workspace.
* [ ] Menu item to navigate to the provided workspace.
* [ ] Ctrl-click on menu item to rename the workspace, through a dialog.

## Navigating workspaces

* [ ] Optional display the workspace name after moving to a new workspace.
* [ ] The transition between workspaces is animated.

## Window assignments

* [x] :white_check_mark: Send window to another workspace through ctrl-shift-arrow (or configurable combination)
* [ ] Navigate to workspace using key combination (eg. Alt-<number>)
* [ ] Navigate to same workspace in next group, using key combination.
* [ ] Support workspace groups, addressable by an extra modifier of key combinations.

## Menu contents

### Root menu

* [x] :white_check_mark: Menu configurable as a plist.
* [x] :white_check_mark: Generated from XDG repository ([#90](https://github.com/phkaeser/wlmaker/issues/90)).

### Window commands

* [x] :white_check_mark: Static contents, shown when right-clicking on title bar.
* [ ] Items and contents adapting to state (eg. no "maximize" when maximized).

## Wayland protocol support

### `ext-idle-notify-v1`

* [ ] Implement.

### `ext-session-lock-v1`

* [x] :white_check_mark: Implement, verified on single output.
* [x] :white_check_mark: Make it work on multiple outputs: Lock surface shown on each.

### `fractional-scale-v1`

* [x] :white_check_mark: Support fractional surface scales.

### `idle-inhibit-unstable-v1`

* [x] :white_check_mark: Implement.
* [ ] Test it. Didn't have a tool when adding it.

### `wlr-foreign-toplevel-management-unstable-v1`

* [ ] Implement.

### `wlr-layer-shell-unstable-v1`

* [x] :white_check_mark: Support layer panels.
* [ ] Support `keyboard_interactivity` for upper layers.

### `wlr-output-management-unstable-v1`

* [x] :white_check_mark: Implemented, and verified with `wlr-randr` and `wldisplays`.
* [x] :white_check_mark: Scale, transformation and mode of output is configurable
  in *config* file, by matching output attributes.

### ``wlr-screencopy-unstable-v1`

* [x] :white_check_mark: Implemented, works for `wdisplays`.

### `wp_viewporter`

* [x] :white_check_mark: Support surface cropping and scaling.

### `xdg-activation-v1`

* [ ] Implement.

### `xdg-decoration`

* [x] :white_check_mark: Implemented.

### `xdg-shell`

* [x] :white_check_mark: Support toplevel shell and popups.
* [x] :white_check_mark: Refactor: split `xdg_surface` off `xdg_toplevel`, encode as separate classes.
* [ ] Accept state (maximize, fullscreen, ...) before mapping the surface, but apply
  them only after first commit.
* [x] :white_check_mark: Accept decoration requests before first commit, and forward them after the first
  commit was received (see also https://gitlab.freedesktop.org/wlroots/wlroots/-/merge_requests/4648#note_2386593).
* [ ] Support `set_parent`, associating a child `wlmtk_window_t` with a paraent.
* [x] :white_check_mark: Consider suggested position on `show_window_menu`

## X11 client support (XWayland)

* [x] :white_check_mark: Support windows and popups, enough for `xterm` and `emacs`.
* [x] :white_check_mark: Investigate if the connection can identify the real X client, not the
  XWayland connection (yes, it does).
* [ ] Modal windows should be a child `wlmtk_window_t`

## Dock, Clip, Icon Area

* [x] :white_check_mark: Launcher item display when the app is "launching", and "running".
* [ ] Drag-and-drop icons between icon area, dock app and clip.
* [ ] Investigate if & how to use icons specified in XDG desktop entry.
* [ ] Aspirational: Aim to have *Dock* and *Clip* running as separate process(es).
* [ ] Entries in Dock/Clip have a settings menu to define path & icon.

### Dock

* [x] :white_check_mark: The entries in the dock are loaded from the *state* file.
* [x] :white_check_mark: Edge and anchor are configured in *state* config file.
* [x] :white_check_mark: Entries have configurable icon image, for when app isn't running.
* [ ] Entries can be configured to autostart upon wlmaker startup.
* [ ] Entries can be configured to be "locked", preventing accidental removal.
* [ ] Entries can have a drawer, to nest a second layer of entries.
* [ ] When saving state, dock entries are saved into *state* config file.

### Clip

* [x] :white_check_mark: edge and anchor are configured in *state* config file.
* [ ] Entries can be configured to be "omnipresent", but are per-workspace by default.
* [ ] When saving state, clip entries are saved into *state* config file.

### Icon Area

* [ ] Display running apps using the configured icon.
* [ ] Consider showing a miniature of the toplevel surface in the icon.
TBD.

### Dock Apps

* [x] :white_check_mark: Have a demo app using the icon protocol (`wlmclock`)
* [ ] Configurable to show in Dock or Clip.
* [ ] When starting from anywhere, dock apps show in icon area.
* [ ] When starting from a docked or clipped position, show there.
* [ ] Add another demo DockApp (julia set).

## Toolkit

* [ ] Use pango proper over cairo toy interface. Use relative sizes to scale.
* [ ] Support configurable fractional scale for all decoration elements.
* [ ] Text strings are looked up from a table, for internationalization.

### Menu

* [x] :white_check_mark: Permit navigation by keys
* [ ] Position all menus to remain within output.
* [ ] Re-position to remain within output when submenu opens.
* [ ] Handle case of too manu menu items that exceed output space.
* [ ] Dynamically set item width to hold longer item strings. Apply ellipsis on overflow.
* [ ] Show keyboard shortcut to corresponding action, if available.

## Devices

### Multiple outputs ([#122](https://github.com/phkaeser/wlmaker/issues/122))

* [x] :white_check_mark: Output layout configurable via third-party tool, eg. `wlr-randr`.
* [x] :white_check_mark: Save state in a state file (in the *config* directory currently).
* [x] :white_check_mark: When saving state, store the current layout in the *state* config file.

## General

* [x] :white_check_mark: Add a logo.
* [ ] Use SVG as principal format for icons.
* [ ] Add an info panel, showing version, name, copyright and link to documentation.
* [ ] Upon first launch, show an onboarding screen with basic instructions
  ([#131](https://github.com/phkaeser/wlmaker/issues/131)).

### Configuration files

* [x] :white_check_mark: Support `Execute` action (vs. `ShellExecute`) ([#261](https://github.com/phkaeser/wlmaker/issues/261))
* [ ] Change KeyBindings format to be similar to menu actions: Lists, where item 1+
  holds the action and optional parameters.

### System integration

* [x] :books: Store config files not in `${HOME}/~`, but in `${HOME}/.config/`, according
      FreeDesktop specification ([#262](https://github.com/phkaeser/wlmaker/issues/262)).
* [ ] Review and define what to support from https://specifications.freedesktop.org.
    * [ ] Set `XDG_CURRENT_DESKTOP` to a sensible value.
* [ ] System Tray (potentially through a Dock App)
* [ ] Notifications (potentially through a Dock App)
* [ ] Review whether to support Icon themes.

### CI/CD

* [x] :white_check_mark: Have github workflows to build with GCC and Clang, x86_64
  and arm64, and Linux + *BSD.
* [ ] Generate the screenshots (extend wlroots for a PNG backend?)
* [ ] Provide a binary package of wlmaker, from HEAD.
* [ ] Run static checks and enforce them on pull requests (eg. https://www.kitware.com/static-checks-with-cmake-cdash-iwyu-clang-tidy-lwyu-cpplint-and-cppcheck/).
