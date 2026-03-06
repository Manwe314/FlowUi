#include "Ui/Vk_UiRenderer.hpp"
#include "managers/FontManager.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "vk_mem_alloc.h"

namespace {

struct UiQuadVertex {
	float x = 0.0f;
	float y = 0.0f;
	float u = 0.0f;
	float v = 0.0f;
};

struct GlyphQuad {
	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;
	float u0 = 0.0f;
	float v0 = 0.0f;
	float u1 = 1.0f;
	float v1 = 1.0f;
};

struct UiPushConstants {
	float viewportW = 0.0f;
	float viewportH = 0.0f;
	uint32_t instanceBaseIndex = 0;
	uint32_t _pad = 0;
};

constexpr VkDeviceSize kInitialInstanceBufferBytes = 1024u * 1024u;
constexpr uint32_t kDefaultMaxUiImageDescriptors = 256;

constexpr const char* kUiVertexShaderFile = "flowui_ui_quad.vert.spv";
constexpr const char* kUiSolidFragmentShaderFile = "flowui_ui_solid.frag.spv";
constexpr const char* kUiMsdfFragmentShaderFile = "flowui_ui_msdf.frag.spv";
constexpr const char* kUiTexturedFragmentShaderFile = "flowui_ui_textured.frag.spv";

static void vkCheck(VkResult result, const char* message) {
	if (result != VK_SUCCESS) {
		throw std::runtime_error(message);
	}
}

static std::vector<char> readFile(const std::string& path) {
	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (!file) {
		throw std::runtime_error(std::string("Failed to open shader file: ") + path);
	}

	const std::streamsize size = file.tellg();
	if (size <= 0) {
		throw std::runtime_error(std::string("Shader file is empty: ") + path);
	}

	std::vector<char> buffer(static_cast<size_t>(size));
	file.seekg(0, std::ios::beg);
	file.read(buffer.data(), size);
	if (!file) {
		throw std::runtime_error(std::string("Failed to read shader file: ") + path);
	}
	if ((buffer.size() % 4) != 0) {
		throw std::runtime_error(std::string("Shader file size is not SPIR-V aligned: ") + path);
	}
	return buffer;
}

static std::vector<char> readShaderFile(const char* fileName) {
	const std::string relativePath = std::string("shaders/") + fileName;
#if defined(FLOWUI_SHADER_OUTPUT_DIR)
	const std::string configuredPath = std::string(FLOWUI_SHADER_OUTPUT_DIR) + "/" + fileName;
	if (std::filesystem::exists(configuredPath)) {
		return readFile(configuredPath);
	}
#endif
	if (std::filesystem::exists(relativePath)) {
		return readFile(relativePath);
	}

#if defined(FLOWUI_SHADER_OUTPUT_DIR)
	throw std::runtime_error(
		"Failed to open shader file: " + std::string(fileName) +
		" (looked in '" + configuredPath + "' and '" + relativePath + "')");
#else
	throw std::runtime_error(
		"Failed to open shader file: " + std::string(fileName) +
		" (looked in '" + relativePath + "')");
#endif
}

static VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule module = VK_NULL_HANDLE;
	vkCheck(vkCreateShaderModule(device, &createInfo, nullptr, &module), "Failed to create shader module.");
	return module;
}

static RectF Intersect(const RectF& a, const RectF& b) {
	const float x0 = std::max(a.x, b.x);
	const float y0 = std::max(a.y, b.y);
	const float x1 = std::min(a.x + a.w, b.x + b.w);
	const float y1 = std::min(a.y + a.h, b.y + b.h);
	return RectF{
		x0,
		y0,
		std::max(0.0f, x1 - x0),
		std::max(0.0f, y1 - y0),
	};
}

