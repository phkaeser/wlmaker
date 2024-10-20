# wlmaker - Wayland Maker

A [Wayland](https://wayland.freedesktop.org/) compositor inspired by
[Window Maker](https://www.windowmaker.org/).

Key features:

* Compositor for windows in stacking mode.
* Supports multiple workspaces.
* Appearance inspired by Window Maker, following the look and feel of NeXTSTEP.
* Easy to use, lightweight, low gimmicks and fast.
* Dock and clip, to be extended for dockable apps.

How it looks, running in a window using the default theme:

![Screenshot of wlmaker running in a window](doc/wlmaker-default-screenshot.png)

### Current status

**Early access**: Wayland Maker provides basic functionality for using it on a single monitor. There will be bugs... reports are welcome! 

Highlights for current version ([0.4](https://github.com/phkaeser/wlmaker/releases/tag/v0.4)):

* *new:* Configurable layout and scaling for the output.
* *new:* Hot corners with configurable actions, default to 'lock' or 'inhibit' locking.
* *new:* Ready to build with [wlroots 0.18](https://gitlab.freedesktop.org/wlroots/wlroots/-/tags).
* Screen saver support, through `ext_session_lock_v1` and `idle_inhibit_unstable_v1` protocols.
* Configurable through plist text files: [base configuration](etc/wlmaker.plist), [style](/etc/style-default.plist) and [docks & workspaces](etc/wlmaker-state.plist).
* wlr layer shell support (`wlr_layer_shell_unstable_v1`), fully implemented & tested.
* Appearance matches Window Maker: Decorations, dock, clip.
* Support for Wayland XDG shell (mostly complete. Bug reports welcome).
* Initial support for X11 applications (positioning and specific modes are missing).
  Use `--start_xwayland` argument to enable XWayland, it's off by default.
* A prototype DockApp (`apps/wlmclock`).

For further details, see the [roadmap](doc/ROADMAP.md).

Protocol support:

* `xdg-decoration-unstable-v1`: Implemented & tested.
* `ext_session_lock_v1`: Implemented & tested.
* `wlr_layer_shell_unstable_v1`: Implemented & tested.
* `xdg_shell`: Largely implemented & tested.
* `idle_inhibit_unstable_v1`: Implemented, untested.

### Build & use it!

* From source: Please follow the [detailled build instructions](doc/BUILD.md)
  for a step-by-step guide.

* Once compiled, see the [these instructions](doc/RUN.md) on how to run
  Wayland Maker in a window or standalone, and to configure it for your needs.

* Alternatively, use a pre-built package:

  [![Packaging status](https://repology.org/badge/vertical-allrepos/wlmaker.svg)](https://repology.org/project/wlmaker/versions)

> [!NOTE]
> `wlmaker` covers basic functionality for using it on a single monitor. Please
> report bugs you find, and functionality you're missing.

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
