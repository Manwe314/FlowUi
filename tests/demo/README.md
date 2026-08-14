# FSEL Immediate Gallery

This executable is an interactive gallery for the current Flow Standard Element
Library primitives. It intentionally uses the public `createElement()` surface
instead of demo-only wrappers, so each card also serves as a concrete usage
example.

Build and run from the repository root:

```sh
cmake --build build --target flowui_fsel_demo -j4
./build/tests/flowui_fsel_demo
```

The target is available when both `FLOWUI_BUILD_TESTS` and `COMPILE_FSELI` are
enabled.
