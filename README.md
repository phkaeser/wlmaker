# wlmaker - Wayland Maker

A [Wayland](https://wayland.freedesktop.org/) compositor inspired by
[Window Maker](https://www.windowmaker.org/).

Key features:

* Compositor for windows in stacking mode.
* Supports multiple workspaces.
* Appearance inspired by Window Maker, following the look and feel of NeXTSTEP.
* Easy to use, lightweight, low gimmicks and fast.
* Dock and clip, to be extended for dockable apps.

### Current status

Wayland Maker is in early development stage. Highlights for current version (0.2):

* Appearance matches Window Maker: Decorations, dock, clip.
* Support for Wayland XDG shell (mostly complete. Bug reports welcome).
* Initial support for X11 applications (positioning and specific modes are missing).
  Use `--start_xwayland` argument to enable XWayland, it's off by default.
* Appearance, workspaces, dock, keyboard: All hardcoded.
* A prototype DockApp (`apps/wlmclock`).

For further details, see the [roadmap](doc/ROADMAP.md).

Protocol support:

* `xdg-decoration-unstable-v1`: Implemented & tested.
* `ext-session-lock-v1`: Implemented & tested.
* `xdg-shell`: Largely implemented & tested.
* `idle-inhibit-unstable-v1`: Implemented, untested.

### Build & use it!

* From source: Please follow the [detailled build instructions](doc/BUILD.md)
  for a step-by-step guide.

* Once compiled: see [these instructions](doc/RUN.md) on how to run Wayland
  Maker in a window or standalone, and to configure it for your needs.

> [!NOTE]
> `wlmaker` is still in early development, so it's not recommended to use it as
> your primary compositor.

## Contributing

Contributions and help are highly welcome! See
[`CONTRIBUTING.md`](CONTRIBUTING.md) for details, and
[code of conduct](CODE_OF_CONDUCT.md) for more.

## License

Apache 2.0; see [`LICENSE`](LICENSE) for details.

## Disclaimer

This project is not an official Google project. It is not supported by
Google and Google specifically disclaims all warranties as to its quality,
merchantability, or fitness for a particular purpose.
