# Configuring wlmaker {#config}

# Configuration file {#config_file}

wlmaker is highly configurable, and uses a human-readable configuration file in
[human-readable Property List](https://en.wikipedia.org/wiki/Property_list)
format.

wlmaker will load try loading a configuration file from these locations:
* If a `--config_file=<FILE>` argument is provided, load `<FILE>`.
* Otherwise, from the configured lookup paths: @ref _wlmaker_config_fname_ptrs
  @snippet{trimleft} src/config.c LookupPathsConfig
* Otherwise, compiled-in setttings from @ref etc_wlmaker_plist will be loaded.

The configuration file is a dictionary that may hold values for the following
keys:

* @ref config_keyboard (required)
* @ref config_decoration (required)
* @ref config_keybindings (required)
* @ref config_hotcorner (required)
* @ref config_screenlock (required)
* @ref config_output (required)

## Keyboard {#config_keyboard}

This dictionary configures the keyboard layout and properties.

* `XkbRMLVO` (required): A dictionary holding an [XKB](xkbcommon.org) keyboard
  configuration.
  * `Rules` (required): Defines the XKB mapping.
  * `Model`: The name of the model of the keyboard hardware in use.
  * `Layout`: Identifier of the general layout.
  * `Variant`: Any minor variants on the general layout.
  * `Options`: Setof extra options to customize the standard layout.

* `Repeat` (required): A dictionary holding two values that define the initial
  holdback and repeat rate when holding a key pressed.
  * `Delay` (required): Delay before initiating repeats, in milliseconds.
  * `Rate` (required): Repeats per second, once `Delay` has expired.

Example:
@snippet{trimleft} etc/wlmaker-example.plist Keyboard

## Decoration {#config_decoration}

By default, Wayland clients (using the
[XDG Shell](https://wayland.app/protocols/xdg-shell)) will draw window
decorations themselves. For clients that implement and obey the
[XDG decoration](https://wayland.app/protocols/xdg-decoration-unstable-v1)
protocol, wlmaker can whether decorations are drawn by the server, ie. by
wlmaker.

For X11 clients, wlmaker will add decorations if the client requests.

Permitted modes (see @ref _wlmaker_config_decoration_desc):
* `SuggestClient`: Requests the client to draw decoration.
* `SuggestServer`: Requests wlmaker to add decorations.
* `EnforceClient`: wlmaker will refuse to draw decorations, even if the client
  requests.
* `EnforceServer`: wlmaker will add decorations, even if the client refuses.

Example:
@snippet{trimleft} etc/wlmaker-example.plist Decoration

## KeyBindings {#config_keybindings}

A dictionary, where each *key* and *value* define a binding of a key
combination to a wlmaker action. The dictionary may be empty.

See @ref _wlmaker_keybindings_modifiers for the supported modifiers.

See @ref wlmaker_action_desc for the list of permitted actions.

Example:
@snippet{trimleft} etc/wlmaker-example.plist KeyBindings

## HotCorner {#config_hotcorner}

This dictionary configures which action is triggered when the mouse pointer
enters and stays at the specified corner for `TriggerDelay` milliseconds.
It must have the following items:

* `TriggerDelay`: Delay for the pointer residing in a corner before triggering
  the corresponding `Enter` event.
* `TopLeftEnter`, `TopRightEnter`, `BottomLeftEnter`, `BottomRightEnter`: Sets
  the action (@ref wlmaker_action_desc) to trigger when entering the corner.
* `TopLeftLeave`, `TopRightLeave`, `BottomLeftLeave`, `BottomRightLeave`: Sets
  the action (@ref wlmaker_action_desc) to trigger when entering the corner.

Note: As of wlmaker 0.5, the corners are relative to the total layout. They may
not be reachable when using multiple outputs that don't have the same
resolution.

Example:
@snippet{trimleft} etc/wlmaker-example.plist HotCorner

## ScreenLock {#config_screenlock}

Configures the timeout and command for locking the screen. It must have the
following keys:

* `IdleSeconds`: Number of seconds of inactivity before executing `Command`.
* `Command`: Defines the command that will be executed after `IdleSeconds`.

The `LockScreen` action is wired to execute `Command` of `ScreenLock`.

Example:
@snippet{trimleft} etc/wlmaker-example.plist ScreenLock

## Autostart {#config_autostart}

An array of strings, each denoting an executable that will be executed once
wlmaker has started.

Example:
@snippet{trimleft} etc/wlmaker-example.plist Autostart

## Outputs {#config_output}

Using the `Outputs` array, wlmaker will configure and combine monitors into a
multi-monitor setup as configured. When an output (monitor) is detected,
wlmaker will first see if the output is found in *State*. Otherwise, it will
scan the `Outputs` array for an entry that matches `Name`, `Manufacturer`,
`Model` and `Serial`. Once found, it checks `Enabled` whether that output
should be used. If so, the settings from `Scale`, `Transformation`, `Position`
and `Mode` will be applied.

wlmaker supports the
[wlr output management](https://wayland.app/protocols/wlr-output-management-unstable-v1)
protocol for dynamically modifying the output device configuration. There are
third-party tools (`wlr-randr`, `wldisplays`, ...) to dynamically set output
propertis.

* *Optional* `Enabled`: A boolean value, specifies whether that output should
  be used (`Yes`) or not (`No`). Defaults to `Yes`.

* *Optional* `Scale`: A fractional number, specifies the scaling factor to
  apply for contents on that output. Defaults to `1.0`.

* *Optional* `Transformation`: Defines whether the monitor's contents should be
  rotated and/or flipped. Permitted values:
  @snippet src/backend/output_config.c OutputTransformation
  Defaults to `Normal`.

* *Optional* `Position`: Defines the position of the output's top left corner.
  Defaults to `"0,0"`.

* *Optional* `Mode`: A string of the format `<WIDTH>x<HEIGHT>[@<RATE>]`,
  denoting a resolution of `WIDTH` x `HEIGHT`, optionally with explicitly
  specified refresh rate `<RATE>` in Hz.

Example:
@snippet{trimleft} etc/wlmaker-example.plist Outputs

That example will configure any added output to use `Normal` transformation
with a scale of `1.0`.
