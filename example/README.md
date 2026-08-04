# Multi-window text example

This example uses FlowUi's developer input-field and button elements. Enter
text in the main window and press **Open text window**. Every created secondary
window displays the current input text.

Build and run from the repository root:

```sh
cmake -S example -B build-example -DCMAKE_BUILD_TYPE=Release
cmake --build build-example --parallel
./build-example/flowui_multi_window_example
```

Secondary-window creation requires the Vulkan present-completion capabilities
enforced by FlowUi's current multi-window API. If they are unavailable, the
main window stays usable and shows the creation error beneath the button.
