# Build Wayland Maker

Wayland Maker is developed and tested on Debian, hence we're using package
names and versions as found on that distribution. The code is aimed to
compile well on **Debian Trixie** using pre-compiled libraries; with detailed
build intructions just below.

For compiling on **Debian Bookworm**, further dependencies need to be
compiled, built and installed. This is described
[further below](BUILD.md#build-on-debian-bookworm-stable).

## Build on Debian Trixie

### Install required packages

```
apt-get install -y \
  bison \
  clang \
  cmake \
  doxygen \
  flex \
  gcc \
  git \
  libcairo2-dev \
  libncurses-dev \
  libwlroots-0.18-dev \
  pkg-config \
  plantuml \
  xwayland
```

See the [github build workflow](../.github/workflows/build-for-linux.yml)
as reference.

### Get Wayland Maker

```
git clone https://github.com/phkaeser/wlmaker.git
(cd wlmaker && git submodule update --init submodules/)
```

Run the commands below from the directory you cloned the source into.

### Configure, build and install Wayland Maker

Wayland Maker and tools will be installed to `${HOME}/.local`:

```bash
cmake -DCMAKE_INSTALL_PREFIX="${HOME}/.local" -B build/
(cd build && make && make install)
```

That's it! Now up to the [running instructions]!


## Build on Debian Bookworm (stable)

On **Debian Bookworm**, further dependencies need to be configured,
built & installed. See the [github build workflow for
Bookworm](../.github/workflows/build-for-bookworm-wlroots-018.yml) as reference
and for the list of packages.

### Get Wayland Maker

```
git clone https://github.com/phkaeser/wlmaker.git
```

### Get, build and install dependencies

Wayland Maker is still in development and is depending on a set of rapidly
evolving libraries. To keep the API between code and dependencies synchronized,
some dependencies are included as github submodules. Here's how to configure
and build these.

The dependencies will be installed to `${HOME}/.local`:

``` bash
git submodule update --init --checkout --recursive --merge
(cd dependencies &&
 LD_LIBRARY_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)" \
 PKG_CONFIG_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/pkgconfig/:${HOME}/.local/share/pkgconfig/" \
 cmake -DCMAKE_INSTALL_PREFIX:PATH=${HOME}/.local -B build &&
 cd build &&
 LD_LIBRARY_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)" \
 PKG_CONFIG_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/pkgconfig/:${HOME}/.local/share/pkgconfig/" \
 make)
```

## Configure, build and install Wayland Maker

```bash
LD_LIBRARY_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)" \
PKG_CONFIG_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/pkgconfig/:${HOME}/.local/share/pkgconfig/" \
cmake -DCMAKE_INSTALL_PREFIX="${HOME}/.local" -B build/
```

``` bash
(cd build && \
 LD_LIBRARY_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)" \
 PKG_CONFIG_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/pkgconfig/:${HOME}/.local/share/pkgconfig/" \
 make && \
 make install)
```

Now you have an installed binary, and can run it with the appropriate
environment. See [running instructions](RUN.md) for more details.

```bash
LD_LIBRARY_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)" \
PKG_CONFIG_PATH="${HOME}/.local/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/pkgconfig/:${HOME}/.local/share/pkgconfig/" \
${HOME}/.local/bin/wlmaker
```

Please report if something doesn´t work for you.
