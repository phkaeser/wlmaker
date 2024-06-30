# Running Wayland Maker

If not done yet, please follow the [detailled build instructions](BUILD.md) to
build and install from source.

The commands below assume that dependencies and `wlmaker` were installed to
`${HOME}/.local`

Once running: Press `Ctrl-Window-Alt+T` to open a terminal (`foot`), or
`Ctrl-Window-Alt+Q` to exit.

## Option 1: Run in a window

The most accessible option is to run Wayland Maker in a window in your
already-running graphical environment. It can run both on a X11 or a Wayland
session. To run:

```bash
LD_LIBRARY_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)" \
PKG_CONFIG_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/pkgconfig/:${HOME}/.local/share/pkgconfig/" \
${HOME}/.local/bin/wlmaker
```

## Option 2: Run from a Linux virtual terminal

If your distribution does not have `seatd` 0.8 (or more recent) installed anr
running, you first need to run `seatd` with proper permissions.

To run it as root user, for your normal user:

```
su -c "${HOME}/.local/bin/seatd -g $(id -un) &"
```

Once seatd is running, start Wayland Maker:

```
${HOME}/.local/wlmaker
```

Note: You may need to `su -c "pkill seatd"` to stop `seatd` after you're done.

## Make it run for you!

* [etc/wlmaker.plist](../etc/wlmaker.plist) is the where keyboard options, key
  bindings, screensaver details and more can be configured. The file in the
  source tree is the compiled-in default.

  To extend: Create a copy of the file, modify it suitably, and pass to
  `wlmaker` via the `--config_file=...` arugment.

* To run X11 applications in Wayland Maker, XWayland must be enabled. It is
  compiled in, if the `xwayland` package is installed. In that case, use the
  `--start_xwayland` option. The `DISPLAY` environment variable will be set
  suitably.

* [etc/style-default.plist](../etc/style-default.plist) is the compiled-in
  default theme. With [etc/style-debian.plist](../etc/style-debian.plist),
  there is an alternative theme you can use -- or extend it on your own.

  Run `wlmaker` with `--style_file=...` to use an alternative style.

