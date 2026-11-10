#pragma once

#include <cstdint>

namespace FlowUi {

/**
 * @brief Configures SVG icon rasterization, caching, and atlas storage.
 *
 * Icons are rasterized on demand at the size required by the current frame,
 * then cached in atlas pages for reuse by later frames.
 */
struct IconManagerConfig {
	/** @brief Width and height in pixels of each icon atlas page. */
	uint32_t atlasSize = 2048;

	/**
	 * @brief Maximum number of icon atlas pages the manager may allocate.
	 *
	 * More pages allow more cached icon variants to stay resident, at the cost of
	 * additional GPU memory.
	 */
	uint32_t maxAtlasPages = 10;

	/**
	 * @brief Pixel tolerance for reusing cached icon raster sizes.
	 *
	 * FlowUi first looks for an existing cached raster of the same icon whose
	 * width and height are within this many pixels of the requested size. Larger
	 * cached rasters are preferred over smaller ones to preserve quality. If no
	 * cached raster is close enough, the SVG is rasterized at the exact requested
	 * size.
	 */
	uint32_t sizeBucketStep = 8;

	/** @brief Empty pixel padding reserved around each atlas allocation. */
	uint32_t atlasPadding = 1;
};

} // namespace FlowUi
