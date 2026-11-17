# Resource and default helpers

Include [Resources.hpp](include/FlowUi/Public/Resources.hpp). All four filesystem helpers return `Result<T>` and are `[[nodiscard]]` and `noexcept`. Check success before accessing the value; see [result handling](docs/api/result_handling.md). Returned paths, strings and byte vectors own their storage. Examples declare local argument values; `icons` refers to an existing icon manager.

## Public API

### **path_from_utf8**

#### `Result<std::filesystem::path> path_from_utf8(std::string_view text) noexcept`

Copies path bytes into an owned native path. It performs no resource lookup or existence check. Encoding/conversion and allocation failures are errors; do not use it as a general Unicode validator.

**Example:**

```cpp
std::string_view utf8 = "assets/logo.png";
auto result = FlowUi::path_from_utf8(utf8);
if (!result)
    throw FlowUi::FlowUiException(result.error());
std::filesystem::path path = std::move(*result);
```

See: [Owning header](include/FlowUi/Public/Resources.hpp).

### **path_to_utf8**

#### `Result<std::string> path_to_utf8(const std::filesystem::path& path) noexcept`

Returns an owned UTF-8 string with generic separators. It does not open the path. Conversion and allocation failures are returned.

**Example:**

```cpp
std::filesystem::path path{"assets/logo.png"};
auto result = FlowUi::path_to_utf8(path);
if (!result)
    throw FlowUi::FlowUiException(result.error());
std::string utf8 = std::move(*result);
```

See: [Owning header](include/FlowUi/Public/Resources.hpp).

### **locate_resource**

#### `Result<std::filesystem::path> locate_resource(const std::filesystem::path& name, const ResourceConfig& config = {}) noexcept`

Checks a direct absolute path, or searches a relative name within configured roots. Explicit roots precede enabled environment/platform roots and the optional build directory. Relative parent traversal is rejected. Discovery returns a path and does not open or cache the file.

**Example:**

```cpp
FlowUi::ResourceConfig configuration{};
configuration.roots.emplace_back("assets");
configuration.use_platform_defaults = false;
configuration.use_build_directory = false;

auto result = FlowUi::locate_resource("logo.png", configuration);
if (!result)
    throw FlowUi::FlowUiException(result.error());
std::filesystem::path path = std::move(*result);
```

See: [Owning header](include/FlowUi/Public/Resources.hpp).

### **read_file_bytes**

#### `Result<std::vector<unsigned char>> read_file_bytes(const std::filesystem::path& path) noexcept`

Reads a native path directly into an owned vector. An empty file is successful with an empty vector. Open/read/size/short-read and allocation failures return errors; this call does not apply resource-root discovery.

**Example:**

```cpp
std::filesystem::path path{"assets/logo.png"};
auto result = FlowUi::read_file_bytes(path);
if (!result)
    throw FlowUi::FlowUiException(result.error());
std::vector<unsigned char> bytes = std::move(*result);
```

See: [Owning header](include/FlowUi/Public/Resources.hpp).

### **missing_font**

#### `[[nodiscard]] Result<FontFaceCreateInfo> defaults::missing_font() noexcept`

`defaults::missing_font() noexcept` returns an owned `FontFaceCreateInfo` describing the disk-independent emergency U+FFFD rectangle. Its baked bytes borrow static storage; its metadata can fail allocation. Other non-whitespace characters fall back to that rectangle, so it is not a readable alphabet.

**Example:**

```cpp
auto result = FlowUi::defaults::missing_font();
if (!result)
    throw FlowUi::FlowUiException(result.error());
FlowUi::FontFaceCreateInfo face = std::move(*result);
```

See: [Owning header](include/FlowUi/Public/defaults/FontDefaults.hpp).

### **text_shortcuts**

#### `[[nodiscard]] Result<defaults::TextShortcutDefaults> defaults::text_shortcuts(const DefaultTextShortcutConfig& configuration) noexcept`

`defaults::text_shortcuts(const DefaultTextShortcutConfig&) noexcept` returns `Result<defaults::TextShortcutDefaults>`, an owned fixed array of up to 14 `TextShortcutBinding` values. Iterate only `binding_count`. It produces a plan and does not register callbacks. Disabled configurations succeed with an empty plan; an invalid enabled platform returns `ShortcutInvalid`. `Auto` selects Command on Apple and Control elsewhere. Scope and priority are chosen during installation.

**Example:**

```cpp
FlowUi::DefaultTextShortcutConfig configuration{};
auto result = FlowUi::defaults::text_shortcuts(configuration);
if (!result)
    throw FlowUi::FlowUiException(result.error());
FlowUi::defaults::TextShortcutDefaults plan = std::move(*result);
```

See: [Owning header](include/FlowUi/Public/defaults/ShortcutDefaults.hpp).

### **input_field_overlay_style**

#### `[[nodiscard]] InputFieldOverlayStyle defaults::input_field_overlay_style(const InputManagerConfig& config) noexcept`

`defaults::input_field_overlay_style(const InputManagerConfig&) noexcept` returns an `InputFieldOverlayStyle` value derived from caret and selection settings.

**Example:**

```cpp
FlowUi::InputManagerConfig configuration{};
configuration.caretWidthPx = 3.0f;
FlowUi::InputFieldOverlayStyle style =
    FlowUi::defaults::input_field_overlay_style(configuration);
```

See: [Owning header](include/FlowUi/Public/defaults/InputDefaults.hpp).

### **missing_texture**

#### `[[nodiscard]] defaults::TexturePixels defaults::missing_texture() noexcept`

`defaults::missing_texture() noexcept` returns `TexturePixels`: a borrowed RGBA8 byte span and width/height of the static opaque 2×2 purple/cyan checkerboard. It does not create a GPU texture.

**Example:**

```cpp
const FlowUi::defaults::TexturePixels pixels = FlowUi::defaults::missing_texture();
const auto bytes = pixels.bytes; // Static, borrowed RGBA8 storage.
```

See: [Owning header](include/FlowUi/Public/defaults/TextureDefaults.hpp).

### **registerStandardIcons**

#### `void FSEL::standard_icons::registerStandardIcons(IconManager& icons) noexcept`

`FSEL::standard_icons::registerStandardIcons(IconManager&) noexcept` is available with `FLOWUI_INCLUDE_ICON_MANAGER`. It returns `void`, preserves existing semantic keys, and ignores registration failures. Controls use their documented fallback if artwork is unavailable; there is no success result to inspect.

**Example:**

```cpp
#if FLOWUI_INCLUDE_ICON_MANAGER
FlowUi::FSEL::standard_icons::registerStandardIcons(icons);
#endif
```

See: [Owning header](include/FlowUi/Public/defaults/FselIcons.hpp).

