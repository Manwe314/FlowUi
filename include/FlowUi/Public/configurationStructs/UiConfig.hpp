#pragma once

#include "FlowUi/Public/configurationStructs/InputConfig.hpp"
#include "FlowUi/Public/configurationStructs/ShortcutManagerConfig.hpp"
#include "FlowUi/Public/managerStructs/FontTypes.hpp"
#include <cstddef>
#include <cstdint>

namespace FlowUi {

/**
 * @brief Configures FlowUi layout, text, and UI resource defaults.
 *
 * These values are consumed during app and UI manager initialization. They affect
 * Clay layout memory, transient per-frame UI storage, text scaling, and the
 * default font resources used by text rendering.
 */
struct UiConfig {
	/**
	 * @brief Logical dots-per-inch used for point-to-pixel conversion.
	 *
	 * Text sizes are specified in points and converted to pixels as dpi / 72.
	 * Values below 1 are clamped to 1 during initialization.
	 *
	 * For example, a 12 pt text size at the default 96 dpi is laid out as
	 * 12 * (96 / 72) = 16 px before fontScale is applied.
	 */
	float dpi = 96.0f;

	/**
	 * @brief Global layout scale applied to UI coordinates and input.
	 *
	 * Values above 1 make the logical layout space smaller relative to the
	 * native window size, producing larger rendered UI.
	 */
	float uiScale = 1.0f;

	/**
	 * @brief Global multiplier applied after DPI font scaling.
	 *
	 * If the configured value is zero or negative, FlowUi falls back to the
	 * DPI-derived point-to-pixel scale.
	 */
	float fontScale = 1.0f;

	/**
	 * @brief Byte capacity of each transient UI arena.
	 *
	 * FlowUi creates one arena per frame in flight and resets the active arena at
	 * the beginning of each UI frame. These arenas store temporary Clay strings,
	 * element ids, and texture references created during UI construction.
	 *
	 * @note The Size set here is for ONE arena. Total Size used will be stringArenaSize *
	 * framesInFlight
	 *
	 * @see FlowUi::VulkanConfig::framesInFlight
	 */
	size_t stringArenaSize = 256 * 1024;

	/**
	 * @brief Byte capacity of the persistent Clay layout arena.
	 *
	 * A value of 0 uses Clay_MinMemorySize(). Non-zero values are clamped up to
	 * at least Clay_MinMemorySize() before Clay_Initialize is called.
	 */
	size_t clayArenaCapacityBytes = 0;

	/**
	 * @brief Default font family loaded during app startup.
	 *
	 * This family is registered first, so family id 0 is the fallback family used
	 * by default text rendering. If loading fails, FlowUi attempts built-in
	 * fallback font candidates.
	 *
	 * @see FlowUi::FontFamilyCreateInfo
	 */
	FontFamilyCreateInfo defaultFontFamily{};

	/**
	 * @brief Width and height in pixels of each font atlas page.
	 *
	 * The font manager uploads each loaded font face into square atlas layers of
	 * this size. The value must be non-zero and large enough for the source
	 * .arfont atlas or runtime-baked glyph atlas.
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
};

} // namespace FlowUi
