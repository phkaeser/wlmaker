# Running Wayland Maker

If not done yet, please follow the [detailled build instructions][BUILD.md] to
build and install from source.

The commands below assume that dependencies and `wlmaker` were installed to
`${HOME}/.local`

Once running: Press `Ctrl-Window-Alt+T` to open a terminal (`foot`), or
`Ctrl-Window-Alt+Q` to exit.

## Option 1: Run as window

The most accessible option is to run Wayland Maker as a window, in your 
already-running graphical environment. It can run both as a window on a X11
or a Wayland session. To run:

```bash
LD_LIBRARY_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)" \
PKG_CONFIG_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/pkgconfig/:${HOME}/.local/share/pkgconfig/" \
${HOME}/.local/bin/wlmaker
```
