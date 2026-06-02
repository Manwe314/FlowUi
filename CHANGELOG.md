# Changelog

All notable changes to this project will be documented in this file.

## [0.9.0] - 2026-05-21

This is the first public pre-release of FlowUi. It establishes the initial
project baseline for experimentation, feedback, and early integration work.

This version should not be treated as a polished final product. APIs, tooling,
performance characteristics, and internal systems may still change as the
library moves toward a stable release.

### Added

- Initial FlowUi application/runtime structure.
- Immediate-mode Flow element system with typed params, state, resources, and callbacks.
- Core UI managers for fonts, icons, images, input fields, shortcuts, viewports, and frame-level UI coordination.
- Vulkan-backed rendering path and supporting resource management.
- Developer-mode registration, inspection, runtime editing, JSON export, and source update tooling.
- Initial documentation set covering setup, concepts, public APIs, tutorials, and developer-mode workflow.
- Standalone tooling for font baking and applying exported developer-mode changes.

### Changed

- No migration notes. This is the first tracked release.

### Deprecated

- Nothing deprecated. This release is the starting compatibility point.

### Removed

- Nothing removed. This release is the starting compatibility point.

### Fixed

- No previous tracked release exists, so there are no release-to-release fixes to list.

### Known Limitations

- This pre-release is heavily unoptimized and still fragile in places.
- API stability is not guaranteed before a later stable release.
- Developer tooling is functional but still early and may require manual review or cleanup after generated source updates.
- Renderer behavior, resource lifetime rules, and performance-sensitive paths still need more hardening and profiling.

### Security

- No dedicated security review has been completed for this pre-release.

## [0.9.1] - 2026-05-21


### Added

- No added Features

### Changed

- Updated Doxyfile.
- Updated Docs layout.
- Updated linking across docs.

### Deprecated

- Nothing deprecated

### Removed

- Nothing removed.

### Fixed

- no release-to-release fixes.

### Known Limitations

- Still same as v0.9.0

### Security

- No dedicated security review has been completed for v0.9.1.

## [0.9.2] - 2026-06-02


### Added

- Added `App::setShouldClose(int)` for setting or clearing the native window close flag.
- Added `InputFieldManager::replaceText(std::string_view, std::string_view, bool)` for replacing managed input field text while optionally preserving caret state.
- Added `UiManager::inputContentElement(const Clay_TextElementConfig&)` for creating stable inner input-field content layout declarations.

### Changed

- Updated input field caret fallback behavior so empty fields prefer text element bounds and preserve consistent caret height after text is deleted.
- Updated public API documentation for the new App, input field manager, and UI manager APIs.

### Deprecated

- Nothing deprecated

### Removed

- Nothing removed.

### Fixed

- Fixed empty input field caret fallback alignment and height consistency for padded input layouts.

### Known Limitations

- Still same as v0.9.0

### Security

- No dedicated security review has been completed for v0.9.2.

<!-- Release links -->


[0.9.0]: https://github.com/manwe314/FlowUi/releases/tag/v0.9.0
[0.9.1]: https://github.com/manwe314/FlowUi/releases/tag/v0.9.1
