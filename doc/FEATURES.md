# wlmaker - Detailed Feature List

This document lists features implemented, planned or proposed for wlmaker,
and sets them in reference to documented [Window Maker](https://www.windowmaker.org/)
features, where available.

Features should be in the following state(s):

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

TBD.

### Shading a Window

* [x] :white_check_mark: Shade or unshade the window by the mouse's scrollwheel.
* [ ] Double-click on the titlebar shades or unshades the window ([#246](https://github.com/phkaeser/wlmaker/issues/246)).
* [ ] Permit a configurable keybinding for shading/unshading the window.
* [ ] Shading and unshading has a short animation to enlarge the window.

### Miniaturizing a Window

TBD.

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
* [ ] Add a configuration option whether 'Maximize' may obscure Dock, Clip and Icons.

### Window Placement

TBD.
