# IPC

Simple cross-platform C++ IPC library

## Build

This project is built using Meson: https://mesonbuild.com/Getting-meson.html

```
meson setup builddir --buildtype=debug
meson compile -C builddir
meson test -vC builddir
```
