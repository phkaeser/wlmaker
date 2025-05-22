# Configuring wlmaker {#config}

## Configuration files {#config_precedence}

wlmaker is highly configurable, and uses a human-readable configuration file in
[human-readable Property List](https://en.wikipedia.org/wiki/Property_liste)
format.

wlmaker will load try loading a configuration file from these locations:
* If a `--config_file=<FILE>` argument is provided, load `<FILE>`.
* Otherwise, from the configured lookup paths:
  @snippet{trimleft} src/config.c LookupPathsConfig
* Otherwise, compiled-in setttings from @ref etc_wlmaker_plist will be loaded.

## Output configuration

Using the `Outputs` array, wlmaker will configure and combine monitors into a
multi-monitor setup as configured. When an output (monitor) is detected,
wlmaker will scan the `Outputs` array for an entry that matches `Name`,
`Manufacturer`, `Model` and `Serial`. Once found, it checks `Enabled` whether
that output should be used. If so, the settings from `Scale`, `Transformation`,
`Position` and `Mode` will be applied.

Example:
@snippet{trimleft} etc/wlmaker.plist Outputs

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
