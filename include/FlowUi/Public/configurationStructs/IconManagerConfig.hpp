#pragma once

#include <cstdint>

namespace FlowUi {

/**
 * SVG raster cache and atlas settings; used when FLOWUI_INCLUDE_ICON_MANAGER is enabled.
 * Icons are rasterized on demand and retained in atlas pages for reuse.
 */
struct IconManagerConfig {
	/** @brief Width and height in pixels of each icon atlas page. */
	uint32_t atlasSize = 2048;

	/**
	 * Maximum atlas pages; raising this limit retains more cached variants and uses more GPU memory.
	 */
	uint32_t maxAtlasPages = 10;

	/**
	 * Pixel tolerance for reusing an icon raster near the requested width and height.
	 * Larger candidates are preferred; no matching candidate triggers exact-size rasterization.
	 */
	uint32_t sizeBucketStep = 8;

	/** @brief Empty pixel padding reserved around each atlas allocation. */
	uint32_t atlasPadding = 1;
};

} // namespace FlowUi
