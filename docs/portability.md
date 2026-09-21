# Resources and portable builds

FlowUi uses CMake 3.25+, C++23, Vulkan and GLFW. MSVC receives `/Zc:preprocessor`, `/bigobj`, and `/utf-8` through `FlowUi::FlowUi`, including installed consumers. Bundled font libraries follow `CMAKE_MSVC_RUNTIME_LIBRARY`; the default is the dynamic CRT. Use a separate build directory for each compiler, target platform and architecture.

## Development and packaging

Link `FlowUi::FlowUi` as usual. Building FlowUi automatically compiles and stages its six shaders and default Inter font. Application startup finds these without depending on the current working directory.

For a deployed application, add one call after creating and linking its target:

```cmake
find_package(FlowUi CONFIG REQUIRED) # Or add_subdirectory(FlowUi).
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE FlowUi::FlowUi)
flowui_package_resources(my_app)
```

The same helper works in source builds and installed packages. It refreshes resources when the application target is built, even when its executable does not need relinking. On Windows it also copies CMake-known runtime DLL dependencies after linking. Dependencies without an imported DLL location, the Vulkan runtime/driver, and the MSVC runtime still need their normal deployment arrangements.

| Platform | Application resource layout |
| --- | --- |
| Windows | `<executable directory>/flowui/{shaders,fonts}` |
| Linux / Unix | `<executable directory>/../share/flowui/{shaders,fonts}` |
| macOS application bundle | `<app>.app/Contents/Resources/flowui/{shaders,fonts}` |
| macOS command-line application | Same `share/flowui` layout as Unix |

