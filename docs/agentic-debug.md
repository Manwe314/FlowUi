# AgenticDebug frame capture

`AgenticDebug` is a separate, private instrumentation axis. It has no dependency on
`FLOW_UI_DEV_MODE`, FSEL, or a particular AI provider. It adds no application API,
CMake `option()`, cache declaration, or exported compile definition.

Enable it explicitly in an existing configured build:

```sh
cmake -S . -B build -DAgenticDebug=ON
cmake --build build --target flowui_fsel_demo
```

For a fresh Linux/Clang build, select the compiler and provide the repository's
usual dependency/toolchain settings as needed:

```sh
cmake -S . -B build-agentic -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DAgenticDebug=ON
cmake --build build-agentic --target flowui_fsel_demo
```

On Windows, configure/build from an MSVC developer shell. The capture code uses
C++ standard filesystem, streams, environment access, and Vulkan/VMA; it does not
use PowerShell, Win32 screenshot APIs, POSIX process APIs, or platform-specific paths.
The internal implementation is also guarded by `#if defined(AgenticDebug) && AgenticDebug`.
Without the definition, capture code, runtime environment checks, readback resources,
and additional image usage flags are absent. `-DAgenticDebug=OFF` disables it;
`cmake -S . -B build -UAgenticDebug` removes a previously supplied cache entry.
Omitting the argument on an already enabled build does not erase CMake's cache.

## Capture a run

Set these environment variables **before launching** the application:

| Variable | Meaning |
| --- | --- |
| `FLOWUI_AGENTIC_DEBUG_DIR` | Output parent directory. Unset or empty disables capture at runtime. |
| `FLOWUI_AGENTIC_DEBUG_START_FRAME` | First submitted frame to capture, independently per window; zero-based, default `0`. |
| `FLOWUI_AGENTIC_DEBUG_FRAME_COUNT` | Number of frames to capture per window, default `8`; `0` captures none. |

PowerShell:

```powershell
$env:FLOWUI_AGENTIC_DEBUG_DIR = 'build/agentic-captures'
$env:FLOWUI_AGENTIC_DEBUG_START_FRAME = '120'
$env:FLOWUI_AGENTIC_DEBUG_FRAME_COUNT = '12'
& ./build/tests/flowui_fsel_demo.exe
```

Linux shell:

```sh
FLOWUI_AGENTIC_DEBUG_DIR=build/agentic-captures \
FLOWUI_AGENTIC_DEBUG_START_FRAME=120 \
FLOWUI_AGENTIC_DEBUG_FRAME_COUNT=12 \
./build/tests/flowui_fsel_demo
```

Relative paths resolve against the application's working directory. Each Vulkan
device/run creates a unique `capture-*` subdirectory, printed to stderr. Existing
captures are never overwritten. Invalid numeric settings fall back to their defaults.

## Agent-readable output

Each completed capture produces a binary RGB PPM (`P6`) and a record in `frames.jsonl`:

```json
{"schema":1,"file":"frame-3.ppm","sequence":3,"window":2,"frame":121,"frame_slot":1,"source":"viewport","key":"preview","width":480,"height":424,"vk_format":44,"origin":"top-left","alpha":"discarded","encoding":"native-rgb8"}
```

`source` is `window` for the final composed window image before presentation, or
`viewport` for an offscreen viewport immediately after rendering. `key` identifies
the viewport. Match `window` and `frame` to compare what the viewport rendered with
what its window sampled. Compare successive `frame_slot` values to investigate
frames-in-flight problems. Sequence numbers identify captures; completion order can
differ, and cancelled/unsubmitted captures may leave gaps. Files appear only after
the owning slot's fence completes, or during drained window/application cleanup.

An agent can read PPM directly, or convert it using any image tool. For example,
with Python/Pillow installed:

```python
import json
from pathlib import Path
from PIL import Image

session = Path("build/agentic-captures/capture-...")
for line in (session / "frames.jsonl").read_text(encoding="utf-8").splitlines():
    entry = json.loads(line)
    path = session / entry["file"]
    Image.open(path).save(path.with_suffix(".png"))
```

This toolbox captures arbitrary windows and viewports; it does not force a demo
selection, drive input, or require an agent-specific executable. Reproduce input
through the application's usual controls or a dedicated test harness.

## Behavior and limits

- Readback is asynchronous and uses the application's existing frame fences; it
  adds no device/queue idle waits. Captures own their readback buffers separately
  from source images, so resizing and image retirement do not invalidate them.
- The implementation follows Vulkan's
  [GPU-to-CPU readback synchronization](https://docs.vulkan.org/guide/latest/synchronization_examples.html):
  a transfer-to-host memory dependency, completed submission, then mapped-memory
  invalidation before CPU access.
- RGBA8/BGRA8 UNORM and sRGB targets retain their stored RGB values; BGRA is reordered.
  RGBA16 float is clamped to `[0,1]` and converted to linear RGB8. Alpha is discarded.
  This is a visual diagnostic, not a lossless HDR or alpha dump.
- Unsupported formats and surfaces without transfer-source support are skipped.
  Capture never requests unsupported swapchain usage.
- Output I/O and GPU copies add overhead. Use a bounded capture range when studying
  timing-sensitive behavior. Disk/allocation failures are reported to stderr and do
  not throw into application callbacks. Abrupt process termination can lose pending
  captures; a completed graphics frame is not a guarantee of successful presentation.

## Regression check

With `AgenticDebug` enabled and repository tests enabled:

```sh
cmake --build build --target flowui_agentic_capture_tests
ctest --test-dir build -R '^flowui.agentic.capture$' --output-on-failure
```

The GPU test checks pixel data, frame-slot isolation, multiple windows, resized
targets, cancelled captures, JSON escaping, and drained cleanup. It is independent
of developer mode and runs without a window system. Linux/Clang execution still
needs to be verified on that platform.