static bool RectEqual(const RectF& a, const RectF& b) {
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static VkRect2D ToVkRect2D(const RectF& r, int framebufferWidth, int framebufferHeight) {
	int32_t x0 = static_cast<int32_t>(std::floor(r.x));
	int32_t y0 = static_cast<int32_t>(std::floor(r.y));
	int32_t x1 = static_cast<int32_t>(std::ceil(r.x + r.w));
	int32_t y1 = static_cast<int32_t>(std::ceil(r.y + r.h));

	x0 = std::max<int32_t>(0, std::min<int32_t>(x0, framebufferWidth));
	y0 = std::max<int32_t>(0, std::min<int32_t>(y0, framebufferHeight));
	x1 = std::max<int32_t>(0, std::min<int32_t>(x1, framebufferWidth));
	y1 = std::max<int32_t>(0, std::min<int32_t>(y1, framebufferHeight));

	VkRect2D rect{};
	rect.offset = { x0, y0 };
	rect.extent = {
		static_cast<uint32_t>(std::max(0, x1 - x0)),
		static_cast<uint32_t>(std::max(0, y1 - y0)),
	};
	return rect;
}

static uint32_t PackRGBA8(const Clay_Color& color) {
	const auto pack = [](float v) -> uint32_t {
		const float clamped = std::clamp(v, 0.0f, 255.0f);
		return static_cast<uint32_t>(clamped + 0.5f);
	};
	return pack(color.r) | (pack(color.g) << 8u) | (pack(color.b) << 16u) | (pack(color.a) << 24u);
}

static uint32_t ResolveUiTextureIndex(const Clay_RenderCommand& command) {
	// Placeholder: app texture lifetime/version tracking is not connected yet.
	const auto* handle = reinterpret_cast<const UiTextureHandle*>(command.renderData.image.imageData);
	return handle ? handle->texIndex : 0u;
}

static bool DecodeNextUtf8Codepoint(const Clay_StringSlice& stringSlice, int& byteOffset, uint32_t& outCodepoint) {
	if (!stringSlice.chars || byteOffset >= stringSlice.length) {
		return false;
	}

	const auto* bytes = reinterpret_cast<const uint8_t*>(stringSlice.chars);
	const uint8_t first = bytes[byteOffset];

	if (first < 0x80u) {
		outCodepoint = first;
		byteOffset += 1;
		return true;
	}

	auto continuation = [&](int index) -> uint8_t {
		return (index < stringSlice.length) ? bytes[index] : 0u;
	};

	if ((first & 0xE0u) == 0xC0u && byteOffset + 1 < stringSlice.length) {
		const uint8_t c1 = continuation(byteOffset + 1);
		if ((c1 & 0xC0u) == 0x80u) {
			outCodepoint = ((first & 0x1Fu) << 6u) | (c1 & 0x3Fu);
			byteOffset += 2;
			return true;
		}
	} else if ((first & 0xF0u) == 0xE0u && byteOffset + 2 < stringSlice.length) {
		const uint8_t c1 = continuation(byteOffset + 1);
		const uint8_t c2 = continuation(byteOffset + 2);
		if ((c1 & 0xC0u) == 0x80u && (c2 & 0xC0u) == 0x80u) {
			outCodepoint = ((first & 0x0Fu) << 12u) | ((c1 & 0x3Fu) << 6u) | (c2 & 0x3Fu);
			byteOffset += 3;
			return true;
		}
	} else if ((first & 0xF8u) == 0xF0u && byteOffset + 3 < stringSlice.length) {
		const uint8_t c1 = continuation(byteOffset + 1);
		const uint8_t c2 = continuation(byteOffset + 2);
		const uint8_t c3 = continuation(byteOffset + 3);
		if ((c1 & 0xC0u) == 0x80u && (c2 & 0xC0u) == 0x80u && (c3 & 0xC0u) == 0x80u) {
			outCodepoint = ((first & 0x07u) << 18u) | ((c1 & 0x3Fu) << 12u) | ((c2 & 0x3Fu) << 6u) | (c3 & 0x3Fu);
			byteOffset += 4;
			return true;
		}
	}

	// Replace malformed UTF-8 byte and continue.
	outCodepoint = 0xFFFDu;
	byteOffset += 1;
	return true;
}

static const FontManager::FontFaceData* ResolveFontFace(const FontManager* fontManager, uint16_t fontId) {
	if (!fontManager) {
		return nullptr;
	}

	const FontManager::FontFaceData* face = fontManager->getFontById(static_cast<int>(fontId));
	if (!face) {
		face = fontManager->getFontById(0);
	}
	return face;
}

static bool LayoutMsdfTextToGlyphs(
	const Clay_TextRenderData& text,
	const Clay_BoundingBox& bounds,
	const FontManager* fontManager,
	float pointsToPixelsScale,
	std::vector<GlyphQuad>& outGlyphs,
	uint32_t& outAtlasLayer,
	float& outDistanceRangePx)
{
	outGlyphs.clear();
	outAtlasLayer = 0u;
	outDistanceRangePx = 2.0f;

	const FontManager::FontFaceData* fontFace = ResolveFontFace(fontManager, text.fontId);
	if (!fontFace) {
		return false;
	}

	const FontManager::FontVariantData* variant = fontFace->defaultVariant();
	if (!variant || variant->glyphs.empty() || fontFace->atlasWidth == 0 || fontFace->atlasHeight == 0) {
		return false;
	}
	if (variant->distanceRange > 0.0f) {
		outDistanceRangePx = variant->distanceRange;
	}

	float emPixels = variant->fontSizePx;
	if (text.fontSize > 0) {
		emPixels = static_cast<float>(text.fontSize) * pointsToPixelsScale;
	}
	if (emPixels <= 0.0f) {
		return false;
	}

	const float emToPixels = emPixels / std::max(variant->emSize, 1.0e-6f);
	const float baselineY = bounds.y + variant->ascender * emToPixels;
	const float invAtlasWidth = 1.0f / static_cast<float>(fontFace->atlasWidth);
	const float invAtlasHeight = 1.0f / static_cast<float>(fontFace->atlasHeight);
	float penX = bounds.x;
	const float letterSpacingPx = static_cast<float>(text.letterSpacing);

	uint32_t previousCodepoint = 0;
	bool hasPreviousCodepoint = false;
	int byteOffset = 0;

	while (byteOffset < text.stringContents.length) {
		uint32_t codepoint = 0;
		if (!DecodeNextUtf8Codepoint(text.stringContents, byteOffset, codepoint)) {
			break;
		}

		if (codepoint == '\n') {
			hasPreviousCodepoint = false;
			penX = bounds.x;
			continue;
		}

		if (hasPreviousCodepoint) {
			penX += variant->kerningAdvance(previousCodepoint, codepoint) * emToPixels;
		}

		// Treat spacing codepoints as advance-only to avoid rendering fallback glyphs
		// when the font doesn't provide explicit whitespace glyph mappings.
		if (codepoint == ' ' || codepoint == '\t' || codepoint == 0x00A0u) {
			float whitespaceAdvance = std::max(variant->emSize * emToPixels * 0.33f, 1.0f);
			const auto spaceGlyphIt = variant->unicodeToGlyphIndex.find(' ');
			if (spaceGlyphIt != variant->unicodeToGlyphIndex.end() && spaceGlyphIt->second < variant->glyphs.size()) {
				whitespaceAdvance = variant->glyphs[spaceGlyphIt->second].advanceX * emToPixels;
			}
			if (codepoint == '\t') {
				whitespaceAdvance *= 4.0f;
			}
			penX += whitespaceAdvance + letterSpacingPx;
			previousCodepoint = codepoint;
			hasPreviousCodepoint = true;
			continue;
		}

		uint32_t glyphIndex = variant->fallbackGlyphIndex;
		const auto glyphIt = variant->unicodeToGlyphIndex.find(codepoint);
		if (glyphIt != variant->unicodeToGlyphIndex.end()) {
			glyphIndex = glyphIt->second;
		}
		if (glyphIndex >= variant->glyphs.size()) {
			if (variant->glyphs.empty()) {
				hasPreviousCodepoint = false;
				continue;
			}
			glyphIndex = 0;
		}

		const FontManager::GlyphData& glyph = variant->glyphs[glyphIndex];
		const float x0 = penX + glyph.planeLeft * emToPixels;
		const float x1 = penX + glyph.planeRight * emToPixels;
		const float y0 = baselineY - glyph.planeTop * emToPixels;
		const float y1 = baselineY - glyph.planeBottom * emToPixels;
		const float width = x1 - x0;
		const float height = y1 - y0;

		if (width > 0.0f && height > 0.0f) {
			GlyphQuad quad{};
			quad.x = x0;
			quad.y = y0;
			quad.w = width;
			quad.h = height;
			quad.u0 = std::clamp(glyph.imageLeft * invAtlasWidth, 0.0f, 1.0f);
			quad.u1 = std::clamp(glyph.imageRight * invAtlasWidth, 0.0f, 1.0f);
			quad.v0 = std::clamp(1.0f - glyph.imageTop * invAtlasHeight, 0.0f, 1.0f);
			quad.v1 = std::clamp(1.0f - glyph.imageBottom * invAtlasHeight, 0.0f, 1.0f);
			outGlyphs.push_back(quad);
		}

		penX += glyph.advanceX * emToPixels + letterSpacingPx;
		previousCodepoint = codepoint;
		hasPreviousCodepoint = true;
	}

	outAtlasLayer = fontFace->atlasLayer;
	return true;
}

static UiType PickType(const Clay_RenderCommand& command) {
	switch (command.commandType) {
		case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
		case CLAY_RENDER_COMMAND_TYPE_BORDER:
			return UiType::Solid;
		case CLAY_RENDER_COMMAND_TYPE_TEXT:
			return UiType::Msdf;
		case CLAY_RENDER_COMMAND_TYPE_IMAGE:
		case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
			return UiType::Textured;
		default:
			return UiType::Solid;
	}
}

static void EmitSolidRect(const Clay_RenderCommand& command, std::vector<UiInstance>& instances) {
	const Clay_BoundingBox& bounds = command.boundingBox;
	UiInstance inst{};
	inst.type = static_cast<uint32_t>(UiType::Solid);
	inst.x = bounds.x;
	inst.y = bounds.y;
	inst.w = bounds.width;
	inst.h = bounds.height;
	inst.colorRGBA = PackRGBA8(command.renderData.rectangle.backgroundColor);
	inst.r0 = command.renderData.rectangle.cornerRadius.topLeft;
	inst.r1 = command.renderData.rectangle.cornerRadius.topRight;
	inst.r2 = command.renderData.rectangle.cornerRadius.bottomRight;
	inst.r3 = command.renderData.rectangle.cornerRadius.bottomLeft;
	inst.solidMode = 0u;
	instances.push_back(inst);
}

static void EmitSolidBorder(const Clay_RenderCommand& command, std::vector<UiInstance>& instances) {
	const Clay_BoundingBox& bounds = command.boundingBox;
	UiInstance inst{};
	inst.type = static_cast<uint32_t>(UiType::Solid);
	inst.x = bounds.x;
	inst.y = bounds.y;
	inst.w = bounds.width;
	inst.h = bounds.height;
	inst.colorRGBA = PackRGBA8(command.renderData.border.color);
	inst.r0 = command.renderData.border.cornerRadius.topLeft;
	inst.r1 = command.renderData.border.cornerRadius.topRight;
	inst.r2 = command.renderData.border.cornerRadius.bottomRight;
	inst.r3 = command.renderData.border.cornerRadius.bottomLeft;
	inst.borderL = static_cast<float>(command.renderData.border.width.left);
	inst.borderT = static_cast<float>(command.renderData.border.width.top);
	inst.borderR = static_cast<float>(command.renderData.border.width.right);
	inst.borderB = static_cast<float>(command.renderData.border.width.bottom);
	inst.solidMode = 1u;
	instances.push_back(inst);
}

static void EmitTextMsdf(
	const Clay_RenderCommand& command,
	const FontManager* fontManager,
	float pointsToPixelsScale,
	std::vector<UiInstance>& instances)
{
	const Clay_BoundingBox& bounds = command.boundingBox;
	const Clay_TextRenderData& textData = command.renderData.text;

	std::vector<GlyphQuad> glyphs;
	glyphs.reserve(static_cast<size_t>(std::max(1, textData.stringContents.length)));
	uint32_t atlasLayer = 0u;
	float distanceRangePx = 2.0f;
	const bool hasLayout = LayoutMsdfTextToGlyphs(
		textData,
		bounds,
		fontManager,
		pointsToPixelsScale,
		glyphs,
		atlasLayer,
		distanceRangePx);
	if (!hasLayout) {
		glyphs.push_back(GlyphQuad{
			bounds.x,
			bounds.y,
			bounds.width,
			bounds.height,
			0.0f,
			0.0f,
			1.0f,
			1.0f,
		});
	}

	const uint32_t color = PackRGBA8(textData.textColor);

	for (const GlyphQuad& glyph : glyphs) {
		UiInstance inst{};
		inst.type = static_cast<uint32_t>(UiType::Msdf);
		inst.x = glyph.x;
		inst.y = glyph.y;
		inst.w = glyph.w;
		inst.h = glyph.h;
		inst.uv0x = glyph.u0;
		inst.uv0y = glyph.v0;
		inst.uv1x = glyph.u1;
		inst.uv1y = glyph.v1;
		inst.colorRGBA = color;
		inst.atlasLayer = atlasLayer;
		inst.r0 = distanceRangePx;
		instances.push_back(inst);
	}
}

static void EmitTexturedImage(const Clay_RenderCommand& command, std::vector<UiInstance>& instances) {
	const Clay_BoundingBox& bounds = command.boundingBox;
	UiInstance inst{};
	inst.type = static_cast<uint32_t>(UiType::Textured);
	inst.x = bounds.x;
	inst.y = bounds.y;
	inst.w = bounds.width;
	inst.h = bounds.height;
	inst.uv0x = 0.0f;
	inst.uv0y = 0.0f;
	inst.uv1x = 1.0f;
	inst.uv1y = 1.0f;
	inst.colorRGBA = PackRGBA8(command.renderData.image.backgroundColor);
	inst.texIndex = ResolveUiTextureIndex(command);
	inst.r0 = command.renderData.image.cornerRadius.topLeft;
	inst.r1 = command.renderData.image.cornerRadius.topRight;
	inst.r2 = command.renderData.image.cornerRadius.bottomRight;
	inst.r3 = command.renderData.image.cornerRadius.bottomLeft;
	instances.push_back(inst);
}

static void BuildInstancesAndRunsFromClay(
	const Clay_RenderCommandArray& commands,
	VkExtent2D extent,
	const FontManager* fontManager,
	float pointsToPixelsScale,
	std::vector<UiInstance>& outInstances,
	std::vector<UiRun>& outRuns) {
	outInstances.clear();
	outRuns.clear();

	const RectF fullScissor{ 0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height) };
	std::vector<RectF> scissorStack;
	scissorStack.push_back(fullScissor);
	RectF currentScissor = fullScissor;

	bool runBarrier = false;

	auto closeActiveRun = [&]() {
		if (outRuns.empty()) {
			return;
		}
		UiRun& run = outRuns.back();
		run.instanceCount = static_cast<uint32_t>(outInstances.size()) - run.firstInstance;
		if (run.instanceCount == 0) {
			outRuns.pop_back();
		}
	};

	auto beginRunIfNeeded = [&](UiType type, const RectF& scissor) {
		if (outRuns.empty() || runBarrier) {
			closeActiveRun();
			outRuns.emplace_back(UiRun{ type, scissor, static_cast<uint32_t>(outInstances.size()), 0u });
			runBarrier = false;
			return;
		}

		const UiRun& current = outRuns.back();
		if (current.type != type || !RectEqual(current.scissor, scissor)) {
			closeActiveRun();
			outRuns.emplace_back(UiRun{ type, scissor, static_cast<uint32_t>(outInstances.size()), 0u });
		}
	};

	for (int32_t i = 0; i < commands.length; ++i) {
		const Clay_RenderCommand& command = commands.internalArray[i];

		if (command.commandType == CLAY_RENDER_COMMAND_TYPE_NONE) {
			continue;
		}
		if (command.commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_START) {
			const RectF clip{
				command.boundingBox.x,
				command.boundingBox.y,
				command.boundingBox.width,
				command.boundingBox.height,
			};
			scissorStack.push_back(Intersect(scissorStack.back(), clip));
			currentScissor = scissorStack.back();
			runBarrier = true;
			continue;
		}
		if (command.commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_END) {
			if (scissorStack.size() > 1) {
				scissorStack.pop_back();
			}
			currentScissor = scissorStack.back();
			runBarrier = true;
			continue;
		}
		if (command.commandType == CLAY_RENDER_COMMAND_TYPE_CUSTOM) {
			// Placeholder: custom command execution hook should run here in strict command order.
			closeActiveRun();
			runBarrier = true;
			continue;
		}

		const UiType type = PickType(command);
		beginRunIfNeeded(type, currentScissor);

		switch (command.commandType) {
			case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
				EmitSolidRect(command, outInstances);
				break;
			case CLAY_RENDER_COMMAND_TYPE_BORDER:
				EmitSolidBorder(command, outInstances);
				break;
			case CLAY_RENDER_COMMAND_TYPE_TEXT:
				EmitTextMsdf(command, fontManager, pointsToPixelsScale, outInstances);
				break;
			case CLAY_RENDER_COMMAND_TYPE_IMAGE:
				EmitTexturedImage(command, outInstances);
				break;
			default:
				break;
		}
	}

	closeActiveRun();
}

