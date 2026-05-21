# Examples or tools suitable for wlmaker

Through git submodules, this directory points to various tools for using with,
or for experimenting with wlmaker.

The subdirectories in this directory (such as submodules like `gtk-layer-shell`)
are not built by default. See `CMakeLists.txt` for potential further package
dependencies. The example applications at the toplevel of this directory are,
however, built by default as part of the main project build.

```
git submodule update --init --recursive --merge
git submodule update --checkout --recursive --merge
cmake -DCMAKE_INSTALL_PREFIX:PATH=${HOME}/.local . -B build
```

