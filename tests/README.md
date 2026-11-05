# Decoration integration tests

`decorationstate` and `outlinecolor` load the built Material plugin with a test
compositor backend. They run offscreen in an isolated D-Bus session and use a
temporary configuration directory.

`tabletreply` checks that a delayed initial D-Bus reply cannot overwrite a newer
tablet-mode signal, including a signal matching the default value. It requires a
running Wayland compositor and the Qt Wayland platform plugin. By default CTest
marks it as skipped. To run it against an existing compositor:

```sh
MATERIAL_TEST_WAYLAND_DISPLAY="$WAYLAND_DISPLAY" ctest --test-dir build -R '^tabletreply$' --output-on-failure
```

Keep `XDG_RUNTIME_DIR` set to the compositor's runtime directory. The test starts
its own D-Bus session with a fake KWin service; it does not change the compositor's
tablet mode.