Unix shared data follows the [Filesystem Hierarchy Standard](https://www.debian.org/doc/packaging-manuals/fhs/fhs-3.0.html). Apple bundles use Apple's [Resources directory layout](https://developer.apple.com/library/archive/documentation/CoreFoundation/Conceptual/CFBundles/BundleTypes/BundleTypes.html). Windows assets stay with the installed application, rather than in a writable user-data directory.

`cmake --install build --prefix <prefix>` installs the library, headers, package configuration, and resource pack. The default resource destination is `bin/flowui` on Windows and `share/flowui` elsewhere. Resources are read-only installation data; user settings and generated content belong elsewhere.

Optional packaging controls:

```cmake
flowui_package_resources(my_app
    EXTRA_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets")
# EXTRA_DIRECTORY contents are merged into the pack and may override defaults.
# DESTINATION can replace the default application resource location.
```

`FLOWUI_RESOURCE_SUBDIRECTORY` changes the `flowui` directory name at library configuration time. `FLOWUI_INSTALL_RESOURCE_DIRECTORY` changes the installation destination and respects GNUInstallDirs. A custom nonstandard destination requires a matching runtime root. Standard layouts remain relocatable with the executable or library. On Unix, `/usr/local/share/flowui` and `/usr/share/flowui` are also searched.

## Runtime configuration and UTF-8

```cpp
#include <FlowUi/Resources.hpp>

FlowUi::AppConfig config;
config.resources.roots = {std::filesystem::path(u8"/my/package/资源")};
// Optional strict deployment mode:
config.resources.use_build_directory = false;
// config.resources.use_platform_defaults = false; // Use only explicit roots.

auto texture_path = FlowUi::locate_resource("textures/default.png", config.resources);
if (texture_path) {
    auto registered = app.images().register_image_file("default", *texture_path);
    // Handle registered through the normal Result API.
}
```

Lookup order is explicit roots, `FLOWUI_RESOURCE_ROOT`, executable/library-relative platform locations, Unix system locations, then the build-tree pack. `ResourceConfig::executable_directory` overrides executable discovery for embedding hosts. Disabling platform defaults also disables environment and module lookup. Absolute resource paths are explicit overrides; relative resource names may not contain parent traversal. Explicit relative file paths supplied to image/font/SVG loading retain normal filesystem semantics.

The resource service is shared by shader and default-font loading. Explicit custom font files remain caller-controlled. `locate_resource` and `read_file_bytes` return normal FlowUi error results. Pack roots contain `shaders/`, `fonts/`, and any application-specific directories.

Use `std::filesystem::path` for filenames, `u8"..."` path literals, and `FlowUi::path_from_utf8` / `path_to_utf8` at text boundaries. Native image/SVG entry points are `register_image_file` and `register_file`; existing string-based entry points interpret filenames as UTF-8. Font paths already use `std::filesystem::path`. Image and font codecs receive file contents from native-path streams, avoiding narrow Windows filename APIs. Text supplied to the UI remains UTF-8.

The command-line tools convert Windows wide arguments to UTF-8 and open input/output streams with native paths. Their Windows manifest requests UTF-8 for the remaining vendor font-export filename API; this requires Windows 10 version 1903 or newer. See Microsoft's [UTF-8 application code-page documentation](https://learn.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page).

## Shared libraries and cross-compilation

`-DBUILD_SHARED_LIBS=ON` builds a DLL/shared library. Windows uses automatic function exports plus explicit imports for Clay's public data. Link the exported CMake target so consumers receive the build definitions. Keep the same compiler ABI, architecture, library configuration, and compatible CRT across the application and FlowUi; this is a C++ API, not a stable cross-compiler binary ABI.

Cross-compilation uses a normal [CMake toolchain file](https://cmake.org/cmake/help/manual/cmake-toolchains.7.html):

```sh
cmake -S . -B build-target -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/target-toolchain.cmake \
  -DFLOWUI_BUILD_TESTS=OFF -DFLOWUI_BUILD_FONT_BAKER=OFF
cmake --build build-target --target flowui
```

Provide target Vulkan/GLFW dependencies through the toolchain/sysroot. Shader compilers are discovered on the host, outside the target root; `FLOWUI_HOST_GLSLC` or `FLOWUI_HOST_GLSLANG` can select one explicitly. Baked-change generation builds a separate native host executable with Ninja. Override `FLOWUI_HOST_CXX_COMPILER` when needed, or supply an existing native executable with `FLOWUI_HOST_DEV_GENERATOR`. The generator can also be built independently from `tools/flowui-dev-generate`. Host generators are not installed as target executables. The optional font baker is a target executable; bake fonts in a native build before deploying when cross-compiling.

Windows diagnostic stacks use `CaptureStackBackTrace`; Linux/macOS retain unwind-based capture. Vulkan swapchain maintenance is enabled only when its [required extensions](https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_swapchain_maintenance1.html) and device feature are available. Window cleanup retains the element registry until its mutex guard is released, fixing the MSVC shutdown assertion caused by unlocking a destroyed mutex.

## Validation

`flowui.resources` covers resource precedence, relocation, missing files, Unicode paths, binary loading and automatically staged assets. `flowui.dev.errors` exercises the native stack provider. `tests/PackageConsumer` is a separate `find_package` consumer for installed static and shared builds; run its executable with `--render` for a short Vulkan/font/shader startup check. Build its executable in a directory outside the package prefix to check packaging, then relocate the resulting directory and run it from a different working directory.

Linux/Clang and Apple execution still require testing on those systems. Compiler-dependent identity hashing and a CI portability matrix are outside this change.

Verified on Windows/MSVC: DEV_MODE FSEL static and DLL builds; installed static, DLL and runtime-font-baking consumers; Unicode font baking; relocated Unicode-directory Vulkan rendering with build-tree lookup disabled; clean shutdown; resource, native-stack, frame-capture and two-window tests. The cross-compilation CMake branch was exercised with a native Windows host generator and host shader compiler; this validates host-tool separation, not a Linux/Apple target build. For the runtime font smoke check, place a TTF at `fonts/runtime.ttf` in the consumer's pack and run with `--render --runtime-font` against a runtime-font-baking build.
