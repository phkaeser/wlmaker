# Build Wayland Maker

## Install required packages

Wayland Maker is developed and tested on Debian, hence we're using package
names and versions as found on that distribution. You need to run:

```
apt-get install -y \
  bison \
  clang \
  cmake \
  doxygen \
  flex \
  foot \
  git \
  glslang-dev \
  glslang-tools \
  graphviz \
  libcairo2-dev \
  libgbm-dev \
  libinput-dev \
  libncurses-dev \
  libudev-dev \
  libvulkan-dev \
  libwayland-dev \
  libxcb-composite0-dev \
  libxcb-dri3-dev \
  libxcb-ewmh-dev \
  libxcb-icccm4-dev \
  libxcb-present-dev \
  libxcb-render-util0-dev \
  libxcb-res0-dev \
  libxcb-xinput-dev \
  libxkbcommon-dev \
  libxml2-dev \
  meson \
  plantuml \
  wayland-protocols \
  xmlto \
  xsltproc \
  xwayland
```

See the [github build workflow](../.github/workflows/build-for-linux.yml) as reference.

## Get Wayland Maker

```
git clone https://github.com/phkaeser/wlmaker.git
```

Run the commands below from the directory you cloned the source into.

## Get, build and install dependencies

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