static void DestroyBuffer(VulkanContext& vk, VulkanUiRenderer::AllocatedBuffer& buffer) {
	if (buffer.buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(vk.allocator, buffer.buffer, buffer.allocation);
	}
	buffer.buffer = VK_NULL_HANDLE;
	buffer.allocation = nullptr;
	buffer.mapped = nullptr;
	buffer.size = 0;
}

static void CreateMappedBuffer(
	VulkanContext& vk,
	VkDeviceSize size,
	VkBufferUsageFlags usage,
	VulkanUiRenderer::AllocatedBuffer& outBuffer) {
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocationCreateInfo{};
	allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	allocationCreateInfo.flags =
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
		VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VmaAllocationInfo allocationInfo{};
	vkCheck(
		vmaCreateBuffer(
			vk.allocator,
			&bufferInfo,
			&allocationCreateInfo,
			&outBuffer.buffer,
			&outBuffer.allocation,
			&allocationInfo),
		"Failed to create mapped UI buffer.");

	outBuffer.size = size;
	outBuffer.mapped = allocationInfo.pMappedData;
	if (!outBuffer.mapped) {
		vkCheck(vmaMapMemory(vk.allocator, outBuffer.allocation, &outBuffer.mapped), "Failed to map UI buffer.");
	}
}

