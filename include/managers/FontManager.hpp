#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Vulkan/Vk_Context.hpp"

struct VmaAllocation_T;

struct FontManager {
	static constexpr uint32_t kInitialAtlasLayerCapacity = 32;
	static constexpr uint32_t kAtlasLayerGrowthStep = 32;

	struct GlyphData {
		uint32_t codepoint = 0;
		uint32_t sourceImageIndex = 0;
		float planeLeft = 0.0f;
		float planeBottom = 0.0f;
		float planeRight = 0.0f;
		float planeTop = 0.0f;
		float imageLeft = 0.0f;
		float imageBottom = 0.0f;
		float imageRight = 0.0f;
		float imageTop = 0.0f;
		float advanceX = 0.0f;
		float advanceY = 0.0f;
	};

	struct FontVariantData {
		uint32_t flags = 0;
		uint32_t weight = 0;
		uint32_t fallbackGlyphIndex = 0;
		float fontSizePx = 0.0f;
		float distanceRange = 0.0f;
		float emSize = 0.0f;
		float ascender = 0.0f;
		float descender = 0.0f;
		float lineHeight = 0.0f;
		float underlineY = 0.0f;
		float underlineThickness = 0.0f;
		float distanceRangeMiddle = 0.0f;
		std::string name;
		std::string metadata;
		std::vector<GlyphData> glyphs;
		std::unordered_map<uint32_t, uint32_t> unicodeToGlyphIndex;
		std::unordered_map<uint64_t, float> kerningPairs;

		static uint64_t kerningKey(uint32_t leftCodepoint, uint32_t rightCodepoint) {
			return (static_cast<uint64_t>(leftCodepoint) << 32u) | static_cast<uint64_t>(rightCodepoint);
		}

		float kerningAdvance(uint32_t leftCodepoint, uint32_t rightCodepoint) const {
			const auto it = kerningPairs.find(kerningKey(leftCodepoint, rightCodepoint));
			return (it != kerningPairs.end()) ? it->second : 0.0f;
		}
	};

	struct FontFaceData {
		int id = -1;
		std::string name;
		std::filesystem::path sourcePath;
		uint32_t atlasLayer = 0;
		uint32_t atlasWidth = 0;
		uint32_t atlasHeight = 0;
		uint32_t imageType = 0;
		std::string metadata;
		uint32_t defaultVariantIndex = 0;
		std::vector<FontVariantData> variants;

		const FontVariantData* defaultVariant() const {
			if (variants.empty() || defaultVariantIndex >= variants.size()) {
				return nullptr;
			}
			return &variants[defaultVariantIndex];
		}
	};

	struct AtlasArrayResource {
		VkImage image = VK_NULL_HANDLE;
		VmaAllocation_T* allocation = nullptr;
		VkImageView view = VK_NULL_HANDLE;
		VkSampler sampler = VK_NULL_HANDLE;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t layersUsed = 0;
		uint32_t layersCapacity = 0;
		uint32_t bindingRevision = 0;
	};

	void init(VulkanContext& vk, uint32_t atlasSize);
	int loadFont(VulkanContext& vk, std::string_view path, float px);
	int registerOfflineBakedFont(VulkanContext& vk, std::string_view arfontPath, std::string_view requestedName = {});
	int findFontByName(std::string_view fontName) const;
	const FontFaceData* getFontById(int fontId) const;
	const FontFaceData* getFontByName(std::string_view fontName) const;
	const AtlasArrayResource& atlasResource() const { return atlas_; }
	void destroy(VulkanContext& vk);

private:
	AtlasArrayResource atlas_{};
	VkCommandPool uploadCommandPool_ = VK_NULL_HANDLE;
	uint32_t atlasSizeHint_ = 0;
	uint32_t nextFontId_ = 0;
	std::vector<FontFaceData> fonts_;
	std::unordered_map<int, size_t> fontIndexById_;
	std::unordered_map<std::string, int> fontIdByName_;
};
