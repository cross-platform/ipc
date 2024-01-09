# IPC

Simple cross-platform C++ IPC library

## Build

To use this library in your own projects, all you need are the files under `src/` - No third-party dependancies required.

If you'd like to build this project, you'll need Meson installed: https://mesonbuild.com/Getting-meson.html

```
meson setup builddir --buildtype=debug
meson compile -C builddir
meson test -vC builddir
```