static void UploadBytesToMappedBuffer(
	VulkanContext& vk,
	const VulkanUiRenderer::AllocatedBuffer& buffer,
	const void* data,
	VkDeviceSize size) {
	if (size == 0) {
		return;
	}
	if (buffer.buffer == VK_NULL_HANDLE || buffer.mapped == nullptr || size > buffer.size) {
		throw std::runtime_error("UI upload buffer is invalid or too small.");
	}
	std::memcpy(buffer.mapped, data, static_cast<size_t>(size));
	vkCheck(vmaFlushAllocation(vk.allocator, buffer.allocation, 0, size), "Failed to flush UI upload buffer.");
}

static void DestroyImage(VulkanContext& vk, VulkanUiRenderer::AllocatedImage& image) {
	if (image.view != VK_NULL_HANDLE) {
		vkDestroyImageView(vk.device, image.view, nullptr);
		image.view = VK_NULL_HANDLE;
	}
	if (image.image != VK_NULL_HANDLE) {
		vmaDestroyImage(vk.allocator, image.image, image.allocation);
	}
	image.image = VK_NULL_HANDLE;
	image.allocation = nullptr;
}

static void TransitionImageToShaderRead(VulkanContext& vk, VkImage image, uint32_t layers) {
	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkCommandPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolCreateInfo.queueFamilyIndex = vk.graphicsQFamily;
	poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	vkCheck(vkCreateCommandPool(vk.device, &poolCreateInfo, nullptr, &commandPool), "Failed to create temp command pool.");

	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;
	vkCheck(vkAllocateCommandBuffers(vk.device, &allocInfo, &commandBuffer), "Failed to allocate temp command buffer.");

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin temp command buffer.");

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = layers;

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&barrier);

	vkCheck(vkEndCommandBuffer(commandBuffer), "Failed to end temp command buffer.");

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	vkCheck(vkQueueSubmit(vk.graphicsQ, 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit temp command buffer.");
	vkCheck(vkQueueWaitIdle(vk.graphicsQ), "Failed to wait for temp queue idle.");

	vkDestroyCommandPool(vk.device, commandPool, nullptr);
}

static void CreatePlaceholderImage(
	VulkanContext& vk,
	uint32_t arrayLayers,
	VkImageViewType viewType,
	VulkanUiRenderer::AllocatedImage& outImage) {
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageInfo.extent = { 1, 1, 1 };
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = arrayLayers;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocationCreateInfo{};
	allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	vkCheck(
		vmaCreateImage(
			vk.allocator,
			&imageInfo,
			&allocationCreateInfo,
			&outImage.image,
			&outImage.allocation,
			nullptr),
		"Failed to create placeholder UI image.");

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = outImage.image;
	viewInfo.viewType = viewType;
	viewInfo.format = imageInfo.format;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = arrayLayers;
	vkCheck(vkCreateImageView(vk.device, &viewInfo, nullptr, &outImage.view), "Failed to create placeholder UI image view.");

	TransitionImageToShaderRead(vk, outImage.image, arrayLayers);
}

static void CreateLinearSampler(VulkanContext& vk, VkSampler& sampler) {
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	vkCheck(vkCreateSampler(vk.device, &samplerInfo, nullptr, &sampler), "Failed to create UI sampler.");
}

static VkPipeline createGraphicsPipeline(VkDevice device, VkPipelineLayout layout, VkFormat format, const char* fragmentFile) {
	if (format == VK_FORMAT_UNDEFINED) {
		throw std::runtime_error("Cannot create UI pipeline with undefined target format.");
	}

	const std::vector<char> vertexCode = readShaderFile(kUiVertexShaderFile);
	const std::vector<char> fragmentCode = readShaderFile(fragmentFile);
	VkShaderModule vertexModule = createShaderModule(device, vertexCode);
	VkShaderModule fragmentModule = createShaderModule(device, fragmentCode);

	VkPipelineShaderStageCreateInfo shaderStages[2]{};
	shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStages[0].module = vertexModule;
	shaderStages[0].pName = "main";
	shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStages[1].module = fragmentModule;
	shaderStages[1].pName = "main";

	VkVertexInputBindingDescription binding{};
	binding.binding = 0;
	binding.stride = sizeof(UiQuadVertex);
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attributes[2]{};
	attributes[0].location = 0;
	attributes[0].binding = 0;
	attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
	attributes[0].offset = static_cast<uint32_t>(offsetof(UiQuadVertex, x));
	attributes[1].location = 1;
	attributes[1].binding = 0;
	attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
	attributes[1].offset = static_cast<uint32_t>(offsetof(UiQuadVertex, u));

	VkPipelineVertexInputStateCreateInfo vertexInput{};
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.vertexBindingDescriptionCount = 1;
	vertexInput.pVertexBindingDescriptions = &binding;
	vertexInput.vertexAttributeDescriptionCount = 2;
	vertexInput.pVertexAttributeDescriptions = attributes;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterization{};
	rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.depthClampEnable = VK_FALSE;
	rasterization.rasterizerDiscardEnable = VK_FALSE;
	rasterization.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization.cullMode = VK_CULL_MODE_NONE;
	rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterization.depthBiasEnable = VK_FALSE;
	rasterization.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisample{};
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample.sampleShadingEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState blendAttachment{};
	blendAttachment.blendEnable = VK_TRUE;
	blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	blendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo blendState{};
	blendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blendState.logicOpEnable = VK_FALSE;
	blendState.attachmentCount = 1;
	blendState.pAttachments = &blendAttachment;

	const VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	VkPipelineRenderingCreateInfo rendering{};
	rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	rendering.colorAttachmentCount = 1;
	rendering.pColorAttachmentFormats = &format;

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = &rendering;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInput;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterization;
	pipelineInfo.pMultisampleState = &multisample;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &blendState;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = layout;
	pipelineInfo.renderPass = VK_NULL_HANDLE;
	pipelineInfo.subpass = 0;

	VkPipeline pipeline = VK_NULL_HANDLE;
	const VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

	vkDestroyShaderModule(device, fragmentModule, nullptr);
	vkDestroyShaderModule(device, vertexModule, nullptr);
	vkCheck(result, "Failed to create UI graphics pipeline.");
	return pipeline;
}

static void DestroyPipelines(VkDevice device, VulkanUiRenderer::Pipelines& pipelines) {
	if (pipelines.solid != VK_NULL_HANDLE) {
		vkDestroyPipeline(device, pipelines.solid, nullptr);
		pipelines.solid = VK_NULL_HANDLE;
	}
	if (pipelines.msdf != VK_NULL_HANDLE) {
		vkDestroyPipeline(device, pipelines.msdf, nullptr);
		pipelines.msdf = VK_NULL_HANDLE;
	}
	if (pipelines.textured != VK_NULL_HANDLE) {
		vkDestroyPipeline(device, pipelines.textured, nullptr);
		pipelines.textured = VK_NULL_HANDLE;
	}
}

static void CreatePipelines(VkDevice device, VulkanUiRenderer::Pipelines& pipelines, VkFormat format) {
	pipelines.solid = createGraphicsPipeline(device, pipelines.layout, format, kUiSolidFragmentShaderFile);
	pipelines.msdf = createGraphicsPipeline(device, pipelines.layout, format, kUiMsdfFragmentShaderFile);
	pipelines.textured = createGraphicsPipeline(device, pipelines.layout, format, kUiTexturedFragmentShaderFile);
}

static void UpdateInstanceBufferDescriptor(const VulkanUiRenderer& renderer, VkDevice device) {
	VkDescriptorBufferInfo ssboInfo{};
	ssboInfo.buffer = renderer.instanceBuffer.buffer;
	ssboInfo.offset = 0;
	ssboInfo.range = renderer.instanceBuffer.size;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = renderer.descriptors.globalsSet;
	write.dstBinding = 0;
	write.dstArrayElement = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.pBufferInfo = &ssboInfo;
	vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

static void UpdateTextureDescriptors(const VulkanUiRenderer& renderer, VkDevice device) {
	VkDescriptorImageInfo fontAtlasInfo{};
	fontAtlasInfo.sampler = renderer.linearSampler;
	fontAtlasInfo.imageView = renderer.placeholderFontAtlas.view;
	fontAtlasInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	if (renderer.fontManager) {
		const FontManager::AtlasArrayResource& atlas = renderer.fontManager->atlasResource();
		if (atlas.view != VK_NULL_HANDLE && atlas.sampler != VK_NULL_HANDLE && atlas.layersUsed > 0u) {
			fontAtlasInfo.sampler = atlas.sampler;
			fontAtlasInfo.imageView = atlas.view;
		}
	}

	VkDescriptorImageInfo uiImageTemplate{};
	uiImageTemplate.sampler = renderer.linearSampler;
	uiImageTemplate.imageView = renderer.placeholderUiTexture.view;
	uiImageTemplate.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	std::vector<VkDescriptorImageInfo> uiImageInfos(renderer.maxUiImageDescriptors, uiImageTemplate);

	std::array<VkWriteDescriptorSet, 2> writes{};
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = renderer.descriptors.texturesSet;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &fontAtlasInfo;

	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = renderer.descriptors.texturesSet;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = renderer.maxUiImageDescriptors;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = uiImageInfos.data();

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

static void EnsureInstanceBufferCapacity(VulkanContext& vk, VulkanUiRenderer& renderer, size_t requiredInstances) {
	if (requiredInstances == 0) {
		return;
	}
	if (requiredInstances > (std::numeric_limits<VkDeviceSize>::max() / sizeof(UiInstance))) {
		throw std::runtime_error("UI instance count exceeds addressable buffer size.");
	}

	const VkDeviceSize requiredBytes = static_cast<VkDeviceSize>(requiredInstances) * sizeof(UiInstance);
	if (renderer.instanceBuffer.buffer != VK_NULL_HANDLE && renderer.instanceBuffer.size >= requiredBytes) {
		return;
	}

	VkDeviceSize newSize = renderer.instanceBuffer.size > 0 ? renderer.instanceBuffer.size : kInitialInstanceBufferBytes;
	while (newSize < requiredBytes) {
		newSize *= 2;
	}

	DestroyBuffer(vk, renderer.instanceBuffer);
	CreateMappedBuffer(vk, newSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, renderer.instanceBuffer);
	UpdateInstanceBufferDescriptor(renderer, vk.device);
}

static VkPipeline PipelineForType(const VulkanUiRenderer::Pipelines& pipelines, UiType type) {
	switch (type) {
		case UiType::Solid:
			return pipelines.solid;
		case UiType::Msdf:
			return pipelines.msdf;
		case UiType::Textured:
			return pipelines.textured;
		default:
			return pipelines.solid;
	}
}

static void FlushRun(VkCommandBuffer commandBuffer, const VulkanUiRenderer& renderer, VkExtent2D extent, const UiRun& run)
{
	if (run.instanceCount == 0)
		return;

	const VkRect2D scissor = ToVkRect2D(run.scissor, static_cast<int>(extent.width), static_cast<int>(extent.height));
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	const VkPipeline pipeline = PipelineForType(renderer.pipelines, run.type);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	UiPushConstants push{};
	push.viewportW = static_cast<float>(extent.width);
	push.viewportH = static_cast<float>(extent.height);
	push.instanceBaseIndex = run.firstInstance;
	vkCmdPushConstants(
		commandBuffer,
		renderer.pipelines.layout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0,
		static_cast<uint32_t>(sizeof(UiPushConstants)),
		&push);

	vkCmdDraw(commandBuffer, 4, run.instanceCount, 0, 0);
}

} // namespace

void VulkanUiRenderer::init(const FlowUi::AppConfig& config, VulkanContext& vk, VkFormat swapFormat) {
	if (vk.device == VK_NULL_HANDLE || vk.allocator == nullptr) {
		throw std::runtime_error("Vulkan device + allocator must be initialized before UI renderer init.");
	}
	if (swapFormat == VK_FORMAT_UNDEFINED) {
		throw std::runtime_error("Swapchain format must be valid before UI renderer init.");
	}

	destroy(vk);

	try {
		maxUiImageDescriptors = kDefaultMaxUiImageDescriptors;
		targetFormat = swapFormat;
		pointsToPixelsScale = std::max(0.0f, config.ui.fontScale) * (96.0f / 72.0f);
		if (pointsToPixelsScale <= 0.0f) {
			pointsToPixelsScale = 96.0f / 72.0f;
		}
		boundFontAtlasRevision = UINT32_MAX;

		CreateLinearSampler(vk, linearSampler);

		CreatePlaceholderImage(vk, 1, VK_IMAGE_VIEW_TYPE_2D_ARRAY, placeholderFontAtlas);
		CreatePlaceholderImage(vk, 1, VK_IMAGE_VIEW_TYPE_2D, placeholderUiTexture);

		VkDescriptorSetLayoutBinding globalsBinding{};
		globalsBinding.binding = 0;
		globalsBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		globalsBinding.descriptorCount = 1;
		globalsBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo set0Info{};
		set0Info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		set0Info.bindingCount = 1;
		set0Info.pBindings = &globalsBinding;
		vkCheck(vkCreateDescriptorSetLayout(vk.device, &set0Info, nullptr, &descriptors.set0), "Failed to create UI set0 layout.");

		std::array<VkDescriptorSetLayoutBinding, 2> textureBindings{};
		textureBindings[0].binding = 0;
		textureBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		textureBindings[0].descriptorCount = 1;
		textureBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		textureBindings[1].binding = 1;
		textureBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		textureBindings[1].descriptorCount = maxUiImageDescriptors;
		textureBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		const std::array<VkDescriptorBindingFlags, 2> bindingFlags = {
			0u,
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
		};
		VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
		bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
		bindingFlagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
		bindingFlagsInfo.pBindingFlags = bindingFlags.data();

		VkDescriptorSetLayoutCreateInfo set1Info{};
		set1Info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		set1Info.pNext = &bindingFlagsInfo;
		set1Info.bindingCount = static_cast<uint32_t>(textureBindings.size());
		set1Info.pBindings = textureBindings.data();
		vkCheck(vkCreateDescriptorSetLayout(vk.device, &set1Info, nullptr, &descriptors.set1), "Failed to create UI set1 layout.");

		std::array<VkDescriptorPoolSize, 2> poolSizes{};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSizes[0].descriptorCount = 1;
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = 1 + maxUiImageDescriptors;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.maxSets = 2;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		vkCheck(vkCreateDescriptorPool(vk.device, &poolInfo, nullptr, &descriptors.pool), "Failed to create UI descriptor pool.");

		VkDescriptorSetAllocateInfo set0AllocInfo{};
		set0AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		set0AllocInfo.descriptorPool = descriptors.pool;
		set0AllocInfo.descriptorSetCount = 1;
		set0AllocInfo.pSetLayouts = &descriptors.set0;
		vkCheck(vkAllocateDescriptorSets(vk.device, &set0AllocInfo, &descriptors.globalsSet), "Failed to allocate UI globals set.");

		VkDescriptorSetAllocateInfo set1AllocInfo{};
		set1AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		set1AllocInfo.descriptorPool = descriptors.pool;
		set1AllocInfo.descriptorSetCount = 1;
		set1AllocInfo.pSetLayouts = &descriptors.set1;
		vkCheck(vkAllocateDescriptorSets(vk.device, &set1AllocInfo, &descriptors.texturesSet), "Failed to allocate UI textures set.");

		const std::array<VkDescriptorSetLayout, 2> setLayouts = { descriptors.set0, descriptors.set1 };
		VkPushConstantRange pushRange{};
		pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushRange.offset = 0;
		pushRange.size = sizeof(UiPushConstants);

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
		pipelineLayoutInfo.pSetLayouts = setLayouts.data();
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushRange;
		vkCheck(vkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, nullptr, &pipelines.layout), "Failed to create UI pipeline layout.");

		CreatePipelines(vk.device, pipelines, targetFormat);

		const std::array<UiQuadVertex, 4> quadVertices = {
			UiQuadVertex{ 0.0f, 0.0f, 0.0f, 0.0f },
			UiQuadVertex{ 1.0f, 0.0f, 1.0f, 0.0f },
			UiQuadVertex{ 0.0f, 1.0f, 0.0f, 1.0f },
			UiQuadVertex{ 1.0f, 1.0f, 1.0f, 1.0f },
		};
		CreateMappedBuffer(
			vk,
			static_cast<VkDeviceSize>(quadVertices.size() * sizeof(UiQuadVertex)),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			quadVertexBuffer);
		UploadBytesToMappedBuffer(
			vk,
			quadVertexBuffer,
			quadVertices.data(),
			static_cast<VkDeviceSize>(quadVertices.size() * sizeof(UiQuadVertex)));

		CreateMappedBuffer(vk, kInitialInstanceBufferBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, instanceBuffer);
		UpdateInstanceBufferDescriptor(*this, vk.device);
		UpdateTextureDescriptors(*this, vk.device);

		instancesScratch.clear();
		runsScratch.clear();
		instancesScratch.reserve(4096);
		runsScratch.reserve(256);
	} catch (...) {
		destroy(vk);
		throw;
	}
}

void VulkanUiRenderer::setFontManager(const FontManager* manager) {
	fontManager = manager;
	boundFontAtlasRevision = UINT32_MAX;
}

void VulkanUiRenderer::destroy(VulkanContext& vk)
{
	instancesScratch.clear();
	runsScratch.clear();

	if (vk.device == VK_NULL_HANDLE || vk.allocator == nullptr) {
		pipelines = Pipelines{};
		descriptors = Descriptors{};
		instanceBuffer = AllocatedBuffer{};
		quadVertexBuffer = AllocatedBuffer{};
		placeholderFontAtlas = AllocatedImage{};
		placeholderUiTexture = AllocatedImage{};
		linearSampler = VK_NULL_HANDLE;
		targetFormat = VK_FORMAT_UNDEFINED;
		fontManager = nullptr;
		boundFontAtlasRevision = UINT32_MAX;
		pointsToPixelsScale = 96.0f / 72.0f;
		return;
	}

	DestroyPipelines(vk.device, pipelines);

	if (pipelines.layout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(vk.device, pipelines.layout, nullptr);
		pipelines.layout = VK_NULL_HANDLE;
	}

	if (descriptors.pool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(vk.device, descriptors.pool, nullptr);
		descriptors.pool = VK_NULL_HANDLE;
	}
	descriptors.globalsSet = VK_NULL_HANDLE;
	descriptors.texturesSet = VK_NULL_HANDLE;

	if (descriptors.set0 != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(vk.device, descriptors.set0, nullptr);
		descriptors.set0 = VK_NULL_HANDLE;
	}
	if (descriptors.set1 != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(vk.device, descriptors.set1, nullptr);
		descriptors.set1 = VK_NULL_HANDLE;
	}

	DestroyBuffer(vk, instanceBuffer);
	DestroyBuffer(vk, quadVertexBuffer);
	DestroyImage(vk, placeholderFontAtlas);
	DestroyImage(vk, placeholderUiTexture);

	if (linearSampler != VK_NULL_HANDLE) {
		vkDestroySampler(vk.device, linearSampler, nullptr);
		linearSampler = VK_NULL_HANDLE;
	}

	targetFormat = VK_FORMAT_UNDEFINED;
	fontManager = nullptr;
	boundFontAtlasRevision = UINT32_MAX;
	pointsToPixelsScale = 96.0f / 72.0f;
}

void VulkanUiRenderer::onSwapchainFormatChanged(VulkanContext& vk, VkFormat newFormat)
{
	if (vk.device == VK_NULL_HANDLE || newFormat == VK_FORMAT_UNDEFINED) {
		return;
	}
	if (newFormat == targetFormat && pipelines.solid != VK_NULL_HANDLE && pipelines.msdf != VK_NULL_HANDLE &&
		pipelines.textured != VK_NULL_HANDLE) {
		return;
	}

	DestroyPipelines(vk.device, pipelines);
	targetFormat = newFormat;
	CreatePipelines(vk.device, pipelines, targetFormat);
}

void VulkanUiRenderer::render(VulkanContext& vk, VkCommandBuffer cmd, const Clay_RenderCommandArray& renderCommands, VkExtent2D extent, VkImageView targetView)
{
	if (cmd == VK_NULL_HANDLE || targetView == VK_NULL_HANDLE) {
		return;
	}
	if (pipelines.layout == VK_NULL_HANDLE || pipelines.solid == VK_NULL_HANDLE || instanceBuffer.buffer == VK_NULL_HANDLE) {
		return;
	}

	uint32_t latestFontAtlasRevision = 0u;
	if (fontManager) {
		latestFontAtlasRevision = fontManager->atlasResource().bindingRevision;
	}
	if (latestFontAtlasRevision != boundFontAtlasRevision) {
		UpdateTextureDescriptors(*this, vk.device);
		boundFontAtlasRevision = latestFontAtlasRevision;
	}

	BuildInstancesAndRunsFromClay(renderCommands, extent, fontManager, pointsToPixelsScale, instancesScratch, runsScratch);
	if (instancesScratch.empty() || runsScratch.empty()) {
		return;
	}

	EnsureInstanceBufferCapacity(vk, *this, instancesScratch.size());
	UploadBytesToMappedBuffer(
		vk,
		instanceBuffer,
		instancesScratch.data(),
		static_cast<VkDeviceSize>(instancesScratch.size() * sizeof(UiInstance)));

	VkRenderingAttachmentInfo colorAttachment{};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageView = targetView;
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea = { { 0, 0 }, extent };
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;

	vkCmdBeginRendering(cmd, &renderingInfo);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(extent.width);
	viewport.height = static_cast<float>(extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D fullScissor{};
	fullScissor.offset = { 0, 0 };
	fullScissor.extent = extent;
	vkCmdSetScissor(cmd, 0, 1, &fullScissor);

	const VkBuffer vertexBuffer = quadVertexBuffer.buffer;
	const VkDeviceSize vertexOffset = 0;
	vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &vertexOffset);

	const VkDescriptorSet sets[2] = { descriptors.globalsSet, descriptors.texturesSet };
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.layout, 0, 2, sets, 0, nullptr);

	for (const UiRun& run : runsScratch) {
		FlushRun(cmd, *this, extent, run);
	}

	vkCmdEndRendering(cmd);
}
