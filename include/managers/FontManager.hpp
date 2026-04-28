#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>

struct VmaAllocation_T;
struct VulkanContext;

namespace FlowUi {
class App;
}

/** @addtogroup flowui_font_manager
 * @{
 */

/** @brief Loads baked fonts and owns the font atlas used by text rendering. */
struct FontManager {
	/** @brief Initial number of font atlas array layers. */
	static constexpr uint32_t kInitialAtlasLayerCapacity = 32;
	/** @brief Number of atlas layers added when the atlas grows. */
	static constexpr uint32_t kAtlasLayerGrowthStep = 32;

	/** @brief Baked glyph metrics and atlas coordinates. */
	struct GlyphData {
		/** @brief Unicode codepoint represented by the glyph. */
		uint32_t codepoint = 0;
		/** @brief Source atlas image index. */
		uint32_t sourceImageIndex = 0;
		/** @brief Brief goes here. */
		float planeLeft = 0.0f;
		/** @brief Brief goes here. */
		float planeBottom = 0.0f;
		/** @brief Brief goes here. */
		float planeRight = 0.0f;
		/** @brief Brief goes here. */
		float planeTop = 0.0f;
		/** @brief Brief goes here. */
		float imageLeft = 0.0f;
		/** @brief Brief goes here. */
		float imageBottom = 0.0f;
		/** @brief Brief goes here. */
		float imageRight = 0.0f;
		/** @brief Brief goes here. */
		float imageTop = 0.0f;
		/** @brief Horizontal glyph advance. */
		float advanceX = 0.0f;
		/** @brief Vertical glyph advance. */
		float advanceY = 0.0f;
	};

	/** @brief One baked variant of a font face. */
	struct FontVariantData {
		/** @brief Brief goes here. */
		uint32_t flags = 0;
		/** @brief Font weight. */
		uint32_t weight = 0;
		/** @brief Glyph index used as fallback. */
		uint32_t fallbackGlyphIndex = 0;
		/** @brief Baked font size in pixels. */
		float fontSizePx = 0.0f;
		/** @brief MSDF distance range. */
		float distanceRange = 0.0f;
		/** @brief EM size from font metadata. */
		float emSize = 0.0f;
		/** @brief Font ascender metric. */
		float ascender = 0.0f;
		/** @brief Font descender metric. */
		float descender = 0.0f;
		/** @brief Font line height metric. */
		float lineHeight = 0.0f;
		/** @brief Underline Y metric. */
		float underlineY = 0.0f;
		/** @brief Underline thickness metric. */
		float underlineThickness = 0.0f;
		/** @brief MSDF distance range midpoint. */
		float distanceRangeMiddle = 0.0f;
		/** @brief Variant name. */
		std::string name;
		/** @brief Raw metadata associated with the variant. */
		std::string metadata;
		/** @brief Glyph table. */
		std::vector<GlyphData> glyphs;
		/** @brief Unicode codepoint to glyph index lookup. */
		std::unordered_map<uint32_t, uint32_t> unicodeToGlyphIndex;
		/** @brief Kerning pair lookup table. */
		std::unordered_map<uint64_t, float> kerningPairs;

		/** @brief Pack two codepoints into the kerning lookup key. */
		static uint64_t kerningKey(uint32_t leftCodepoint, uint32_t rightCodepoint) {
			return (static_cast<uint64_t>(leftCodepoint) << 32u) | static_cast<uint64_t>(rightCodepoint);
		}

		/** @brief Return kerning advance for a codepoint pair. */
		float kerningAdvance(uint32_t leftCodepoint, uint32_t rightCodepoint) const {
			const auto it = kerningPairs.find(kerningKey(leftCodepoint, rightCodepoint));
			return (it != kerningPairs.end()) ? it->second : 0.0f;
		}
	};

	/** @brief Loaded font face and its baked variants. */
	struct FontFaceData {
		/** @brief FlowUi font id. */
		int id = -1;
		/** @brief Font face name. */
		std::string name;
		/** @brief Source file path. */
		std::filesystem::path sourcePath;
		/** @brief Atlas layer used by this font face. */
		uint32_t atlasLayer = 0;
		/** @brief Atlas width in pixels. */
		uint32_t atlasWidth = 0;
		/** @brief Atlas height in pixels. */
		uint32_t atlasHeight = 0;
		/** @brief Brief goes here. */
		uint32_t imageType = 0;
		/** @brief Raw metadata associated with the face. */
		std::string metadata;
		/** @brief Default variant index. */
		uint32_t defaultVariantIndex = 0;
		/** @brief Baked variants for this face. */
		std::vector<FontVariantData> variants;

		/** @brief Return the default variant, or nullptr if unavailable. */
		const FontVariantData* defaultVariant() const {
			if (variants.empty() || defaultVariantIndex >= variants.size()) {
				return nullptr;
			}
			return &variants[defaultVariantIndex];
		}
	};

	/** @brief Vulkan resources for the font atlas array. */
	struct AtlasArrayResource {
		/** @brief Vulkan image handle. */
		VkImage image = VK_NULL_HANDLE;
		/** @brief VMA allocation handle. */
		VmaAllocation_T* allocation = nullptr;
		/** @brief Vulkan image view handle. */
		VkImageView view = VK_NULL_HANDLE;
		/** @brief Vulkan sampler handle. */
		VkSampler sampler = VK_NULL_HANDLE;
		/** @brief Atlas width in pixels. */
		uint32_t width = 0;
		/** @brief Atlas height in pixels. */
		uint32_t height = 0;
		/** @brief Number of atlas layers currently used. */
		uint32_t layersUsed = 0;
		/** @brief Number of atlas layers currently allocated. */
		uint32_t layersCapacity = 0;
		/** @brief Revision incremented when atlas binding data changes. */
		uint32_t bindingRevision = 0;
	};

	/** @brief Load a font file at the requested pixel size. */
	int loadFont(std::string_view path, float px);
	/** @brief Register a baked `.arfont` file. */
	int registerBakedFont(std::string_view arfontPath, std::string_view requestedName = {});
	/** @brief Find a font id by registered font name. */
	int getFontId(std::string_view fontName) const;
	/** @brief Return font data by id, or nullptr if not found. */
	const FontFaceData* getFontById(int fontId) const;
	/** @brief Return font data by name, or nullptr if not found. */
	const FontFaceData* getFontByName(std::string_view fontName) const;
	/** @brief Return the active font atlas Vulkan resource. */
	const AtlasArrayResource& getAtlasResource() const { return atlas_; }

private:
	friend class FlowUi::App;

	void init(VulkanContext& vk, uint32_t atlasSize);
	void destroy(VulkanContext& vk);

	VulkanContext* vk_ = nullptr;
	AtlasArrayResource atlas_{};
	VkCommandPool uploadCommandPool_ = VK_NULL_HANDLE;
	uint32_t atlasSizeHint_ = 0;
	uint32_t nextFontId_ = 0;
	std::vector<FontFaceData> fonts_;
	std::unordered_map<int, size_t> fontIndexById_;
	std::unordered_map<std::string, int> fontIdByName_;
};

/** @} */
