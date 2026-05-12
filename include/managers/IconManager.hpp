#pragma once

#include <cstdint>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <clay.h>

#include "FlowUi/BuildConfig.hpp"
#include "FlowUi/PublicStructs.hpp"
#include "Vulkan/Vk_Context.hpp"

struct plutosvg_document;
using plutosvg_document_t = plutosvg_document;
struct plutovg_surface;
using plutovg_surface_t = plutovg_surface;
struct VmaAllocation_T;
struct VkCommandPool_T;
struct VkSampler_T;

using VkCommandPool = VkCommandPool_T*;
using VkSampler = VkSampler_T*;

namespace FlowUi {

#if FLOWUI_INCLUDE_ICON_MANAGER
class App;
struct IconManagerConfig;

namespace detail {
struct IUiTextureRegistry;
} // namespace detail

/** @addtogroup flowui_icon_manager
 * @{
 */

/** @brief Registers SVG icons and resolves them to atlas-backed texture references. */
struct IconManager {
	/** @brief Register an SVG document from source text. */
	bool registerSvg(std::string_view key, std::string_view svgSource);
	/** @brief Register an SVG document from a file path. */
	bool registerFromFile(std::string_view key, std::string_view filePath);
	/** @brief Remove a registered SVG document by key. */
	bool remove(std::string_view key);
	/** @brief Return true if an SVG key is registered. */
	bool contains(std::string_view key) const;
	/** @brief Return a texture request reference for a registered icon key. */
	TextureRef textureRef(std::string_view key);

private:
	friend class App;

	void setRegistry(detail::IUiTextureRegistry* registry);
	void init(VulkanContext& vk, const IconManagerConfig& config);
	void prepareFrameTextures(
		Clay_RenderCommandArray& renderCommands,
		float uiToFramebufferScaleX,
		float uiToFramebufferScaleY);
	void destroy(VulkanContext& vk);

	struct DocumentRecord {
		plutosvg_document_t* document = nullptr;
		float intrinsicWidth = 0.0f;
		float intrinsicHeight = 0.0f;
	};

	struct SurfaceOwner {
		plutovg_surface_t* surface = nullptr;

		SurfaceOwner() = default;
		explicit SurfaceOwner(plutovg_surface_t* value) : surface(value) {}
		SurfaceOwner(const SurfaceOwner&) = delete;
		SurfaceOwner& operator=(const SurfaceOwner&) = delete;
		SurfaceOwner(SurfaceOwner&& other) noexcept : surface(other.surface) {
			other.surface = nullptr;
		}
		SurfaceOwner& operator=(SurfaceOwner&& other) noexcept;
		~SurfaceOwner();
	};

	struct TransientRasterResult {
		SurfaceOwner owner{};
		const uint8_t* rgbaPixels = nullptr;
		uint32_t width = 0u;
		uint32_t height = 0u;
		uint32_t strideBytes = 0u;
		uint32_t requestedWidth = 0u;
		uint32_t requestedHeight = 0u;
	};

	struct AtlasRect {
		uint32_t x = 0u;
		uint32_t y = 0u;
		uint32_t w = 0u;
		uint32_t h = 0u;
	};

	struct AtlasAllocation {
		uint32_t pageIndex = std::numeric_limits<uint32_t>::max();
		AtlasRect paddedRect{};
		AtlasRect contentRect{};
	};

	struct VariantKey {
		std::string nameKey{};
		uint32_t requestedWidth = 0u;
		uint32_t requestedHeight = 0u;

		bool operator==(const VariantKey& other) const noexcept {
			return
				requestedWidth == other.requestedWidth &&
				requestedHeight == other.requestedHeight &&
				nameKey == other.nameKey;
		}
	};

	struct VariantKeyHash {
		std::size_t operator()(const VariantKey& key) const noexcept;
	};

	struct VariantEntry {
		VariantKey key{};
		uint32_t pageIndex = std::numeric_limits<uint32_t>::max();
		uint32_t slotId = 0u;
		AtlasRect paddedRect{};
		AtlasRect contentRect{};
		float uv0x = 0.0f;
		float uv0y = 0.0f;
		float uv1x = 1.0f;
		float uv1y = 1.0f;
		uint32_t sourceWidth = 0u;
		uint32_t sourceHeight = 0u;
		uint32_t lastUsedFrame = 0u;
		bool referencedThisFrame = false;
	};

	struct AtlasPage {
		std::string namespacedKey{};
		uint32_t slotId = 0u;
		VkImage image = VK_NULL_HANDLE;
		VmaAllocation_T* allocation = nullptr;
		VkImageView view = VK_NULL_HANDLE;
		uint32_t width = 0u;
		uint32_t height = 0u;
		uint64_t usedArea = 0u;
		uint32_t lastUsedFrame = 0u;
		std::vector<AtlasRect> freeRects{};
	};

