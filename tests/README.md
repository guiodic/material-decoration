# Decoration integration tests

`decorationstate` and `outlinecolor` load the built Material plugin with a test
compositor backend. They run offscreen in an isolated D-Bus session and use a
temporary configuration directory.

`tabletreply` checks that a delayed initial D-Bus reply cannot overwrite a newer
tablet-mode signal, including a signal matching the default value. It requires a
running Wayland compositor and the Qt Wayland platform plugin. It is built and
registered only when `FORCE_X11=OFF`, since tablet-mode support is Wayland-only.
Install `dbus-run-session` before configuring: CMake registers these integration
tests only if it finds that executable. Tests are disabled by default; from the
repository root, configure and build them with:

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DFORCE_X11=OFF
cmake --build build
```

Without an explicit display, CTest marks `tabletreply` as skipped. To run it
against an existing Wayland compositor:

```sh
MATERIAL_TEST_WAYLAND_DISPLAY="$WAYLAND_DISPLAY" ctest --test-dir build -R '^tabletreply$' --output-on-failure
```

Keep `XDG_RUNTIME_DIR` set to the compositor's runtime directory. The test starts
its own D-Bus session with a fake KWin service; it does not change the compositor's
tablet mode.

`buttonsvg` is registered when Qt SVG development files are available
(`qt6-svg` on Arch, `libqt6svg6-dev` on Debian). It exports real GTK button
icons through `QSvgGenerator` and checks their pixel alignment when rendered
at 18×18. This checks SVG geometry, not GTK image caching or HiDPI behavior.
