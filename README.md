# wlmaker - Wayland Maker

A [Wayland](https://wayland.freedesktop.org/) compositor inspired by 
[Window Maker](https://www.windowmaker.org/).

Key features:

* Compositor for windows in stacking mode.
* Supports multiple workspaces.
* Appearance inspired by Window Maker, following the look and feel of NeXTSTEP.
* Easy to use, lightweight, low gimmicks and fast.
* Dock and clip, to be extended for dockable apps.

Wayland Maker is in early development stage. See the [roadmap](doc/ROADMAP.md) 
for existing and planned features.

### To configure

```bash
cmake -Dconfig_DOXYGEN_CRITICAL=ON -DCMAKE_INSTALL_PREFIX="${HOME}/.local" -B build/
```

### To build

```bash
(cd build && make)
```

### To install

```bash
(cd build && make install)
```

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for details, and 
[code of conduct](CODE_OF_CONDUCT.md) for more.

## License

Apache 2.0; see [`LICENSE`](LICENSE) for details.

## Disclaimer

This project is not an official Google project. It is not supported by
Google and Google specifically disclaims all warranties as to its quality,
merchantability, or fitness for a particular purpose.
