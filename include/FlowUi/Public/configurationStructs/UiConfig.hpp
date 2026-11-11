#pragma once

#include "FlowUi/Public/configurationStructs/InputConfig.hpp"
#include "FlowUi/Public/configurationStructs/ShortcutManagerConfig.hpp"
#include "FlowUi/Public/managerStructs/FontTypes.hpp"
#include <cstddef>
#include <cstdint>

namespace FlowUi {

/** Startup analytic coverage for solid primitives, borders, and image silhouettes. */
struct PrimitiveAntialiasingConfig {
	/** Positive finite multiplier for derivative-based linear coverage, measured in framebuffer pixels. */
	float coverage_width_scale = 1.0f;
	/** Disable analytic coverage and its raster fringe to use hard primitive edges. Glyph AA is separate. */
	bool enabled = true;
};

/**
 * Layout, font, and text-editing defaults shared by application windows at startup.
 * Temporary UI storage is sized by StorageMemoryCapacities::transientBytesPerFramePerWindow.
 */
struct UiConfig {
	/**
	 * Logical dots per inch for text: pixels = points * dpi / 72 * fontScale.
	 * Initialization clamps dpi to at least one; 12 points at 96 dpi and scale one is 16 pixels.
	 */
	float dpi = 96.0f;

	/**
	 * Layout scale applied to UI coordinates and input; use a positive value.
	 * Values above one produce larger UI within the same native window.
	 */
	float uiScale = 1.0f;

	/**
	 * Multiplier applied after DPI font scaling; zero or negative values use the DPI scale alone.
	 */
	float fontScale = 1.0f;

	/**
	 * Persistent Clay layout arena bytes; zero selects Clay_MinMemorySize().
	 * Nonzero values below that minimum are raised to it.
	 */
	size_t clayArenaCapacityBytes = 0;

	/**
	 * Startup fallback font family, registered as family zero.
	 * Loading failures follow ErrorPolicy::defaultFont, which can try built-in fallback fonts.
	 */
	FontFamilyCreateInfo defaultFontFamily{};

	/**
	 * Square font atlas page size in pixels; must fit the source or runtime-baked glyph atlas.
	 * Use a nonzero value supported by the GPU.
	 */
	uint32_t fontAtlasSize = 2048;

	/**
	 * @brief Input field caret and selection rendering configuration.
	 *
	 * @see FlowUi::InputManagerConfig
	 */
	InputManagerConfig inputManager{};

	/** @brief Built-in shortcut registrations installed for this UI manager. */
	ShortcutManagerConfig shortcuts{};

	/** Copied at renderer startup; changing the caller's config does not affect existing windows. */
	PrimitiveAntialiasingConfig primitive_antialiasing{};
};

} // namespace FlowUi
