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

Some of Wayland Maker's core dependencies are also in development and are a
moving target, hence these are referred to as git submodules. Build and install
these first (default to `${HOME}/.local`):

``` bash
git submodule update --init --checkout --recursive --merge
(cd dependencies &&
 LD_LIBRARY_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)" \
 PKG_CONFIG_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/pkgconfig/:${HOME}/.local/share/pkgconfig/" \
 cmake -DCMAKE_INSTALL_PREFIX:PATH=${HOME}/.local -B build &&
 LD_LIBRARY_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)" \
 PKG_CONFIG_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/pkgconfig/:${HOME}/.local/share/pkgconfig/" \
 cd build && make)
```

With the dependencies installed, proceed to configure wlmaker:

```bash
LD_LIBRARY_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)" \
PKG_CONFIG_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/pkgconfig/:${HOME}/.local/share/pkgconfig/" \
cmake -Dconfig_DOXYGEN_CRITICAL=ON -Dconfig_TOOLKIT_PROTOTYPE=ON -DCMAKE_INSTALL_PREFIX="${HOME}/.local" -B build/
```

### To build and install

``` bash
(cd build && \
 LD_LIBRARY_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)" \
 PKG_CONFIG_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/pkgconfig/:${HOME}/.local/share/pkgconfig/" \
 make && \
 make install)
```

### To run it

Since `wlmaker` is in early development, it's not recommended to use it as your
primary compositor. It should run fine in it's own window, though:

```bash
LD_LIBRARY_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)" \
PKG_CONFIG_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/pkgconfig/:${HOME}/.local/share/pkgconfig/" \
${HOME}/.local/bin/wlmaker
```

Press `ctrl+window+alt T` to open a Terminal (`foot`), and `ctrl-window-alt Q`
to exit.

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for details, and 
[code of conduct](CODE_OF_CONDUCT.md) for more.

## License

Apache 2.0; see [`LICENSE`](LICENSE) for details.

## Disclaimer

This project is not an official Google project. It is not supported by
Google and Google specifically disclaims all warranties as to its quality,
merchantability, or fitness for a particular purpose.
