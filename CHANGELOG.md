# Changelog

All notable changes to this project will be documented in this file.

## [0.10.0] - 2026-08-07

### Added

- **Registrable Themes System**:
  - Added `ThemeManager` (`app.themes()`) for type-safe theme struct registration, named variants (e.g. `"dark"`, `"light"`), active variant switching, and active theme queries.
  - Added standard `FlowUiTheme` design system struct in `FlowUi/PublicStructs.hpp` containing tokens for colors, corner radii, typography, and spacing, with `dark()` and `light()` factories.
  - Added `UiManager` theme accessors: `ui.theme<T>()`, `ui.theme<T>("name")`, and `ui.flowTheme()`.
  - Added atomic frame-boundary staged theme mutations (`updateTheme<T>()` and `updateActiveTheme<T>()`) evaluated during `app.pollEvents()` / `beginFrame()`.
  - Extended `FlowStorageSystem` with `ResourceKind::UiTheme` and `ThemeRecordHeader` type-erased persistent payload storage.
  - Added `include/internal/TypeOperations.hpp` providing compile-time type tokens and 64-bit FNV-1a hashing under `FlowUi::detail`.
  - Added `flowui_theme_manager_tests` unit test suite in `tests/ThemesTests/`.
- **Multi-Window & Storage Architecture**:
  - Added explicit multi-window lifecycle APIs (`createWindow`, `destroyWindow`, `hasWindow`, `mainWindowId`) and per-window UI/Viewport access based on stable `WindowId` values.
  - Added `App::pollEvents()` for app-global event polling and shared manager tick passes.
  - Added centralized storage system (`IStorageSystem` / `FlowStorageSystem`) managing CPU/GPU memory pools, generational handles, frame-scoped transient arenas, descriptor binding revisions, and deferred resource retirement.
  - Added `App::setShouldClose(int)` for setting or clearing native window close flags.
  - Added `InputFieldManager::replaceText(std::string_view, std::string_view, bool)` for programmatic input text mutation with optional caret preservation.
  - Added `UiManager::inputContentElement(const Clay_TextElementConfig&)` for stable inner text bounds layout.
  - Added `WindowConfig::decorated` for undecorated window support.

### Changed

- Defined no-argument `App::beginFrame()` as convenience path that polls global events before beginning the main window. `beginFrame(WindowId)` begins only the selected window.
- Multi-window rendering requires one active `beginFrame(id)` → `endFrame(id)` → `drawFrame(id)` triplet at a time.
- Standardized `devMode::registry` type operations to use `FlowUi::detail` utilities from `TypeOperations.hpp`.
- Updated input field caret fallback behavior so empty fields prefer text element bounds and preserve consistent caret height after text deletion.

### Fixed

- Fixed empty input field caret fallback alignment and height consistency for padded input layouts.

### Known Limitations

- `beginFrame(WindowId)` waits for selected window frame slot; non-blocking scheduler is planned for a future release.
- Only one window frame triplet may be active at a time.
- Upload flushing remains synchronous; renderer performance tuning will continue in subsequent pre-releases.

## [0.9.1] - 2026-05-21

### Changed

- Updated Doxyfile.
- Updated Docs layout.
- Updated linking across docs.

## [0.9.0] - 2026-05-21

This is the first public pre-release of FlowUi establishing the initial project baseline.

### Added

- Initial FlowUi application/runtime structure.
- Immediate-mode Flow element system with typed params, state, resources, and callbacks.
- Core UI managers for fonts, icons, images, input fields, shortcuts, viewports, and frame-level UI coordination.
- Vulkan-backed rendering path and supporting resource management.
- Developer-mode registration, inspection, runtime editing, JSON export, and source update tooling.
- Initial documentation set covering setup, concepts, public APIs, tutorials, and developer-mode workflow.
- Standalone tooling for font baking and applying exported developer-mode changes.

[0.9.0]: https://github.com/manwe314/FlowUi/releases/tag/v0.9.0
[0.9.1]: https://github.com/manwe314/FlowUi/releases/tag/v0.9.1
[0.10.0]: https://github.com/manwe314/FlowUi/releases/tag/v0.10.0
