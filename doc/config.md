# Configuring wlmaker {#config}

# Configuration file {#config_file}

wlmaker is highly configurable, and uses a human-readable configuration file in
[human-readable Property List](https://en.wikipedia.org/wiki/Property_list)
format.

wlmaker will load try loading a configuration file from these locations:
* If a `--config_file=<FILE>` argument is provided, load `<FILE>`.
* Otherwise, from the configured lookup paths:
  @snippet{trimleft} src/config.c LookupPathsConfig
* Otherwise, compiled-in setttings from @ref etc_wlmaker_plist will be loaded.

The configuration file is a dictionary that may hold values for the following
keys:
* @ref config_keyboard
* @ref config_decoration
* @ref config_output

## Keyboard {#config_keyboard}

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

Permitted modes:
* `SuggestClient`: Requests the client to draw decoration.
* `SuggestServer`: Requests wlmaker to add decorations.
* `EnforceClient`: wlmaker will refuse to draw decorations, even if the client
  requests.
* `EnforceServer`: wlmaker will add decorations, even if the client refuses.

Example:
@snippet{trimleft} etc/wlmaker-example.plist Decoration

## Outputs {#config_output}

Using the `Outputs` array, wlmaker will configure and combine monitors into a
multi-monitor setup as configured. When an output (monitor) is detected,
wlmaker will scan the `Outputs` array for an entry that matches `Name`,
`Manufacturer`, `Model` and `Serial`. Once found, it checks `Enabled` whether
that output should be used. If so, the settings from `Scale`, `Transformation`,
`Position` and `Mode` will be applied.

Example:
@snippet{trimleft} etc/wlmaker-example.plist Outputs

That example will configure any added output to use `Normal` transformation
with a scale of `1.0`.

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


FIXME: Document sections for...
* Keyboard
* Decoration
* KeyBindings
* HotCorner
* ScreenLock
* Autostart
* Output config: Clarify dyamic config through wlr-randr, ...

FIXME: Add page on protocol support, includinng
* output layout
* screen lock
* inhibit
* decoration

FIXME: Push generated documentation to github.
