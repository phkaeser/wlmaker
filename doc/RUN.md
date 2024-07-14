# Running Wayland Maker

If not done yet, please follow the [detailled build instructions](BUILD.md) to
build and install from source.

The commands below assume that dependencies and `wlmaker` were installed to
`${HOME}/.local`.

Once running: Press `Ctrl-Window-Alt+T` to open a terminal (`foot`), or
`Ctrl-Window-Alt+Q` to exit.

## Option 1: Run in a window

The most accessible option is to run Wayland Maker in a window in your
already-running graphical environment. It can run both on a X11 or a Wayland
session. Easiest is to run it via the wrapper script:

```bash
${HOME}/.local/bin/wrap-wlmaker.sh
```

## Option 2: Run from a Linux virtual terminal

> [!IMPORTANT]
> Make sure your distribution has `seatd` installed and running.

```
${HOME}/.local/bin/wrap-wlmaker.sh
```

Note: You may need to `su -c "pkill seatd"` to stop `seatd` after you're done.

## Option 3: Run as wayland session

> [!NOTE] As of 2024-07-14, this appears to work only with Wayland-only display
> managers. `gdm3` has been found to work, but `lightdm` did not.

> [!IMPORTANT]
> It is not recommended to run wlmaker as your main compositor. This approach
> will not work if dependencies are not all operating correctly, and is hardest
> to debug.

* Copy `${HOME}/.local/share/wlmaker.desktop` to `/usr/share/wayland-sessions/wlmaker.desktop`
* Restart your session manager, to reload the sessions.

The desktop entry will execute `${HOME}/.local/bin/wrap-wlmaker.sh`.

## Make it run for you!

* [etc/wlmaker.plist](../etc/wlmaker.plist) is the where keyboard options, key
  bindings, screensaver details and more can be configured. That file in the
  source tree is the compiled-in default.

  To extend: Create a copy of the file to `~/.wlmaker.plist` and modify it
  according to your needs. Or, move it somewhere else and call `wlmaker` with
  the `--config_file=...` arugment.

* To run X11 applications in Wayland Maker, XWayland must be enabled. It is
  compiled in, if the `xwayland` package is installed. In that case, use the
  `--start_xwayland` option. The `DISPLAY` environment variable will be set
  suitably.

* [etc/style-default.plist](../etc/style-default.plist) is the compiled-in
  default theme. With [etc/style-debian.plist](../etc/style-debian.plist),
  there is an alternative theme you can use -- or extend it on your own.

  Run `wlmaker` with `--style_file=...` to use an alternative style.

# Debugging issues

> [!NOTE] Run `wlmaker` with the `--log_level=DEBUG` argument to get more
> verbose debug information.

1. `wlmaker` fails with an *ERROR* log of `Could not initialize renderer`.

    This indicates that `wlroots` was unable to pick a suitable renderer. For
    triaging & debugging, try the following:

    1. Verify whether another `wlroots`-based compositor [^1] starts up. If
        not, it's a `wlroots` issue, please follow up there.

    2. Try using a different renderer, eg. by setting `WLR_RENDERER=pixman` [^2].

    If that does not help: Please file an issue, including a full paste of your
    the configuration & build log, and of the startup attempt.

[^1]: https://gitlab.freedesktop.org/wlroots/wlroots/-/wikis/Projects-which-use-wlroots#compositors
[^2]: https://gitlab.freedesktop.org/wlroots/wlroots/-/blob/master/docs/env_vars.md?ref_type=heads
