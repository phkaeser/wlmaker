# Examples or tools suitable for wlmaker

Through git submodules, this directory points to various tools for using with,
or for experimenting with wlmaker.

These tools are not built by default. See `CMakeLists.txt` for potential
further package dependencies.

```
git submodule update --init --recursive --merge
git submodule update --checkout --recursive --merge
cmake -DCMAKE_INSTALL_PREFIX:PATH=${HOME}/.local . -B build
```