	TransientRasterResult rasterizeForAtlas(std::string_view key, uint32_t requestedWidth, uint32_t requestedHeight) const;
	VariantKey makeVariantKey(std::string_view key, uint32_t requestedWidth, uint32_t requestedHeight) const;
	void advanceFrameCounter();
	void touchVariant(VariantEntry& variant, uint32_t frameIndex);
	void resetVariantFrameMarks();
	static uint32_t frameAge(uint32_t currentFrame, uint32_t lastUsedFrame);

	AtlasPage createAtlasPage(uint32_t pageIndex) const;
	void destroyAtlasPage(AtlasPage& page);
	void destroyAtlasPages();
	bool tryAllocateInPage(
		AtlasPage& page,
		uint32_t contentWidth,
		uint32_t contentHeight,
		uint32_t padding,
		AtlasAllocation& outAllocation);
	void releasePageRegion(AtlasPage& page, const AtlasRect& paddedRect);
	void mergeFreeRects(AtlasPage& page);
	void recalcAtlasUvs(VariantEntry& variant, const AtlasPage& page) const;
	void uploadRasterToAtlasPage(
		const AtlasPage& page,
		const TransientRasterResult& raster,
		const AtlasRect& contentRect);
	bool tryAllocateAtlasRegion(
		uint32_t contentWidth,
		uint32_t contentHeight,
		AtlasAllocation& outAllocation);
	bool evictLeastRecentlyUsedVariant();
	VariantEntry* findBestCachedVariant(
		std::string_view nameKey,
		uint32_t requestedWidth,
		uint32_t requestedHeight);
	VariantEntry& ensureVariantForRequest(
		std::string_view nameKey,
		uint32_t requestedWidth,
		uint32_t requestedHeight);
	const std::string* findRequestedKeyByTextureId(uint32_t textureId) const;
	std::string makeRequestNamespacedKey(std::string_view key) const;

	detail::IUiTextureRegistry* registry_ = nullptr;
	VulkanContext* vk_ = nullptr;
	VkSampler atlasSampler_ = VK_NULL_HANDLE;
	VkCommandPool commandPool_ = VK_NULL_HANDLE;
	uint32_t atlasSize_ = 0u;
	uint32_t atlasPadding_ = 1u;
	uint32_t sizeReuseTolerance_ = 8u;
	uint32_t maxAtlasPages_ = 10u;
	uint32_t frameCounter_ = 0u;
	std::unordered_map<std::string, DocumentRecord> documentsByKey_;
	std::unordered_map<std::string, uint32_t> requestTextureIdByKey_;
	std::unordered_map<uint32_t, std::string> requestedKeyByTextureId_;
	std::unordered_map<VariantKey, VariantEntry, VariantKeyHash> variantsByKeyAndSize_;
	std::vector<AtlasPage> atlasPages_;
};

/** @} */
#else
class App;
struct IconManagerConfig;

namespace detail {
struct IUiTextureRegistry;
} // namespace detail

/** @addtogroup flowui_icon_manager
 * @{
 */

/** @brief Stub IconManager used when FlowUi is built with icon support disabled. */
struct IconManager {
	/** @brief Throws because icon support is disabled. */
	bool registerSvg(std::string_view, std::string_view) {
		throw std::runtime_error("FlowUi was built with FLOWUI_INCLUDE_ICON_MANAGER=OFF.");
	}

	/** @brief Throws because icon support is disabled. */
	bool registerFromFile(std::string_view, std::string_view) {
		throw std::runtime_error("FlowUi was built with FLOWUI_INCLUDE_ICON_MANAGER=OFF.");
	}

	/** @brief Throws because icon support is disabled. */
	bool remove(std::string_view) {
		throw std::runtime_error("FlowUi was built with FLOWUI_INCLUDE_ICON_MANAGER=OFF.");
	}

	/** @brief Throws because icon support is disabled. */
	bool contains(std::string_view) const {
		throw std::runtime_error("FlowUi was built with FLOWUI_INCLUDE_ICON_MANAGER=OFF.");
	}

	/** @brief Throws because icon support is disabled. */
	TextureRef textureRef(std::string_view) {
		throw std::runtime_error("FlowUi was built with FLOWUI_INCLUDE_ICON_MANAGER=OFF.");
	}

private:
	friend class App;

	void setRegistry(detail::IUiTextureRegistry*) {}

	void init(VulkanContext&, const IconManagerConfig&) {
		throw std::runtime_error("FlowUi was built with FLOWUI_INCLUDE_ICON_MANAGER=OFF.");
	}

	void prepareFrameTextures(Clay_RenderCommandArray&, float, float) {}

	void destroy(VulkanContext&) {}
};

/** @} */
#endif

} // namespace FlowUi
